#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokServerInventorySubsystem.generated.h"

class URokUnityEconomyCatalogSubsystem;

USTRUCT(BlueprintType)
struct FRokServerInventoryStack
{
	GENERATED_BODY()
	// The server's stack identity; multiple indices may share one item ID.
	UPROPERTY(BlueprintReadOnly) int64 ItemIndex = 0;
	UPROPERTY(BlueprintReadOnly) int64 ItemId = 0;
	UPROPERTY(BlueprintReadOnly) int64 Overlay = 0;
	UPROPERTY(BlueprintReadOnly) int64 HeroId = 0;
	UPROPERTY(BlueprintReadOnly) bool bCatalogKnown = false;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Icon;
	UPROPERTY(BlueprintReadOnly) int32 Quality = 0;
	UPROPERTY(BlueprintReadOnly) int32 SubType = 0;
	UPROPERTY(BlueprintReadOnly) int32 BagType = 0;
	UPROPERTY(BlueprintReadOnly) int32 TypeGroup = 0;
	UPROPERTY(BlueprintReadOnly) int32 Rank = 0;
};

USTRUCT(BlueprintType)
struct FRokServerInventorySnapshot
{
	GENERATED_BODY()
	// Available means connecting or RoleReady. Loaded means packaged economy catalog is ready.
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly) bool bLoaded = false;
	// A missing itemInfo collection differs from a known empty collection.
	UPROPERTY(BlueprintReadOnly) bool bItemsKnown = false;
	// Every recorded row is either a complete live stack or a deletion marker.
	UPROPERTY(BlueprintReadOnly) bool bRowsComplete = false;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerInventoryStack> Stacks;
	UPROPERTY(BlueprintReadOnly) FString Signature;
};

UCLASS()
class ROK_API URokServerInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Rok Server Inventory")
	FRokServerInventorySnapshot GetSnapshot() const;

	static FRokServerInventorySnapshot ProjectSnapshot(
		const TSharedPtr<FJsonObject>& ItemInfo,
		const URokUnityEconomyCatalogSubsystem* Catalog);
#if WITH_DEV_AUTOMATION_TESTS
	void SetSnapshotForTesting(const FRokServerInventorySnapshot& InSnapshot) { TestSnapshot = InSnapshot; }
	void ClearSnapshotForTesting() { TestSnapshot.Reset(); }
#endif
private:
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<FRokServerInventorySnapshot> TestSnapshot;
#endif
};
