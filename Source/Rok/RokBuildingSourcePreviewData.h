#pragma once
#include "CoreMinimal.h"
class AActor;
class FJsonObject;
class UGameInstance;
class UTexture2D;
namespace RokBuildingSourcePreview
{
struct FLayout
{
    FString ModelId, SpritePath, SpriteGuid, SpriteHash;
    FVector2D Position=FVector2D::ZeroVector, Size=FVector2D::ZeroVector;
    FVector2D ImagePosition=FVector2D::ZeroVector, ImageSize=FVector2D::ZeroVector;
    FIntPoint TextureSize=FIntPoint::ZeroValue;
};
bool ResolveDeclaredModel(const TArray<FName>& ActorTags,const FString& InitialStateJson,FString& OutModel,FString& Error);
bool ResolveActorModel(AActor* Actor,FString& OutModel,FString& Error);
bool LoadLayout(const FString& ExplicitModel,const FString& MetadataPath,FLayout& Out,FString& Error);
FString MetadataFile();

struct FVisual
{
    FString ImagePath;
    FVector2D Size = FVector2D::ZeroVector;
    FVector2D Offset = FVector2D::ZeroVector;
    FBox2D Visible = FBox2D(EForceInit::ForceInit);

    FBox2D FitInPane(const FVector2D& Center, const FVector2D& PaneSize) const;
};

FIntPoint ResolveProfile(UGameInstance* Instance, int32 LocalHallLevel);
UTexture2D* LoadTexture(const FVisual& Visual);
bool FindVisual(int32 Type, int32 Country, int32 Age, FVisual& OutVisual);
bool FindSceneVisual(int32 Type, int32 InitId, const FString& Model, FVisual& OutVisual);
}
