#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RokCityGammaInputAuditSubsystem.generated.h"

class FJsonObject;
class UTexture2D;
struct FRokCityGammaInputReadback;

/** Read-only, opt-in GPU input audit for the 22 sealed masked-city textures. */
UCLASS()
class ROK_API URokCityGammaInputAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
	struct FRow
	{
		FString Id, Asset, Role, ExpectedSha, SourcePngSha, UAssetSha;
		int32 Width = 0, Height = 0;
	};
	bool Prepare();
	bool StartNext();
	void Poll();
	void RecordReady();
	void Finish(const FString& Failure = FString());
	void CancelReadback();
	FString Directory;
	FString InputSha;
	TArray<FRow> Rows;
	TArray<TStrongObjectPtr<UTexture2D>> PinnedTextures;
	TArray<TSharedPtr<FJsonObject>> Records;
	TArray<FString> Failures;
	TSharedPtr<FRokCityGammaInputReadback, ESPMode::ThreadSafe> Active;
	int32 Index = 0;
	double StartSeconds = 0.0, PendingSeconds = 0.0;
	uint64 StartFrame = 0;
	bool bPrepared = false, bFinished = false;
};
