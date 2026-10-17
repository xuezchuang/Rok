#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RokWorldMapHud.generated.h"

UCLASS()
class ROK_API ARokWorldMapHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
