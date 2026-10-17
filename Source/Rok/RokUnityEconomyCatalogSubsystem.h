#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokUnityEconomyCatalogSubsystem.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct FRokUnityEconomyCatalogSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 LocalizedNameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 UnresolvedNameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 IconCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ShopItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ShopCategoryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 SpeedItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 AcquisitionSourceItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ChargePageCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ChargePagingTypeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ChargeGemPackageCount = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityEconomyItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 NameLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString DisplayNameZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString DisplayNameEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 TipsLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 SubType = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Type = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 TypeDescriptionLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString TypeDescriptionZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 TypeGroup = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString IconAtlas;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString IconSprite;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 IconX = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 IconY = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 IconWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 IconHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 IconAtlasWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 IconAtlasHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 RequiredLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Quality = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	bool bBatchUse = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ButtonLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ItemFunction = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Data1 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Data2 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 DescriptionLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString DescriptionTemplateZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString DescriptionTemplateEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 DescriptionData1 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 DescriptionData2 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 TopLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 TopData = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	TArray<int32> AcquisitionSourceIds;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ShortcutPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ShopPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ShopCurrencyTypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	bool bNormalShopItem = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 RedDotPrompt = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 SpeedDurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString SpeedApplicability;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString SourcePath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 SourceRow = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityEconomyChargePage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 PagingLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 PagingType = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Preposition = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Sort = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString SourcePath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 SourceRow = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityEconomyChargeGemPackage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 NameLocaleId = 0;

	// RechargeGemMall.price is a Price-table lookup key, not a cash amount.
	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 ProductId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 DenarNum = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 FirstPresenter = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 Presenter = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString GemIcon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 RechargeProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 MailId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	FString SourcePath;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Economy")
	int32 SourceRow = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityAllianceCreateConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 EstablishCost = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 CurrencyTypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 NameMinCharacters = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 NameMaxCharacters = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 TagMinCharacters = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 TagMaxCharacters = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 NoticeMaxCharacters = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Alliance")
	int32 SearchMinCharacters = 0;
};

UCLASS()
class ROK_API URokUnityEconomyCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Rok Unity Economy")
	bool LoadCatalog();

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	bool IsCatalogLoaded() const { return bCatalogLoaded; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	FRokUnityEconomyCatalogSummary GetSummary() const { return Summary; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	TArray<FRokUnityEconomyItem> GetItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	bool FindItemById(int32 ItemId, FRokUnityEconomyItem& OutItem) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	TArray<FRokUnityEconomyItem> GetShopItemsByCategory(int32 ItemType) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	TArray<FRokUnityEconomyChargePage> GetChargePages() const { return ChargePages; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	TArray<FRokUnityEconomyChargeGemPackage> GetChargeGemPackages() const { return ChargeGemPackages; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Alliance")
	FRokUnityAllianceCreateConfig GetAllianceCreateConfig() const { return AllianceCreateConfig; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Economy")
	bool FindChargePageByPagingType(
		int32 PagingType,
		FRokUnityEconomyChargePage& OutPage) const;

private:
	FRokUnityEconomyCatalogSummary Summary;
	TArray<FRokUnityEconomyItem> Items;
	TMap<int32, int32> ItemIndexById;
	TArray<FRokUnityEconomyChargePage> ChargePages;
	TArray<FRokUnityEconomyChargeGemPackage> ChargeGemPackages;
	TMap<int32, int32> ChargePageIndexByPagingType;
	FRokUnityAllianceCreateConfig AllianceCreateConfig;
	bool bCatalogLoaded = false;

	static FRokUnityEconomyCatalogSummary ParseSummary(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityEconomyItem ParseItem(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityEconomyChargePage ParseChargePage(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityEconomyChargeGemPackage ParseChargeGemPackage(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityAllianceCreateConfig ParseAllianceCreateConfig(const TSharedPtr<FJsonObject>& Object);
};
