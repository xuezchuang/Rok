#include "RokGameNetSubsystem.h"
#include "RokAudioRouterSubsystem.h"
#include "RokServerBuildQueue.h"
#include "RokServerInventorySubsystem.h"
#include "RokUnityEconomyCatalogSubsystem.h"
#include "RokServerTaskSubsystem.h"

#include "Containers/Queue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "IPAddress.h"
#include "Misc/Base64.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokDataPaths.h"
#include "RokSkynetCrypt.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/TopLevelAssetPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogRokGameNet, Log, All);

namespace
{
	// Unity Assets/Protocol/Protocol.sproto, copied byte for byte.
	const TCHAR* SchemaRelativePath = TEXT("Protocol/Protocol.sproto");
	// NetProxy.cs m_game_session_len / m_game_compressed_len.
	constexpr int32 GameSessionLength = 4;
	constexpr int32 CompressedFlagLength = 1;
	// NetProxy.UpdateHeart: a Role_Heart every 15 s.
	constexpr double HeartIntervalSeconds = 15.0;
	constexpr double SocketWaitSeconds = 0.02;
	constexpr double LoginStepTimeoutSeconds = 10.0;

	void PlayCityWriteSfx(UGameInstance* Instance, int32 EventId)
	{
		if (auto* Audio = Instance ? Instance->GetSubsystem<URokAudioRouterSubsystem>() : nullptr)
			Audio->PlaySourceSfx(EventId);
	}

	FString B64(const FString& Text)
	{
		return FBase64::Encode(RokSkynetCrypt::Utf8Bytes(Text));
	}

	FString B64Decode(const FString& Text)
	{
		TArray<uint8> Bytes;
		if (!FBase64::Decode(Text.TrimStartAndEnd(), Bytes)) return FString();
		const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Converter.Length(), Converter.Get());
	}

	// Keeps ASCII and drops anything else: the schema is GBK, and only its comments are not ASCII.
	FString AsciiOnly(const TArray<uint8>& Bytes)
	{
		FString Out;
		Out.Reserve(Bytes.Num());
		for (const uint8 Byte : Bytes)
		{
			Out.AppendChar(Byte < 0x80 ? TCHAR(Byte) : TCHAR(' '));
		}
		return Out;
	}

	FString SprotoRowKey(const TSharedPtr<FJsonObject>& Row, const FString& FieldName)
	{
		if (!Row.IsValid()) return FString();
		const TSharedPtr<FJsonValue> Value = Row->TryGetField(FieldName);
		if (!Value.IsValid()) return FString();
		int64 Number = 0;
		if (Value->TryGetNumber(Number)) return FString::Printf(TEXT("%lld"), Number);
		FString String;
		return Value->TryGetString(String) ? String : FString();
	}

	void MergeSprotoObject(const FRokSprotoSchema& Schema, const FRokSprotoStruct& Type,
		const TSharedPtr<FJsonObject>& Into, const TSharedPtr<FJsonObject>& From, bool bProtocolRoot)
	{
		for (const FRokSprotoField& Field : Type.Fields)
		{
			const TSharedPtr<FJsonValue> Incoming = From->TryGetField(Field.Name);
			if (!Incoming.IsValid()) continue;
			const FRokSprotoStruct* ChildType = Field.Type == ERokSprotoType::Struct
				? Schema.GetStruct(Field.StructIndex) : nullptr;
			// HospitalLogic sends the entire soldier selection for each treatment queue.
			if (Field.Name == TEXT("treatmentSoldiers"))
			{
				Into->SetField(Field.Name, Incoming);
				continue;
			}
			if (Field.bArray && ChildType && !Field.MapKey.IsEmpty())
			{
				const TArray<TSharedPtr<FJsonValue>>* NewRows = nullptr;
				if (!Incoming->TryGetArray(NewRows)) continue;
				const TSharedPtr<FJsonObject>* ExistingMap = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* ExistingRows = nullptr;
				// Look up the existing map even at the protocol root. The push is a delta;
				// short-circuiting on bProtocolRoot would discard all untouched rows.
				const bool bHasExistingMap = Into->TryGetObjectField(Field.Name, ExistingMap);
				const bool bObjectShape = bProtocolRoot || bHasExistingMap;
				if (bObjectShape)
				{
					TSharedPtr<FJsonObject> Map = ExistingMap ? *ExistingMap : MakeShared<FJsonObject>();
					if (!ExistingMap && Into->TryGetArrayField(Field.Name, ExistingRows))
					{
						for (const TSharedPtr<FJsonValue>& OldValue : *ExistingRows)
						{
							const TSharedPtr<FJsonObject>* OldRow = nullptr;
							if (OldValue.IsValid() && OldValue->TryGetObject(OldRow))
							{
								const FString Key = SprotoRowKey(*OldRow, Field.MapKey);
								if (!Key.IsEmpty()) Map->SetObjectField(Key, *OldRow);
							}
						}
					}
					for (const TSharedPtr<FJsonValue>& NewValue : *NewRows)
					{
						const TSharedPtr<FJsonObject>* NewRow = nullptr;
						if (!NewValue.IsValid() || !NewValue->TryGetObject(NewRow)) continue;
						const FString Key = SprotoRowKey(*NewRow, Field.MapKey);
						if (Key.IsEmpty()) continue;
						const TSharedPtr<FJsonObject>* OldRow = nullptr;
						TSharedPtr<FJsonObject> Target = Map->TryGetObjectField(Key, OldRow) ? *OldRow : MakeShared<FJsonObject>();
						if (bProtocolRoot && Field.Name == TEXT("buildingInfo") && Field.MapKey == TEXT("buildingIndex"))
						{
							int64 IncomingVersion = 0, AppliedVersion = 0;
							if (RokSprotoJson::TryGetInt64(*NewRow, TEXT("version"), IncomingVersion)
								&& IncomingVersion > 0
								&& RokSprotoJson::TryGetInt64(Target, TEXT("version"), AppliedVersion)
								&& IncomingVersion < AppliedVersion) continue;
						}
						MergeSprotoObject(Schema, *ChildType, Target, *NewRow, false);
						Map->SetObjectField(Key, Target);
					}
					Into->SetObjectField(Field.Name, Map);
				}
				else
				{
					TArray<TSharedPtr<FJsonValue>> Rows;
					if (Into->TryGetArrayField(Field.Name, ExistingRows)) Rows = *ExistingRows;
					for (const TSharedPtr<FJsonValue>& NewValue : *NewRows)
					{
						const TSharedPtr<FJsonObject>* NewRow = nullptr;
						if (!NewValue.IsValid() || !NewValue->TryGetObject(NewRow)) continue;
						const FString Key = SprotoRowKey(*NewRow, Field.MapKey);
						if (Key.IsEmpty()) continue;
						TSharedPtr<FJsonObject> Target;
						for (const TSharedPtr<FJsonValue>& OldValue : Rows)
						{
							const TSharedPtr<FJsonObject>* OldRow = nullptr;
							if (OldValue.IsValid() && OldValue->TryGetObject(OldRow)
								&& SprotoRowKey(*OldRow, Field.MapKey) == Key)
							{
								Target = *OldRow;
								break;
							}
						}
						if (!Target.IsValid())
						{
							Target = MakeShared<FJsonObject>();
							Rows.Add(MakeShared<FJsonValueObject>(Target));
						}
						MergeSprotoObject(Schema, *ChildType, Target, *NewRow, false);
					}
					Into->SetArrayField(Field.Name, Rows);
				}
			}
			else if (!Field.bArray && ChildType)
			{
				const TSharedPtr<FJsonObject>* NewObject = nullptr;
				if (!Incoming->TryGetObject(NewObject)) continue;
				const TSharedPtr<FJsonObject>* OldObject = nullptr;
				TSharedPtr<FJsonObject> Target = Into->TryGetObjectField(Field.Name, OldObject)
					? *OldObject : MakeShared<FJsonObject>();
				MergeSprotoObject(Schema, *ChildType, Target, *NewObject, false);
				Into->SetObjectField(Field.Name, Target);
			}
			else
			{
				// Unkeyed arrays retain the protocol's replace-on-push behavior.
				Into->SetField(Field.Name, Incoming);
			}
		}
	}
}

// Socket side of SprotoSocketAp: runs the line-based login handshake, the 2-byte framed
// game redirect, then pumps frames both ways. Frames reach the game thread undecrypted.
class FRokGameNetWorker : public FRunnable
{
public:
	struct FConfig
	{
		FString LoginHost;
		int32 LoginPort = 0;
		FString GameHostOverride;
		// iggid:token:platform:language:clientIp:gameNode (NetProxy EAuth2).
		FString LoginToken;
	};

	enum class EEventKind : uint8 { Log, Failed, LoggedIn, Frame, Closed };

	struct FEvent
	{
		EEventKind Kind = EEventKind::Log;
		FString Text;
		TArray<uint8> Bytes;
		FString ServerName;
		FString Uid;
	};

	explicit FRokGameNetWorker(const FConfig& InConfig) : Config(InConfig) {}

	virtual ~FRokGameNetWorker() override
	{
		CloseSocket();
	}

	virtual uint32 Run() override
	{
		FString Error;
		if (!RunLogin(Error))
		{
			Post(EEventKind::Failed, Error);
			CloseSocket();
			return 1;
		}
		Pump();
		CloseSocket();
		Post(EEventKind::Closed, bStop ? TEXT("stopped") : TEXT("connection closed"));
		return 0;
	}

	virtual void Stop() override
	{
		bStop = true;
	}

	TQueue<FEvent, EQueueMode::Mpsc> Events;
	TQueue<TArray<uint8>, EQueueMode::Mpsc> Outbound;

private:
	FConfig Config;
	FThreadSafeBool bStop = false;
	FSocket* Socket = nullptr;
	TArray<uint8> Received;

	void Post(EEventKind Kind, const FString& Text)
	{
		FEvent Event;
		Event.Kind = Kind;
		Event.Text = Text;
		Events.Enqueue(MoveTemp(Event));
	}

	void CloseSocket()
	{
		if (Socket)
		{
			Socket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
			Socket = nullptr;
		}
		Received.Reset();
	}

	bool Open(const FString& Host, int32 Port, FString& OutError)
	{
		CloseSocket();
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		TSharedPtr<FInternetAddr> Address = Sockets->GetAddressFromString(Host);
		if (!Address.IsValid())
		{
			const FAddressInfoResult Info = Sockets->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None, ESocketType::SOCKTYPE_Streaming);
			if (Info.ReturnCode != SE_NO_ERROR || Info.Results.Num() == 0)
			{
				OutError = FString::Printf(TEXT("cannot resolve %s"), *Host);
				return false;
			}
			Address = Info.Results[0].Address;
		}
		Address->SetPort(Port);
		Socket = Sockets->CreateSocket(NAME_Stream, TEXT("RokGameNet"), Address->GetProtocolType());
		if (!Socket || !Socket->Connect(*Address))
		{
			OutError = FString::Printf(TEXT("cannot connect to %s:%d"), *Host, Port);
			return false;
		}
		Socket->SetNoDelay(true);
		return true;
	}

	bool SendAll(const uint8* Data, int32 Size)
	{
		int32 Offset = 0;
		while (Offset < Size)
		{
			int32 Sent = 0;
			if (!Socket->Send(Data + Offset, Size - Offset, Sent) || Sent <= 0) return false;
			Offset += Sent;
		}
		return true;
	}

	bool SendLine(const FString& Line)
	{
		TArray<uint8> Bytes = RokSkynetCrypt::Utf8Bytes(Line);
		Bytes.Add('\n');
		return SendAll(Bytes.GetData(), Bytes.Num());
	}

	bool SendFramed(const TArray<uint8>& Payload)
	{
		if (Payload.Num() > 0xffff) return false;
		TArray<uint8> Frame;
		Frame.Add(uint8((Payload.Num() >> 8) & 0xff));
		Frame.Add(uint8(Payload.Num() & 0xff));
		Frame.Append(Payload);
		return SendAll(Frame.GetData(), Frame.Num());
	}

	// Reads what is available; false once the peer closed or the socket failed.
	bool ReceiveSome(double WaitSeconds)
	{
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(WaitSeconds)))
		{
			return Socket->GetConnectionState() == SCS_Connected;
		}
		uint8 Buffer[16 * 1024];
		int32 Read = 0;
		if (!Socket->Recv(Buffer, sizeof(Buffer), Read) || Read <= 0) return false;
		Received.Append(Buffer, Read);
		return true;
	}

	bool ReadLine(FString& OutLine, FString& OutError)
	{
		const double Deadline = FPlatformTime::Seconds() + LoginStepTimeoutSeconds;
		while (!bStop)
		{
			const int32 Newline = Received.Find('\n');
			if (Newline != INDEX_NONE)
			{
				const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Received.GetData()), Newline);
				OutLine = FString(Converter.Length(), Converter.Get()).TrimEnd();
				Received.RemoveAt(0, Newline + 1);
				return true;
			}
			if (FPlatformTime::Seconds() > Deadline) { OutError = TEXT("login server timed out"); return false; }
			if (!ReceiveSome(0.1)) { OutError = TEXT("login server closed the connection"); return false; }
		}
		OutError = TEXT("stopped");
		return false;
	}

	bool TakeFrame(TArray<uint8>& OutFrame)
	{
		if (Received.Num() < 2) return false;
		const int32 Length = int32(Received[0]) << 8 | int32(Received[1]);
		if (Received.Num() < Length + 2) return false;
		OutFrame = TArray<uint8>(Received.GetData() + 2, Length);
		Received.RemoveAt(0, Length + 2);
		return true;
	}

	bool RunLogin(FString& OutError)
	{
		// 1-2: challenge, then our DH public key.
		if (!Open(Config.LoginHost, Config.LoginPort, OutError)) return false;
		FString Line;
		if (!ReadLine(Line, OutError)) return false;
		TArray<uint8> Challenge;
		if (!FBase64::Decode(Line, Challenge) || Challenge.Num() != 8) { OutError = TEXT("bad challenge"); return false; }
		const TArray<uint8> ClientKey = RokSkynetCrypt::RandomKey();
		if (!SendLine(FBase64::Encode(RokSkynetCrypt::DhExchange(ClientKey)))) { OutError = TEXT("send failed"); return false; }

		// 4-7: server key -> secret, hmac of the challenge, DES token.
		if (!ReadLine(Line, OutError)) return false;
		TArray<uint8> ServerKey;
		if (!FBase64::Decode(Line, ServerKey) || ServerKey.Num() != 8) { OutError = TEXT("bad server key"); return false; }
		const TArray<uint8> Secret = RokSkynetCrypt::DhSecret(ServerKey, ClientKey);
		if (Secret.Num() != 8) { OutError = TEXT("bad DH secret"); return false; }
		if (!SendLine(FBase64::Encode(RokSkynetCrypt::Hmac64(Challenge, Secret)))) { OutError = TEXT("send failed"); return false; }
		const TArray<uint8> TokenBytes = RokSkynetCrypt::Utf8Bytes(Config.LoginToken);
		TArray<uint8> EncodedToken;
		RokSkynetCrypt::DesEncode(Secret, TokenBytes.GetData(), TokenBytes.Num(), EncodedToken);
		if (!SendLine(FBase64::Encode(EncodedToken))) { OutError = TEXT("send failed"); return false; }

		// 10: "200 base64(b64(iggid)@b64(subid)@b64(host)@b64(port)@b64(realIp)@b64(node)@b64(uid))".
		if (!ReadLine(Line, OutError)) return false;
		if (!Line.StartsWith(TEXT("200 ")))
		{
			OutError = FString::Printf(TEXT("login refused: %s"), *Line.Left(3));
			return false;
		}
		TArray<FString> Parts;
		B64Decode(Line.Mid(4)).ParseIntoArray(Parts, TEXT("@"), false);
		if (Parts.Num() < 7) { OutError = TEXT("bad login result"); return false; }
		const FString SubId = B64Decode(Parts[1]);
		FString GameHost = B64Decode(Parts[2]);
		const int32 GamePort = FCString::Atoi(*B64Decode(Parts[3]));
		const FString ServerName = B64Decode(Parts[5]);
		const FString Uid = B64Decode(Parts[6]);
		if (!Config.GameHostOverride.IsEmpty()) GameHost = Config.GameHostOverride;
		Post(EEventKind::Log, FString::Printf(TEXT("login server accepted: node=%s game=%s:%d"), *ServerName, *GameHost, GamePort));

		// Redirect: base64(uid)@base64(server)#base64(subid):index:base64(hmac64(hashkey(text), secret)).
		if (!Open(GameHost, GamePort, OutError)) return false;
		const FString Text = FString::Printf(TEXT("%s@%s#%s:%d"), *B64(Uid), *B64(ServerName), *B64(SubId), 1);
		const FString Hmac = FBase64::Encode(RokSkynetCrypt::Hmac64(RokSkynetCrypt::HashKey(RokSkynetCrypt::Utf8Bytes(Text)), Secret));
		if (!SendFramed(RokSkynetCrypt::Utf8Bytes(Text + TEXT(":") + Hmac))) { OutError = TEXT("send failed"); return false; }
		const double Deadline = FPlatformTime::Seconds() + LoginStepTimeoutSeconds;
		TArray<uint8> Reply;
		while (!TakeFrame(Reply))
		{
			if (bStop) { OutError = TEXT("stopped"); return false; }
			if (FPlatformTime::Seconds() > Deadline) { OutError = TEXT("game server timed out"); return false; }
			if (!ReceiveSome(0.1)) { OutError = TEXT("game server closed the connection"); return false; }
		}
		const FUTF8ToTCHAR ReplyText(reinterpret_cast<const ANSICHAR*>(Reply.GetData()), Reply.Num());
		const FString ReplyString(ReplyText.Length(), ReplyText.Get());
		if (!ReplyString.StartsWith(TEXT("200")))
		{
			OutError = FString::Printf(TEXT("game server refused: %s"), *ReplyString.Left(24));
			return false;
		}

		FEvent Event;
		Event.Kind = EEventKind::LoggedIn;
		Event.Bytes = Secret;
		Event.ServerName = ServerName;
		Event.Uid = Uid;
		Events.Enqueue(MoveTemp(Event));
		return true;
	}

	void Pump()
	{
		while (!bStop)
		{
			TArray<uint8> Out;
			while (Outbound.Dequeue(Out))
			{
				if (!SendAll(Out.GetData(), Out.Num())) return;
			}
			if (!ReceiveSome(SocketWaitSeconds)) return;
			TArray<uint8> Frame;
			while (TakeFrame(Frame))
			{
				FEvent Event;
				Event.Kind = EEventKind::Frame;
				Event.Bytes = MoveTemp(Frame);
				Events.Enqueue(MoveTemp(Event));
			}
		}
	}
};

bool URokGameNetSubsystem::ShouldAutoConnect(const TCHAR* CommandLine, bool bAutomation, bool bDefaultConnect,
	const FString& ProjectUserConfig)
{
	if (bAutomation || FParse::Param(CommandLine, TEXT("RokNoNet")) || FParse::Param(CommandLine, TEXT("RokLocalOnly")))
	{
		return false;
	}
	if (FParse::Param(CommandLine, TEXT("RokNet"))) return true;

	// A running editor can cache the merged Game config and recreate the settings CDO
	// during hot reload. Read the per-project user override again for each Play session.
	if (!ProjectUserConfig.IsEmpty() && IFileManager::Get().FileExists(*ProjectUserConfig))
	{
		FConfigFile UserSettings;
		UserSettings.Read(ProjectUserConfig);
		bool bUserConnect = false;
		if (UserSettings.GetBool(TEXT("/Script/Rok.RokGameNetSettings"), TEXT("bConnectOnStart"), bUserConnect))
		{
			return bUserConnect;
		}
	}
	return bDefaultConnect;
}

bool URokGameNetSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if WITH_EDITOR
	// Hot Reload can leave the old native class discoverable by the subsystem collection.
	// Only the class registered at the original path may own the live connection.
	static const FTopLevelAssetPath GameNetClassPath(TEXT("/Script/Rok"), TEXT("RokGameNetSubsystem"));
	if (FTopLevelAssetPath(GetClass()) != GameNetClassPath)
	{
		return false;
	}
#endif
	return Super::ShouldCreateSubsystem(Outer);
}

void URokGameNetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	MergedState = MakeShared<FJsonObject>();
	ResetWorldMapAoi();
	#if !UE_BUILD_SHIPPING
	FString ReplayPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("RokNetReplay="), ReplayPath))
	{
		FString Error;
		if (!FParse::Param(FCommandLine::Get(), TEXT("RokNoNet")))
		{
			Fail(TEXT("-RokNetReplay requires -RokNoNet"));
		}
		else if (!StartSnapshotReplay(ReplayPath, Error))
		{
			Fail(TEXT("snapshot replay: ") + Error);
		}
		return;
	}
	#endif
	const URokGameNetSettings* Settings = GetDefault<URokGameNetSettings>();
	FString ProjectUserConfig;
#if WITH_EDITOR
	if (GIsEditor) ProjectUserConfig = FPaths::ProjectConfigDir() / TEXT("UserGame.ini");
#endif
	if (ShouldAutoConnect(FCommandLine::Get(), GIsAutomationTesting, Settings->bConnectOnStart, ProjectUserConfig))
	{
		FString Error;
		if (!Connect(Error))
		{
			UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net not started: %s"), *Error);
		}
	}
}

bool URokGameNetSubsystem::StartSnapshotReplay(const FString& Path, FString& OutError)
{
#if UE_BUILD_SHIPPING
	OutError = TEXT("snapshot replay is unavailable in Shipping");
	return false;
#else
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = TEXT("cannot read ") + Path;
		return false;
	}
	TSharedPtr<FJsonObject> Snapshot;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Snapshot) || !Snapshot.IsValid())
	{
		OutError = TEXT("invalid JSON in ") + Path;
		return false;
	}
	const TSharedPtr<FJsonObject>* StateByProtocol = nullptr;
	if (!Snapshot->TryGetObjectField(TEXT("state_by_protocol"), StateByProtocol))
	{
		OutError = TEXT("missing state_by_protocol");
		return false;
	}
	int64 ServerMilliseconds = 0;
	if (!RokSprotoJson::TryGetInt64(Snapshot, TEXT("replay_server_time_ms"), ServerMilliseconds))
	{
		const TSharedPtr<FJsonObject>* RolePush = nullptr;
		const TSharedPtr<FJsonObject>* RoleInfo = nullptr;
		if (!(*StateByProtocol)->TryGetObjectField(TEXT("Role_RoleInfo"), RolePush)
			|| !(*RolePush)->TryGetObjectField(TEXT("roleInfo"), RoleInfo)
			|| !RokSprotoJson::TryGetInt64(*RoleInfo, TEXT("serverTime"), ServerMilliseconds))
		{
			OutError = TEXT("missing Role_RoleInfo.roleInfo.serverTime or replay_server_time_ms");
			return false;
		}
	}
	ServerClock.Reset();
	if (!ServerClock.SeedFromServerMilliseconds(ServerMilliseconds, FPlatformTime::Seconds()))
	{
		OutError = TEXT("invalid Unix-millisecond server time");
		return false;
	}
	PendingHeartSends.Reset();
	bLoggedFirstHeartSample = false;
	MergedState = *StateByProtocol;
	// New dumps contain a keyed active collection. Old dumps only retained the
	// last Map_ObjectInfo delta, which cannot represent the AOI at replay time.
	const TSharedPtr<FJsonObject>* MapSlot = nullptr;
	const TSharedPtr<FJsonObject>* ReplayObjects = nullptr;
	TSharedPtr<FJsonObject> ActiveObjects;
	if (MergedState->TryGetObjectField(TEXT("Map_ObjectInfo"), MapSlot)
		&& (*MapSlot)->TryGetObjectField(TEXT("mapObjectInfo"), ReplayObjects)
		&& !(*ReplayObjects)->HasField(TEXT("objectId")))
	{
		ActiveObjects = *ReplayObjects;
	}
	ResetWorldMapAoi();
	if (ActiveObjects.IsValid())
	{
		WorldMapObjects = ActiveObjects;
		TSharedPtr<FJsonObject> ActiveSlot = MakeShared<FJsonObject>();
		ActiveSlot->SetObjectField(TEXT("mapObjectInfo"), WorldMapObjects);
		MergedState->SetObjectField(TEXT("Map_ObjectInfo"), ActiveSlot);
		WorldMapAoiRevision = WorldMapObjects->Values.Num();
	}
	RokSprotoJson::TryGetInt64(Snapshot, TEXT("rid"), RoleId);
	bSnapshotReplay = true;
	LastError.Reset();
	UE_LOG(LogRokGameNet, Log, TEXT("Rok game net source=snapshot_replay path=%s rid=%lld serverTimeMs=%lld"),
		*Path, RoleId, ServerMilliseconds);
	SetState(ERokGameNetState::RoleReady);
	return true;
#endif
}

void URokGameNetSubsystem::Deinitialize()
{
	Disconnect(TEXT("shutdown"));
	Super::Deinitialize();
}

bool URokGameNetSubsystem::LoadSchema(FString& OutError)
{
	if (Schema.IsValid()) return true;
	const FString Path = RokDataPaths::Resolve(SchemaRelativePath);
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = TEXT("missing protocol schema ") + Path;
		return false;
	}
	if (!Schema.Parse(AsciiOnly(Bytes), OutError))
	{
		OutError = TEXT("protocol schema: ") + OutError;
		return false;
	}
	Rpc = MakeUnique<FRokSprotoRpc>(Schema);
	return true;
}

bool URokGameNetSubsystem::LoadAccount(FString& OutIggId, FString& OutToken, FString& OutError) const
{
	const URokGameNetSettings* Settings = GetDefault<URokGameNetSettings>();
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), Settings->AccountFile);
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = TEXT("missing account file ") + Settings->AccountFile;
		return false;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		FString Key, Value;
		if (!Line.Split(TEXT("="), &Key, &Value)) continue;
		Key.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();
		if (Key == TEXT("IggId")) OutIggId = Value;
		else if (Key == TEXT("AccessToken")) OutToken = Value;
	}
	if (OutIggId.IsEmpty() || OutToken.IsEmpty())
	{
		OutError = TEXT("account file needs IggId= and AccessToken=");
		return false;
	}
	return true;
}

bool URokGameNetSubsystem::Connect(FString& OutError)
{
	if (bSnapshotReplay)
	{
		OutError = TEXT("snapshot replay is active");
		return false;
	}
#if !UE_BUILD_SHIPPING
	FString ReplayPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("RokNetReplay="), ReplayPath))
	{
		OutError = TEXT("network connection disabled by -RokNetReplay");
		return false;
	}
#endif
	if (Worker.IsValid())
	{
		OutError = TEXT("already connected");
		return false;
	}
	ServerClock.Reset();
	PendingHeartSends.Reset();
	bLoggedFirstHeartSample = false;
	if (!LoadSchema(OutError)) return false;
	FString IggId, Token;
	if (!LoadAccount(IggId, Token, OutError)) return false;

	const URokGameNetSettings* Settings = GetDefault<URokGameNetSettings>();
	FRokGameNetWorker::FConfig Config;
	Config.LoginHost = Settings->LoginHost;
	Config.LoginPort = Settings->LoginPort;
	Config.GameHostOverride = Settings->GameHostOverride;
	Config.LoginToken = FString::Printf(TEXT("%s:%s:%d:%d:%s:%s"), *IggId, *Token, Settings->Platform,
		Settings->Language, *Settings->ClientIp, *Settings->GameNode);

	Rpc = MakeUnique<FRokSprotoRpc>(Schema);
	MergedState = MakeShared<FJsonObject>();
	SessionMail.Reset();
	BattleReportMail.Reset();
	BattleReportRequests.Reset();
	FailedBattleReportDownloads.Reset();
	RecentBattleDamage.Reset();
	++BattleReportRevision;
	++BattleReportSession;
	ResetWorldMapAoi();
	MessageCounts.Reset();
	MessageOrder.Reset();
	PendingInnerByOuter.Reset();
	bCityWriteNeedsReconnect = false;
	bTaskClaimNeedsReconnect = false;
	bVipClaimNeedsReconnect = false;
	bHeroWriteNeedsReconnect = false;
	bArmyDispatchNeedsReconnect = false;
	bGatherRecallNeedsReconnect = false;
	bTrainingWriteNeedsReconnect = false;
	bResearchWriteNeedsReconnect = false;
	bHospitalWriteNeedsReconnect = false;
	SprotoSession = 0;
	GameSession = 0;
	RoleId = 0;
	LastError.Reset();

	Worker = MakeShared<FRokGameNetWorker, ESPMode::ThreadSafe>(Config);
	WorkerThread = FRunnableThread::Create(Worker.Get(), TEXT("RokGameNet"));
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &URokGameNetSubsystem::Tick));
	UE_LOG(LogRokGameNet, Log, TEXT("Rok game net connecting: login=%s:%d node=%s"), *Settings->LoginHost, Settings->LoginPort, *Settings->GameNode);
	SetState(ERokGameNetState::LoggingIn);
	return true;
}

void URokGameNetSubsystem::Disconnect(const FString& Reason)
{
	const bool bWasSnapshotReplay = bSnapshotReplay;
	if (bSnapshotReplay)
	{
		UE_LOG(LogRokGameNet, Log, TEXT("Rok game net source=snapshot_replay stopped: %s"), *Reason);
		bSnapshotReplay = false;
	}
	ServerClock.Reset();
	PendingHeartSends.Reset();
	bLoggedFirstHeartSample = false;
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	if (WorkerThread)
	{
		WorkerThread->Kill(true);
		delete WorkerThread;
		WorkerThread = nullptr;
	}
	Worker.Reset();
	Secret.Reset();
	// Server mail belongs to the connection that supplied it, including when
	// Disconnect is called again after the state already became Offline/Failed.
	SessionMail.Reset();
	++BattleReportRevision;
	++BattleReportSession;
	if (State != ERokGameNetState::Offline && State != ERokGameNetState::Failed)
	{
		UE_LOG(LogRokGameNet, Log, TEXT("Rok game net disconnected: %s"), *Reason);
		if (!bWasSnapshotReplay) WriteSessionDump();
		SetState(ERokGameNetState::Offline);
	}
	ResetWorldMapAoi();
}

void URokGameNetSubsystem::SetState(ERokGameNetState NewState)
{
	if (State == NewState) return;
	State = NewState;
	// Remote close/failure transitions do not necessarily call Disconnect.
	if ((State == ERokGameNetState::Offline || State == ERokGameNetState::Failed) && !SessionMail.IsEmpty())
	{
		SessionMail.Reset();
		++BattleReportRevision;
	}
	if (State != ERokGameNetState::RoleReady)
	{
		CityHarvestGainTimes.Reset();
		if (CityHarvestDeadline > 0)
			FinishCityHarvest(TEXT("连接已断开，请重新连接后查看产出。"), false);
		WorldSearchCacheUntil = 0;
		WorldSearchHits.Reset();
		if (PendingWorldSearch.IsSet()) FinishWorldSearch(TEXT("连接已断开，请重新搜索。"));
	}
	if (State != ERokGameNetState::RoleReady && CityWrite.IsSet())
		FinishCityWrite(TEXT("连接已断开，建造结果未确认。请重新连接后查看城市。"));
	if (State != ERokGameNetState::RoleReady && PendingTaskClaim.IsSet())
		FinishTaskClaim(TEXT("连接已断开，领取结果待确认。请重新连接后查看任务。"));
	if (State != ERokGameNetState::RoleReady && PendingVipClaim.IsSet())
		FinishVipClaim(TEXT("连接已断开，请重新连接后查看 VIP 领取结果。"));
	if (State != ERokGameNetState::RoleReady && PendingHeroWrite.IsSet())
		FinishHeroWrite(TEXT("连接已断开，请重新连接后确认统帅状态。"));
	if (State != ERokGameNetState::RoleReady && PendingArmyDispatch.IsSet())
		FinishArmyDispatch(0, TEXT("连接已断开。请重新连接后查看部队状态。"));
	if (State != ERokGameNetState::RoleReady && PendingGatherRecall.IsSet())
		FinishGatherRecall(TEXT("连接已断开。请重新连接后查看部队状态。"));
	if (State != ERokGameNetState::RoleReady && PendingHospitalWrite.IsSet())
		FinishHospitalWrite(TEXT("连接已断开，请重新连接后查看治疗结果。"));
	if (State != ERokGameNetState::RoleReady && PendingTrainingWrite.IsSet())
		FinishTrainingWrite(TEXT("连接已断开。请重新连接后查看训练结果。"));
	if (State != ERokGameNetState::RoleReady && PendingResearchWrite.IsSet())
		FinishResearchWrite(TEXT("连接已断开，请重新连接后查看研究结果。"));
	OnStateChanged.Broadcast(State);
}

void URokGameNetSubsystem::Fail(const FString& Error)
{
	ServerClock.Reset();
	PendingHeartSends.Reset();
	bLoggedFirstHeartSample = false;
	LastError = Error;
	ResetWorldMapAoi();
	UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net failed: %s"), *Error);
	SetState(ERokGameNetState::Failed);
}

bool URokGameNetSubsystem::Tick(float)
{
	if (!Worker.IsValid()) return true;
	FRokGameNetWorker::FEvent Event;
	while (Worker.IsValid() && Worker->Events.Dequeue(Event))
	{
		switch (Event.Kind)
		{
		case FRokGameNetWorker::EEventKind::Log:
			UE_LOG(LogRokGameNet, Log, TEXT("Rok game net: %s"), *Event.Text);
			break;
		case FRokGameNetWorker::EEventKind::Failed:
			Fail(Event.Text);
			break;
		case FRokGameNetWorker::EEventKind::Closed:
			ServerClock.Reset();
			PendingHeartSends.Reset();
			bLoggedFirstHeartSample = false;
			UE_LOG(LogRokGameNet, Log, TEXT("Rok game net closed: %s"), *Event.Text);
			WriteSessionDump();
			ResetWorldMapAoi();
			if (State != ERokGameNetState::Failed) SetState(ERokGameNetState::Offline);
			break;
		case FRokGameNetWorker::EEventKind::LoggedIn:
			Secret = MoveTemp(Event.Bytes);
			ServerName = Event.ServerName;
			Uid = Event.Uid;
			UE_LOG(LogRokGameNet, Log, TEXT("Rok game net authenticated: node=%s uid=%s"), *ServerName, *Uid);
			SetState(ERokGameNetState::Connected);
			SendRequest(TEXT("Role_GetRoleList"), nullptr);
			break;
		case FRokGameNetWorker::EEventKind::Frame:
			HandleFrame(Event.Bytes);
			break;
		}
	}
	const double Now = FPlatformTime::Seconds();
	ExpireStaleRequests(Now);
	CheckCityWrite(Now);
	if (CityHarvestDeadline > 0 && Now >= CityHarvestDeadline)
		FinishCityHarvest(TEXT("领取结果待确认，请重新连接后查看产出。"), true);
	CheckTaskClaim(Now);
	CheckVipClaim(Now);
	CheckHeroWrite(Now);
	CheckArmyDispatch(Now);
	CheckGatherRecall(Now);
	CheckTrainingWrite(Now);
	CheckResearchWrite(Now);
	CheckHospitalWrite(Now);
	if (PendingWorldSearch.IsSet() && Now >= PendingWorldSearch->Deadline)
		FinishWorldSearch(TEXT("搜索超时，请重试。"));
	if (State == ERokGameNetState::RoleReady && Now - LastHeartTime >= HeartIntervalSeconds)
	{
		SendHeart();
	}
	if (DumpDueTime > 0.0 && Now >= DumpDueTime)
	{
		DumpDueTime = 0.0;
		WriteSessionDump();
	}
	return true;
}

bool URokGameNetSubsystem::BuildRequestFrame(const FString& ProtocolName, const TSharedPtr<FJsonObject>& Body,
	TArray<uint8>& OutFrame, FString& OutError)
{
	if (!Rpc.IsValid() && !LoadSchema(OutError)) return false;
	if (Secret.Num() != 8)
	{
		OutError = TEXT("not authenticated");
		return false;
	}
	const FRokSprotoProtocol* Protocol = Schema.FindProtocol(ProtocolName);
	const FRokSprotoProtocol* Gate = Schema.FindProtocol(TEXT("GateMessage"));
	if (!Protocol || !Gate)
	{
		OutError = TEXT("unknown protocol ") + ProtocolName;
		return false;
	}
	// NetProxy.SendSproto: the request goes inside GateMessage.request.content[0].networkMessage.
	const int64 InnerSession = ++SprotoSession;
	TArray<uint8> Inner;
	if (!Rpc->EncodeRequest(Protocol->Tag, Body.Get(), true, InnerSession, Inner, OutError,
		Protocol->ResponseType != INDEX_NONE)) return false;
	FJsonObject GateBody;
	TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetStringField(TEXT("networkMessage"), FBase64::Encode(Inner));
	TArray<TSharedPtr<FJsonValue>> Contents;
	Contents.Add(MakeShared<FJsonValueObject>(Content));
	GateBody.SetArrayField(TEXT("content"), Contents);
	const int64 OuterSession = ++SprotoSession;
	TArray<uint8> Wrapped;
	if (!Rpc->EncodeRequest(Gate->Tag, &GateBody, true, OuterSession, Wrapped, OutError)) return false;
	// Even requests without a protocol response may get an outer gate reply
	// carrying an error or AOI pushes. Retain its tag until that reply or TTL.
	PendingInnerByOuter.Add(OuterSession, Protocol->Tag);
	TArray<uint8> Encrypted;
	RokSkynetCrypt::DesEncode(Secret, Wrapped.GetData(), Wrapped.Num(), Encrypted);
	const int32 Session = ++GameSession;
	const int32 PayloadSize = Encrypted.Num() + GameSessionLength;
	if (PayloadSize > 0xffff)
	{
		OutError = TEXT("request too large");
		return false;
	}
	OutFrame.Reset(PayloadSize + 2);
	OutFrame.Add(uint8((PayloadSize >> 8) & 0xff));
	OutFrame.Add(uint8(PayloadSize & 0xff));
	OutFrame.Append(Encrypted);
	OutFrame.Add(uint8((Session >> 24) & 0xff));
	OutFrame.Add(uint8((Session >> 16) & 0xff));
	OutFrame.Add(uint8((Session >> 8) & 0xff));
	OutFrame.Add(uint8(Session & 0xff));
	return true;
}

bool URokGameNetSubsystem::SendRequest(const FString& ProtocolName, const TSharedPtr<FJsonObject>& Body)
{
	FString Error;
	TArray<uint8> Frame;
	if (!Worker.IsValid() || !BuildRequestFrame(ProtocolName, Body, Frame, Error))
	{
		UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net cannot send %s: %s"), *ProtocolName, *Error);
		return false;
	}
	Worker->Outbound.Enqueue(MoveTemp(Frame));
	UE_LOG(LogRokGameNet, Verbose, TEXT("Rok game net sent %s"), *ProtocolName);
	return true;
}

namespace
{
FString CityWriteError(int64 Code)
{
	switch (Code)
	{
	case 1010: return TEXT("粮食不足。");
	case 1011: return TEXT("木材不足。");
	case 1012: return TEXT("石料不足。");
	case 1013: return TEXT("金币不足。");
	case 1014: return TEXT("宝石不足。");
	case 1035: return TEXT("建造已完成。");
	case 1066:
	case 8000: return TEXT("道具不足。");
	case 6000: return TEXT("未满足建筑前置条件。");
	case 6001: return TEXT("该位置无法建造，请重新选择。");
	case 6002: return TEXT("已达到当前建筑数量上限。");
	case 6003: return TEXT("没有可用的建造队列。");
	case 6004: return TEXT("建筑已达到最高等级。");
	case 6005: return TEXT("训练结束后才能升级兵营。");
	case 6006: return TEXT("研究结束后才能升级学院。");
	case 6007: return TEXT("治疗结束后才能升级医院。");
	case 6009: return TEXT("服务器上已无此建筑。");
	case 6010:
	case 6032: return TEXT("该建筑正在升级。");
	case 6020: return TEXT("已达到建造队列上限。");
	default: return FString::Printf(TEXT("服务器拒绝了建造操作（错误 %lld）。"), Code);
	}
}

FString ArmyDispatchError(int64 Code, bool bAttack)
{
	// ref/server/data/work/common/errorcode/ErrorCode.lua
	switch (Code)
	{
	case 500: return TEXT("服务器处理异常。请重新连接后确认出征结果。");
	case 1004:
	case 1005: return TEXT("所选统帅已不可用，请重新编队。");
	case 1006: return TEXT("主将需达到三星才能携带副将。");
	case 1007: return TEXT("城内可用士兵不足，请重新编队。");
	case 1008: return TEXT("士兵数量超过部队容量，请减少后重试。");
	case 1009: return TEXT("所有出征队列均已占用。");
	case 1033: return bAttack ? TEXT("乱军已离开视野或消失。") : TEXT("资源点已离开视野或消失。");
	case 1034: return TEXT("请至少选择一名士兵。");
	case 1036: return TEXT("尚未研究采集该资源所需的科技。");
	case 1037: return TEXT("该资源点已有部队在采集。");
	case 1028:
	case 2006: return TEXT("行动力不足。");
	case 2007: return TEXT("该乱军等级尚未解锁。");
	case 2000: return TEXT("找不到前往该目标的行军路线。");
	case 2044: return TEXT("该目标当前无法采集。");
	default: return Code <= 0 ? TEXT("服务器未确认出征结果。请重新连接后查看部队。")
		: FString::Printf(TEXT("服务器拒绝出征（错误 %lld）。"), Code);
	}
}
}

bool URokGameNetSubsystem::CanRequestArmyDispatch(FString& Error) const
{
	if (bSnapshotReplay) Error = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) Error = TEXT("连接服务器后可派遣部队。");
	else if (bArmyDispatchNeedsReconnect) Error = TEXT("请重新连接后确认上一次出征结果。");
	else if (PendingArmyDispatch.IsSet()) Error = TEXT("正在等待服务器确认上一次出征。");
	else Error.Reset();
	return Error.IsEmpty();
}

bool URokGameNetSubsystem::SendArmyDispatch(const TSharedPtr<FJsonObject>& Body, int64 TargetId, int32 MainHeroId,
	TFunction<void(int32, const FString&)> OnResult, FString& Error, int32 ArmyIndex)
{
	if (!CanRequestArmyDispatch(Error)) return false;
	const bool bNewArmy = ArmyIndex == 0;
	if (!SendRequest(bNewArmy ? TEXT("Role_CreateArmy") : TEXT("Map_March"), Body))
	{
		Error = TEXT("出征请求发送失败。");
		return false;
	}
	int64 TargetType = 4;
	RokSprotoJson::TryGetInt64(Body, TEXT("targetType"), TargetType);
	PendingArmyDispatch = FArmyDispatchWrite{SprotoSession, bNewArmy ? SprotoSession - 1 : 0, TargetId, MainHeroId, ArmyIndex,
		FPlatformTime::Seconds() + 12.0, MoveTemp(OnResult), static_cast<int32>(TargetType)};
	return true;
}

void URokGameNetSubsystem::AcknowledgeArmyDispatch(const FRokSprotoMessage& Message)
{
	if (!PendingArmyDispatch.IsSet() || Message.bRequest || !Message.bHasSession
		|| Message.Session != PendingArmyDispatch->InnerSession) return;
	int64 Index = 0;
	if (RokSprotoJson::TryGetInt64(Message.Body, TEXT("armyIndex"), Index) && Index > 0 && Index <= MAX_int32)
	{
		PendingArmyDispatch->ArmyIndex = static_cast<int32>(Index);
	}
}

void URokGameNetSubsystem::CheckArmyDispatch(double Now)
{
	if (!PendingArmyDispatch.IsSet()) return;
	const FArmyDispatchWrite& Write = *PendingArmyDispatch;
	if (Write.ArmyIndex > 0)
	{
		const auto Armies = GetCollection(TEXT("Army_ArmyList"), TEXT("armyInfo"));
		const TSharedPtr<FJsonObject>* Army = nullptr;
		const TSharedPtr<FJsonObject>* Target = nullptr;
		int64 HeroId = 0, TargetId = 0, TargetType = 0;
		if (Armies && Armies->TryGetObjectField(LexToString(Write.ArmyIndex), Army) && Army && *Army
			&& RokSprotoJson::TryGetInt64(*Army, TEXT("mainHeroId"), HeroId) && HeroId == Write.MainHeroId
			&& (*Army)->TryGetObjectField(TEXT("targetArg"), Target)
			&& RokSprotoJson::TryGetInt64(*Target, TEXT("targetObjectIndex"), TargetId) && TargetId == Write.TargetId
			&& (Write.TargetType != 1 || (RokSprotoJson::TryGetInt64(*Army, TEXT("targetType"), TargetType) && TargetType == 1)))
		{
			FinishArmyDispatch(Write.ArmyIndex, FString());
			return;
		}
	}
	if (Now >= Write.Deadline)
	{
		bArmyDispatchNeedsReconnect = true;
		FinishArmyDispatch(Write.ArmyIndex, Write.ArmyIndex > 0
			? TEXT("服务器已受理出征。部队同步超时，请重新连接后查看。")
			: TEXT("等待出征确认超时。请重新连接后查看部队。"));
	}
}

TSharedPtr<FJsonObject> URokGameNetSubsystem::MakeGatherRecallBody(int32 ArmyIndex)
{
	// Unity UI_Pop_WorldObjectInfoMediator.OnReturn -> Map_March, target type Return.
	auto Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("armyIndex"), ArmyIndex);
	Body->SetNumberField(TEXT("targetType"), 5);
	return Body;
}

bool URokGameNetSubsystem::SendGatherRecall(int32 ArmyIndex, TFunction<void(const FString&)> OnResult, FString& Error)
{
	if (bSnapshotReplay) Error = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) Error = TEXT("连接服务器后可召回部队。");
	else if (bGatherRecallNeedsReconnect) Error = TEXT("请重新连接后确认上一次召回结果。");
	else if (PendingGatherRecall.IsSet()) Error = TEXT("正在等待服务器确认上一次召回。");
	else if (!SendRequest(TEXT("Map_March"), MakeGatherRecallBody(ArmyIndex))) Error = TEXT("召回请求发送失败。");
	else
	{
		Error.Reset();
		PendingGatherRecall = FGatherRecall{SprotoSession, ArmyIndex, FPlatformTime::Seconds() + 12.0, MoveTemp(OnResult)};
		return true;
	}
	return false;
}

void URokGameNetSubsystem::CheckGatherRecall(double Now)
{
	if (!PendingGatherRecall.IsSet()) return;

	const auto Armies = GetCollection(TEXT("Army_ArmyList"), TEXT("armyInfo"));
	const TSharedPtr<FJsonObject>* Army = nullptr;
	int64 HeroId = 0, TargetType = 0;
	if (Armies && (!Armies->TryGetObjectField(LexToString(PendingGatherRecall->ArmyIndex), Army)
		|| (RokSprotoJson::TryGetInt64(*Army, TEXT("mainHeroId"), HeroId) && HeroId == 0)
		|| (RokSprotoJson::TryGetInt64(*Army, TEXT("targetType"), TargetType) && TargetType == 5)))
	{
		FinishGatherRecall(FString());
	}
	else if (Now >= PendingGatherRecall->Deadline)
	{
		bGatherRecallNeedsReconnect = true;
		FinishGatherRecall(TEXT("等待召回确认超时。请重新连接后查看部队。"));
	}
}

void URokGameNetSubsystem::FinishGatherRecall(const FString& Error)
{
	if (!PendingGatherRecall.IsSet()) return;
	auto Callback = MoveTemp(PendingGatherRecall->OnResult);
	PendingGatherRecall.Reset();
	if (Callback) Callback(Error);
}

namespace
{
FString ResearchWriteError(int64 Code)
{
    switch (Code)
    {
    case 1010: return TEXT("粮食不足。");
    case 1011: return TEXT("木材不足。");
    case 1012: return TEXT("石料不足。");
    case 1013: return TEXT("金币不足。");
    case 5000: return TEXT("请先建造学院。");
    case 5001: return TEXT("学院正在升级。");
    case 5002: return TEXT("已有科技正在研究。");
    case 5003: return TEXT("前置科技条件未满足。");
    case 5004: return TEXT("学院等级不足。");
    case 5005: return TEXT("科技尚未完成或已领取。");
    case 5006: return TEXT("当前没有进行中的研究。");
    case 5007: return TEXT("服务器未找到科技配置。");
    default: return FString::Printf(TEXT("服务器拒绝研究操作（%lld）。"), Code);
    }
}

FString TrainingWriteError(int64 Code)
{
	switch (Code)
	{
	case 1010: return TEXT("粮食不足。");
	case 1011: return TEXT("木材不足。");
	case 1012: return TEXT("石料不足。");
	case 1013: return TEXT("金币不足。");
	case 1014: return TEXT("宝石不足。");
	case 1015: return TEXT("该兵阶尚未解锁。");
	case 1016: return TEXT("训练建筑正在升级。");
	case 1017: return TEXT("该兵种已有训练队列。");
	case 1018: return TEXT("训练尚未完成。");
	case 1039: return TEXT("训练数量超过服务器允许的容量。");
	default: return Code == 500 || Code <= 0
		? TEXT("服务器处理异常，请重新连接后确认训练结果。")
		: FString::Printf(TEXT("服务器拒绝训练操作（%lld）。"), Code);
	}
}

// Nested keyed arrays in Role_RoleInfo usually remain arrays; replay and other
// projections can already have the keyed-object representation.
TSharedPtr<FJsonObject> FindTrainingProtocolRow(const TSharedPtr<FJsonObject>& Parent,
	const TCHAR* Field, const TCHAR* Key, int64 Id)
{
	if (!Parent) return nullptr;
	const TSharedPtr<FJsonObject>* Map = nullptr;
	const TSharedPtr<FJsonObject>* Row = nullptr;
	if (Parent->TryGetObjectField(Field, Map))
		return (*Map)->TryGetObjectField(LexToString(Id), Row) ? *Row : nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Parent->TryGetArrayField(Field, Array))
	{
		for (const auto& Value : *Array)
		{
			int64 RowId = 0;
			if (Value && Value->TryGetObject(Row) && RokSprotoJson::TryGetInt64(*Row, Key, RowId) && RowId == Id)
				return *Row;
		}
	}
	return nullptr;
}
}

TSharedPtr<FJsonObject> URokGameNetSubsystem::MakeTrainingBody(const FTrainingWrite& Write)
{
	auto Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("type"), Write.ArmyType);
	if (!Write.bReceive)
	{
		// Unity TrainArmyMediator.OnTrain / OnComplete.
		Body->SetNumberField(TEXT("buildingIndex"), Write.BuildingIndex);
		Body->SetNumberField(TEXT("level"), Write.Tier);
		Body->SetNumberField(TEXT("trainNum"), Write.Count);
		Body->SetNumberField(TEXT("isUpdate"), 0);
		if (Write.bImmediately) Body->SetBoolField(TEXT("immediately"), true);
	}
	return Body;
}

bool URokGameNetSubsystem::CanRequestTrainingWrite(FString& Error) const
{
	if (bSnapshotReplay) Error = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) Error = TEXT("连接服务器后可训练或领取部队。");
	else if (bTrainingWriteNeedsReconnect) Error = TEXT("请重新连接后确认上一次训练操作结果。");
	else if (PendingTrainingWrite.IsSet()) Error = TEXT("正在等待服务器确认上一次训练操作。");
	else Error.Reset();
	return Error.IsEmpty();
}

bool URokGameNetSubsystem::SendTrainingWrite(FTrainingWrite Write, FString& Error)
{
	if (!CanRequestTrainingWrite(Error)) return false;
	if (!SendRequest(Write.bReceive ? TEXT("Role_AwardArmy") : TEXT("Role_TrainArmy"), MakeTrainingBody(Write)))
	{
		Error = TEXT("训练操作发送失败。");
		return false;
	}
	Write.InnerSession = SprotoSession - 1;
	Write.OuterSession = SprotoSession;
	Write.Deadline = FPlatformTime::Seconds() + 12.0;
	PendingTrainingWrite = MoveTemp(Write);
	return true;
}

void URokGameNetSubsystem::AcknowledgeTrainingWrite(const FRokSprotoMessage& Message)
{
	if (!PendingTrainingWrite.IsSet() || Message.bRequest || !Message.bHasSession
		|| Message.Session != PendingTrainingWrite->InnerSession || !Message.Body) return;
	auto& Write = PendingTrainingWrite.GetValue();
	if (!Write.bReceive && !Write.bImmediately)
	{
		// Timed training returns an empty response. The matching queue push confirms the new job.
		Write.bAcknowledged = true;
		return;
	}
	const int32 AwardId = Write.bImmediately ? Write.TroopId : Write.ArmyType * 100 + Write.Tier;
	const auto Award = FindTrainingProtocolRow(Message.Body, TEXT("soldiers"), TEXT("id"), AwardId);
	int64 Count = 0;
	Write.bAcknowledged = RokSprotoJson::TryGetInt64(Award, TEXT("num"), Count) && Count == Write.Count;
}

void URokGameNetSubsystem::CheckTrainingWrite(double Now)
{
	if (!PendingTrainingWrite.IsSet()) return;
	const auto& Write = PendingTrainingWrite.GetValue();
	if (Write.bAcknowledged)
	{
		const auto Role = GetRoleInfo();
		const auto Queue = FindTrainingProtocolRow(Role, TEXT("armyQueue"), TEXT("queueIndex"), Write.ArmyType);
		int64 Finish = 0;
		RokSprotoJson::TryGetInt64(Queue, TEXT("finishTime"), Finish);
		if (Write.bImmediately || (Write.bReceive && Finish == -1))
		{
			const auto Soldier = FindTrainingProtocolRow(Role, TEXT("soldiers"), TEXT("id"), Write.TroopId);
			int64 Current = 0;
			if (RokSprotoJson::TryGetInt64(Soldier, TEXT("num"), Current)
				&& Current >= Write.PreviousTroops && Current - Write.PreviousTroops >= Write.Count)
			{
				FinishTrainingWrite(FString());
				return;
			}
		}
		else if (!Write.bReceive && Finish > 0)
		{
			int64 Building = 0, Type = 0, Tier = 0, Count = 0;
			if (RokSprotoJson::TryGetInt64(Queue, TEXT("buildingIndex"), Building) && Building == Write.BuildingIndex
				&& RokSprotoJson::TryGetInt64(Queue, TEXT("armyType"), Type) && Type == Write.ArmyType
				&& RokSprotoJson::TryGetInt64(Queue, TEXT("newArmyLevel"), Tier) && Tier == Write.Tier
				&& RokSprotoJson::TryGetInt64(Queue, TEXT("armyNum"), Count) && Count == Write.Count)
			{
				FinishTrainingWrite(FString());
				return;
			}
		}
	}
	if (Now >= Write.Deadline)
	{
		bTrainingWriteNeedsReconnect = true;
		FinishTrainingWrite(TEXT("等待服务器确认超时。请重新连接后查看训练结果。"));
	}
}

void URokGameNetSubsystem::FinishTrainingWrite(const FString& Error)
{
	if (!PendingTrainingWrite.IsSet()) return;
	auto Callback = MoveTemp(PendingTrainingWrite->OnResult);
	PendingTrainingWrite.Reset();
	if (Callback) Callback(Error);
}

bool URokGameNetSubsystem::RequestResearchCommand(ERokResearchCommand Command, int32 StudyType,
	TFunction<void(const FString&)> OnResult, FString& Error)
{
	if (bSnapshotReplay) Error = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) Error = TEXT("服务器连接尚未就绪。");
	else if (bResearchWriteNeedsReconnect) Error = TEXT("上次研究结果待确认，请重新连接后查看。");
	else if (PendingResearchWrite.IsSet()) Error = TEXT("正在等待服务器确认研究操作。");
	else if (StudyType <= 0) Error = TEXT("科技编号无效。");
	else Error.Reset();
	if (!Error.IsEmpty()) return false;
	const auto Role = GetRoleInfo();
	const TSharedPtr<FJsonObject>* Queue = nullptr;
	if (!Role || !Role->HasField(TEXT("technologies")) || !Role->TryGetObjectField(TEXT("technologyQueue"), Queue))
	{ Error = TEXT("研究状态尚未同步。"); return false; }
	int64 CurrentType = 0, Finish = 0;
	if ((*Queue)->HasField(TEXT("technologyType")) && !RokSprotoJson::TryGetInt64(*Queue, TEXT("technologyType"), CurrentType))
	{ Error = TEXT("研究队列数据无效。"); return false; }
	if (CurrentType > 0 && !RokSprotoJson::TryGetInt64(*Queue, TEXT("finishTime"), Finish))
	{ Error = TEXT("研究队列数据不完整。"); return false; }
	if (((Command == ERokResearchCommand::Start || Command == ERokResearchCommand::Instant) && CurrentType != 0) ||
		((Command == ERokResearchCommand::Award || Command == ERokResearchCommand::Stop) && CurrentType != StudyType) ||
		(Command == ERokResearchCommand::Award && Finish > 0) ||
		(Command == ERokResearchCommand::Stop && Finish <= 0))
	{ Error = TEXT("研究队列状态已变化，请重新查看。"); return false; }
	int64 Level = 0;
	const auto Previous = FindTrainingProtocolRow(Role, TEXT("technologies"), TEXT("technologyType"), StudyType);
	if (Previous && (!RokSprotoJson::TryGetInt64(Previous, TEXT("level"), Level) || Level < 0))
	{ Error = TEXT("科技等级数据无效。"); return false; }
	const TCHAR* Protocol = (Command == ERokResearchCommand::Start || Command == ERokResearchCommand::Instant) ? TEXT("Technology_ResearchTechnology") :
		Command == ERokResearchCommand::Award ? TEXT("Technology_AwardTechnology") : TEXT("Technology_StopTechnology");
	auto Body = MakeShared<FJsonObject>();
	if (Command == ERokResearchCommand::Start || Command == ERokResearchCommand::Instant)
	{
		Body->SetNumberField(TEXT("technologyType"), StudyType);
		Body->SetBoolField(TEXT("immediately"), Command == ERokResearchCommand::Instant);
	}
	if (!SendRequest(Protocol, Body)) { Error = TEXT("研究操作发送失败。"); return false; }
	FResearchWrite Write;
	Write.Command = Command; Write.StudyType = StudyType; Write.PreviousLevel = Level;
	Write.InnerSession = SprotoSession - 1; Write.OuterSession = SprotoSession;
	Write.Deadline = FPlatformTime::Seconds() + 12.0;
	Write.OnResult = MoveTemp(OnResult);
	PendingResearchWrite = MoveTemp(Write);
	return true;
}

void URokGameNetSubsystem::AcknowledgeResearchWrite(const FRokSprotoMessage& Message)
{
	if (!PendingResearchWrite.IsSet() || Message.bRequest || !Message.bHasSession ||
		Message.Session != PendingResearchWrite->InnerSession || !Message.Body) return;
	if (PendingResearchWrite->Command == ERokResearchCommand::Award) return; // no response in the source protocol
	bool bResult = false;
	if (Message.Body->TryGetBoolField(TEXT("result"), bResult) && bResult)
		PendingResearchWrite->bAcknowledged = true;
}

void URokGameNetSubsystem::CheckResearchWrite(double Now)
{
	if (!PendingResearchWrite.IsSet()) return;
	const auto& Write = *PendingResearchWrite;
	const auto Role = GetRoleInfo();
	const TSharedPtr<FJsonObject>* Queue = nullptr;
	int64 Type = 0, Finish = 0;
	if (Role && Role->TryGetObjectField(TEXT("technologyQueue"), Queue) &&
		(!(*Queue)->HasField(TEXT("technologyType")) || RokSprotoJson::TryGetInt64(*Queue, TEXT("technologyType"), Type)))
	{
		if (Write.Command == ERokResearchCommand::Start && Write.bAcknowledged && Type == Write.StudyType &&
			RokSprotoJson::TryGetInt64(*Queue, TEXT("finishTime"), Finish) && Finish != 0)
		{ FinishResearchWrite(FString()); return; }
		if (Write.Command == ERokResearchCommand::Stop && Write.bAcknowledged && Type == 0)
		{ FinishResearchWrite(FString()); return; }
		if ((Write.Command == ERokResearchCommand::Award ||
			(Write.Command == ERokResearchCommand::Instant && Write.bAcknowledged)) && Type == 0)
		{
			const auto Row = FindTrainingProtocolRow(Role, TEXT("technologies"), TEXT("technologyType"), Write.StudyType);
			int64 Level = 0;
			if (RokSprotoJson::TryGetInt64(Row, TEXT("level"), Level) && Level == Write.PreviousLevel + 1)
			{ FinishResearchWrite(FString()); return; }
		}
	}
	if (Now >= Write.Deadline)
	{
		bResearchWriteNeedsReconnect = true;
		FinishResearchWrite(TEXT("等待服务器确认超时，请重新连接后查看研究结果。"));
	}
}

void URokGameNetSubsystem::FinishResearchWrite(const FString& Error)
{
	if (!PendingResearchWrite.IsSet()) return;
	auto Callback = MoveTemp(PendingResearchWrite->OnResult);
	PendingResearchWrite.Reset();
	if (Callback) Callback(Error);
}

void URokGameNetSubsystem::FinishArmyDispatch(int32 ArmyIndex, const FString& Error)
{
	if (!PendingArmyDispatch.IsSet()) return;
	auto Callback = MoveTemp(PendingArmyDispatch->OnResult);
	PendingArmyDispatch.Reset();
	if (Callback) Callback(ArmyIndex, Error);
}

bool URokGameNetSubsystem::CanRequestCityWrite(FString& OutFailure) const
{
	if (bSnapshotReplay) OutFailure = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) OutFailure = TEXT("服务器连接尚未就绪。");
	else if (bCityWriteNeedsReconnect) OutFailure = TEXT("上一次建造结果未确认，请重新连接后查看城市。");
	else if (CityWrite.IsSet()) OutFailure = TEXT("正在等待服务器确认上一项建造操作。");
	else OutFailure.Reset();
	return OutFailure.IsEmpty();
}

bool URokGameNetSubsystem::RequestCityHarvest(const TMap<int32, int64>& GainTimes,
	TFunction<void(const FString&)> OnResult, FString& OutError)
{
	if (bSnapshotReplay || State != ERokGameNetState::RoleReady || !Worker.IsValid())
	{
		OutError = bSnapshotReplay ? TEXT("当前为服务器快照回放。") : TEXT("服务器连接尚未就绪。");
		return false;
	}
	if (CityHarvestDeadline > 0)
	{
		OutError = TEXT("正在等待服务器确认资源领取。");
		return false;
	}
	const auto Collection = GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo"));
	TArray<TSharedPtr<FJsonValue>> Indices;
	TArray<int32> Submitted;
	int64 ResourceType = 0;
	for (const auto& Entry : GainTimes)
	{
		const TSharedPtr<FJsonObject>* Building = nullptr;
		const TSharedPtr<FJsonObject>* Gain = nullptr;
		int64 Type = 0, Level = 0, Finish = 0, Time = 0;
		if (Entry.Key <= 0 || IsCityHarvestWaiting(Entry.Key, Entry.Value) || !Collection
			|| !Collection->TryGetObjectField(FString::FromInt(Entry.Key), Building)
			|| !RokSprotoJson::TryGetInt64(*Building, TEXT("type"), Type) || Type < 2 || Type > 5
			|| !RokSprotoJson::TryGetInt64(*Building, TEXT("level"), Level) || Level <= 0
			|| !RokSprotoJson::TryGetInt64(*Building, TEXT("finishTime"), Finish) || Finish != -1
			|| !(*Building)->TryGetObjectField(TEXT("buildingGainInfo"), Gain)
			|| !RokSprotoJson::TryGetInt64(*Gain, TEXT("changeTime"), Time) || Time != Entry.Value) continue;
		if (ResourceType != 0 && Type != ResourceType)
		{
			OutError = TEXT("请分开领取不同类型的建筑产出。");
			return false;
		}
		ResourceType = Type;
		Indices.Add(MakeShared<FJsonValueNumber>(Entry.Key));
		Submitted.Add(Entry.Key);
	}
	if (Indices.IsEmpty())
	{
		OutError = TEXT("当前没有可领取的建筑产出。");
		return false;
	}
	const auto Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("buildingIndexs"), MoveTemp(Indices));
	if (!SendRequest(TEXT("Build_GetBuildResources"), Body))
	{
		OutError = TEXT("资源领取请求未能发送。");
		return false;
	}
	CityHarvestInFlight = MoveTemp(Submitted);
	for (int32 Index : CityHarvestInFlight) CityHarvestGainTimes.Add(Index, GainTimes.FindChecked(Index));
	CityHarvestOuterSession = SprotoSession;
	CityHarvestInnerSession = SprotoSession - 1;
	CityHarvestDeadline = FPlatformTime::Seconds() + 15.0;
	OnCityHarvestResult = MoveTemp(OnResult);
	OutError.Reset();
	return true;
}

bool URokGameNetSubsystem::IsCityHarvestWaiting(int32 Index, int64 GainTime) const
{
	const int64* PendingTime = CityHarvestGainTimes.Find(Index);
	return PendingTime && *PendingTime == GainTime;
}

void URokGameNetSubsystem::FinishCityHarvest(const FString& Error, bool bKeepGainTimes)
{
	if (!bKeepGainTimes)
	{
		for (int32 Index : CityHarvestInFlight) CityHarvestGainTimes.Remove(Index);
	}
	CityHarvestInFlight.Reset();
	CityHarvestDeadline = 0;
	CityHarvestInnerSession = 0;
	CityHarvestOuterSession = 0;
	auto Callback = MoveTemp(OnCityHarvestResult);
	if (Callback) Callback(Error);
}

bool URokGameNetSubsystem::RequestCreateBuilding(int32 Type, FIntPoint Grid,
	FRokCityWriteResult OnResult, FString& OutFailure)
{
	if (Type <= 0)
	{
		OutFailure = TEXT("建筑类型无效。");
		return false;
	}
	const TSharedRef<FJsonObject> Pos = MakeShared<FJsonObject>();
	Pos->SetNumberField(TEXT("x"), Grid.X);
	Pos->SetNumberField(TEXT("y"), Grid.Y);
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("type"), Type);
	Body->SetObjectField(TEXT("pos"), Pos);
	FCityWrite Write;
	Write.Type = Type;
	Write.Grid = Grid;
	Write.OnResult = MoveTemp(OnResult);
	return SendCityWrite(MoveTemp(Write), TEXT("Build_CreateBuilding"), Body, OutFailure);
}

bool URokGameNetSubsystem::RequestBuildingUpgrade(int32 Index, int32 CurrentLevel, bool bImmediately,
	FRokCityWriteResult OnResult, FString& OutFailure)
{
	if (Index <= 0 || CurrentLevel <= 0 || CurrentLevel == MAX_int32)
	{
		OutFailure = TEXT("建筑索引或等级无效。");
		return false;
	}
	if (!CanRequestCityWrite(OutFailure)) return false;
	const TSharedPtr<FJsonObject> Buildings = GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo"));
	const TSharedPtr<FJsonObject>* Row = nullptr;
	int64 Level = 0, FinishTime = 0;
	if (!Buildings || !Buildings->TryGetObjectField(FString::FromInt(Index), Row)
		|| !RokSprotoJson::TryGetInt64(*Row, TEXT("level"), Level) || Level != CurrentLevel
		|| (RokSprotoJson::TryGetInt64(*Row, TEXT("finishTime"), FinishTime) && FinishTime > 0))
	{
		OutFailure = TEXT("建筑状态已变化，请重新打开升级界面。");
		return false;
	}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("buildingIndex"), Index);
	Body->SetBoolField(TEXT("immediately"), bImmediately);
	FCityWrite Write;
	Write.Index = Index;
	Write.TargetLevel = CurrentLevel + 1;
	Write.bImmediately = bImmediately;
	Write.OnResult = MoveTemp(OnResult);
	return SendCityWrite(MoveTemp(Write), TEXT("Build_UpGradeBuilding"), Body, OutFailure);
}

bool URokGameNetSubsystem::TryGetOwnedItemCount(int32 ItemId, int64& OutCount) const
{
	OutCount = 0;
	const auto* Inventory = GetGameInstance()->GetSubsystem<URokServerInventorySubsystem>();
	if (!Inventory) return false;
	const auto Snapshot = Inventory->GetSnapshot();
	if (!Snapshot.bItemsKnown || !Snapshot.bRowsComplete) return false;
	for (const auto& Stack : Snapshot.Stacks)
	{
		if (Stack.ItemId == ItemId) OutCount += Stack.Overlay;
	}
	return true;
}

bool URokGameNetSubsystem::RequestWorkerHire(bool bUseItem, FRokCityWriteResult OnResult, FString& OutFailure)
{
	if (!CanRequestCityWrite(OutFailure)) return false;
	TArray<FRokServerBuildQueue> Queues;
	if (!RokServerBuildQueue::Parse(GetRoleInfo(), Queues, OutFailure) || !Queues.ContainsByPredicate(
		[](const FRokServerBuildQueue& Queue) { return Queue.QueueIndex == 1; }))
	{
		OutFailure = TEXT("建造队列尚未同步。");
		return false;
	}
	const auto* Secondary = Queues.FindByPredicate([](const FRokServerBuildQueue& Queue) { return Queue.QueueIndex == 2; });
	if (Secondary && Secondary->ExpiredTime == -1)
	{
		OutFailure = TEXT("第二队列已永久开通。");
		return false;
	}
	int64 Owned = 0, Gems = 0;
	if (!TryGetOwnedItemCount(WorkerHireItemId, Owned) || bUseItem != (Owned > 0))
	{
		OutFailure = TEXT("雇佣费用已变化。");
		return false;
	}
	if (!bUseItem && (!RokSprotoJson::TryGetInt64(GetRoleInfo(), TEXT("denar"), Gems) || Gems < WorkerHireGemCost))
	{
		OutFailure = TEXT("宝石不足。");
		return false;
	}
	auto Body = MakeShared<FJsonObject>();
	if (bUseItem) Body->SetNumberField(TEXT("itemId"), WorkerHireItemId);
	FCityWrite Write;
	Write.QueueAction = FCityWrite::EQueueAction::Hire;
	Write.QueueIndex = 2;
	Write.PreviousQueueTime = Secondary ? Secondary->ExpiredTime : 0;
	Write.OnResult = MoveTemp(OnResult);
	return SendCityWrite(MoveTemp(Write), TEXT("Build_UnlockBuildQueue"), Body, OutFailure);
}

bool URokGameNetSubsystem::RequestBuildingSpeedUp(int32 QueueIndex, int32 BuildingIndex, int32 ItemId,
	int32 Count, bool bCostGems, FRokCityWriteResult OnResult, FString& OutFailure)
{
	if (!CanRequestCityWrite(OutFailure)) return false;
	const auto Queue = FindTrainingProtocolRow(GetRoleInfo(), TEXT("buildQueue"), TEXT("queueIndex"), QueueIndex);
	int64 CurrentBuilding = 0, Finish = 0;
	double Now = 0;
	if ((QueueIndex != 1 && QueueIndex != 2) || BuildingIndex <= 0
		|| !RokSprotoJson::TryGetInt64(Queue, TEXT("buildingIndex"), CurrentBuilding) || CurrentBuilding != BuildingIndex
		|| !RokSprotoJson::TryGetInt64(Queue, TEXT("finishTime"), Finish)
		|| !TryGetServerTimeSeconds(Now) || Finish <= Now)
	{
		OutFailure = TEXT("建造队列已变化。");
		return false;
	}
	const auto* Catalog = GetGameInstance()->GetSubsystem<URokUnityEconomyCatalogSubsystem>();
	FRokUnityEconomyItem Item;
	int64 Owned = 0, Gems = 0;
	if (!Catalog || !Catalog->FindItemById(ItemId, Item) || Item.SpeedDurationSeconds <= 0
		|| (Item.SubType != 20101 && Item.SubType != 20102) || Count < 1
		|| !TryGetOwnedItemCount(ItemId, Owned))
	{
		OutFailure = TEXT("加速道具不可用。");
		return false;
	}
	if (bCostGems && (Count != 1 || Item.SubType != 20101 || Item.ShortcutPrice <= 0
		|| !RokSprotoJson::TryGetInt64(GetRoleInfo(), TEXT("denar"), Gems) || Gems < Item.ShortcutPrice))
	{
		OutFailure = TEXT("无法使用宝石购买该加速道具。");
		return false;
	}
	if (!bCostGems && Owned < Count)
	{
		OutFailure = TEXT("道具不足。");
		return false;
	}
	auto Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("queueIndex"), QueueIndex);
	Body->SetNumberField(TEXT("type"), 20102);
	Body->SetNumberField(TEXT("itemId"), ItemId);
	Body->SetNumberField(TEXT("itemNum"), Count);
	Body->SetBoolField(TEXT("costDenar"), bCostGems);
	FCityWrite Write;
	Write.QueueAction = FCityWrite::EQueueAction::SpeedUp;
	Write.QueueIndex = QueueIndex;
	Write.Index = BuildingIndex;
	Write.PreviousQueueTime = Finish;
	Write.OnResult = MoveTemp(OnResult);
	return SendCityWrite(MoveTemp(Write), TEXT("Role_SpeedUp"), Body, OutFailure);
}

bool URokGameNetSubsystem::SendCityWrite(FCityWrite Write, const FString& Protocol,
	const TSharedPtr<FJsonObject>& Body, FString& OutFailure)
{
	if (!CanRequestCityWrite(OutFailure)) return false;
	if (!SendRequest(Protocol, Body))
	{
		OutFailure = TEXT("建造请求未能发送，请检查服务器连接。");
		return false;
	}
	TrackSentCityWrite(MoveTemp(Write));
	return true;
}

void URokGameNetSubsystem::TrackSentCityWrite(FCityWrite Write)
{
	// BuildRequestFrame assigns the inner session immediately before the outer session.
	Write.OuterSession = SprotoSession;
	Write.InnerSession = SprotoSession - 1;
	Write.Deadline = FPlatformTime::Seconds() + 30.0;
	CityWrite = MoveTemp(Write);
	if (CityWrite->QueueAction == FCityWrite::EQueueAction::None && CityWrite->Type == 0)
		PlayCityWriteSfx(GetGameInstance(), 4025); // Unity plays on submitting an upgrade.
}

void URokGameNetSubsystem::AcknowledgeCityWrite(const FRokSprotoMessage& Message)
{
	if (!CityWrite.IsSet() || CityWrite->bAcknowledged || Message.bRequest
		|| !Message.bHasSession || Message.Session != CityWrite->InnerSession) return;
	if (CityWrite->QueueAction == FCityWrite::EQueueAction::Hire)
	{
		bool bResult = false;
		if (Message.Body && Message.Body->TryGetBoolField(TEXT("result"), bResult))
		{
			if (bResult) CityWrite->bAcknowledged = true;
			else FinishCityWrite(TEXT("雇佣失败。"));
		}
		return;
	}
	if (CityWrite->QueueAction == FCityWrite::EQueueAction::SpeedUp)
	{
		int64 Finish = -1;
		CityWrite->bAcknowledged = RokSprotoJson::TryGetInt64(Message.Body, TEXT("finishTime"), Finish) && Finish >= 0;
		if (CityWrite->bAcknowledged) PlayCityWriteSfx(GetGameInstance(), 4014);
		return;
	}
	int64 Index = 0;
	if (!RokSprotoJson::TryGetInt64(Message.Body, TEXT("buildingIndex"), Index) || Index <= 0 || Index > MAX_int32) return;
	if (CityWrite->Type == 0)
	{
		bool bImmediately = false;
		Message.Body->TryGetBoolField(TEXT("immediately"), bImmediately);
		if (Index != CityWrite->Index || bImmediately != CityWrite->bImmediately) return;
	}
	CityWrite->Index = static_cast<int32>(Index);
	CityWrite->bAcknowledged = true;
}

void URokGameNetSubsystem::CheckCityWrite(double NowSeconds)
{
	if (!CityWrite.IsSet()) return;
	if (CityWrite->bAcknowledged && CityWrite->QueueAction != FCityWrite::EQueueAction::None)
	{
		const auto Queue = FindTrainingProtocolRow(GetRoleInfo(), TEXT("buildQueue"), TEXT("queueIndex"), CityWrite->QueueIndex);
		int64 Time = 0, Building = 0;
		const bool bChanged = CityWrite->QueueAction == FCityWrite::EQueueAction::Hire
			? RokSprotoJson::TryGetInt64(Queue, TEXT("expiredTime"), Time) && Time > CityWrite->PreviousQueueTime
			: RokSprotoJson::TryGetInt64(Queue, TEXT("finishTime"), Time) && Time < CityWrite->PreviousQueueTime
				&& (Time < 0 || (RokSprotoJson::TryGetInt64(Queue, TEXT("buildingIndex"), Building) && Building == CityWrite->Index));
		if (bChanged)
		{
			FinishCityWrite(FString());
			return;
		}
	}
	else if (CityWrite->bAcknowledged)
	{
		const TSharedPtr<FJsonObject> Buildings = GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo"));
		const TSharedPtr<FJsonObject>* Row = nullptr;
		int64 Level = -1;
		if (Buildings && Buildings->TryGetObjectField(FString::FromInt(CityWrite->Index), Row)
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("level"), Level))
		{
			if (CityWrite->Type > 0)
			{
				const TSharedPtr<FJsonObject>* Pos = nullptr;
				int64 Type = 0, X = 0, Y = 0;
				if (Level >= 0 && RokSprotoJson::TryGetInt64(*Row, TEXT("type"), Type) && Type == CityWrite->Type
					&& (*Row)->TryGetObjectField(TEXT("pos"), Pos)
					&& RokSprotoJson::TryGetInt64(*Pos, TEXT("x"), X) && X == CityWrite->Grid.X
					&& RokSprotoJson::TryGetInt64(*Pos, TEXT("y"), Y) && Y == CityWrite->Grid.Y)
				{
					FinishCityWrite(FString());
					return;
				}
			}
			else
			{
				int64 FinishTime = 0;
				if (Level >= CityWrite->TargetLevel || (Level == CityWrite->TargetLevel - 1
					&& !CityWrite->bImmediately && RokSprotoJson::TryGetInt64(*Row, TEXT("finishTime"), FinishTime)
					&& FinishTime > 0))
				{
					FinishCityWrite(FString());
					return;
				}
			}
		}
	}
	if (NowSeconds >= CityWrite->Deadline)
	{
		// The request may have reached the server. Avoid resending it blindly.
		bCityWriteNeedsReconnect = true;
		FinishCityWrite(TEXT("等待服务器确认超时，请重新连接后查看城市。"));
	}
}

void URokGameNetSubsystem::FinishCityWrite(const FString& Failure)
{
	FRokCityWriteResult OnResult = MoveTemp(CityWrite->OnResult);
	CityWrite.Reset();
	if (OnResult) OnResult(Failure);
}

bool URokGameNetSubsystem::RequestTaskClaim(ERokTaskClaim Kind, int32 Id,
	TFunction<void(const FString&)> OnResult, FString& OutError)
{
	if (bSnapshotReplay) OutError = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) OutError = TEXT("服务器连接尚未就绪。");
	else if (bTaskClaimNeedsReconnect) OutError = TEXT("上一笔领取结果待确认，请重新连接后查看任务。");
	else if (PendingTaskClaim.IsSet()) OutError = TEXT("正在等待服务器确认领取。");
	else if (Id <= 0) OutError = TEXT("任务奖励编号无效。");
	else OutError.Reset();
	if (!OutError.IsEmpty()) return false;

	const auto Body = MakeShared<FJsonObject>();
	const TCHAR* Protocol = TEXT("Task_TaskFinish");
	if (Kind == ERokTaskClaim::Chapter) Protocol = TEXT("Task_ChapterFinish");
	else if (Kind == ERokTaskClaim::ActivePoints)
	{
		Protocol = TEXT("Task_TakeActivePointReward");
		Body->SetNumberField(TEXT("activePoint"), Id);
	}
	else Body->SetNumberField(TEXT("taskId"), Id);
	if (!SendRequest(Protocol, Body))
	{
		OutError = TEXT("领取请求未能发送。");
		return false;
	}
	// Active-point claims have no inner response. Their server push confirms the claim.
	PendingTaskClaim = FTaskClaim{Kind, Id, SprotoSession, SprotoSession - 1,
		FPlatformTime::Seconds() + 30.0, Kind == ERokTaskClaim::ActivePoints, MoveTemp(OnResult)};
	return true;
}

void URokGameNetSubsystem::CheckTaskClaim(double Now)
{
	if (!PendingTaskClaim.IsSet()) return;
	const FTaskClaim& Claim = *PendingTaskClaim;
	if (Claim.bAcknowledged && URokServerTaskSubsystem::IsClaimApplied(Claim.Kind, Claim.Id,
		GetRoleInfo(), GetCollection(TEXT("Task_TaskInfo"), TEXT("taskInfo"))))
	{
		FinishTaskClaim(FString());
	}
	else if (Now >= Claim.Deadline)
	{
		bTaskClaimNeedsReconnect = true;
		FinishTaskClaim(TEXT("领取结果待确认，请重新连接后查看任务。"));
	}
}

void URokGameNetSubsystem::FinishTaskClaim(const FString& Error)
{
	auto Callback = MoveTemp(PendingTaskClaim->OnResult);
	PendingTaskClaim.Reset();
	if (Callback) Callback(Error);
}

namespace
{
const TCHAR* VipClaimFlag(bool bFreeChest)
{
	return bFreeChest ? TEXT("vipFreeBox") : TEXT("vipExpFlag");
}
}

bool URokGameNetSubsystem::RequestVipClaim(bool bFreeChest,
	TFunction<void(const FString&)> OnResult, FString& OutError)
{
	const auto Role = GetRoleInfo();
	bool bClaimed = false;
	if (bSnapshotReplay) OutError = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) OutError = TEXT("服务器连接尚未就绪。");
	else if (bVipClaimNeedsReconnect) OutError = TEXT("领取结果待确认，请重新连接后查看 VIP。");
	else if (PendingVipClaim.IsSet()) OutError = TEXT("正在等待服务器确认领取。");
	else if (!Role || !Role->TryGetBoolField(VipClaimFlag(bFreeChest), bClaimed)) OutError = TEXT("VIP 奖励状态尚未同步。");
	else if (bClaimed) OutError = TEXT("今日奖励已领取。");
	else OutError.Reset();
	if (!OutError.IsEmpty()) return false;

	const TCHAR* Protocol = bFreeChest ? TEXT("Role_GetVipFreeBox") : TEXT("Role_GetVipPoint");
	if (!SendRequest(Protocol, nullptr))
	{
		OutError = TEXT("领取请求未能发送。");
		return false;
	}
	PendingVipClaim = FVipClaim{bFreeChest, SprotoSession, FPlatformTime::Seconds() + 30.0, MoveTemp(OnResult)};
	return true;
}

void URokGameNetSubsystem::CheckVipClaim(double Now)
{
	if (!PendingVipClaim.IsSet()) return;
	const auto Role = GetRoleInfo();
	bool bClaimed = false;
	// Role_GetVipPoint has no response body. The role push confirms both claims.
	if (Role && Role->TryGetBoolField(VipClaimFlag(PendingVipClaim->bFreeChest), bClaimed) && bClaimed)
	{
		FinishVipClaim(FString());
	}
	else if (Now >= PendingVipClaim->Deadline)
	{
		bVipClaimNeedsReconnect = true;
		FinishVipClaim(TEXT("领取结果待确认，请重新连接后查看 VIP。"));
	}
}

void URokGameNetSubsystem::FinishVipClaim(const FString& Error)
{
	auto Callback = MoveTemp(PendingVipClaim->OnResult);
	PendingVipClaim.Reset();
	if (Callback) Callback(Error);
}

bool URokGameNetSubsystem::RequestHeroCommand(const FRokHeroCommand& Request,
	TFunction<void(const FString&)> OnResult, FString& Error)
{
	if (bSnapshotReplay) Error = TEXT("当前为服务器快照回放。");
	else if (State != ERokGameNetState::RoleReady || !Worker.IsValid()) Error = TEXT("服务器连接尚未就绪。");
	else if (bHeroWriteNeedsReconnect) Error = TEXT("上次操作结果待确认，请重新连接后查看统帅。");
	else if (PendingHeroWrite.IsSet()) Error = TEXT("正在等待服务器确认统帅操作。");
	else Error.Reset();
	if (!Error.IsEmpty()) return false;

	const TCHAR* Protocol = nullptr;
	const auto Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("heroId"), Request.HeroId);
	switch (Request.Type)
	{
	case ERokHeroCommand::Summon: Protocol = TEXT("Hero_SummonHero"); break;
	case ERokHeroCommand::UpgradeSkill: Protocol = TEXT("Hero_HeroSkillLevelUp"); break;
	case ERokHeroCommand::LearnTalent:
		Protocol = TEXT("Hero_TalentUp");
		Body->SetNumberField(TEXT("id"), Request.TalentId);
		break;
	case ERokHeroCommand::SwitchTalent:
	case ERokHeroCommand::ResetTalent:
		Protocol = Request.Type == ERokHeroCommand::SwitchTalent ? TEXT("Hero_ChangeTalentIndex") : TEXT("Hero_ResetTalent");
		Body->SetBoolField(TEXT("useDenar"), Request.bUseGems);
		break;
	case ERokHeroCommand::RenameTalent:
		Protocol = TEXT("Hero_ModifyTalentName");
		Body->SetStringField(TEXT("name"), Request.Name);
		break;
	}
	if (Request.Type != ERokHeroCommand::Summon && Request.Type != ERokHeroCommand::UpgradeSkill)
		Body->SetNumberField(TEXT("index"), Request.Page);
	if (!Protocol || !SendRequest(Protocol, Body))
	{
		Error = TEXT("统帅操作请求未能发送。");
		return false;
	}
	auto& Write = PendingHeroWrite.Emplace();
	Write.Request = Request;
	Write.OuterSession = SprotoSession;
	Write.InnerSession = SprotoSession - 1;
	Write.Deadline = FPlatformTime::Seconds() + 30.0;
	Write.bAcknowledged = Request.Type == ERokHeroCommand::Summon; // Push-only response.
	Write.OnResult = MoveTemp(OnResult);
	return true;
}

void URokGameNetSubsystem::AcknowledgeHeroWrite(const FRokSprotoMessage& Message)
{
	if (!PendingHeroWrite.IsSet() || Message.bRequest || !Message.bHasSession
		|| Message.Session != PendingHeroWrite->InnerSession || !Message.Body) return;
	if (PendingHeroWrite->Request.Type == ERokHeroCommand::UpgradeSkill)
	{
		int64 SkillId = 0, SkillLevel = 0;
		if (RokSprotoJson::TryGetInt64(Message.Body, TEXT("skillId"), SkillId) && SkillId > 0 && SkillId <= MAX_int32
			&& RokSprotoJson::TryGetInt64(Message.Body, TEXT("skillLevel"), SkillLevel) && SkillLevel > 0 && SkillLevel <= MAX_int32)
		{
			PendingHeroWrite->SkillId = static_cast<int32>(SkillId);
			PendingHeroWrite->SkillLevel = static_cast<int32>(SkillLevel);
			PendingHeroWrite->bAcknowledged = true;
		}
	}
	else
	{
		bool bResult = false;
		if (Message.Body->TryGetBoolField(TEXT("result"), bResult))
		{
			if (bResult) PendingHeroWrite->bAcknowledged = true;
			else FinishHeroWrite(TEXT("服务器拒绝了天赋操作。"));
		}
	}
}

void URokGameNetSubsystem::CheckHeroWrite(double Now)
{
	if (!PendingHeroWrite.IsSet()) return;
	const auto& Write = *PendingHeroWrite;
	if (Write.bAcknowledged && URokServerHeroSubsystem::IsCommandApplied(Write.Request,
		GetCollection(TEXT("Hero_HeroInfo"), TEXT("heroInfo")), Write.SkillId, Write.SkillLevel))
	{
		FinishHeroWrite(FString());
	}
	else if (Now >= Write.Deadline)
	{
		bHeroWriteNeedsReconnect = true;
		FinishHeroWrite(TEXT("统帅操作结果待确认，请重新连接后查看。"));
	}
}

void URokGameNetSubsystem::FinishHeroWrite(const FString& Error)
{
	auto Callback = MoveTemp(PendingHeroWrite->OnResult);
	PendingHeroWrite.Reset();
	if (Callback) Callback(Error);
}

void URokGameNetSubsystem::ExpireStaleRequests(double NowSeconds)
{
	if (!Rpc.IsValid()) return;
	// Agent.lua only sends a GateMessage response when it has content. A clean
	// Map_Move with no immediate AOI pushes therefore leaves its optional ACK
	// session pending. Thirty seconds bounds those entries while allowing delayed
	// responses on a busy connection.
	constexpr double PendingResponseTtlSeconds = 30.0;
	for (const int64 Session : Rpc->ExpirePendingResponses(NowSeconds, PendingResponseTtlSeconds))
	{
		PendingInnerByOuter.Remove(Session);
	}
}

void URokGameNetSubsystem::HandleFrame(const TArray<uint8>& Frame)
{
	if (Frame.Num() < GameSessionLength + CompressedFlagLength + 8)
	{
		UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net: short frame (%d bytes)"), Frame.Num());
		return;
	}
	if (Frame.Last() != 0)
	{
		// GameGate.lua / Agent.lua have compression commented out; lzip is not ported.
		UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net: compressed frame not supported"));
		return;
	}
	TArray<uint8> Plain;
	if (!RokSkynetCrypt::DesDecode(Secret, Frame.GetData(), Frame.Num() - GameSessionLength - CompressedFlagLength, Plain))
	{
		UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net: DES decode failed"));
		return;
	}
	FRokSprotoMessage Gate;
	FString Error;
	if (!Rpc->Dispatch(Plain.GetData(), Plain.Num(), Gate, Error))
	{
		UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net: bad gate message: %s"), *Error);
		return;
	}
	int32 RequestTag = INDEX_NONE;
	if (!Gate.bRequest && Gate.bHasSession) PendingInnerByOuter.RemoveAndCopyValue(Gate.Session, RequestTag);

	const TArray<TSharedPtr<FJsonValue>>* Contents = nullptr;
	if (!Gate.Body.IsValid() || !Gate.Body->TryGetArrayField(TEXT("content"), Contents)) return;
	for (const TSharedPtr<FJsonValue>& Item : *Contents)
	{
		const TSharedPtr<FJsonObject>* Content = nullptr;
		if (!Item.IsValid() || !Item->TryGetObject(Content)) continue;
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if ((*Content)->TryGetObjectField(TEXT("error"), ErrorObject))
		{
			int64 Code = 0;
			RokSprotoJson::TryGetInt64(*ErrorObject, TEXT("errorCode"), Code);
			const FRokSprotoProtocol* Failed = Schema.FindProtocol(RequestTag);
			UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net: %s returned error %lld"),
				Failed ? *Failed->Name : TEXT("request"), Code);
			if (CityWrite.IsSet() && !Gate.bRequest && Gate.bHasSession && Gate.Session == CityWrite->OuterSession)
			{
				// Agent.lua reports a handler exception as SERVER_DUMP, even if
				// the handler performed some of its writes before failing.
				if (Code == 500 || Code <= 0)
				{
					bCityWriteNeedsReconnect = true;
					FinishCityWrite(TEXT("服务器处理异常，建造结果未确认。请重新连接后查看城市。"));
				}
				else
				{
					FinishCityWrite(CityWriteError(Code));
				}
			}
			if (PendingTaskClaim.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingTaskClaim->OuterSession)
			{
				bTaskClaimNeedsReconnect = Code == 500 || Code <= 0;
				FinishTaskClaim(bTaskClaimNeedsReconnect
					? TEXT("服务器处理异常，领取结果待确认。请重新连接后查看任务。")
					: FString::Printf(TEXT("服务器拒绝领取奖励（%lld）。"), Code));
			}
			if (PendingHeroWrite.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingHeroWrite->OuterSession)
			{
				bHeroWriteNeedsReconnect = Code == 500 || Code <= 0;
				FinishHeroWrite(bHeroWriteNeedsReconnect
					? TEXT("服务器处理异常，请重新连接后确认统帅状态。")
					: FString::Printf(TEXT("服务器拒绝统帅操作（%lld）。"), Code));
			}
			if (PendingVipClaim.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingVipClaim->OuterSession)
			{
				bVipClaimNeedsReconnect = Code == 500 || Code <= 0 || Code == 1052;
				FinishVipClaim(bVipClaimNeedsReconnect
					? TEXT("领取结果待确认，请重新连接后查看 VIP。")
					: FString::Printf(TEXT("服务器拒绝领取 VIP 奖励（%lld）。"), Code));
			}
			if (PendingArmyDispatch.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingArmyDispatch->OuterSession)
			{
				bArmyDispatchNeedsReconnect = Code == 500 || Code <= 0;
				FinishArmyDispatch(0, ArmyDispatchError(Code, PendingArmyDispatch->TargetType == 1));
			}
			if (PendingGatherRecall.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingGatherRecall->OuterSession)
			{
				bGatherRecallNeedsReconnect = Code == 500 || Code <= 0;
				FinishGatherRecall(bGatherRecallNeedsReconnect
					? TEXT("服务器处理异常，请重新连接后确认召回结果。")
					: FString::Printf(TEXT("服务器拒绝召回（%lld）。"), Code));
			}
			if (PendingWorldSearch.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingWorldSearch->OuterSession)
			{
				FinishWorldSearch(Code == 2003 ? TEXT("该等级的乱军尚未开放。")
					: FString::Printf(TEXT("搜索失败（%lld），请重试。"), Code));
			}
			if (PendingHospitalWrite.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingHospitalWrite->OuterSession)
			{
				bHospitalWriteNeedsReconnect = Code == 500 || Code <= 0;
				FinishHospitalWrite(bHospitalWriteNeedsReconnect
					? TEXT("治疗结果待确认，请重新连接后查看。")
					: FString::Printf(TEXT("服务器拒绝治疗操作（%lld）。"), Code));
			}
			if (PendingTrainingWrite.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingTrainingWrite->OuterSession)
			{
				bTrainingWriteNeedsReconnect = Code == 500 || Code <= 0;
				FinishTrainingWrite(TrainingWriteError(Code));
			}
			if (PendingResearchWrite.IsSet() && !Gate.bRequest && Gate.bHasSession
				&& Gate.Session == PendingResearchWrite->OuterSession)
			{
				bResearchWriteNeedsReconnect = Code == 500 || Code <= 0;
				FinishResearchWrite(bResearchWriteNeedsReconnect
					? TEXT("研究结果待确认，请重新连接后查看。")
					: ResearchWriteError(Code));
			}
			if (CityHarvestDeadline > 0 && !Gate.bRequest && Gate.bHasSession && Gate.Session == CityHarvestOuterSession)
			{
				const bool bUncertain = Code == 500 || Code <= 0;
				FinishCityHarvest(bUncertain ? TEXT("领取结果待确认，请重新连接后查看产出。")
					: FString::Printf(TEXT("资源领取失败（%lld）。"), Code), bUncertain);
			}
			if (Failed && Failed->Name == TEXT("Role_RoleLogin")) Fail(FString::Printf(TEXT("Role_RoleLogin error %lld"), Code));
			continue;
		}
		FString Message;
		if (!(*Content)->TryGetStringField(TEXT("networkMessage"), Message)) continue;
		TArray<uint8> Packed;
		FBase64::Decode(Message, Packed);
		FRokSprotoMessage Inner;
		if (!Rpc->Dispatch(Packed.GetData(), Packed.Num(), Inner, Error))
		{
			UE_LOG(LogRokGameNet, Warning, TEXT("Rok game net: bad inner message: %s"), *Error);
			continue;
		}
		const FRokSprotoProtocol* Protocol = Schema.FindProtocol(Inner.Tag);
		if (!Protocol) continue;
		if (CityHarvestDeadline > 0 && !Inner.bRequest && Inner.bHasSession && Inner.Session == CityHarvestInnerSession)
		{
			int64 Result = 0;
			const bool bKnown = RokSprotoJson::TryGetInt64(Inner.Body, TEXT("result"), Result);
			FinishCityHarvest(!bKnown ? TEXT("领取结果待确认，请重新连接后查看产出。")
				: Result > 0 ? FString() : FString(TEXT("资源领取失败。")), !bKnown || Result > 0);
		}
		AcknowledgeCityWrite(Inner);
		AcknowledgeHeroWrite(Inner);
		AcknowledgeArmyDispatch(Inner);
		AcknowledgeTrainingWrite(Inner);
		AcknowledgeResearchWrite(Inner);
		if (PendingHospitalWrite.IsSet() && !Inner.bRequest && Inner.bHasSession
			&& Inner.Session == PendingHospitalWrite->InnerSession && Inner.Body)
		{
			PendingHospitalWrite->bAcknowledged = true;
		}
		AcknowledgeWorldSearch(Inner);
		if (PendingTaskClaim.IsSet() && !Inner.bRequest && Inner.bHasSession
			&& Inner.Session == PendingTaskClaim->InnerSession)
		{
			int64 ReturnedId = 0;
			const bool bChapter = PendingTaskClaim->Kind == ERokTaskClaim::Chapter;
			PendingTaskClaim->bAcknowledged = RokSprotoJson::TryGetInt64(
				Inner.Body, bChapter ? TEXT("chapterId") : TEXT("taskId"), ReturnedId)
				&& (bChapter ? ReturnedId != PendingTaskClaim->Id : ReturnedId == PendingTaskClaim->Id);
		}
		// AOI observers need the active collection to reflect this push when notified.
		const bool bAoiPush = Inner.bRequest && (Protocol->Name == TEXT("Map_ObjectInfo")
			|| Protocol->Name == TEXT("Map_ObjectDelete"));
		if (bAoiPush) MergePush(*Protocol, Inner.Body);
		HandleMessage(Protocol->Name, !Inner.bRequest, Inner.Body);
		if (Inner.bRequest && !bAoiPush) MergePush(*Protocol, Inner.Body);
	}
}

void URokGameNetSubsystem::HandleMessage(const FString& ProtocolName, bool bResponse, const TSharedPtr<FJsonObject>& Body)
{
	const FString Key = bResponse ? ProtocolName + TEXT(".response") : ProtocolName;
	int32& Count = MessageCounts.FindOrAdd(Key);
	if (Count++ == 0) MessageOrder.Add(Key);
	if (ProtocolName != TEXT("Role_Heart"))
	{
		UE_LOG(LogRokGameNet, Log, TEXT("Rok game net recv %s"), *Key);
		DumpDueTime = FPlatformTime::Seconds() + 2.0;
	}

	if (!bResponse && ProtocolName == TEXT("Role_RoleInfo") && Body.IsValid())
	{
		const TSharedPtr<FJsonObject>* RoleInfo = nullptr;
		int64 ServerMilliseconds = 0;
		if (Body->TryGetObjectField(TEXT("roleInfo"), RoleInfo)
			&& RokSprotoJson::TryGetInt64(*RoleInfo, TEXT("serverTime"), ServerMilliseconds))
		{
			ServerClock.SeedFromServerMilliseconds(ServerMilliseconds, FPlatformTime::Seconds());
		}
	}
	else if (bResponse && ProtocolName == TEXT("Role_Heart") && Body.IsValid())
	{
		int64 EchoedClientTicks = 0;
		int64 ServerMilliseconds = 0;
		if (RokSprotoJson::TryGetInt64(Body, TEXT("clientTime"), EchoedClientTicks)
			&& RokSprotoJson::TryGetInt64(Body, TEXT("serverTime"), ServerMilliseconds))
		{
			double SentMonotonicSeconds = 0.0;
			if (PendingHeartSends.RemoveAndCopyValue(EchoedClientTicks, SentMonotonicSeconds))
			{
				const double ReceivedMonotonicSeconds = FPlatformTime::Seconds();
				if (ServerClock.UpdateFromHeartbeat(ServerMilliseconds, SentMonotonicSeconds, ReceivedMonotonicSeconds)
					&& !bLoggedFirstHeartSample)
				{
					bLoggedFirstHeartSample = true;
					double ServerSeconds = 0.0;
					ServerClock.TryGetServerTimeSeconds(ReceivedMonotonicSeconds, ServerSeconds);
					UE_LOG(LogRokGameNet, Log, TEXT("Rok game net server time synchronized: server_time_seconds=%.3f rtt_ms=%.1f"),
						ServerSeconds, ServerClock.GetPingMilliseconds());
				}
			}
		}
	}
	else if (bResponse && ProtocolName == TEXT("Role_GetRoleList"))
	{
		HandleRoleList(Body);
	}
	else if (bResponse && ProtocolName == TEXT("Role_RoleLogin"))
	{
		bool bResult = false;
		Body->TryGetBoolField(TEXT("result"), bResult);
		if (!bResult)
		{
			Fail(TEXT("Role_RoleLogin returned result=false"));
		}
		else
		{
			UE_LOG(LogRokGameNet, Log, TEXT("Rok game net role ready: rid=%lld"), RoleId);
			SetState(ERokGameNetState::RoleReady);
			SendHeart();
			SendRequest(TEXT("Email_GetEmails"), nullptr);
		}
	}
	else if (!bResponse && ProtocolName == TEXT("System_KickConnect"))
	{
		int64 Reason = 0;
		RokSprotoJson::TryGetInt64(Body, TEXT("reason"), Reason);
		Fail(FString::Printf(TEXT("kicked by server (reason %lld)"), Reason));
	}
	OnMessage.Broadcast(Key, Body);
}

void URokGameNetSubsystem::HandleRoleList(const TSharedPtr<FJsonObject>& Body)
{
	// LoadingMediator: newest role (lastLoginTime) whose gameNode is the node we logged into.
	const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
	int64 BestRid = 0;
	int64 BestTime = -1;
	if (Body.IsValid() && Body->TryGetArrayField(TEXT("roleInfos"), Roles))
	{
		for (const TSharedPtr<FJsonValue>& Item : *Roles)
		{
			const TSharedPtr<FJsonObject>* Role = nullptr;
			if (!Item.IsValid() || !Item->TryGetObject(Role)) continue;
			FString Node;
			(*Role)->TryGetStringField(TEXT("gameNode"), Node);
			int64 Rid = 0, LoginTime = 0;
			RokSprotoJson::TryGetInt64(*Role, TEXT("rid"), Rid);
			RokSprotoJson::TryGetInt64(*Role, TEXT("lastLoginTime"), LoginTime);
			if (Node == ServerName && Rid > 0 && LoginTime > BestTime)
			{
				BestRid = Rid;
				BestTime = LoginTime;
			}
		}
	}
	if (BestRid == 0)
	{
		Fail(TEXT("no role on ") + ServerName + TEXT(" (role creation is not done from UE)"));
		return;
	}
	RoleId = BestRid;
	const URokGameNetSettings* Settings = GetDefault<URokGameNetSettings>();
	TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
	Request->SetNumberField(TEXT("rid"), double(BestRid));
	Request->SetStringField(TEXT("ip"), Settings->ClientIp);
	Request->SetStringField(TEXT("phone"), TEXT("RokUE"));
	Request->SetNumberField(TEXT("language"), Settings->Language);
	Request->SetNumberField(TEXT("platform"), Settings->Platform);
	Request->SetStringField(TEXT("version"), Settings->ClientVersion);
	SendRequest(TEXT("Role_RoleLogin"), Request);
}

void URokGameNetSubsystem::SendHeart()
{
	LastHeartTime = FPlatformTime::Seconds();
	TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
	// Unity NetProxy uses 100 ns ticks since the Unix epoch. JSON numbers are
	// doubles here, so key the pending send by the exact rounded wire value.
	const int64 ClientTicks = int64(double(FDateTime::UtcNow().GetTicks() - FDateTime(1970, 1, 1).GetTicks()));
	Request->SetNumberField(TEXT("clientTime"), double(ClientTicks));
	double ServerSeconds = 0.0;
	Request->SetNumberField(TEXT("serverTime"), TryGetServerTimeSeconds(ServerSeconds) ? ServerSeconds * 1000.0 : 0.0);
	if (SendRequest(TEXT("Role_Heart"), Request))
	{
		if (PendingHeartSends.Num() >= 16) PendingHeartSends.Reset();
		PendingHeartSends.Add(ClientTicks, LastHeartTime);
	}
}

bool URokGameNetSubsystem::TryGetServerTimeSeconds(double& OutServerSeconds) const
{
	return ServerClock.TryGetServerTimeSeconds(FPlatformTime::Seconds(), OutServerSeconds);
}

void URokGameNetSubsystem::ResetWorldMapAoi()
{
	WorldMapObjects = MakeShared<FJsonObject>();
	++WorldMapAoiGeneration;
	WorldMapAoiRevision = 0;
	bHasSentViewportAoi = false;
	LastViewportAoiSentTime = 0.0;
	if (MergedState.IsValid())
	{
		TSharedPtr<FJsonObject> Slot = MakeShared<FJsonObject>();
		Slot->SetObjectField(TEXT("mapObjectInfo"), WorldMapObjects);
		MergedState->SetObjectField(TEXT("Map_ObjectInfo"), Slot);
		MergedState->RemoveField(TEXT("Map_ObjectDelete"));
	}
}

bool URokGameNetSubsystem::RequestViewportAoi(int64 GridX, int64 GridY, bool bIsPreview)
{
	if (bSnapshotReplay || State != ERokGameNetState::RoleReady || !Worker.IsValid()) return false;
	TSharedPtr<FJsonObject> Request;
	if (!MakeViewportAoiBody(GridX, GridY, bIsPreview, Request)) return false;
	const double Now = FPlatformTime::Seconds();
	if (bHasSentViewportAoi)
	{
		if (GridX == LastViewportAoiGridX && GridY == LastViewportAoiGridY
			&& bIsPreview == bLastViewportAoiPreview) return false;
		if (Now - LastViewportAoiSentTime < 0.25) return false;
	}
	if (!SendRequest(TEXT("Map_Move"), Request)) return false;
	LastViewportAoiGridX = GridX;
	LastViewportAoiGridY = GridY;
	bLastViewportAoiPreview = bIsPreview;
	LastViewportAoiSentTime = Now;
	bHasSentViewportAoi = true;
	UE_LOG(LogRokGameNet, Log, TEXT("Rok viewport AOI request grid=(%lld,%lld) pos=(%lld,%lld) preview=%d"),
		GridX, GridY, GridX * 100, GridY * 100, bIsPreview ? 1 : 0);
	return true;
}

bool URokGameNetSubsystem::MakeViewportAoiBody(int64 GridX, int64 GridY, bool bIsPreview,
	TSharedPtr<FJsonObject>& OutBody)
{
	OutBody.Reset();
	// Grid units become protocol units *100, matching RssProxy.SendMapMove.
	// Guard this query API against overflow and implausible camera coordinates.
	constexpr int64 MaxGrid = 1000000;
	if (GridX < 0 || GridY < 0 || GridX > MaxGrid || GridY > MaxGrid)
	{
		return false;
	}
	TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
	Pos->SetNumberField(TEXT("x"), double(GridX * 100));
	Pos->SetNumberField(TEXT("y"), double(GridY * 100));
	OutBody = MakeShared<FJsonObject>();
	OutBody->SetObjectField(TEXT("posInfo"), Pos);
	OutBody->SetBoolField(TEXT("isPreview"), bIsPreview);
	return true;
}

void URokGameNetSubsystem::MergePush(const FRokSprotoProtocol& Protocol, const TSharedPtr<FJsonObject>& Body)
{
	const FRokSprotoStruct* Type = Schema.GetStruct(Protocol.RequestType);
	if (!Type || !Body.IsValid()) return;
	if (Protocol.Name == TEXT("Battle_BattleDamageInfo"))
	{
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!Body->TryGetArrayField(TEXT("battleDamageInfo"), Rows)) return;
		const double Now = FPlatformTime::Seconds();
		for (auto It = RecentBattleDamage.CreateIterator(); It; ++It)
			if (Now - It.Value().At >= 2.0) It.RemoveCurrent();
		for (const auto& Value : *Rows)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			int64 ObjectId = 0, Amount = 0;
			if (Value && Value->TryGetObject(Row)
				&& RokSprotoJson::TryGetInt64(*Row, TEXT("objectIndex"), ObjectId)
				&& RokSprotoJson::TryGetInt64(*Row, TEXT("damage"), Amount) && ObjectId > 0 && Amount > 0)
				RecentBattleDamage.Add(ObjectId, {Amount, Now});
		}
		return;
	}
	if (Protocol.Name == TEXT("Map_ObjectDelete"))
	{
		int64 ObjectId = 0;
		if (WorldMapObjects.IsValid() && RokSprotoJson::TryGetInt64(Body, TEXT("objectId"), ObjectId)
			&& ObjectId > 0 && WorldMapObjects->HasField(FString::Printf(TEXT("%lld"), ObjectId)))
		{
			WorldMapObjects->RemoveField(FString::Printf(TEXT("%lld"), ObjectId));
			++WorldMapAoiRevision;
		}
		return;
	}
	if (Protocol.Name == TEXT("Map_ObjectInfo"))
	{
		const TSharedPtr<FJsonObject>* Info = nullptr;
		int64 ObjectId = 0;
		if (!Body->TryGetObjectField(TEXT("mapObjectInfo"), Info)
			|| !RokSprotoJson::TryGetInt64(*Info, TEXT("objectId"), ObjectId) || ObjectId <= 0)
		{
			return;
		}
		const FRokSprotoStruct* ObjectType = nullptr;
		for (const FRokSprotoField& Field : Type->Fields)
		{
			if (Field.Name == TEXT("mapObjectInfo") && Field.Type == ERokSprotoType::Struct)
			{
				ObjectType = Schema.GetStruct(Field.StructIndex);
				break;
			}
		}
		if (!ObjectType) return;
		if (!WorldMapObjects.IsValid()) ResetWorldMapAoi();
		const FString Key = FString::Printf(TEXT("%lld"), ObjectId);
		const TSharedPtr<FJsonObject>* OldObject = nullptr;
		TSharedPtr<FJsonObject> Target = WorldMapObjects->TryGetObjectField(Key, OldObject)
			? *OldObject : MakeShared<FJsonObject>();
		MergeSprotoObject(Schema, *ObjectType, Target, *Info, false);
		WorldMapObjects->SetObjectField(Key, Target);
		++WorldMapAoiRevision;
		return;
	}
	TSharedPtr<FJsonObject> Slot;
	const TSharedPtr<FJsonObject>* Existing = nullptr;
	if (MergedState->TryGetObjectField(Protocol.Name, Existing)) Slot = *Existing;
	else
	{
		Slot = MakeShared<FJsonObject>();
		MergedState->SetObjectField(Protocol.Name, Slot);
	}

	if (Protocol.Name == TEXT("Email_EmailList"))
	{
		// Keep the full inbox in session memory and all mail bodies out of the state dump.
		const TArray<TSharedPtr<FJsonValue>>* Emails = nullptr;
		if (!Body->TryGetArrayField(TEXT("emailInfo"), Emails)) return;
		TArray<TSharedPtr<FJsonValue>> Metadata;
		for (const auto& Value : *Emails)
		{
			const TSharedPtr<FJsonObject>* Email = nullptr;
			if (!Value || !Value->TryGetObject(Email)) continue;
			MergeBattleReportMail(*Email);
			auto Row = MakeShared<FJsonObject>();
			for (const TCHAR* Field : {TEXT("emailIndex"), TEXT("emailId"), TEXT("status")})
			{
				if (auto Data = (*Email)->TryGetField(Field)) Row->SetField(Field, Data);
			}
			Metadata.Add(MakeShared<FJsonValueObject>(Row));
		}
		auto Inbox = MakeShared<FJsonObject>();
		Inbox->SetArrayField(TEXT("emailInfo"), MoveTemp(Metadata));
		MergeSprotoObject(Schema, *Type, Slot, Inbox, true);
		return;
	}
	MergeSprotoObject(Schema, *Type, Slot, Body, true);
}

int32 URokGameNetSubsystem::GetUnreadMailCount() const
{
	const auto Emails = GetCollection(TEXT("Email_EmailList"), TEXT("emailInfo"));
	if (!Emails) return 0;
	int32 Count = 0;
	for (const auto& Pair : Emails->Values)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		int64 EmailId = 0, Status = 0;
		if (Pair.Value && Pair.Value->TryGetObject(Row)
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("emailId"), EmailId) && EmailId > 0
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("status"), Status) && Status == 0) ++Count;
	}
	return Count;
}

TSharedPtr<FJsonObject> URokGameNetSubsystem::GetRoleInfo() const
{
	const TSharedPtr<FJsonObject>* Slot = nullptr;
	const TSharedPtr<FJsonObject>* Role = nullptr;
	if (MergedState.IsValid() && MergedState->TryGetObjectField(TEXT("Role_RoleInfo"), Slot)
		&& (*Slot)->TryGetObjectField(TEXT("roleInfo"), Role))
	{
		return *Role;
	}
	return nullptr;
}

TSharedPtr<FJsonObject> URokGameNetSubsystem::GetCollection(const FString& Protocol, const FString& Field) const
{
	const TSharedPtr<FJsonObject>* Slot = nullptr;
	const TSharedPtr<FJsonObject>* Collection = nullptr;
	if (MergedState.IsValid() && MergedState->TryGetObjectField(Protocol, Slot)
		&& (*Slot)->TryGetObjectField(Field, Collection))
	{
		return *Collection;
	}
	return nullptr;
}

void URokGameNetSubsystem::WriteSessionDump()
{
	if (bSnapshotReplay) return;
	if (!MergedState.IsValid() || MessageOrder.Num() == 0) return;
	// No secret, token or iggid here: login summary, message counts and the merged state.
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("written_utc"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("game_node"), ServerName);
	Root->SetStringField(TEXT("uid"), Uid);
	Root->SetNumberField(TEXT("rid"), double(RoleId));
	Root->SetStringField(TEXT("state"), StaticEnum<ERokGameNetState>()->GetNameStringByValue(int64(State)));
	if (!LastError.IsEmpty()) Root->SetStringField(TEXT("last_error"), LastError);
	TSharedPtr<FJsonObject> Counts = MakeShared<FJsonObject>();
	for (const FString& Key : MessageOrder) Counts->SetNumberField(Key, MessageCounts.FindRef(Key));
	Root->SetObjectField(TEXT("message_counts"), Counts);
	Root->SetObjectField(TEXT("state_by_protocol"), MergedState);
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RokNet"), TEXT("LastSession.json"));
	if (FFileHelper::SaveStringToFile(RokSprotoJson::ToString(Root, true), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogRokGameNet, Log, TEXT("Rok game net session dump: %s (%d message kinds)"), *Path, MessageOrder.Num());
	}
}
