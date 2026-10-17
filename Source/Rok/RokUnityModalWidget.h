#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RokUiWindowAnimation.h"
#include "RokPlayerHeadCatalog.h"
#include "RokLeaderboardCatalog.h"
#include "RokUnityModalWidget.generated.h"

class URokCommanderMeshWidget;

class AActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UImage;
class USizeBox;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UWidget;
class URokBuildingUpgradeSubsystem;
class URokUnityFeatureScreenWidget;

UENUM()
enum class ERokUnityModalMode : uint8
{
	None,
	PlayerProfile,
	PlayerHeadSelection,
	PlayerPowerDetails,
	PlayerRanking,
	CivilizationSelection,
	CivilizationChangeConfirmation,
	ResourceInventory,
	BuildingInfo,
	BuildingUpgrade,
	WarWarning,
	BuffList,
	ChangeName,
};

enum class ERokChangeNameValidationResult : uint8
{
	Valid,
	Empty,
	TooShort,
	TooLong,
	OnlyDigits,
	SameAsCurrent,
	BannedWord,
	BannedWordDataUnavailable,
};

/**
 * Runtime C++ reconstruction of the Unity RoK modal family used by the player
 * profile, quick-use resource list, building information, and upgrade flows.
 * The widget samples original Unity atlas Sprite rects and keeps gameplay state
 * behind the existing C++ subsystems.
 */
UCLASS()
class ROK_API URokUnityModalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenPlayerProfile();
	void OpenPlayerHeadSelection();
	void OpenPlayerPowerDetails();
	void OpenPlayerRanking();
	void OpenCivilizationSelection();
	void OpenCivilizationChangeConfirmation();
	void OpenChangeName();
	void OpenResourceInventory(int32 InitialCategory = 0, bool bUseReferenceCaptureState = false);
	void OpenBuildingInfo(AActor* BuildingActor);
	// Opt-in reference audit only; does not create an owned building or current level.
	void OpenResourceBuildingInfoReferencePreview(int32 UnityBuildingTypeId);
	void OpenBuildingUpgrade(AActor* BuildingActor);
	void OpenWarWarning(bool bUseReferenceCaptureState = false);
	void OpenBuffList(bool bUseReferenceCaptureState = false);
	void SetFeatureScreenWidget(
		URokUnityFeatureScreenWidget* InFeatureScreenWidget);
	void CloseModal();
	/** User dismissal; unlike CloseModal, keeps the input mask until exit ends. */
	void RequestCloseModal();
	bool IsModalOpen() const { return ActiveMode != ERokUnityModalMode::None; }
	// Production handoff proof: the requested actor and BuildingInfo content are mounted.
	bool IsBuildingInfoOpenForActor(const AActor* Actor) const;
#if !UE_BUILD_SHIPPING
	AActor* GetSelectedBuildingActorForAudit() const;
	bool IsBarracksLevelPageOpenForAudit() const { return ActiveMode == ERokUnityModalMode::BuildingUpgrade && bBarracksLevelPage; }
	int32 GetBarracksLevelSelectedLevelForAudit() const { return BarracksLevelSelectedLevel; }
	float GetBarracksLevelScrollOffsetForAudit() const;
	float GetBarracksLevelPageTimeForAudit() const { return BarracksLevelPageTime; }
#endif
#if WITH_DEV_AUTOMATION_TESTS
	void AdvanceWindowAnimationForTesting(float DeltaSeconds) { AdvanceWindowAnimation(DeltaSeconds); }
	const FRokUiWindowAnimation& GetWindowAnimationForTesting() const { return WindowAnimation; }
#endif
	static ERokChangeNameValidationResult ValidateChangeNameCandidate(
		const FString& Candidate,
		const FString& CurrentName,
		bool bHasCurrentName,
		const TArray<FString>& BannedWords,
		bool bBannedWordsLoaded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	friend class URokCityBuffReferenceAuditSubsystem;
	UPROPERTY()
	TObjectPtr<USizeBox> ExpandedDesignSizeBox;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ExpandedDesignCanvas;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ModeCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BuffListAnimationRoot;

	UPROPERTY()
	TObjectPtr<UButton> AnimationInputBlocker;

	UPROPERTY()
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> ChangeNameInput;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChangeNameCharacterCount;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChangeNameValidationFeedback;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChangeNameServiceBoundaryState;

	UPROPERTY()
	TObjectPtr<UButton> ChangeNameConfirmButton;

	UPROPERTY()
	TObjectPtr<URokUnityFeatureScreenWidget> FeatureScreenWidget;

	TWeakObjectPtr<AActor> SelectedBuildingActor;
	ERokUnityModalMode ActiveMode = ERokUnityModalMode::None;
	ERokUnityModalMode AnimatedMode = ERokUnityModalMode::None;
	ERokUnityModalMode CloseReturnMode = ERokUnityModalMode::None;
	FRokUiWindowAnimation WindowAnimation;
	float CivilizationListAnimationTime = 0.5f;
	float CivilizationHeroAnimationTime = 0.5f;
	float CivilizationSelectionAnimationTime = 0;
	UPROPERTY(Transient)
	TObjectPtr<UImage> CivilizationSelectionPulse;
	bool bCivilizationSwitchAnimation = false;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CivilizationDetailRoot;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CivilizationListRoot;
	UPROPERTY(Transient)
	TObjectPtr<URokCommanderMeshWidget> CivilizationHero;
	int32 CivilizationHeroId = INDEX_NONE;
	int32 SelectedResourceCategory = 0;
	int32 SelectedPlayerHeadPage = 0;
	int32 SelectedPlayerHeadIndex = 0;
	TArray<FRokPlayerHeadRow> PlayerHeadPageRows;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> PlayerHeadGridScroll;
	float PlayerHeadScrollOffset = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlayerKillTipLayer;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlayerKillTipRoot;
	float PlayerKillTipTime = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlayerPowerActionTipLayer;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlayerPowerActionTipRoot;
	float PlayerPowerActionTipTime = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BuildingDescriptionRoot;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BuildingLevelDetailsRoot;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> BuildingLevelDetailsScroll;
	UPROPERTY(Transient)
	TObjectPtr<UButton> BuildingLevelBackButton;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BarracksLevelDetailsRoot;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> BarracksLevelDetailsScroll;
	UPROPERTY(Transient)
	TObjectPtr<UButton> BarracksLevelBackButton;
	int32 ResourceBuildingReferenceType = 0;
	bool bBuildingLevelPage = false;
	float BuildingPageTime = 0.25f;
	bool bBarracksLevelPage = false;
	int32 BarracksLevelSelectedLevel = 0;
	float BarracksLevelPageTime = 0.25f;
	int32 SelectedRankingCategory = INDEX_NONE;
	TArray<FRokLeaderboardRow> PlayerRankingRows;
	int32 SelectedCivilizationId = 0;
	bool bReferenceWarWarningCaptureState = false;
	bool bReferenceBuffListCaptureState = false;
	bool bReferenceResourceCaptureState = false;
	bool bChangeNameHasCurrentName = false;
	bool bChangeNameBannedWordsLoadAttempted = false;
	bool bChangeNameBannedWordsLoaded = false;
	bool bChangeNameSubmitServiceAvailable = false;
	bool bUpdatingChangeNameInput = false;
	FVector2D CurrentExpandedDesignSize = FVector2D(1440.0f, 900.0f);
	FString ChangeNameDraft;
	FString ChangeNameCurrentName;
	TArray<FString> ChangeNameBannedWords;

	void BuildWidgetTree();
	void RebuildModeContent(bool bOpening = false);
	void UpdateWindowAnimationForMode(bool bOpening);
	void ApplyWindowAnimation();
	void AdvanceWindowAnimation(float DeltaSeconds);
	void BuildType1Window(const FText& Title);
	void BuildType2Window(const FText& Title, bool bSourceBarracksUpgradeParent = false);
	void BuildType3Window(const FText& Title);
	void BuildTypeMidWindow(const FText& Title);
	void BuildPlayerProfile();
	void BuildPlayerHeadSelection();
	void BuildPlayerPowerDetails();
	void BuildPlayerRanking();
	void BuildCivilizationSelection();
	void BuildCivilizationChangeWindow(const FText& Title);
	void BuildCivilizationChangeConfirmation();
	void BuildChangeName();
	void BuildResourceInventory();
	void BuildBuildingInfo();
	void BuildResourceBuildingInfo(int32 Type, int32 KnownLevel);
	friend class FRokBuildingLevelDetailsTest;
	UFUNCTION()
	void HandleBuildingLevelInfoClicked();
	UFUNCTION()
	void HandleBuildingLevelBackClicked();
	void ApplyBuildingPageAnimation();
	void BuildBuildingUpgrade();
	void BuildBarracksLevelDetails(int32 KnownLevel);
	void ApplyBarracksLevelPageAnimation();
	UFUNCTION()
	void HandleBarracksLevelInfoClicked();
	UFUNCTION()
	void HandleBarracksLevelBackClicked();
	void BuildWarWarning();
	void BuildBuffList();
	void ResetChangeNameWidgetPointers();
	bool EnsureChangeNameBannedWordsLoaded();
	void UpdateChangeNameValidationState();
	FText GetChangeNameValidationMessage(
		ERokChangeNameValidationResult Result) const;
	void UpdateExpandedDesignResolution(const FVector2D& RootSize);

	UCanvasPanel* MakeResourceRow(
		int32 RowIndex,
		int32 Amount,
		int32 Quality,
		int32 OwnedCount,
		int32 ShortcutPrice,
		const FText& ResourceName,
		const FIntRect& ItemIconRect);
	UButton* MakeResourceTab(int32 CategoryIndex, bool bSelected);
	UButton* MakePlayerDataButton(const FName& Name, const FIntRect& IconRect, const FText& Label);
	UButton* MakeTextAtlasButton(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		const FText& Label,
		int32 FontSize,
		const FLinearColor& TextColor,
		const FText& SecondaryLabel = FText::GetEmpty());
	UButton* MakeAtlasButton(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize);
	UButton* MakeIndexedHitButton(
		const FName& Name,
		int32 ActionId);
	UImage* MakeAtlasImage(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize);
	UBorder* MakeAtlasBorder(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		const FMargin& BrushMargin);
	UTextBlock* MakeModalText(
		const FName& Name,
		int32 FontSize,
		const FLinearColor& Color,
		bool bBold = false,
		ETextJustify::Type Justification = ETextJustify::Left);
	void PlaceOnCanvas(
		UCanvasPanel* Parent,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder = 0);
	UTexture2D* LoadUiTexture(const TCHAR* TexturePath) const;
	FSlateBrush MakeAtlasBrush(
		UTexture2D* Texture,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin& BrushMargin = FMargin(0.0f)) const;
	URokBuildingUpgradeSubsystem* GetUpgradeSubsystem() const;

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleResourceTabFood();

	UFUNCTION()
	void HandleResourceTabWood();

	UFUNCTION()
	void HandleResourceTabStone();

	UFUNCTION()
	void HandleResourceTabGold();

	UFUNCTION()
	void HandlePreviewItemAction();

	UFUNCTION()
	void HandleProfileArmyClicked();

	UFUNCTION()
	void HandleProfileAvatarClicked();

	UFUNCTION()
	void HandleProfileRenameClicked();

	UFUNCTION()
	void HandleChangeNameTextChanged(const FText& Text);

	UFUNCTION()
	void HandleChangeNameConfirmClicked();

	UFUNCTION()
	void HandleChangeNameCancelClicked();

	UFUNCTION()
	void HandleProfileCivilizationClicked();

	UFUNCTION()
	void HandleCivilizationContinueClicked();

	UFUNCTION()
	void HandleProfileKillsHelpClicked();

	UFUNCTION()
	void ClosePlayerKillTip();
	UFUNCTION()
	void ClosePlayerPowerActionTip();

	UFUNCTION()
	void HandleProfileActionHelpClicked();

	UFUNCTION()
	void HandleProfileActionAddClicked();

	UFUNCTION()
	void HandleProfileMoreClicked();

	UFUNCTION()
	void HandleProfileRankingClicked();

	UFUNCTION()
	void HandleProfileSettingsClicked();

	void HandleProfileSubscreenAction(int32 ActionId);

	UFUNCTION()
	void HandleStartUpgrade();

	UFUNCTION()
	void HandleInstantUpgradePreview();

	UFUNCTION()
	void HandleBuffListMoreClicked();

	void ShowProfileBoundaryFeedback(
		const FText& Message,
		const TCHAR* LogAction);
};
