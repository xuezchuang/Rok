#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RokCityGammaCompositor.h"
#include "RokCityGammaCompositeAuditSubsystem.generated.h"

class AActor;
class APlayerController;
class ARokCitySpriteActor;
class UCameraComponent;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;
class FJsonObject;

/** Flag-only GameApp GPU audit. It never modifies the city map or construction ledger. */
UCLASS()
class ROK_API URokCityGammaCompositeAuditSubsystem : public UTickableWorldSubsystem
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
	bool Begin();
	bool StartFrame();
	bool CapturePreparedFrame();
	bool FinishFrame();
	bool Restore();
	void Finish(const FString& Error = FString());
	TSharedPtr<FJsonObject> RecordModel(int32 Index) const;
	TSharedPtr<FJsonObject> RecordStats(const FRokCityGammaCompositorStats& Stats) const;

	FString RunDirectory;
	FString EconomyBefore;
	FString NativeRenderSha;
	FString NativeContractSha;
	FString NativeVerifySha;
	FString ResourceMeshVerifySha;
	FString PatternSha;
	TArray<TSharedPtr<FJsonObject>> Frames;
	TArray<FString> Failures;
	TArray<TSharedPtr<FJsonObject>> Models;
	TSharedPtr<FJsonObject> Native;
	TSharedPtr<FJsonObject> Contract;
	TArray<TWeakObjectPtr<ARokCitySpriteActor>> TemporaryActors;
	TArray<TStrongObjectPtr<UMaterialInstanceDynamic>> ModelMaterials;
	TStrongObjectPtr<UMaterialInstanceDynamic> FlatBackgroundMaterial;
	TStrongObjectPtr<UMaterialInstanceDynamic> PatternBackgroundMaterial;
	TStrongObjectPtr<UMaterialInstanceDynamic> ExactOutputMaterial;
	TStrongObjectPtr<UTexture2D> PatternTexture;
	TWeakObjectPtr<UStaticMeshComponent> BackgroundComponent;
	TStrongObjectPtr<USceneCaptureComponent2D> CaptureComponent;
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
	FRokCityGammaTargetRegistration Registration;
	FRokCityGammaCompositorStats InitialStats;
	FRokCityGammaCompositorStats FrameBeforeStats;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<AActor> SavedViewTarget;
	FTransform SavedCameraTransform;
	float SavedCameraFov = 0.0f;
	int32 ActorCountBefore = 0;
	int32 TemporaryCountBefore = 0;
	int32 Stage = -1;
	double Elapsed = 0.0;
	double StageStart = 0.0;
	double FramePreparationSeconds = 0.0;
	bool bPreparingFrame = false;
	bool bWaitingForGpu = false;
	bool bFinished = false;
	bool bRestored = false;
};
