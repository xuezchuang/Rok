#include "RokHospitalTreatment.h"

#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "RokInstantPrice.h"
#include "RokSproto.h"
#include "RokUnityConfigSubsystem.h"

namespace
{
TArray<FRokHospitalSoldier> ReadSoldiers(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	TArray<FRokHospitalSoldier> Soldiers;
	auto ReadRow = [&Soldiers](const TSharedPtr<FJsonValue>& Value)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		int64 Id = 0, Type = 0, Level = 0, Count = 0;
		if (Value && Value->TryGetObject(Row)
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("id"), Id) && Id > 0 && Id <= MAX_int32
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("type"), Type) && Type >= 1 && Type <= 4
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("level"), Level) && Level >= 1 && Level <= MAX_int32
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("num"), Count) && Count > 0 && Count <= MAX_int32)
		{
			Soldiers.Add({int32(Id), int32(Type), int32(Level), int32(Count)});
		}
	};
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	const TSharedPtr<FJsonObject>* Map = nullptr;
	if (Object && Object->TryGetArrayField(Field, Array))
	{
		for (const auto& Row : *Array) ReadRow(Row);
	}
	else if (Object && Object->TryGetObjectField(Field, Map) && *Map)
	{
		for (const auto& Pair : (*Map)->Values) ReadRow(Pair.Value);
	}
	Soldiers.Sort([](const FRokHospitalSoldier& A, const FRokHospitalSoldier& B)
	{
		return A.Id < B.Id;
	});
	return Soldiers;
}
}

FRokHospitalState FRokHospitalState::Read(const TSharedPtr<FJsonObject>& Role)
{
	FRokHospitalState State;
	State.bKnown = Role.IsValid();
	if (!Role) return State;
	State.Wounded = ReadSoldiers(Role, TEXT("seriousInjured"));
	const TSharedPtr<FJsonObject>* Queue = nullptr;
	if (Role->TryGetObjectField(TEXT("treatmentQueue"), Queue)
		&& RokSprotoJson::TryGetInt64(*Queue, TEXT("finishTime"), State.FinishTime)
		&& State.FinishTime > 0)
	{
		RokSprotoJson::TryGetInt64(*Queue, TEXT("queueIndex"), State.QueueIndex);
		State.InTreatment = ReadSoldiers(*Queue, TEXT("treatmentSoldiers"));
	}
	return State;
}

FRokHospitalQuote FRokHospitalQuote::Calculate(URokUnityConfigSubsystem& Config,
	const TMap<int32, int32>& Counts, const TSharedPtr<FJsonObject>& Role, int64 RemainingQueueSeconds)
{
	FRokHospitalQuote Quote;
	if (!Role || (Counts.IsEmpty() && RemainingQueueSeconds <= 0)) return Quote;

	int64 HealSpeed = 0;
	RokSprotoJson::TryGetInt64(Role, TEXT("healSpeedMulti"), HealSpeed);
	const FName Fields[] = {TEXT("woundedFood"), TEXT("woundedWood"), TEXT("woundedStone"), TEXT("woundedGlod"), TEXT("woundedTime")};
	int64 Totals[UE_ARRAY_COUNT(Fields)] = {};
	for (const auto& Pair : Counts)
	{
		if (Pair.Value <= 0) return Quote;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Fields); ++Index)
		{
			int32 PerUnit = 0;
			if (!Config.GetIntFieldByKey(TEXT("Arms"), TEXT("ID"), Pair.Key, Fields[Index], PerUnit) || PerUnit < 0)
			{
				return Quote;
			}
			const int64 Amount = int64(PerUnit) * Pair.Value;
			if (Amount > MAX_int64 - Totals[Index]) return Quote;
			Totals[Index] += Amount;
		}
	}
	int64 MinimumSeconds = 3;
	if (const auto* Rules = Config.GetTableRows(TEXT("Config")))
	{
		for (const auto& Row : *Rules)
		{
			FString Key, Value;
			if (Row && Row->TryGetStringField(TEXT("Scouts"), Key) && Key == TEXT("cureMinTime")
				&& Row->TryGetStringField(TEXT("Column5"), Value))
			{
				MinimumSeconds = FMath::Max<int64>(1, FCString::Atoi64(*Value));
				break;
			}
		}
	}
	const double SpeedMultiplier = 1.0 + HealSpeed / 1000.0;
	Quote.Seconds = RemainingQueueSeconds > 0 ? RemainingQueueSeconds
		: FMath::Max(MinimumSeconds, FMath::FloorToInt64(Totals[4] / FMath::Max(0.01, SpeedMultiplier)));
	int64 PriceInputs[5] = {Quote.Seconds};
	const TCHAR* Resources[] = {TEXT("food"), TEXT("wood"), TEXT("stone"), TEXT("gold")};
	Quote.bHasResources = true;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Resources); ++Index)
	{
		Quote.Resources[Index] = Totals[Index];
		int64 Available = 0;
		RokSprotoJson::TryGetInt64(Role, Resources[Index], Available);
		PriceInputs[Index + 1] = FMath::Max<int64>(0, Totals[Index] - FMath::Max<int64>(0, Available));
		Quote.bHasResources &= PriceInputs[Index + 1] == 0;
	}
	const auto* Prices = Config.GetTableRows(TEXT("InstantPrice"));
	double Total = 0;
	Quote.bValid = Prices && RokInstantPrice::AddQuote(*Prices, PriceInputs, Total);
	if (Quote.bValid)
	{
		Quote.Gems = FMath::FloorToInt64(Total);
		int64 Gems = 0;
		Quote.bHasGems = RokSprotoJson::TryGetInt64(Role, TEXT("denar"), Gems) && Gems >= Quote.Gems;
	}
	return Quote;
}

void URokHospitalQuantity::Bind(int32 InMaximum, USlider* InSlider, UEditableTextBox* InText,
	TFunction<void(int32)> InChanged)
{
	Maximum = InMaximum;
	Slider = InSlider;
	Text = InText;
	OnChanged = MoveTemp(InChanged);
	Slider->OnValueChanged.AddDynamic(this, &URokHospitalQuantity::SliderChanged);
	Text->OnTextCommitted.AddDynamic(this, &URokHospitalQuantity::TextCommitted);
}

void URokHospitalQuantity::SetCount(int32 Count)
{
	Slider->SetValue(Maximum > 0 ? float(Count) / Maximum : 0.0f);
	Text->SetText(FText::AsNumber(Count, &FNumberFormattingOptions::DefaultNoGrouping()));
}

void URokHospitalQuantity::SliderChanged(float Value)
{
	if (OnChanged) OnChanged(FMath::RoundToInt(double(Value) * Maximum));
}

void URokHospitalQuantity::TextCommitted(const FText& Value, ETextCommit::Type Commit)
{
	const FString Input = Value.ToString().TrimStartAndEnd();
	const int32 Count = Input.IsNumeric() ? int32(FMath::Clamp<int64>(FCString::Atoi64(*Input), 0, Maximum))
		: FMath::RoundToInt(double(Slider->GetValue()) * Maximum);
	if (OnChanged) OnChanged(Count);
}
