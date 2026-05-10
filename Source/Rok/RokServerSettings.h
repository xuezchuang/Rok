#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RokServerSettings.generated.h"

UCLASS(Config=Game, DefaultConfig)
class ROK_API URokServerSettings : public UObject
{
	GENERATED_BODY()

public:
	URokServerSettings();

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Rok Server")
	FString ServerBaseUrl;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Rok Server")
	FString DefaultPlayerId;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Rok Server")
	bool bAutoConnectOnGameInstanceStart;
};
