#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokScoutCatalog.h"
#include "RokUiWindowAnimation.h"
#include "RokScoutScreenWidget.generated.h"

class UCanvasPanel;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UButton;

DECLARE_MULTICAST_DELEGATE(FRokScoutScreenClosed);

/** Unity UI_Win_Scout browse path. Camp configuration never creates a live scout queue. */
UCLASS()
class ROK_API URokScoutScreenWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Open(int32 KnownCampLevel = INDEX_NONE);
	UFUNCTION() void RequestClose();
	void CloseImmediately();
	UFUNCTION() void ShowDescription();
	UFUNCTION() void ShowList();
	bool IsScreenOpen() const { return bOpen; }
	bool IsDescriptionVisible() const { return bDescription; }
	FRokScoutScreenClosed OnClosed;
#if WITH_DEV_AUTOMATION_TESTS
	void AdvanceForTesting(float Seconds) { Advance(Seconds); }
#endif
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void NativeDestruct() override;
private:
	void Build();
	void Advance(float Seconds);
	void ApplyAnimation();
	void AcquireKeyboardFocus();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> MotionCanvas;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> List;
	UPROPERTY(Transient) TObjectPtr<USizeBox> Description;
	UPROPERTY(Transient) TObjectPtr<UButton> BackButton;
	UPROPERTY(Transient) TObjectPtr<UButton> AnimationBlocker;
	FRokScoutCatalog Catalog;
	FRokUiWindowAnimation WindowAnimation;
	FString CatalogError;
	int32 CampLevel = INDEX_NONE;
	float PanelSeconds = 0.25f;
	bool bOpen = false;
	bool bDescription = false;
	bool bPendingKeyboardFocus = false;
	TWeakPtr<SWidget> PreviousKeyboardFocus;
};
