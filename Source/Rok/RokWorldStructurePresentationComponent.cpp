#include "RokWorldStructurePresentationComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "ImageUtils.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
FVector ConvertUnityVector(const FVector& Value)
{
	return FVector(-Value.X, Value.Z, Value.Y);
}

UTexture2D* LoadRawSourceTexture(const FString& Path)
{
	static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
	if (const TWeakObjectPtr<UTexture2D>* Found = Cache.Find(Path))
	{
		if (Found->IsValid()) return Found->Get();
	}
	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Path);
	if (Texture)
	{
		// The source shader samples raw channels before its Gamma-space arithmetic.
		Texture->SRGB = false;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->UpdateResource();
		Cache.Add(Path, Texture);
	}
	return Texture;
}
}

void URokWorldStructurePresentationComponent::ClearCards()
{
	for (UStaticMeshComponent* Card : Cards)
	{
		if (IsValid(Card)) Card->DestroyComponent();
	}
	for (UProceduralMeshComponent* Mesh : MeshLayers)
	{
		if (IsValid(Mesh)) Mesh->DestroyComponent();
	}
	MeshLayers.Reset();
	Cards.Reset();
	Textures.Reset();
	ActiveCardCount = 0;
	ActiveMeshCount = 0;
}

bool URokWorldStructurePresentationComponent::InitializeSource(const FString& ModelId)
{
	ClearCards();
	if (!Data.Load(ModelId)) return false;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokVillagePresentationDay.M_RokVillagePresentationDay"));
	UMaterialInterface* TransparentParent = nullptr;
	if (Data.Items.ContainsByPredicate([](const auto& Item) { return Item.ShaderMode == TEXT("unity_builtin_10750"); }))
	{
		TransparentParent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldStructureTransparent.M_RokWorldStructureTransparent"));
		if (!TransparentParent) return false;
	}
	if (!Plane || !Parent || !GetOwner()) return false;

	// Load and retain every layer before registering any visible geometry.
	for (const FRokWorldStructurePresentationItem& Item : Data.Items)
	{
		UTexture2D* Texture = LoadRawSourceTexture(Item.TexturePath);
		UTexture2D* Mask = Item.MaskPath.IsEmpty() ? nullptr : LoadRawSourceTexture(Item.MaskPath);
		if (!Texture || (!Item.MaskPath.IsEmpty() && !Mask))
		{
			Textures.Reset();
			return false;
		}
		Textures.Add(Texture);
		Textures.Add(Mask);
	}
	UMaterialInterface* MeshParent = nullptr;
	if (!Data.Meshes.IsEmpty())
	{
		MeshParent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldStructureOpaque.M_RokWorldStructureOpaque"));
		if (!MeshParent) { Textures.Reset(); return false; }
		for (const auto& Mesh : Data.Meshes)
		{
			UTexture2D* Texture = LoadRawSourceTexture(Mesh.TexturePath);
			if (!Texture) { Textures.Reset(); return false; }
			Textures.Add(Texture);
		}
	}
	for (int32 Index = 0; Index < Data.Items.Num(); ++Index)
	{
		const FRokWorldStructurePresentationItem& Item = Data.Items[Index];
		UStaticMeshComponent* Card = NewObject<UStaticMeshComponent>(GetOwner(),
			MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), FName(TEXT("WorldSourceLayer"))));
		GetOwner()->AddInstanceComponent(Card);
		Card->SetupAttachment(this);
		Card->SetStaticMesh(Plane);
		Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Card->SetGenerateOverlapEvents(false);
		Card->SetCanEverAffectNavigation(false);
		Card->SetCastShadow(false);
		Card->bReceivesDecals = false;
		Card->bUseAsOccluder = false;
		const FVector Right = ConvertUnityVector(Item.Right).GetSafeNormal();
		const FVector Up = ConvertUnityVector(Item.Up).GetSafeNormal();
		const FVector Normal = FVector::CrossProduct(Right, Up).GetSafeNormal();
		Card->SetRelativeRotation(FMatrix(Right, Up, Normal, FVector::ZeroVector).ToQuat());
		Card->SetTranslucentSortPriority(46 + Item.SortLayerOrder * 1000 + Item.SortOrder
			- (Item.ShaderMode == TEXT("unity_builtin_10750") ? 1 : 0));
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Item.ShaderMode == TEXT("unity_builtin_10750") ? TransparentParent : Parent, this);
		Material->SetTextureParameterValue(TEXT("Sprite"), Textures[Index * 2]);
		Material->SetTextureParameterValue(TEXT("Mask"), Textures[Index * 2 + 1]);
		Material->SetVectorParameterValue(TEXT("Color1"), Item.Color1);
		Material->SetVectorParameterValue(TEXT("Color2"), Item.Color2);
		Material->SetVectorParameterValue(TEXT("RendererColor"), Item.RendererColor);
		Material->SetVectorParameterValue(TEXT("NightLight"), FLinearColor::Black);
		Card->SetMaterial(0, Material);
		Card->ComponentTags.Add(FName(*Item.SourceId));
		Card->ComponentTags.Add(FName(*Item.Name));
		Card->RegisterComponent();
		Cards.Add(Card);
	}
	for (int32 Index = 0; Index < Data.Meshes.Num(); ++Index)
	{
		const auto& SourceMesh = Data.Meshes[Index];
		auto* Mesh = NewObject<UProceduralMeshComponent>(GetOwner(),
			MakeUniqueObjectName(GetOwner(), UProceduralMeshComponent::StaticClass(), FName(TEXT("WorldSourceMesh"))));
		GetOwner()->AddInstanceComponent(Mesh);
		Mesh->SetupAttachment(this);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->bReceivesDecals = false;
		Mesh->bUseAsOccluder = false;
		TArray<FVector> Vertices, Normals;
		TArray<FVector2D> UV;
		for (const auto& Position : SourceMesh.Positions) Vertices.Add(ConvertUnityVector(Position) * Data.UnitsToCentimeters);
		for (const auto& Normal : SourceMesh.Normals) Normals.Add(ConvertUnityVector(Normal).GetSafeNormal());
		for (const auto& Coord : SourceMesh.UV) UV.Add(FVector2D(Coord.X, 1.0 - Coord.Y));
		// The position basis preserves handedness. UE mesh front faces use
		// Cross(C-A, B-A), while the imported Unity indices use Cross(B-A, C-A).
		TArray<int32> UeIndices = SourceMesh.Indices;
		for (int32 Triangle = 0; Triangle < UeIndices.Num(); Triangle += 3)
			Swap(UeIndices[Triangle + 1], UeIndices[Triangle + 2]);
		Mesh->CreateMeshSection_LinearColor(0, Vertices, UeIndices, Normals, UV,
			SourceMesh.Colors, TArray<FProcMeshTangent>(), false);
		auto* Material = UMaterialInstanceDynamic::Create(MeshParent, this);
		Material->SetTextureParameterValue(TEXT("Sprite"), Textures[Data.Items.Num() * 2 + Index]);
		Mesh->SetMaterial(0, Material);
		Mesh->ComponentTags.Add(FName(*SourceMesh.SourceId));
		Mesh->ComponentTags.Add(FName(*SourceMesh.Name));
		Mesh->RegisterComponent();
		MeshLayers.Add(Mesh);
	}
	CollisionData = Data.CollisionData;
	CollisionState.Reset(Data.Groups.Num());
	// Serialized m_is_active is a collision flag, separate from GameObject activity.
	GroupVisibility.Init(true, Data.Groups.Num());
	for (int32 Index = 0; Index < Data.Groups.Num(); ++Index)
	{
		CollisionState.Active[Index] = Data.Groups[Index].bActive;
	}
	return Cards.Num() == Data.Items.Num() && !Cards.IsEmpty();
}

void URokWorldStructurePresentationComponent::UpdatePresentation(float Dxf, bool bShowDetail)
{
	if (Cards.Num() != Data.Items.Num() || Cards.IsEmpty()) return;
	const float Scale = Data.EvaluateScale(Dxf);
	if (!Data.Groups.IsEmpty())
	{
		// GridCollideMgr.ShiftPos only adjusts the first registered item.
		const FRokWorldStructurePresentationGroup& First = Data.Groups[0];
		if (First.bShiftPosition)
		{
			float Alpha = 0.0f;
			if (Scale >= First.ShiftEndScale) Alpha = 1.0f;
			else if (Scale > First.ShiftStartScale && First.ShiftEndScale > First.ShiftStartScale)
				Alpha = (Scale - First.ShiftStartScale) / (First.ShiftEndScale - First.ShiftStartScale);
			CollisionData.Items[0].Position = FMath::Lerp(First.Position, First.ShiftOrigin, double(Alpha));
		}
		const TArray<bool> PreviousActive = CollisionState.Active;
		CollisionState.Update(CollisionData, Scale);
		for (int32 Index = 0; Index < GroupVisibility.Num(); ++Index)
		{
			// Activate/Deactivate only call GameObject.SetActive on a flag edge.
			if (PreviousActive[Index] != CollisionState.Active[Index])
				GroupVisibility[Index] = CollisionState.Active[Index];
		}
	}
	ActiveMeshCount = 0;
	for (int32 Index = 0; Index < MeshLayers.Num(); ++Index)
	{
		ActiveMeshCount += bShowDetail && Data.Meshes[Index].bEnabled ? 1 : 0;
		MeshLayers[Index]->SetVisibility(bShowDetail && Data.Meshes[Index].bEnabled);
		MeshLayers[Index]->SetHiddenInGame(!bShowDetail || !Data.Meshes[Index].bEnabled);
	}
	ActiveCardCount = 0;
	for (int32 Index = 0; Index < Data.Items.Num(); ++Index)
	{
		const FRokWorldStructurePresentationItem& Item = Data.Items[Index];
		const bool bGrouped = Data.Groups.IsValidIndex(Item.GroupIndex);
		const float ItemScale = bGrouped ? Scale : 1.0f;
		const FVector Position = bGrouped ? CollisionData.Items[Item.GroupIndex].Position : Item.Position;
		const FVector Center = Item.SpritePosition
			+ Item.Right * ((0.5 - Item.Pivot.X) * Item.Size.X)
			+ Item.Up * ((0.5 - Item.Pivot.Y) * Item.Size.Y);
		Cards[Index]->SetRelativeLocation(ConvertUnityVector(Position + Center * ItemScale) * Data.UnitsToCentimeters);
		Cards[Index]->SetRelativeScale3D(FVector(Item.Size.X * ItemScale * Data.UnitsToCentimeters / 100.0,
			-Item.Size.Y * ItemScale * Data.UnitsToCentimeters / 100.0, 1.0));
		const bool bGroupActive = !bGrouped || GroupVisibility[Item.GroupIndex];
		const bool bGroundVisible = !Item.bGroundLod0Only || Dxf <= 1500.0f;
		const bool bLayerVisible = bShowDetail && Item.bEnabled && bGroupActive && bGroundVisible && ItemScale > UE_SMALL_NUMBER;
		Cards[Index]->SetVisibility(bLayerVisible);
		Cards[Index]->SetHiddenInGame(!bLayerVisible);
		ActiveCardCount += bLayerVisible ? 1 : 0;
	}
}
