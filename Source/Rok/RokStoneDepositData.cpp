#include "RokStoneDepositData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TSharedPtr<FJsonObject> ObjectValue(const TSharedPtr<FJsonValue>& Value)
{
	return Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
}
TSharedPtr<FJsonObject> ObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	const TSharedPtr<FJsonObject>* Value = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(Key, Value) && Value ? *Value : nullptr;
}
bool HasNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	double Value = 0;
	return Object.IsValid() && Object->TryGetNumberField(Key, Value) && FMath::IsFinite(Value);
}
bool HasString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(Key, Value) && !Value.IsEmpty();
}
bool HasSourceId(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	if (!Object.IsValid() || !Object->HasTypedField<EJson::String>(Key) || !HasString(Object, Key)) return false;
	const FString Value = Object->GetStringField(Key);
	for (TCHAR C : Value) if (C < TEXT('0') || C > TEXT('9')) return false;
	return Value != TEXT("0");
}
bool HasVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32 Count)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || !Values || Values->Num() != Count) return false;
	for (const auto& Value : *Values)
	{
		double Number = 0;
		if (!Value.IsValid() || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) return false;
	}
	return true;
}
bool Validate(const TSharedPtr<FJsonObject>& Root)
{
	if (!Root.IsValid() || !HasVector(Root, TEXT("bounds_size"), 2)
		|| !HasNumber(Root, TEXT("max_scale")) || !HasNumber(Root, TEXT("unity_units_to_ue"))
		|| !HasString(ObjectField(Root, TEXT("native_order_evidence")), TEXT("path"))) return false;
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
	if (!Root->TryGetArrayField(TEXT("items"), Items) || !Items || Items->Num() != 18
		|| !Root->TryGetArrayField(TEXT("scale_curve"), Keys) || !Keys || Keys->Num() != 5) return false;
	double PreviousTime = -DBL_MAX;
	for (int32 I = 0; I < Keys->Num(); ++I)
	{
		const auto Key = ObjectValue((*Keys)[I]);
		if (!HasNumber(Key, TEXT("time")) || !HasNumber(Key, TEXT("value")) || !HasNumber(Key, TEXT("out"))) return false;
		// Only the first arrive tangent may be null (source Infinity with clamp extrapolation).
		if (!HasNumber(Key, TEXT("in")) && !(I == 0 && Key->HasTypedField<EJson::Null>(TEXT("in")))) return false;
		const double Time = Key->GetNumberField(TEXT("time"));
		if (Time <= PreviousTime) return false;
		PreviousTime = Time;
	}
	TSet<FString> Names;
	for (const auto& Value : *Items)
	{
		const auto Item = ObjectValue(Value);
		if (!HasString(Item, TEXT("name")) || !HasSourceId(Item, TEXT("source_transform_id"))
			|| !HasNumber(Item, TEXT("sibling_index")) || !HasNumber(Item, TEXT("priority"))
			|| !HasVector(Item, TEXT("position"), 3) || !HasVector(Item, TEXT("size"), 2)) return false;
		const FString Name = Item->GetStringField(TEXT("source_transform_id"));
		if (Names.Contains(Name)) return false;
		Names.Add(Name);
		const TArray<TSharedPtr<FJsonValue>>* Sprites = nullptr;
		if (!Item->TryGetArrayField(TEXT("sprites"), Sprites) || !Sprites || Sprites->Num() != 1) return false;
		const auto Sprite = ObjectValue((*Sprites)[0]);
		if (!HasSourceId(Sprite, TEXT("source_renderer_id"))
			|| !HasString(ObjectField(Sprite, TEXT("texture")), TEXT("path"))
			|| !HasString(ObjectField(Sprite, TEXT("mask")), TEXT("path"))) return false;
		for (const TCHAR* Field : {TEXT("position"), TEXT("right"), TEXT("up"), TEXT("color1"), TEXT("color2")})
			if (!HasVector(Sprite, Field, 3)) return false;
		if (!HasVector(Sprite, TEXT("size"), 2) || !HasVector(Sprite, TEXT("pivot"), 2)) return false;
	}
	return true;
}
FVector Vector3(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	const auto& A = Object->GetArrayField(Key);
	return A.Num() == 3 ? FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) : FVector::ZeroVector;
}
FVector2D Vector2(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	const auto& A = Object->GetArrayField(Key);
	return A.Num() == 2 ? FVector2D(A[0]->AsNumber(), A[1]->AsNumber()) : FVector2D::ZeroVector;
}
struct FSourceRect
{
	FVector2f Corners[4];
	FSourceRect(float X, float Y, float Width, float Height)
	{
		// RotateSquar.cs uses 0.70721, not the mathematical sqrt(0.5).
		constexpr float K = 0.70721f;
		const float W = Width / 2, H = Height / 2;
		const FVector2f World[4] = {
			{X + H*K - W*K, Y + H*K + W*K},
			{X + H*K + W*K, Y + H*K - W*K},
			{X - H*K + W*K, Y - H*K - W*K},
			{X - H*K - W*K, Y - H*K + W*K}};
		for (int32 I = 0; I < 4; ++I)
			Corners[I] = FVector2f(World[I].X*K + World[I].Y*K, -World[I].X*K + World[I].Y*K);
	}
	bool HasCorner(FVector2f P) const
	{
		return Corners[2].X <= P.X && Corners[1].X >= P.X
			&& Corners[2].Y <= P.Y && Corners[0].Y >= P.Y;
	}
	bool Contains(const FSourceRect& Other) const
	{
		for (FVector2f P : Other.Corners)
			if (!HasCorner(P)) return false;
		return true;
	}
	bool Overlaps(const FSourceRect& Other) const
	{
		for (int32 I = 0; I < 4; ++I)
			if (HasCorner(Other.Corners[I]) || Other.HasCorner(Corners[I])) return true;
		return false;
	}
};
}

bool FRokStoneDepositData::Load()
{
	*this = FRokStoneDepositData();
	FString Text;
	return FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_stone_deposit.json"))) && LoadJson(Text);
}

bool FRokStoneDepositData::LoadJson(const FString& Text)
{
	*this = FRokStoneDepositData();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Validate(Root)) return false;
	const TArray<TSharedPtr<FJsonValue>>* SourceItems = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
	if (!Root->HasField(TEXT("native_order_evidence"))
		|| !Root->TryGetArrayField(TEXT("items"), SourceItems) || SourceItems->Num() != 18
		|| !Root->TryGetArrayField(TEXT("scale_curve"), Keys) || Keys->Num() != 5) return false;
	BoundsSize = Vector2(Root, TEXT("bounds_size"));
	MaxScale = Root->GetNumberField(TEXT("max_scale"));
	UnitsToCentimeters = Root->GetNumberField(TEXT("unity_units_to_ue"));
	for (const auto& Value : *Keys)
	{
		const auto Key = Value->AsObject();
		const FKeyHandle Handle = ScaleCurve.AddKey(Key->GetNumberField(TEXT("time")), Key->GetNumberField(TEXT("value")));
		FRichCurveKey& Target = ScaleCurve.GetKey(Handle);
		Target.InterpMode = RCIM_Cubic;
		Target.TangentMode = RCTM_User;
		double In = 0, Out = 0;
		Key->TryGetNumberField(TEXT("in"), In);
		Key->TryGetNumberField(TEXT("out"), Out);
		Target.ArriveTangent = In;
		Target.LeaveTangent = Out;
	}
	for (const auto& Value : *SourceItems)
	{
		const auto Object = Value->AsObject();
		const auto& Sprites = Object->GetArrayField(TEXT("sprites"));
		if (Sprites.Num() != 1) return false;
		const auto Sprite = Sprites[0]->AsObject();
		FRokStoneDepositItem Item;
		Item.SourceTransformId = Object->GetStringField(TEXT("source_transform_id"));
		Item.SourceName = Object->GetStringField(TEXT("name"));
		Item.SourceRendererId = Sprite->GetStringField(TEXT("source_renderer_id"));
		Item.SiblingIndex = Object->GetNumberField(TEXT("sibling_index"));
		Item.Position = Vector3(Object, TEXT("position"));
		Item.Size = Vector2(Object, TEXT("size"));
		Item.Priority = Object->GetNumberField(TEXT("priority"));
		Item.TexturePath = FPaths::ProjectDir() / Sprite->GetObjectField(TEXT("texture"))->GetStringField(TEXT("path"));
		Item.MaskPath = FPaths::ProjectDir() / Sprite->GetObjectField(TEXT("mask"))->GetStringField(TEXT("path"));
		Item.SpritePosition = Vector3(Sprite, TEXT("position"));
		Item.SpriteRight = Vector3(Sprite, TEXT("right"));
		Item.SpriteUp = Vector3(Sprite, TEXT("up"));
		Item.SpriteSize = Vector2(Sprite, TEXT("size"));
		Item.Pivot = Vector2(Sprite, TEXT("pivot"));
		const FVector C1 = Vector3(Sprite, TEXT("color1")), C2 = Vector3(Sprite, TEXT("color2"));
		Item.Color1 = FLinearColor(C1.X, C1.Y, C1.Z, 1);
		Item.Color2 = FLinearColor(C2.X, C2.Y, C2.Z, 1);
		Items.Add(MoveTemp(Item));
	}
	if (BoundsSize.X <= 0 || BoundsSize.Y <= 0 || UnitsToCentimeters <= 0 || MaxScale <= 0)
	{
		*this = FRokStoneDepositData();
		return false;
	}
	return true;
}

float FRokStoneDepositData::EvaluateScale(float Dxf) const
{
	return FMath::Clamp(ScaleCurve.Eval(Dxf), 0.0f, MaxScale);
}

void FRokStoneDepositState::Reset(int32 Count)
{
	Active.Init(true, Count);
	LastScale = -1;
}

void FRokStoneDepositState::Update(const FRokStoneDepositData& Data, float Scale)
{
	check(Active.Num() == Data.Items.Num());
	if (LastScale == Scale) return;
	TArray<FSourceRect> Rects;
	TArray<int32> Candidates;
	const FSourceRect Bounds(0, 0, Data.BoundsSize.X, Data.BoundsSize.Y);
	for (int32 I = 0; I < Data.Items.Num(); ++I)
	{
		const auto& Item = Data.Items[I];
		Rects.Emplace(Item.Position.X, Item.Position.Z, Item.Size.X * Scale, Item.Size.Y * Scale);
		if (Scale > LastScale && !Active[I]) continue;
		if (Item.Priority < 100 && !Bounds.Contains(Rects[I]))
			Active[I] = false;
		else Candidates.Add(I);
	}
	for (int32 J = 0; J < Candidates.Num(); ++J)
	{
		for (int32 K = J + 1; K < Candidates.Num(); ++K)
		{
			if (Rects[Candidates[J]].Overlaps(Rects[Candidates[K]]))
			{
				Active[Candidates[K]] = false;
				Candidates.RemoveAt(K--);
			}
		}
	}
	for (int32 I : Candidates) Active[I] = true;
	LastScale = Scale;
}
