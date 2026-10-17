#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RokWorldMaterialRootReadOnlyBridge.generated.h"

class UMaterial;

/** Editor-only, read-only access to two roots hidden from UE 5.8 Python enums. */
UCLASS()
class ROK_API URokWorldMaterialRootReadOnlyBridge : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Rok|Diagnostic|Material")
    static bool IsMaterialPathSupported(const FString& MaterialObjectPath);

    UFUNCTION(BlueprintCallable, Category = "Rok|Diagnostic|Material")
    static FString GetHiddenRootsJson(UMaterial* Material);
};
