#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokServerSubsystem.generated.h"

class IHttpRequest;
class IHttpResponse;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRokServerJsonEvent, const FString&, JsonPayload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRokServerErrorEvent, const FString&, ErrorMessage);

UCLASS()
class ROK_API URokServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Rok Server")
	void PingHealth();

	UFUNCTION(BlueprintCallable, Category="Rok Server")
	void ConnectAsGuest(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category="Rok Server")
	void FetchWorldState();

	UFUNCTION(BlueprintCallable, Category="Rok Server")
	void StartTestMarch(int32 ToX = 8, int32 ToY = 8, int32 TroopCount = 25);

	UPROPERTY(BlueprintReadOnly, Category="Rok Server")
	FString ServerBaseUrl;

	UPROPERTY(BlueprintReadOnly, Category="Rok Server")
	FString SessionToken;

	UPROPERTY(BlueprintReadOnly, Category="Rok Server")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category="Rok Server")
	FString LastHealthJson;

	UPROPERTY(BlueprintReadOnly, Category="Rok Server")
	FString LastWorldStateJson;

	UPROPERTY(BlueprintReadOnly, Category="Rok Server")
	FString LastError;

	UPROPERTY(BlueprintReadOnly, Category="Rok Server")
	bool bConnected = false;

	UPROPERTY(BlueprintAssignable, Category="Rok Server")
	FRokServerJsonEvent OnConnected;

	UPROPERTY(BlueprintAssignable, Category="Rok Server")
	FRokServerJsonEvent OnWorldStateUpdated;

	UPROPERTY(BlueprintAssignable, Category="Rok Server")
	FRokServerErrorEvent OnServerError;

private:
	FString BuildUrl(const FString& Path) const;
	void SendJsonRequest(
		const FString& Verb,
		const FString& Path,
		const FString& Body,
		const FString& DebugName,
		TFunction<void(const FString&)> OnSuccess);
	void HandleRequestFailure(const FString& DebugName, int32 ResponseCode, const FString& Body);
	static bool TryReadStringField(const FString& Json, const FString& FieldName, FString& OutValue);
};
