#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokBarracksUpgradeAttributes.generated.h"

UCLASS()
class ROK_API URokBarracksUpgradeAttributes : public UUserWidget
{
    GENERATED_BODY()
public:
    static bool Supports(int32 BuildingType, int32 CurrentLevel, int32 NextLevel)
    { return BuildingType == 9 && CurrentLevel == 1 && NextLevel == 2; }
    bool InitializeAttributes();
    int32 GetAttributeCount() const { return AttributeCount; }
    float GetSourceHeight() const { return 110.f; }
    static FVector2D SourcePosition() { return FVector2D(670, 228.17004); }
    static FVector2D LevelPosition(bool Next) { return FVector2D(Next ? 826.7 : 702, 181.82007); }
    static FVector2D LevelSize(bool Next) { return FVector2D(Next ? 13 : 61, 50); }
    const FString& GetLoadError() const { return LoadError; }
private:
    int32 AttributeCount = 0;
    FString LoadError;
};
