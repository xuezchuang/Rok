#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RokWorldPositionMiddleTipAuditSubsystem.generated.h"

class FJsonObject;
class FJsonValue;
class UUserWidget;
class UTexture2D;
class UWorld;
class URokMainHudWidget;
class URokWorldMapHudWidget;
class URokUnityFeatureScreenWidget;
class URokUnityRuntimeStateSubsystem;
class URokServerSubsystem;

/** Opt-in local-only real Slate/keyboard audit, retained across city-to-world OpenLevel. */
UCLASS()
class ROK_API URokWorldPositionMiddleTipAuditSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override { return nullptr; }
	virtual TStatId GetStatId() const override;

private:
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TWeakObjectPtr<URokServerSubsystem> Server;
	UPROPERTY()
	TObjectPtr<UTexture2D> PreparedMiddleAtlas;
	TWeakObjectPtr<URokMainHudWidget> CityHud;
	TWeakObjectPtr<URokMainHudWidget> TacticalHud;
	TWeakObjectPtr<URokWorldMapHudWidget> WorldHud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FDelegateHandle ScreenshotHandle;
	FDelegateHandle RawScreenshotHandle;
	struct FRawScreenshot
	{
		int32 Width = 0;
		int32 Height = 0;
		uint64 Frame = 0;
		double CapturedMonotonicSeconds = 0.0;
		TArray<FColor> Pixels;
	};
	FRawScreenshot PendingRaw;
	TArray<FRawScreenshot> RawScreenshots;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Pending;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> InputAttempts;
	TArray<FString> Failures;
	FString Directory;
	FString PendingPath;
	FString BaselineGameBusiness;
	FString BaselineCityBusiness;
	FString BaselineWorldBusiness;
	FString RetryKey;
	FVector2D OriginalPointer = FVector2D::ZeroVector;
	FVector2D LastPointer = FVector2D::ZeroVector;
	FVector CityCameraLocation = FVector::ZeroVector;
	FRotator CityCameraRotation = FRotator::ZeroRotator;
	float CityCameraDxf = 0.0f;
	double Elapsed = 0.0;
	double StepSince = 0.0;
	uint64 ScreenshotFrame = 0;
	uint64 BaselineCityPrimary = 0;
	int32 Step = 0;
	int32 NextStep = 0;
	int32 StableFrames = 0;
	int32 RetryCount = 0;
	int32 CityHudRootCount = 0;
	int32 WorldHudRootCount = 0;
	int32 TacticalHudCount = 0;
	int32 UnownedMainHudCount = 0;
	double WorldMapReadySince = -1.0;
	double TipTriggerElapsedSeconds = -1.0;
	double TipTriggerMonotonicSeconds = -1.0;
	uint64 TipTriggerFrame = 0;
	bool bBegun = false;
	bool bFinished = false;
	bool bScreenshotDone = false;
	bool bDeferredPngComplete = false;

	UWorld* CurrentWorld() const;
	bool IsCity() const;
	bool IsWorldMap() const;
	bool RefreshWidgets();
	bool Begin();
	bool OwnWindowFocus();
	bool Boundary() const;
	bool Expect(bool Condition, const TCHAR* Reason);
	bool RetryInput(const TCHAR* Kind, const TCHAR* Selector, const FVector2D& Point,
		const TArray<TSharedPtr<FJsonValue>>& PathTypes, const FString& Terminal,
		bool bFocus, bool bOwn, bool bForeground, bool bTarget, bool bHovered);
	bool Click(UUserWidget* Owner, const TCHAR* Selector);
	bool Edit(const TCHAR* Selector, const FString& Text);
	bool ReadySearch() const;
	bool AtWorldCenter(float X, float Y) const;
	bool WorldHudCoordinateMatchesCamera() const;
	bool WorldCoordinateAtlasReady() const;
	bool StableSearch();
	bool TipReady() const;
	bool TipCollapsed() const;
	double TipUiElapsed() const;
	void Enter(int32 NewStep);
	void Capture(const TCHAR* Phase, int32 After);
	void ScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void ScreenshotProcessed();
	bool WriteDeferredScreenshots();
	void AdvanceCapture();
	TSharedRef<FJsonObject> GameBusinessState() const;
	TSharedRef<FJsonObject> WorldBusinessState() const;
	TSharedRef<FJsonObject> State(const TCHAR* GeometryStage = TEXT("final_or_boundary")) const;
	bool Save(bool Complete) const;
	void Finish(const FString& Reason = FString());
};
