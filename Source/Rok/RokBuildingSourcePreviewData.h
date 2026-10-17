#pragma once
#include "CoreMinimal.h"
class AActor;
class FJsonObject;
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
}
