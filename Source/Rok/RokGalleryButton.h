#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "RokGalleryButton.generated.h"

/** A UButton that reports which gallery slot was pressed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRokGalleryEntryClicked, int32, EntryIndex);

UCLASS()
class ROK_API URokGalleryButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Rok|UI")
	FRokGalleryEntryClicked OnGalleryEntryClicked;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rok|UI")
	int32 EntryIndex = 0;

	UFUNCTION()
	void HandleClicked() { OnGalleryEntryClicked.Broadcast(EntryIndex); }
};
