#include "RokServerSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "RokServerSettings.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void URokServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const URokServerSettings* Settings = GetDefault<URokServerSettings>();
	ServerBaseUrl = Settings->ServerBaseUrl;
	PlayerId = Settings->DefaultPlayerId;

	PingHealth();
	if (Settings->bAutoConnectOnGameInstanceStart)
	{
		ConnectAsGuest(PlayerId);
	}
}

void URokServerSubsystem::PingHealth()
{
	SendJsonRequest(TEXT("GET"), TEXT("/health"), FString(), TEXT("health"), [this](const FString& Body)
	{
		LastHealthJson = Body;
		UE_LOG(LogTemp, Log, TEXT("ueRok server health: %s"), *Body);
	});
}

void URokServerSubsystem::ConnectAsGuest(const FString& InPlayerId)
{
	PlayerId = InPlayerId.IsEmpty() ? TEXT("ue-editor") : InPlayerId;

	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("player_id"), PlayerId);
	JsonObject->SetStringField(TEXT("client_version"), TEXT("ue5.2-prototype"));

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObject, Writer);

	SendJsonRequest(TEXT("POST"), TEXT("/api/session/connect"), Body, TEXT("connect"), [this](const FString& ResponseBody)
	{
		FString Token;
		if (!TryReadStringField(ResponseBody, TEXT("session_token"), Token))
		{
			HandleRequestFailure(TEXT("connect"), 200, TEXT("Missing session_token in response"));
			return;
		}

		SessionToken = Token;
		bConnected = true;
		LastError.Reset();
		UE_LOG(LogTemp, Log, TEXT("ueRok server connected as %s"), *PlayerId);
		OnConnected.Broadcast(ResponseBody);
		FetchWorldState();
	});
}

void URokServerSubsystem::FetchWorldState()
{
	if (SessionToken.IsEmpty())
	{
		HandleRequestFailure(TEXT("world_state"), 0, TEXT("No active ueRok session"));
		return;
	}

	SendJsonRequest(TEXT("GET"), FString::Printf(TEXT("/api/world/state?session_token=%s"), *SessionToken), FString(), TEXT("world_state"), [this](const FString& Body)
	{
		LastWorldStateJson = Body;
		UE_LOG(LogTemp, Log, TEXT("ueRok world state: %s"), *Body);
		OnWorldStateUpdated.Broadcast(Body);
	});
}

void URokServerSubsystem::StartTestMarch(int32 ToX, int32 ToY, int32 TroopCount)
{
	if (SessionToken.IsEmpty())
	{
		HandleRequestFailure(TEXT("start_march"), 0, TEXT("No active ueRok session"));
		return;
	}

	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("session_token"), SessionToken);
	JsonObject->SetNumberField(TEXT("from_x"), 0);
	JsonObject->SetNumberField(TEXT("from_y"), 0);
	JsonObject->SetNumberField(TEXT("to_x"), ToX);
	JsonObject->SetNumberField(TEXT("to_y"), ToY);
	JsonObject->SetNumberField(TEXT("troop_count"), FMath::Max(1, TroopCount));
	JsonObject->SetNumberField(TEXT("duration_seconds"), 30);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObject, Writer);

	SendJsonRequest(TEXT("POST"), TEXT("/api/march/start"), Body, TEXT("start_march"), [this](const FString&)
	{
		FetchWorldState();
	});
}

FString URokServerSubsystem::BuildUrl(const FString& Path) const
{
	FString Base = ServerBaseUrl;
	Base.RemoveFromEnd(TEXT("/"));
	return Base + Path;
}

void URokServerSubsystem::SendJsonRequest(
	const FString& Verb,
	const FString& Path,
	const FString& Body,
	const FString& DebugName,
	TFunction<void(const FString&)> OnSuccess)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(Verb);
	Request->SetURL(BuildUrl(Path));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!SessionToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *SessionToken));
	}
	if (!Body.IsEmpty())
	{
		Request->SetContentAsString(Body);
	}

	Request->OnProcessRequestComplete().BindWeakLambda(this, [this, DebugName, OnSuccess](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
	{
		const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		const FString ResponseBody = Response.IsValid() ? Response->GetContentAsString() : FString();
		if (!bSucceeded || ResponseCode < 200 || ResponseCode >= 300)
		{
			HandleRequestFailure(DebugName, ResponseCode, ResponseBody);
			return;
		}
		OnSuccess(ResponseBody);
	});

	if (!Request->ProcessRequest())
	{
		HandleRequestFailure(DebugName, 0, TEXT("ProcessRequest returned false"));
	}
}

void URokServerSubsystem::HandleRequestFailure(const FString& DebugName, int32 ResponseCode, const FString& Body)
{
	bConnected = false;
	LastError = FString::Printf(TEXT("%s failed with HTTP %d: %s"), *DebugName, ResponseCode, *Body);
	UE_LOG(LogTemp, Warning, TEXT("ueRok server error: %s"), *LastError);
	OnServerError.Broadcast(LastError);
}

bool URokServerSubsystem::TryReadStringField(const FString& Json, const FString& FieldName, FString& OutValue)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}
	return JsonObject->TryGetStringField(FieldName, OutValue);
}
