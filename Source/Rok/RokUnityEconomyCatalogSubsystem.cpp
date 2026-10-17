#include "RokUnityEconomyCatalogSubsystem.h"

#include "RokDataPaths.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* EconomyCatalogRelativePath = TEXT("unity_economy_catalog.json");
const TCHAR* EconomyCatalogSchema = TEXT("rok.unity_economy_catalog.v1");

TSharedPtr<FJsonObject> JsonObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	return Value && Value->IsValid() ? (*Value)->AsObject() : nullptr;
}

int32 JsonIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return 0;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return FMath::RoundToInt(NumberValue);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return FCString::Atoi(*StringValue);
	}
	return 0;
}

FString JsonStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : FString();
}

bool JsonBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return false;
	}

	bool BoolValue = false;
	if (Object->TryGetBoolField(FieldName, BoolValue))
	{
		return BoolValue;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return !FMath::IsNearlyZero(NumberValue);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return StringValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || FCString::Atoi(*StringValue) != 0;
	}
	return false;
}

void ReadIntArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, TArray<int32>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	OutValues.Reserve(Values->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		if (Value->Type == EJson::Number)
		{
			OutValues.Add(FMath::RoundToInt(Value->AsNumber()));
		}
		else if (Value->Type == EJson::String)
		{
			OutValues.Add(FCString::Atoi(*Value->AsString()));
		}
	}
}
}

void URokUnityEconomyCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCatalog();
}

bool URokUnityEconomyCatalogSubsystem::LoadCatalog()
{
	Summary = FRokUnityEconomyCatalogSummary();
	Items.Reset();
	ItemIndexById.Reset();
	ChargePages.Reset();
	ChargeGemPackages.Reset();
	ChargePageIndexByPagingType.Reset();
	AllianceCreateConfig = FRokUnityAllianceCreateConfig();
	bCatalogLoaded = false;

	const FString CatalogPath = FPaths::ConvertRelativePathToFull(RokDataPaths::Resolve(EconomyCatalogRelativePath));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *CatalogPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity economy catalog not found: %s"), *CatalogPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity economy catalog is invalid JSON: %s"), *CatalogPath);
		return false;
	}

	FString Schema;
	RootObject->TryGetStringField(TEXT("schema"), Schema);
	if (Schema != EconomyCatalogSchema)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok Unity economy catalog has unexpected schema '%s': %s"),
			*Schema,
			*CatalogPath);
		return false;
	}

	Summary = ParseSummary(JsonObjectField(RootObject, TEXT("summary")));
	AllianceCreateConfig = ParseAllianceCreateConfig(
		JsonObjectField(
			JsonObjectField(RootObject, TEXT("contracts")),
			TEXT("alliance_ui")));

	bool bRowsValid = true;
	const TArray<TSharedPtr<FJsonValue>>* ItemValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("items"), ItemValues) || !ItemValues)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity economy catalog has no items array: %s"), *CatalogPath);
		return false;
	}

	Items.Reserve(ItemValues->Num());
	ItemIndexById.Reserve(ItemValues->Num());
	for (const TSharedPtr<FJsonValue>& Value : *ItemValues)
	{
		const TSharedPtr<FJsonObject> ItemObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!ItemObject.IsValid())
		{
			bRowsValid = false;
			continue;
		}

		FRokUnityEconomyItem Item = ParseItem(ItemObject);
		if (Item.Id <= 0 || ItemIndexById.Contains(Item.Id))
		{
			bRowsValid = false;
			continue;
		}

		const int32 NewIndex = Items.Add(MoveTemp(Item));
		ItemIndexById.Add(Items[NewIndex].Id, NewIndex);
	}

	const TArray<TSharedPtr<FJsonValue>>* ChargePageValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("charge_pages"), ChargePageValues) || !ChargePageValues)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity economy catalog has no charge_pages array: %s"), *CatalogPath);
		return false;
	}

	TSet<int32> ChargePageIds;
	int32 LastChargePageSort = -1;
	ChargePages.Reserve(ChargePageValues->Num());
	ChargePageIndexByPagingType.Reserve(ChargePageValues->Num());
	for (const TSharedPtr<FJsonValue>& Value : *ChargePageValues)
	{
		const TSharedPtr<FJsonObject> PageObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!PageObject.IsValid())
		{
			bRowsValid = false;
			continue;
		}

		FRokUnityEconomyChargePage Page = ParseChargePage(PageObject);
		if (Page.Id <= 0
			|| Page.PagingLocaleId <= 0
			|| Page.PagingType <= 0
			|| Page.Sort <= 0
			|| Page.Icon.IsEmpty()
			|| Page.SourcePath.IsEmpty()
			|| Page.SourceRow <= 0
			|| ChargePageIds.Contains(Page.Id)
			|| ChargePageIndexByPagingType.Contains(Page.PagingType)
			|| Page.Sort < LastChargePageSort)
		{
			bRowsValid = false;
			continue;
		}

		LastChargePageSort = Page.Sort;
		ChargePageIds.Add(Page.Id);
		const int32 NewIndex = ChargePages.Add(MoveTemp(Page));
		ChargePageIndexByPagingType.Add(ChargePages[NewIndex].PagingType, NewIndex);
	}

	const TArray<TSharedPtr<FJsonValue>>* ChargeGemPackageValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("charge_gem_packages"), ChargeGemPackageValues)
		|| !ChargeGemPackageValues)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity economy catalog has no charge_gem_packages array: %s"), *CatalogPath);
		return false;
	}

	TSet<int32> ChargeGemPackageIds;
	TSet<int32> ChargeProductIds;
	ChargeGemPackages.Reserve(ChargeGemPackageValues->Num());
	for (const TSharedPtr<FJsonValue>& Value : *ChargeGemPackageValues)
	{
		const TSharedPtr<FJsonObject> PackageObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!PackageObject.IsValid())
		{
			bRowsValid = false;
			continue;
		}

		FRokUnityEconomyChargeGemPackage Package = ParseChargeGemPackage(PackageObject);
		if (Package.Id <= 0
			|| Package.NameLocaleId <= 0
			|| Package.ProductId <= 0
			|| Package.DenarNum <= 0
			|| Package.FirstPresenter < 0
			|| Package.Presenter < 0
			|| Package.GemIcon.IsEmpty()
			|| Package.RechargeProgress <= 0
			|| Package.MailId <= 0
			|| Package.SourcePath.IsEmpty()
			|| Package.SourceRow <= 0
			|| ChargeGemPackageIds.Contains(Package.Id)
			|| ChargeProductIds.Contains(Package.ProductId))
		{
			bRowsValid = false;
			continue;
		}

		ChargeGemPackageIds.Add(Package.Id);
		ChargeProductIds.Add(Package.ProductId);
		ChargeGemPackages.Add(MoveTemp(Package));
	}

	int32 LocalizedNameCount = 0;
	int32 IconCount = 0;
	int32 ShopItemCount = 0;
	int32 SpeedItemCount = 0;
	int32 AcquisitionSourceItemCount = 0;
	TSet<int32> ShopCategoryTypes;
	for (const FRokUnityEconomyItem& Item : Items)
	{
		LocalizedNameCount += Item.DisplayNameZhCn.IsEmpty() ? 0 : 1;
		IconCount += Item.Icon.IsEmpty() ? 0 : 1;
		SpeedItemCount += Item.SpeedDurationSeconds > 0 && !Item.SpeedApplicability.IsEmpty() ? 1 : 0;
		AcquisitionSourceItemCount += Item.AcquisitionSourceIds.IsEmpty() ? 0 : 1;
		if (Item.bNormalShopItem)
		{
			++ShopItemCount;
			ShopCategoryTypes.Add(Item.Type);
			if (Item.ShopPrice <= 0 || Item.ShopCurrencyTypeId != 104)
			{
				bRowsValid = false;
			}
		}
	}

	const int32 UnresolvedNameCount = Items.Num() - LocalizedNameCount;
	const int32 ChargePagingTypeCount = ChargePageIndexByPagingType.Num();
	const bool bAllianceConfigValid =
		AllianceCreateConfig.EstablishCost > 0
		&& AllianceCreateConfig.CurrencyTypeId == 104
		&& AllianceCreateConfig.NameMinCharacters > 0
		&& AllianceCreateConfig.NameMaxCharacters >= AllianceCreateConfig.NameMinCharacters
		&& AllianceCreateConfig.TagMinCharacters > 0
		&& AllianceCreateConfig.TagMaxCharacters >= AllianceCreateConfig.TagMinCharacters
		&& AllianceCreateConfig.NoticeMaxCharacters > 0
		&& AllianceCreateConfig.SearchMinCharacters > 0;
	bCatalogLoaded = bRowsValid
		&& Items.Num() > 0
		&& ItemIndexById.Num() == Items.Num()
		&& ChargePages.Num() > 0
		&& ChargePageIndexByPagingType.Num() == ChargePages.Num()
		&& ChargeGemPackages.Num() > 0
		&& Summary.ItemCount == Items.Num()
		&& Summary.LocalizedNameCount == LocalizedNameCount
		&& Summary.UnresolvedNameCount == UnresolvedNameCount
		&& Summary.IconCount == IconCount
		&& Summary.ShopItemCount == ShopItemCount
		&& Summary.ShopCategoryCount == ShopCategoryTypes.Num()
		&& Summary.SpeedItemCount == SpeedItemCount
		&& Summary.AcquisitionSourceItemCount == AcquisitionSourceItemCount
		&& Summary.ChargePageCount == ChargePages.Num()
		&& Summary.ChargePagingTypeCount == ChargePagingTypeCount
		&& Summary.ChargeGemPackageCount == ChargeGemPackages.Num()
		&& bAllianceConfigValid;

	if (bCatalogLoaded)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Loaded Rok Unity economy catalog: items=%d localized=%d shop=%d categories=%d speed=%d charge_pages=%d charge_gems=%d alliance_cost=%d"),
			Summary.ItemCount,
			Summary.LocalizedNameCount,
			Summary.ShopItemCount,
			Summary.ShopCategoryCount,
			Summary.SpeedItemCount,
			Summary.ChargePageCount,
			Summary.ChargeGemPackageCount,
			AllianceCreateConfig.EstablishCost);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok Unity economy catalog failed consistency checks: items=%d/%d localized=%d/%d shop=%d/%d speed=%d/%d charge_pages=%d/%d charge_gems=%d/%d path=%s"),
			Items.Num(),
			Summary.ItemCount,
			LocalizedNameCount,
			Summary.LocalizedNameCount,
			ShopItemCount,
			Summary.ShopItemCount,
			SpeedItemCount,
			Summary.SpeedItemCount,
			ChargePages.Num(),
			Summary.ChargePageCount,
			ChargeGemPackages.Num(),
			Summary.ChargeGemPackageCount,
			*CatalogPath);
	}
	return bCatalogLoaded;
}

bool URokUnityEconomyCatalogSubsystem::FindItemById(int32 ItemId, FRokUnityEconomyItem& OutItem) const
{
	const int32* ItemIndex = ItemIndexById.Find(ItemId);
	if (ItemIndex && Items.IsValidIndex(*ItemIndex))
	{
		OutItem = Items[*ItemIndex];
		return true;
	}

	OutItem = FRokUnityEconomyItem();
	return false;
}

TArray<FRokUnityEconomyItem> URokUnityEconomyCatalogSubsystem::GetShopItemsByCategory(int32 ItemType) const
{
	TArray<FRokUnityEconomyItem> Result;
	for (const FRokUnityEconomyItem& Item : Items)
	{
		if (Item.bNormalShopItem && Item.Type == ItemType)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

bool URokUnityEconomyCatalogSubsystem::FindChargePageByPagingType(
	int32 PagingType,
	FRokUnityEconomyChargePage& OutPage) const
{
	const int32* PageIndex = ChargePageIndexByPagingType.Find(PagingType);
	if (PageIndex && ChargePages.IsValidIndex(*PageIndex))
	{
		OutPage = ChargePages[*PageIndex];
		return true;
	}

	OutPage = FRokUnityEconomyChargePage();
	return false;
}

FRokUnityEconomyCatalogSummary URokUnityEconomyCatalogSubsystem::ParseSummary(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityEconomyCatalogSummary Parsed;
	Parsed.ItemCount = JsonIntField(Object, TEXT("item_count"));
	Parsed.LocalizedNameCount = JsonIntField(Object, TEXT("localized_name_count"));
	Parsed.UnresolvedNameCount = JsonIntField(Object, TEXT("unresolved_name_count"));
	Parsed.IconCount = JsonIntField(Object, TEXT("icon_count"));
	Parsed.ShopItemCount = JsonIntField(Object, TEXT("shop_item_count"));
	Parsed.ShopCategoryCount = JsonIntField(Object, TEXT("shop_category_count"));
	Parsed.SpeedItemCount = JsonIntField(Object, TEXT("speed_item_count"));
	Parsed.AcquisitionSourceItemCount = JsonIntField(Object, TEXT("acquisition_source_item_count"));
	Parsed.ChargePageCount = JsonIntField(Object, TEXT("charge_page_count"));
	Parsed.ChargePagingTypeCount = JsonIntField(Object, TEXT("charge_paging_type_count"));
	Parsed.ChargeGemPackageCount = JsonIntField(Object, TEXT("charge_gem_package_count"));
	return Parsed;
}

FRokUnityAllianceCreateConfig URokUnityEconomyCatalogSubsystem::ParseAllianceCreateConfig(
	const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityAllianceCreateConfig Parsed;
	Parsed.EstablishCost = JsonIntField(Object, TEXT("establish_cost"));
	Parsed.CurrencyTypeId = JsonIntField(Object, TEXT("currency_type_id"));
	Parsed.NameMinCharacters = JsonIntField(Object, TEXT("name_min_characters"));
	Parsed.NameMaxCharacters = JsonIntField(Object, TEXT("name_max_characters"));
	Parsed.TagMinCharacters = JsonIntField(Object, TEXT("tag_min_characters"));
	Parsed.TagMaxCharacters = JsonIntField(Object, TEXT("tag_max_characters"));
	Parsed.NoticeMaxCharacters = JsonIntField(Object, TEXT("notice_max_characters"));
	Parsed.SearchMinCharacters = JsonIntField(Object, TEXT("search_min_characters"));
	return Parsed;
}

FRokUnityEconomyItem URokUnityEconomyCatalogSubsystem::ParseItem(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityEconomyItem Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.NameLocaleId = JsonIntField(Object, TEXT("name_locale_id"));
	Parsed.DisplayNameZhCn = JsonStringField(Object, TEXT("display_name_zh_cn"));
	Parsed.DisplayNameEn = JsonStringField(Object, TEXT("display_name_en"));
	Parsed.TipsLocaleId = JsonIntField(Object, TEXT("tips_locale_id"));
	Parsed.SubType = JsonIntField(Object, TEXT("sub_type"));
	Parsed.Type = JsonIntField(Object, TEXT("type"));
	Parsed.TypeDescriptionLocaleId = JsonIntField(Object, TEXT("type_description_locale_id"));
	Parsed.TypeDescriptionZhCn = JsonStringField(Object, TEXT("type_description_zh_cn"));
	Parsed.TypeGroup = JsonIntField(Object, TEXT("type_group"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	Parsed.IconAtlas = JsonStringField(Object, TEXT("icon_atlas"));
	Parsed.IconSprite = JsonStringField(Object, TEXT("icon_sprite"));
	const TArray<TSharedPtr<FJsonValue>>* IconRect = nullptr;
	if (Object.IsValid()
		&& Object->TryGetArrayField(TEXT("icon_rect"), IconRect)
		&& IconRect
		&& IconRect->Num() == 4)
	{
		Parsed.IconX = FMath::RoundToInt((*IconRect)[0]->AsNumber());
		Parsed.IconY = FMath::RoundToInt((*IconRect)[1]->AsNumber());
		Parsed.IconWidth = FMath::RoundToInt((*IconRect)[2]->AsNumber());
		Parsed.IconHeight = FMath::RoundToInt((*IconRect)[3]->AsNumber());
	}
	const TArray<TSharedPtr<FJsonValue>>* IconAtlasSize = nullptr;
	if (Object.IsValid()
		&& Object->TryGetArrayField(
			TEXT("icon_atlas_size"),
			IconAtlasSize)
		&& IconAtlasSize
		&& IconAtlasSize->Num() == 2)
	{
		Parsed.IconAtlasWidth =
			FMath::RoundToInt((*IconAtlasSize)[0]->AsNumber());
		Parsed.IconAtlasHeight =
			FMath::RoundToInt((*IconAtlasSize)[1]->AsNumber());
	}
	Parsed.RequiredLevel = JsonIntField(Object, TEXT("required_level"));
	Parsed.Quality = JsonIntField(Object, TEXT("quality"));
	Parsed.bBatchUse = JsonBoolField(Object, TEXT("batch_use"));
	Parsed.ButtonLocaleId = JsonIntField(Object, TEXT("button_locale_id"));
	Parsed.ItemFunction = JsonIntField(Object, TEXT("item_function"));
	Parsed.Data1 = JsonIntField(Object, TEXT("data1"));
	Parsed.Data2 = JsonIntField(Object, TEXT("data2"));
	Parsed.DescriptionLocaleId = JsonIntField(Object, TEXT("description_locale_id"));
	Parsed.DescriptionTemplateZhCn = JsonStringField(Object, TEXT("description_template_zh_cn"));
	Parsed.DescriptionTemplateEn = JsonStringField(Object, TEXT("description_template_en"));
	Parsed.DescriptionData1 = JsonIntField(Object, TEXT("description_data1"));
	Parsed.DescriptionData2 = JsonIntField(Object, TEXT("description_data2"));
	Parsed.TopLocaleId = JsonIntField(Object, TEXT("top_locale_id"));
	Parsed.TopData = JsonIntField(Object, TEXT("top_data"));
	ReadIntArray(Object, TEXT("acquisition_source_ids"), Parsed.AcquisitionSourceIds);
	Parsed.ShortcutPrice = JsonIntField(Object, TEXT("shortcut_price"));
	Parsed.ShopPrice = JsonIntField(Object, TEXT("shop_price"));
	Parsed.ShopCurrencyTypeId = JsonIntField(Object, TEXT("shop_currency_type_id"));
	Parsed.bNormalShopItem = JsonBoolField(Object, TEXT("is_normal_shop_item"));
	Parsed.RedDotPrompt = JsonIntField(Object, TEXT("red_dot_prompt"));
	Parsed.Rank = JsonIntField(Object, TEXT("rank"));
	Parsed.SpeedDurationSeconds = JsonIntField(Object, TEXT("speed_duration_seconds"));
	Parsed.SpeedApplicability = JsonStringField(Object, TEXT("speed_applicability"));

	const TSharedPtr<FJsonObject> SourceObject = JsonObjectField(Object, TEXT("source"));
	Parsed.SourcePath = JsonStringField(SourceObject, TEXT("path"));
	Parsed.SourceRow = JsonIntField(SourceObject, TEXT("row"));
	return Parsed;
}

FRokUnityEconomyChargePage URokUnityEconomyCatalogSubsystem::ParseChargePage(
	const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityEconomyChargePage Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.PagingLocaleId = JsonIntField(Object, TEXT("paging_locale_id"));
	Parsed.PagingType = JsonIntField(Object, TEXT("paging_type"));
	Parsed.Preposition = JsonIntField(Object, TEXT("preposition"));
	Parsed.Sort = JsonIntField(Object, TEXT("sort"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));

	const TSharedPtr<FJsonObject> SourceObject = JsonObjectField(Object, TEXT("source"));
	Parsed.SourcePath = JsonStringField(SourceObject, TEXT("path"));
	Parsed.SourceRow = JsonIntField(SourceObject, TEXT("row"));
	return Parsed;
}

FRokUnityEconomyChargeGemPackage URokUnityEconomyCatalogSubsystem::ParseChargeGemPackage(
	const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityEconomyChargeGemPackage Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.NameLocaleId = JsonIntField(Object, TEXT("name_locale_id"));
	Parsed.ProductId = JsonIntField(Object, TEXT("product_id"));
	Parsed.DenarNum = JsonIntField(Object, TEXT("denar_num"));
	Parsed.FirstPresenter = JsonIntField(Object, TEXT("first_presenter"));
	Parsed.Presenter = JsonIntField(Object, TEXT("presenter"));
	Parsed.GemIcon = JsonStringField(Object, TEXT("gem_icon"));
	Parsed.RechargeProgress = JsonIntField(Object, TEXT("recharge_progress"));
	Parsed.MailId = JsonIntField(Object, TEXT("mail"));

	const TSharedPtr<FJsonObject> SourceObject = JsonObjectField(Object, TEXT("source"));
	Parsed.SourcePath = JsonStringField(SourceObject, TEXT("path"));
	Parsed.SourceRow = JsonIntField(SourceObject, TEXT("row"));
	return Parsed;
}
