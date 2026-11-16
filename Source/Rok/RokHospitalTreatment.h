#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "RokHospitalTreatment.generated.h"

class FJsonObject;
class UEditableTextBox;
class URokUnityConfigSubsystem;
class USlider;

struct FRokHospitalSoldier
{
	int32 Id = 0;
	int32 Type = 0;
	int32 Level = 0;
	int32 Count = 0;

	bool operator==(const FRokHospitalSoldier&) const = default;
};

struct FRokHospitalState
{
	bool bKnown = false;
	TArray<FRokHospitalSoldier> Wounded;
	TArray<FRokHospitalSoldier> InTreatment;
	int64 QueueIndex = 0;
	int64 FinishTime = 0;

	static FRokHospitalState Read(const TSharedPtr<FJsonObject>& Role);
	bool operator==(const FRokHospitalState&) const = default;
};

struct FRokHospitalQuote
{
	int64 Resources[4] = {};
	int64 Seconds = 0;
	int64 Gems = 0;
	bool bValid = false;
	bool bHasResources = false;
	bool bHasGems = false;

	static FRokHospitalQuote Calculate(URokUnityConfigSubsystem& Config,
		const TMap<int32, int32>& Counts, const TSharedPtr<FJsonObject>& Role, int64 RemainingQueueSeconds = 0);
};

// A row's slider and text entry share a count and report changes by soldier ID.
UCLASS()
class URokHospitalQuantity : public UObject
{
	GENERATED_BODY()

public:
	void Bind(int32 InMaximum, USlider* InSlider, UEditableTextBox* InText, TFunction<void(int32)> InChanged);
	void SetCount(int32 Count);

private:
	UPROPERTY(Transient) TObjectPtr<USlider> Slider;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> Text;
	TFunction<void(int32)> OnChanged;
	int32 Maximum = 0;
	UFUNCTION() void SliderChanged(float Value);
	UFUNCTION() void TextCommitted(const FText& Value, ETextCommit::Type Commit);
};
