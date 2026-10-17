#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RokWorldLod2GeometryProbeLibrary.generated.h"

class UStaticMesh;

/** Read-only render-data observation for the staged two-OBJ LOD2 diagnostic. */
UCLASS()
class ROK_API URokWorldLod2GeometryProbeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Rok|Diagnostics|WorldLOD2")
    static FString CaptureStaticMeshLod0(UStaticMesh* Mesh);

    /** Read-only full-float render-buffer capture for signed UV1/UV2 recovery checks. */
    UFUNCTION(BlueprintCallable, Category = "Rok|Diagnostics|WorldLOD2")
    static FString CaptureStaticMeshLod0FullUvSigned(UStaticMesh* Mesh);
};
