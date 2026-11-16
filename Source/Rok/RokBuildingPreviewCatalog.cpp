#include "RokBuildingSourcePreviewData.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "RokGameNetSubsystem.h"
#include "RokSproto.h"
#include "RokUnityConfigSubsystem.h"
#include "Misc/FileHelper.h"
#include "RokDataPaths.h"
#include "Serialization/JsonSerializer.h"

namespace RokBuildingSourcePreview
{
namespace
{
struct FCatalog
{
	TMap<FString, FVisual> Art;
	TMap<FIntVector, FString> Variants;
	TMap<FString, FString> SceneModels;
	TMap<int32, FString> InitialModels;
};

FString SceneKey(int32 Type, const FString& Model)
{
	return FString::Printf(TEXT("%d/%s"), Type, *Model);
}

bool ReadNumbers(const FJsonObject& Object, const TCHAR* Field, TArrayView<double> Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.TryGetArrayField(Field, Values) || !Values || Values->Num() != Out.Num()) return false;
	for (int32 Index = 0; Index < Out.Num(); ++Index)
	{
		if (!(*Values)[Index]->TryGetNumber(Out[Index]) || !FMath::IsFinite(Out[Index])) return false;
	}
	return true;
}

FCatalog LoadCatalog()
{
	FCatalog Result;
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *RokDataPaths::Resolve(TEXT("BuildingSourcePreview/catalog.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) return Result;

	FString Schema;
	const TSharedPtr<FJsonObject>* Art = nullptr;
	const TSharedPtr<FJsonObject>* Initial = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Variants = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.building.preview_catalog.v1")
		|| !Root->TryGetObjectField(TEXT("art"), Art) || !Root->TryGetObjectField(TEXT("initial_models"), Initial)
		|| !Root->TryGetArrayField(TEXT("variants"), Variants)) return Result;
	for (const auto& [Name, Value] : (*Art)->Values)
	{
		const auto Object = Value->AsObject();
		FVisual Entry;
		double Size[2], Offset[2], Visible[4];
		if (!Object || !Object->TryGetStringField(TEXT("image"), Entry.ImagePath)
			|| !Entry.ImagePath.StartsWith(TEXT("Images/")) || Entry.ImagePath.Contains(TEXT(".."))
			|| !ReadNumbers(*Object, TEXT("size"), Size) || !ReadNumbers(*Object, TEXT("offset"), Offset)
			|| !ReadNumbers(*Object, TEXT("visible"), Visible)
			|| Size[0] <= 0 || Size[1] <= 0 || Visible[0] < 0 || Visible[1] < 0
			|| Visible[2] > 1 || Visible[3] > 1 || Visible[2] <= Visible[0] || Visible[3] <= Visible[1]) continue;

		Entry.Size = FVector2D(Size[0], Size[1]);
		Entry.Offset = FVector2D(Offset[0], Offset[1]);
		Entry.Visible = FBox2D(FVector2D(Visible[0], Visible[1]), FVector2D(Visible[2], Visible[3]));
		Entry.ImagePath = TEXT("BuildingSourcePreview/") + Entry.ImagePath;
		Result.Art.Add(FString(*Name), MoveTemp(Entry));
	}
	for (const auto& Value : *Variants)
	{
		const auto Object = Value->AsObject();
		int32 Type, Country, Age;
		FString Name, Model;
		if (!Object || !Object->TryGetNumberField(TEXT("type"), Type) || !Object->TryGetNumberField(TEXT("country"), Country)
			|| !Object->TryGetNumberField(TEXT("age"), Age) || !Object->TryGetStringField(TEXT("model"), Model)
			|| !Object->TryGetStringField(TEXT("art"), Name) || !Result.Art.Contains(Name)) continue;
		Result.Variants.Add(FIntVector(Type, Country, Age), Name);
		Result.SceneModels.FindOrAdd(SceneKey(Type, Model), Name);
	}
	for (const auto& [Key, Value] : (*Initial)->Values)
	{
		int32 Id;
		FString Model;
		if (LexTryParseString(Id, *Key) && Value->TryGetString(Model)) Result.InitialModels.Add(Id, MoveTemp(Model));
	}
	return Result;
}

const FCatalog& GetCatalog()
{
	static const FCatalog Catalog = LoadCatalog();
	return Catalog;
}
}

FIntPoint ResolveProfile(UGameInstance* Instance, int32 LocalHallLevel)
{
	// The offline city uses the project's source presentation. With a server
	// role, match CityBuildingProxy.GetImgIdByType (current hall age).
	int64 Country = 101;
	int64 HallLevel = FMath::Max(LocalHallLevel, 1);
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (Net && Net->GetState() == ERokGameNetState::RoleReady)
	{
		const auto Role = Net->GetRoleInfo();
		int64 Value = 0;
		if (RokSprotoJson::TryGetInt64(Role, TEXT("country"), Value) && Value > 0 && Value <= MAX_int32)
			Country = Value;
		if (RokSprotoJson::TryGetInt64(Role, TEXT("level"), Value) && Value > 0 && Value <= MAX_int32)
			HallLevel = Value;
	}
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	int32 Age = 0;
	if (Config) Config->GetIntFieldByKey(TEXT("BuildingTownCenter"), TEXT("ID"), static_cast<int32>(HallLevel), TEXT("age"), Age);
	return FIntPoint(static_cast<int32>(Country), FMath::Max(Age, 1));
}

UTexture2D* LoadTexture(const FVisual& Visual)
{
	static TMap<FString, TWeakObjectPtr<UTexture2D>> Textures;
	auto& Texture = Textures.FindOrAdd(Visual.ImagePath);
	if (!Texture.IsValid())
	{
		if (auto* Loaded = FImageUtils::ImportFileAsTexture2D(RokDataPaths::Resolve(Visual.ImagePath)))
		{
			Loaded->SRGB = true;
			Loaded->AddressX = TA_Clamp;
			Loaded->AddressY = TA_Clamp;
			Loaded->Filter = TF_Bilinear;
			Loaded->UpdateResource();
			Texture = Loaded;
		}
	}
	return Texture.Get();
}

bool FindVisual(int32 Type, int32 Country, int32 Age, FVisual& OutVisual)
{
	const auto& Catalog = GetCatalog();
	const FString* Name = Catalog.Variants.Find(FIntVector(Type, Country, Age));
	if (!Name) return false;
	OutVisual = Catalog.Art.FindChecked(*Name);
	return true;
}

bool FindSceneVisual(int32 Type, int32 InitId, const FString& Model, FVisual& OutVisual)
{
	const auto& Catalog = GetCatalog();
	const FString SceneModel = Model.IsEmpty() ? Catalog.InitialModels.FindRef(InitId) : Model;
	const FString* Name = Catalog.SceneModels.Find(SceneKey(Type, SceneModel));
	if (!Name) return false;
	OutVisual = Catalog.Art.FindChecked(*Name);
	return true;
}

FBox2D FVisual::FitInPane(const FVector2D& Center, const FVector2D& PaneSize) const
{
	constexpr double Padding = 2.0;
	const FVector2D VisibleSize = Size * Visible.GetSize();
	const double Scale = FMath::Min3(1.0, (PaneSize.X - 2 * Padding) / VisibleSize.X,
		(PaneSize.Y - 2 * Padding) / VisibleSize.Y);
	const FVector2D DrawSize = Size * Scale;
	const FVector2D Min = DrawSize * Visible.Min;
	const FVector2D Max = DrawSize * Visible.Max;
	const FVector2D Origin = Center + Offset * Scale - DrawSize * 0.5;
	const FVector2D Fitted(
		FMath::Clamp(Origin.X, Padding - Min.X, PaneSize.X - Padding - Max.X),
		FMath::Clamp(Origin.Y, Padding - Min.Y, PaneSize.Y - Padding - Max.Y));
	return FBox2D(Fitted, Fitted + DrawSize);
}
}
