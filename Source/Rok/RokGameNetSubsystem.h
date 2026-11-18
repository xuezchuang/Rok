#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokSproto.h"
#include "RokServerClock.h"
#include "RokServerHeroSubsystem.h"
#include "RokGameNetSubsystem.generated.h"

class FRokGameNetWorker;
enum class ERokTaskClaim : uint8;

// Where the Unity client connects (HotfixUtil.RokNasServerHost/Port, LoginCommand.cs
// editor branch). The account itself is not here: it is read from a local, untracked
// file (AccountFile, default Saved/RokNet/Account.ini with IggId= and AccessToken=).
UCLASS(Config=Game, DefaultConfig)
class ROK_API URokGameNetSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	FString LoginHost = TEXT("192.168.30.101");

	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	int32 LoginPort = 58888;

	// serverNode the editor login asks for (LoginCommand.cs: "game1").
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	FString GameNode = TEXT("game1");

	// Replaces the game host the login server hands out (empty = use it as is).
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	FString GameHostOverride;

	// NetProxy.Connection: "3" outside iOS/Android.
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	int32 Platform = 3;

	// Unity SystemLanguage.ChineseSimplified.
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	int32 Language = 40;

	// IGGDefault.AppConfigIP; 127.0.0.1 makes AccountMgr.AuthIGGID skip the IGG token check.
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	FString ClientIp = TEXT("127.0.0.1");

	// Unity ProjectSettings bundleVersion (Role_RoleLogin.version).
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	FString ClientVersion = TEXT("1.8.20");

	// Project-relative account file (never committed).
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	FString AccountFile = TEXT("Saved/RokNet/Account.ini");

	// Connect when the game instance starts; -RokNet forces it on, -RokNoNet off.
	UPROPERTY(EditAnywhere, Config, Category="Rok Game Net")
	bool bConnectOnStart = false;
};

UENUM(BlueprintType)
enum class ERokGameNetState : uint8
{
	Offline,
	LoggingIn,
	Connected,
	RoleReady,
	Failed
};

enum class ERokResearchCommand : uint8 { Start, Instant, Award, Stop };

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRokGameNetMessage, const FString& /*Protocol*/, const TSharedPtr<FJsonObject>& /*Body*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRokGameNetStateChanged, ERokGameNetState);
using FRokCityWriteResult = TFunction<void(const FString& /* Failure, empty on success */)>;

struct FRokWorldMapSearchHit
{
	int64 ObjectId = 0;
	FVector2D UnityPosition = FVector2D::ZeroVector;
};
using FRokWorldMapSearchResult = TFunction<void(FRokWorldMapSearchHit, const FString&)>;

// TCP client for the Rok skynet server, a port of the Unity NetProxy/SprotoSocketAp flow:
// login-server handshake (DH + hmac64 + DES token), game-server redirect auth, then
// DES-wrapped GateMessage traffic. After login it asks for Role_GetRoleList and logs in
// the newest role on the selected game node; it never creates a role. City writes
// are explicit player commands and complete against server pushes.
UCLASS()
class ROK_API URokGameNetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool Connect(FString& OutError);
	void Disconnect(const FString& Reason);

	ERokGameNetState GetState() const { return State; }
	const FString& GetLastError() const { return LastError; }
	int64 GetRoleId() const { return RoleId; }
	bool IsSnapshotReplay() const { return bSnapshotReplay; }
	UFUNCTION(BlueprintPure, Category="Rok Game Net|Server Time")
	bool HasServerTime() const { return ServerClock.HasTime(); }
	UFUNCTION(BlueprintPure, Category="Rok Game Net|Server Time")
	bool TryGetServerTimeSeconds(double& OutServerSeconds) const;
	// -1 until a valid echoed heartbeat has supplied an RTT sample.
	UFUNCTION(BlueprintPure, Category="Rok Game Net|Server Time")
	double GetServerPingMilliseconds() const { return ServerClock.GetPingMilliseconds(); }

	// Merged server state: State[Protocol][Field]. Keyed arrays (*T(key)) become objects
	// keyed by the key value; struct pushes are merged field by field like the Unity proxies.
	TSharedPtr<FJsonObject> GetMergedState() const { return MergedState; }
	TSharedPtr<FJsonObject> GetRoleInfo() const;
	int32 GetUnreadMailCount() const;
	// Full inbox is session-only; mail bodies must never enter the persisted state dump.
	const TMap<int64, TSharedPtr<FJsonObject>>& GetSessionMail() const { return SessionMail; }
	bool IsMailInboxLoading() const;
	bool RequestMailDetails(int64 EmailIndex);
	void MarkMailRead(int64 EmailIndex);
	bool TakeMailEnclosure(int64 EmailIndex);
	const TMap<int64, TSharedPtr<FJsonObject>>& GetBattleReportMail() const { return BattleReportMail; }
	uint64 GetBattleReportRevision() const { return BattleReportRevision; }
	bool RequestBattleReport(int64 EmailIndex);
	void MarkBattleReportRead(int64 EmailIndex);
	bool HasBattleReportDownloadFailed(int64 EmailIndex) const { return FailedBattleReportDownloads.Contains(EmailIndex); }
	bool GetRecentBattleDamage(int64 ObjectId, int64& Damage, double& Age) const;
	// Keyed collection of a push, e.g. ("Build_BuildingInfo", "buildingInfo").
	TSharedPtr<FJsonObject> GetCollection(const FString& Protocol, const FString& Field) const;
	// Active Map_ObjectInfo objects keyed by objectId. A legacy dump's last single push
	// is not a complete AOI collection. Generation changes when the session resets;
	// revision changes for each accepted object update or removal within a session.
	TSharedPtr<FJsonObject> GetWorldMapObjects() const { return WorldMapObjects; }
	uint64 GetWorldMapAoiGeneration() const { return WorldMapAoiGeneration; }
	uint64 GetWorldMapAoiRevision() const { return WorldMapAoiRevision; }
	// Unity RssProxy.SendMapMove uses whole map-grid units, multiplied by 100 on wire.
	// This only updates the viewport subscription; it never moves the player's city or army.
	bool RequestViewportAoi(int64 GridX, int64 GridY, bool bIsPreview);
	// SearchType 0 is barbarian; 1–4 are the Unity resource search types.
	bool RequestWorldMapSearch(int32 Type, int32 Level, FRokWorldMapSearchResult OnResult, FString& OutError);
	bool CanRequestCityWrite(FString& OutFailure) const;
	bool RequestCityHarvest(const TMap<int32, int64>& GainTimes, TFunction<void(const FString&)> OnResult, FString& OutError);
	bool IsCityHarvestWaiting(int32 Index, int64 GainTime) const;
	bool RequestCreateBuilding(int32 Type, FIntPoint Grid, FRokCityWriteResult OnResult, FString& OutFailure);
	bool RequestBuildingUpgrade(int32 Index, int32 CurrentLevel, bool bImmediately,
		FRokCityWriteResult OnResult, FString& OutFailure);
	// Unity Configs.data: workQueueItem and workQueueDenar.
	static constexpr int32 WorkerHireItemId = 502020001;
	static constexpr int32 WorkerHireGemCost = 150;
	bool TryGetOwnedItemCount(int32 ItemId, int64& OutCount) const;
	bool RequestWorkerHire(bool bUseItem, FRokCityWriteResult OnResult, FString& OutFailure);
	bool RequestBuildingSpeedUp(int32 QueueIndex, int32 BuildingIndex, int32 ItemId, int32 Count,
		bool bCostGems, FRokCityWriteResult OnResult, FString& OutFailure);

	bool RequestTaskClaim(ERokTaskClaim Kind, int32 Id, TFunction<void(const FString&)> OnResult, FString& OutError);
	bool RequestHeroCommand(const FRokHeroCommand& Request, TFunction<void(const FString&)> OnResult, FString& Error);
	bool RequestHospitalTreatment(const TMap<int32, int32>& Counts, bool bImmediately, bool bCollect,
		TFunction<void(const FString&)> OnResult, FString& Error);
	bool IsHospitalTreatmentPending() const { return PendingHospitalWrite.IsSet(); }
	bool RequestVipClaim(bool bFreeChest, TFunction<void(const FString&)> OnResult, FString& OutError);
	bool IsVipClaimPending() const { return PendingVipClaim.IsSet(); }
	bool IsTrainingWritePending() const { return PendingTrainingWrite.IsSet(); }
	bool RequestResearchCommand(ERokResearchCommand Command, int32 StudyType, TFunction<void(const FString&)> OnResult, FString& Error);
	bool IsResearchWritePending() const { return PendingResearchWrite.IsSet(); }

	FOnRokGameNetMessage OnMessage;
	FOnRokGameNetStateChanged OnStateChanged;

	// Encodes a request into the GateMessage frame the server expects (exposed for tests).
	bool BuildRequestFrame(const FString& Protocol, const TSharedPtr<FJsonObject>& Body, TArray<uint8>& OutFrame, FString& OutError);

private:
	friend class URokServerArmySubsystem;
	friend class FRokResourceGatherNetTest;
	friend class FRokBarbarianAttackNetTest;
	friend class FRokResourceRecallNetTest;
	friend class FRokServerTrainingWriteTest;
	friend class FRokServerResearchWriteTest;
	friend class FRokResearchWidgetTest;
	friend class FRokGameNetServerClockMessageTest;
	friend class FRokGameNetPushMergeTest;
	friend class FRokGameNetPlayerAssetsPushTest;
	friend class FRokGameNetWorldMapPushTest;
	friend class FRokGameNetWorldMapReplayTest;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRokWorldDamageRenderedTest;
	friend class FRokGatherArmyRenderedTest;
#endif
	friend class FRokGameNetViewportRequestTest;
	friend class FRokWorldMapSearchProtocolTest;
	friend class FRokGameNetCityWriteTest;
	friend struct FRokCityUpgradeReplay;
	friend class FRokGameNetStartupSettingsTest;
	friend class FRokVipClaimConfirmationTest;

	static bool ShouldAutoConnect(const TCHAR* CommandLine, bool bAutomation, bool bDefaultConnect,
		const FString& ProjectUserConfig);
	struct FCityWrite
	{
		enum class EQueueAction : uint8 { None, Hire, SpeedUp };
		int64 OuterSession = 0;
		int64 InnerSession = 0;
		int32 Index = 0;
		int32 Type = 0; // Nonzero for creation.
		int32 TargetLevel = 0;
		FIntPoint Grid = FIntPoint::ZeroValue;
		bool bImmediately = false;
		bool bAcknowledged = false;
		EQueueAction QueueAction = EQueueAction::None;
		int32 QueueIndex = 0;
		int64 PreviousQueueTime = 0;
		double Deadline = 0.0;
		FRokCityWriteResult OnResult;
	};

	bool SendCityWrite(FCityWrite Write, const FString& Protocol, const TSharedPtr<FJsonObject>& Body, FString& OutFailure);
	void TrackSentCityWrite(FCityWrite Write);
	void AcknowledgeCityWrite(const FRokSprotoMessage& Message);
	void CheckCityWrite(double NowSeconds);
	void FinishCityWrite(const FString& Failure);
	bool StartSnapshotReplay(const FString& Path, FString& OutError);
	bool LoadSchema(FString& OutError);
	bool LoadAccount(FString& OutIggId, FString& OutToken, FString& OutError) const;
	bool Tick(float DeltaSeconds);
	void SetState(ERokGameNetState NewState);
	void Fail(const FString& Error);
	bool SendRequest(const FString& Protocol, const TSharedPtr<FJsonObject>& Body);
	void HandleFrame(const TArray<uint8>& Frame);
	void HandleMessage(const FString& Protocol, bool bResponse, const TSharedPtr<FJsonObject>& Body);
	void HandleRoleList(const TSharedPtr<FJsonObject>& Body);
	void MergePush(const FRokSprotoProtocol& Protocol, const TSharedPtr<FJsonObject>& Body);
	void ResetWorldMapAoi();
	void ExpireStaleRequests(double NowSeconds);
	static bool MakeViewportAoiBody(int64 GridX, int64 GridY, bool bIsPreview, TSharedPtr<FJsonObject>& OutBody);
	void SendHeart();
	void WriteSessionDump();

	struct FWorldSearch
	{
		int32 Type = 0;
		int32 Level = 0;
		int64 OuterSession = 0;
		int64 InnerSession = 0;
		double Deadline = 0;
		FRokWorldMapSearchResult OnResult;
	};
	TOptional<FWorldSearch> PendingWorldSearch;
	int32 CachedWorldSearchType = INDEX_NONE;
	int32 CachedWorldSearchLevel = 0;
	int32 NextWorldSearchHit = 0;
	double WorldSearchCacheUntil = 0;
	TArray<FRokWorldMapSearchHit> WorldSearchHits;
	static TSharedPtr<FJsonObject> MakeWorldSearchBody(int32 Type, int32 Level);
	static bool ReadWorldSearchHits(const TSharedPtr<FJsonObject>& Body, int32 Type, int32 Level,
		FVector2D Origin, TArray<FRokWorldMapSearchHit>& OutHits);
	void AcknowledgeWorldSearch(const FRokSprotoMessage& Message);
	void FinishWorldSearch(const FString& Error);
	FRokWorldMapSearchHit TakeWorldSearchHit();

	struct FArmyDispatchWrite
	{
		int64 OuterSession = 0;
		int64 InnerSession = 0;
		int64 TargetId = 0;
		int32 MainHeroId = 0;
		int32 ArmyIndex = 0;
		double Deadline = 0.0;
		TFunction<void(int32, const FString&)> OnResult;
		int32 TargetType = 4; // MapMarchTargetType.Collect
	};
	bool CanRequestArmyDispatch(FString& Error) const;
	bool SendArmyDispatch(const TSharedPtr<FJsonObject>& Body, int64 TargetId, int32 MainHeroId,
		TFunction<void(int32, const FString&)> OnResult, FString& Error, int32 ArmyIndex = 0);
	void AcknowledgeArmyDispatch(const FRokSprotoMessage& Message);
	void CheckArmyDispatch(double Now);
	void FinishArmyDispatch(int32 ArmyIndex, const FString& Error);
	TOptional<FArmyDispatchWrite> PendingArmyDispatch;
	bool bArmyDispatchNeedsReconnect = false;

	struct FGatherRecall
	{
		int64 OuterSession;
		int32 ArmyIndex;
		double Deadline;
		TFunction<void(const FString&)> OnResult;
	};
	static TSharedPtr<FJsonObject> MakeGatherRecallBody(int32 ArmyIndex);
	bool SendGatherRecall(int32 ArmyIndex, TFunction<void(const FString&)> OnResult, FString& Error);
	void CheckGatherRecall(double Now);
	void FinishGatherRecall(const FString& Error);
	struct FTrainingWrite
	{
		int32 BuildingIndex = 0;
		int32 ArmyType = 0;
		int32 Tier = 0;
		int32 Count = 0;
		int32 TroopId = 0;
		int64 PreviousTroops = 0;
		bool bReceive = false;
		bool bImmediately = false;
		bool bAcknowledged = false;
		int64 InnerSession = 0;
		int64 OuterSession = 0;
		double Deadline = 0;
		TFunction<void(const FString&)> OnResult;
	};
	static TSharedPtr<FJsonObject> MakeTrainingBody(const FTrainingWrite& Write);
	bool CanRequestTrainingWrite(FString& Error) const;
	bool SendTrainingWrite(FTrainingWrite Write, FString& Error);
	void AcknowledgeTrainingWrite(const FRokSprotoMessage& Message);
	void CheckTrainingWrite(double Now);
	void FinishTrainingWrite(const FString& Error);
	TOptional<FTrainingWrite> PendingTrainingWrite;
	bool bTrainingWriteNeedsReconnect = false;

	struct FResearchWrite
	{
		ERokResearchCommand Command = ERokResearchCommand::Start;
		int32 StudyType = 0;
		int64 PreviousLevel = 0;
		int64 InnerSession = 0, OuterSession = 0;
		double Deadline = 0;
		bool bAcknowledged = false;
		TFunction<void(const FString&)> OnResult;
	};
	void AcknowledgeResearchWrite(const FRokSprotoMessage& Message);
	void CheckResearchWrite(double Now);
	void FinishResearchWrite(const FString& Error);
	TOptional<FResearchWrite> PendingResearchWrite;
	bool bResearchWriteNeedsReconnect = false;

	struct FHospitalWrite
	{
		int64 InnerSession = 0;
		int64 OuterSession = 0;
		double Deadline = 0;
		int64 BeforeQueue = 0;
		TMap<int32, int32> ExpectedWounded;
		bool bStartsQueue = false;
		bool bAcknowledged = false;
		TFunction<void(const FString&)> OnResult;
	};
	TOptional<FHospitalWrite> PendingHospitalWrite;
	bool bHospitalWriteNeedsReconnect = false;
	void CheckHospitalWrite(double Now);
	void FinishHospitalWrite(const FString& Error);

	TOptional<FGatherRecall> PendingGatherRecall;
	bool bGatherRecallNeedsReconnect = false;

	struct FTaskClaim
	{
		ERokTaskClaim Kind{};
		int32 Id = 0;
		int64 OuterSession = 0;
		int64 InnerSession = 0;
		double Deadline = 0;
		bool bAcknowledged = false;
		TFunction<void(const FString&)> OnResult;
	};
	void CheckTaskClaim(double Now);
	void FinishTaskClaim(const FString& Error);
	TOptional<FTaskClaim> PendingTaskClaim;
	bool bTaskClaimNeedsReconnect = false;

	struct FVipClaim
	{
		bool bFreeChest = false;
		int64 OuterSession = 0;
		double Deadline = 0.0;
		TFunction<void(const FString&)> OnResult;
	};
	void CheckVipClaim(double Now);
	void FinishVipClaim(const FString& Error);
	TOptional<FVipClaim> PendingVipClaim;
	bool bVipClaimNeedsReconnect = false;

	struct FHeroWrite
	{
		FRokHeroCommand Request;
		int64 OuterSession = 0;
		int64 InnerSession = 0;
		double Deadline = 0.0;
		bool bAcknowledged = false;
		int32 SkillId = 0;
		int32 SkillLevel = 0;
		TFunction<void(const FString&)> OnResult;
	};
	void AcknowledgeHeroWrite(const FRokSprotoMessage& Message);
	void CheckHeroWrite(double Now);
	void FinishHeroWrite(const FString& Error);
	TOptional<FHeroWrite> PendingHeroWrite;
	bool bHeroWriteNeedsReconnect = false;

	void FinishCityHarvest(const FString& Error, bool bKeepGainTimes);
	TFunction<void(const FString&)> OnCityHarvestResult;
	TMap<int32, int64> CityHarvestGainTimes;
	TArray<int32> CityHarvestInFlight;
	int64 CityHarvestInnerSession = 0;
	int64 CityHarvestOuterSession = 0;
	double CityHarvestDeadline = 0;

	TOptional<FCityWrite> CityWrite;
	bool bCityWriteNeedsReconnect = false;
	FRokSprotoSchema Schema;
	TUniquePtr<FRokSprotoRpc> Rpc;
	// Shared so the header needs no complete worker type for its deleter.
	TSharedPtr<FRokGameNetWorker, ESPMode::ThreadSafe> Worker;
	FRunnableThread* WorkerThread = nullptr;
	FTSTicker::FDelegateHandle TickHandle;

	ERokGameNetState State = ERokGameNetState::Offline;
	FString LastError;
	TArray<uint8> Secret;
	FString ServerName;
	FString Uid;
	int64 RoleId = 0;
	int64 SprotoSession = 0;
	int32 GameSession = 0;
	double LastHeartTime = 0.0;
	FRokServerClock ServerClock;
	TMap<int64, double> PendingHeartSends;
	bool bLoggedFirstHeartSample = false;
	bool bSnapshotReplay = false;
	double DumpDueTime = 0.0;
	TMap<int64, int32> PendingInnerByOuter;
	TMap<FString, int32> MessageCounts;
	TArray<FString> MessageOrder;
	TSharedPtr<FJsonObject> MergedState;
	void MergeBattleReportMail(const TSharedPtr<FJsonObject>& Email);
	void DownloadBattleReport(int64 EmailIndex, const FString& Url);
	TMap<int64, TSharedPtr<FJsonObject>> SessionMail;
	TMap<int64, TSharedPtr<FJsonObject>> BattleReportMail;
	TMap<int64, double> BattleReportRequests;
	TSet<int64> FailedBattleReportDownloads;
	uint64 BattleReportRevision = 0;
	uint64 BattleReportSession = 0;
	struct FRecentBattleDamage { int64 Amount = 0; double At = 0; };
	TMap<int64, FRecentBattleDamage> RecentBattleDamage;
	TSharedPtr<FJsonObject> WorldMapObjects;
	uint64 WorldMapAoiGeneration = 0;
	uint64 WorldMapAoiRevision = 0;
	double LastViewportAoiSentTime = 0.0;
	int64 LastViewportAoiGridX = 0;
	int64 LastViewportAoiGridY = 0;
	bool bLastViewportAoiPreview = false;
	bool bHasSentViewportAoi = false;
};
