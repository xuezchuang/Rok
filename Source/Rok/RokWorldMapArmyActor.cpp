#include "RokWorldMapArmyActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "RokDataPaths.h"
#include "RokSpriteClip.h"
#include "RokGameNetSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
using FRow = TSharedPtr<FJsonObject>;
const FVector SpriteUp(0, UE_INV_SQRT_2, UE_INV_SQRT_2);
const FVector SpriteNormal(0, UE_INV_SQRT_2, -UE_INV_SQRT_2);
const FVector SpriteRight(-1, 0, 0);

FRow Object(const FRow& Row, const TCHAR* Field)
{
	const FRow* Value = nullptr;
	return Row && Row->TryGetObjectField(Field, Value) ? *Value : nullptr;
}

const TArray<TSharedPtr<FJsonValue>>& Rows(const FRow& Row, const TCHAR* Field)
{
	static const TArray<TSharedPtr<FJsonValue>> Empty;
	const TArray<TSharedPtr<FJsonValue>>* Value = nullptr;
	return Row && Row->TryGetArrayField(Field, Value) ? *Value : Empty;
}

int64 Integer(const FRow& Row, const TCHAR* Field)
{
	int64 Value = 0;
	RokSprotoJson::TryGetInt64(Row, Field, Value);
	return Value;
}

bool Position(const FRow& Row, FVector2D& Out)
{
	int64 X = 0, Y = 0;
	if (!RokSprotoJson::TryGetInt64(Row, TEXT("x"), X) || !RokSprotoJson::TryGetInt64(Row, TEXT("y"), Y)) return false;
	Out = FVector2D(X, Y) / 100.0;
	return true;
}

struct FArmyArt
{
	FRow Index;
	FRichCurve Scale;

	FArmyArt()
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *RokDataPaths::Resolve(TEXT("WorldArmySprites/index.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Index))
		{
			UE_LOG(LogTemp, Error, TEXT("Rok world army sprite index missing. Run BuildRokWorldArmySprites.py."));
			return;
		}
		for (const auto& Value : Rows(Index, TEXT("scale")))
		{
			const auto Row = Value->AsObject();
			const FKeyHandle Handle = Scale.AddKey(Row->GetNumberField(TEXT("dxf")), Row->GetNumberField(TEXT("scale")));
			auto& Key = Scale.GetKey(Handle);
			Key.ArriveTangent = Row->GetNumberField(TEXT("in_slope"));
			Key.LeaveTangent = Row->GetNumberField(TEXT("out_slope"));
			Key.InterpMode = Row->GetStringField(TEXT("interpolation")) == TEXT("constant") ? RCIM_Constant : RCIM_Cubic;
		}
	}
};

FArmyArt& Art()
{
	static FArmyArt Data;
	return Data;
}

double Rule(const TCHAR* Table, int32 Type, const TCHAR* Value, double Default, int32 Towards = 0)
{
	for (const auto& Entry : Rows(Object(Art().Index, TEXT("formation")), Table))
	{
		const auto Row = Entry->AsObject();
		if (Integer(Row, TEXT("type")) == Type && (!Towards || Integer(Row, TEXT("towards")) == Towards))
			return Row->GetNumberField(Value);
	}
	return Default;
}

int32 RepresentedCount(int32 Type, int64 Soldiers)
{
	for (const auto& Entry : Rows(Object(Art().Index, TEXT("formation")), TEXT("SquareNumberBySum")))
	{
		const auto Row = Entry->AsObject();
		const int64 Max = Integer(Row, TEXT("rangeMax"));
		if (Integer(Row, TEXT("type")) == Type && Soldiers >= Integer(Row, TEXT("rangeMin")) && (Max < 0 || Soldiers < Max))
			return FMath::Clamp<int32>(Integer(Row, TEXT("num")), 1, 20);
	}
	return 1;
}
}

ARokWorldMapArmyActor::ARokWorldMapArmyActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Formation")));
}

bool ARokWorldMapArmyActor::UpdateFromServer(const FRow& Row, const FRokWorldMapProjection& InProjection,
	const URokGameNetSubsystem& Net)
{
	if (!Position(Object(Row, TEXT("objectPos")), ServerPosition)) return false;
	Network = &Net;
	Projection = InProjection;
	ObjectId = Integer(Row, TEXT("objectId"));
	StartTime = Integer(Row, TEXT("startTime"));
	ArrivalTime = Integer(Row, TEXT("arrivalTime"));
	const auto NextPhase = FRokServerMarch::ClassifyStatus(Integer(Row, TEXT("status")));
	if (NextPhase != Phase)
	{
		Phase = NextPhase;
		AnimationElapsed = 0;
	}
	Path.Reset();
	PathLength = 0;
	for (const auto& Value : Rows(Row, TEXT("objectPath")))
	{
		FVector2D Point;
		if (!Value || !Position(Value->AsObject(), Point)) continue;
		if (!Path.IsEmpty()) PathLength += FVector2D::Distance(Path.Last(), Point);
		Path.Add(Point);
	}
	if (Phase == ERokServerMarchPhase::Battling)
	{
		const FString TargetId = LexToString(Integer(Row, TEXT("targetObjectIndex")));
		const FRow Target = Object(Net.GetWorldMapObjects(), *TargetId);
		FVector2D TargetPosition;
		if (Position(Object(Target, TEXT("objectPos")), TargetPosition) && !ServerPosition.Equals(TargetPosition))
			Heading = (TargetPosition - ServerPosition).GetSafeNormal();
		else if (Row->HasField(TEXT("targetAngle")))
		{
			const double Angle = FMath::DegreesToRadians(Integer(Row, TEXT("targetAngle")) / 100.0);
			Heading = FVector2D(FMath::Sin(Angle), FMath::Cos(Angle));
		}
	}

	const auto Models = Object(Art().Index, TEXT("units"));
	const auto Heroes = Object(Art().Index, TEXT("heroes"));
	const auto Arms = Object(Art().Index, TEXT("arms"));
	TArray<FUnit> Desired;
	const int64 Deputy = Integer(Row, TEXT("deputyHeroId"));
	auto AddCommander = [&](int64 Id, FVector2D Offset)
	{
		FString Model;
		if (Id <= 0 || !Heroes || !Heroes->TryGetStringField(LexToString(Id), Model)) return;
		if (Models && !Models->HasField(Model) && Models->HasField(FString::Printf(TEXT("Hero_%lld"), Id)))
			Model = FString::Printf(TEXT("Hero_%lld"), Id);
		if (Models && !Models->HasField(Model)) Model = TEXT("Hero_2007"); // Mounted fallback for missing source sheets.
		if (Models && Models->HasField(Model)) Desired.Add({Model, Offset});
	};
	const int32 LeaderOffset = Deputy > 0 ? 1 : 2;
	AddCommander(Integer(Row, TEXT("mainHeroId")), {Rule(TEXT("SquareOffset"), LeaderOffset, TEXT("offsetX"), 0),
		Rule(TEXT("SquareOffset"), LeaderOffset, TEXT("offsetZ"), 0)});
	AddCommander(Deputy, {Rule(TEXT("SquareOffset"), 3, TEXT("offsetX"), -0.6), Rule(TEXT("SquareOffset"), 3, TEXT("offsetZ"), -0.3)});

	struct FSoldierModel { FString Model; int64 Count = 0; };
	TArray<FSoldierModel> ByType[5];
	for (const auto& Value : Rows(Row, TEXT("soldiers")))
	{
		const FRow Soldier = Value->AsObject();
		const int64 Count = Integer(Soldier, TEXT("num"));
		const TArray<TSharedPtr<FJsonValue>>* Config = nullptr;
		if (Count <= 0 || !Arms || !Arms->TryGetArrayField(LexToString(Integer(Soldier, TEXT("id"))), Config) || Config->Num() != 2) continue;
		const int32 Type = (*Config)[1]->AsNumber();
		const FString Model = (*Config)[0]->AsString();
		if (Type >= 1 && Type <= 4 && Models && Models->HasField(Model)) ByType[Type].Add({Model, Count});
	}
	double Back = -1.05;
	for (const int32 Type : {2, 1, 3, 4}) // SquareHelper.GetSquareString's row order.
	{
		auto& Group = ByType[Type];
		if (Group.IsEmpty()) continue;
		Group.Sort([](const FSoldierModel& A, const FSoldierModel& B) { return A.Model < B.Model; });
		int64 Total = 0;
		for (const auto& Soldier : Group) Total += Soldier.Count;
		const int32 Count = RepresentedCount(Type, Total);
		const int32 MaxPerRow = FMath::Max(1, int32(Rule(TEXT("SquareMaxNumber"), Type, TEXT("num"), 5)));
		const double Width = Rule(TEXT("SquareRowWidth"), Type, TEXT("RowWidth"), 3.5);
		const double Spacing = Rule(TEXT("SquareSpacing"), Type, TEXT("spacing"), 0.65, 1)
			+ Rule(TEXT("SquareSpacing"), Type, TEXT("spacing"), 0.65, 2);
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			int64 At = (2 * Slot + 1) * Total / (2 * Count);
			const FString* Model = &Group.Last().Model;
			for (const auto& Soldier : Group)
			{
				if (At < Soldier.Count) { Model = &Soldier.Model; break; }
				At -= Soldier.Count;
			}
			const int32 RowCount = FMath::Min(MaxPerRow, Count - (Slot / MaxPerRow) * MaxPerRow);
			const double Across = (Slot % MaxPerRow - (RowCount - 1) * 0.5) * (Width / MaxPerRow);
			Desired.Add({*Model, {Across, Back - (Slot / MaxPerRow) * Spacing}});
		}
		Back -= FMath::DivideAndRoundUp(Count, MaxPerRow) * Spacing;
	}
	if (Units != Desired) BuildCards(MoveTemp(Desired));
	UpdateVisuals();
	return !Units.IsEmpty();
}

void ARokWorldMapArmyActor::BuildCards(TArray<FUnit> Desired)
{
	for (const auto& Unit : Units) if (Unit.Card.IsValid()) Unit.Card->DestroyComponent();
	Units = MoveTemp(Desired);
	auto* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	auto* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldEntitySprite.M_RokWorldEntitySprite"));
	if (!Plane || !Parent) return;
	for (auto& Unit : Units)
	{
		auto* Card = NewObject<UStaticMeshComponent>(this);
		Card->SetupAttachment(GetRootComponent());
		Card->SetStaticMesh(Plane);
		Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Card->SetGenerateOverlapEvents(false);
		Card->SetCanEverAffectNavigation(false);
		Card->SetCastShadow(false);
		Card->bReceivesDecals = false;
		Card->SetVisibility(false);
		Card->SetRelativeRotation(FMatrix(SpriteRight, SpriteUp, SpriteNormal, FVector::ZeroVector).ToQuat());
		Unit.Card = Card;
		Unit.Material = Card->CreateDynamicMaterialInstance(0, Parent);
		Card->RegisterComponent();
	}
	UE_LOG(LogTemp, Display, TEXT("Rok world army formation: object=%lld cards=%d phase=%d."), ObjectId, Units.Num(), int32(Phase));
}

float FRokSpriteClip::EvaluateUnityUnitScale(float Dxf)
{
	return Art().Scale.Eval(Dxf, 1.0f);
}

TSharedPtr<FRokSpriteClip> FRokSpriteClip::Load(
	const FString& Directory, const FString& Model, const TCHAR* Pose)
{
	static TMap<FString, FRow> SpriteIndices;
	static TMap<FString, TWeakPtr<FRokSpriteClip>> SharedClips;
	const FString Key = Directory / (Model + TEXT("_") + Pose);
	auto& Shared = SharedClips.FindOrAdd(Key);
	if (auto Clip = Shared.Pin()) return Clip;
	FRow& Index = SpriteIndices.FindOrAdd(Directory);
	if (Directory == TEXT("WorldArmySprites"))
	{
		Index = Art().Index;
	}
	else if (!Index)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *RokDataPaths::Resolve(Directory / TEXT("index.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Index)) return nullptr;
	}
	const auto Record = Object(Object(Object(Index, TEXT("units")), *Model), Pose);
	if (!Record) return nullptr;
	const int32 Width = Integer(Record, TEXT("width"));
	const int32 Height = Integer(Record, TEXT("height"));
	const auto& Counts = Rows(Record, TEXT("counts"));
	FImage Image;
	if (Width <= 0 || Height <= 0 || Counts.Num() != 5
		|| !FImageUtils::LoadImage(*RokDataPaths::Resolve(Directory / Record->GetStringField(TEXT("sheet"))), Image)
		|| Image.SizeY != 5 * Height || Image.SizeX < Width) return nullptr;
	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const auto Clip = MakeShared<FRokSpriteClip>();
	Clip->SizeInUnity = FVector2D(Width, Height) / FMath::Max(1.0, Record->GetNumberField(TEXT("ppu")));
	Clip->Pivot = {Record->GetNumberField(TEXT("pivot_x")), Record->GetNumberField(TEXT("pivot_y"))};
	Clip->FrameSeconds = FMath::Max(0.033, Record->GetNumberField(TEXT("rate")));
	TArray64<uint8> Pixels;
	Pixels.SetNumUninitialized(int64(Width) * Height * sizeof(FColor));
	for (int32 Dir = 0; Dir < 5; ++Dir)
	{
		const int32 Count = FMath::Clamp(int32(Counts[Dir]->AsNumber()), 0, Image.SizeX / Width);
		for (int32 Frame = 0; Frame < Count; ++Frame)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const int64 Start = int64(Dir * Height + Y) * Image.SizeX + Frame * Width;
				FMemory::Memcpy(Pixels.GetData() + int64(Y) * Width * sizeof(FColor),
					Image.RawData.GetData() + Start * sizeof(FColor), Width * sizeof(FColor));
			}
			if (auto* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, NAME_None, Pixels))
			{
				Texture->Filter = TF_Bilinear;
				Texture->NeverStream = true;
				Texture->UpdateResource();
				Clip->Frames[Dir].Emplace(Texture);
			}
		}
	}
	Shared = Clip;
	return Clip;
}

TSharedPtr<FRokSpriteClip> ARokWorldMapArmyActor::GetClip(const FString& Model, const TCHAR* Pose)
{
	const FString Key = Model + TEXT("_") + Pose;
	if (const auto* Clip = Clips.Find(Key)) return *Clip;
	return Clips.Add(Key, FRokSpriteClip::Load(TEXT("WorldArmySprites"), Model, Pose));
}

void ARokWorldMapArmyActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AnimationElapsed += DeltaSeconds;
	UpdateVisuals();
}

void ARokWorldMapArmyActor::UpdateVisuals()
{
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	const auto* Camera = Controller ? Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()) : nullptr;
	const float Dxf = Camera ? Camera->GetCurrentDxf() : 1509;
	const bool bOnMap = Dxf < 3000 && Phase != ERokServerMarchPhase::Garrisoned && Phase != ERokServerMarchPhase::Collecting;
	SetActorHiddenInGame(!bOnMap);
	if (!bOnMap) return;

	const bool bTravel = Phase == ERokServerMarchPhase::Marching || Phase == ERokServerMarchPhase::Returning;
	FVector2D NowPosition = ServerPosition;
	double ServerTime = 0;
	const bool bFollowPath = bTravel && Path.Num() >= 2 && PathLength > UE_SMALL_NUMBER && ArrivalTime > StartTime
		&& Network.IsValid() && Network->TryGetServerTimeSeconds(ServerTime);
	if (bFollowPath)
	{
		double Remaining = PathLength * FMath::Clamp((ServerTime - StartTime) / (ArrivalTime - StartTime), 0.0, 1.0);
		NowPosition = Path.Last();
		for (int32 Segment = 1; Segment < Path.Num(); ++Segment)
		{
			const FVector2D Delta = Path[Segment] - Path[Segment - 1];
			const double Length = Delta.Length();
			if (Length <= UE_SMALL_NUMBER) continue;
			Heading = Delta / Length;
			if (Remaining <= Length)
			{
				NowPosition = Path[Segment - 1] + Heading * Remaining;
				break;
			}
			Remaining -= Length;
		}
	}
	SetActorLocation(Projection.UnityToUe(NowPosition) + FVector(0, 0, 20));
	const TCHAR* Pose = Phase == ERokServerMarchPhase::Battling ? TEXT("attack")
		: bTravel && (!bFollowPath || ServerTime < ArrivalTime) ? TEXT("walk") : TEXT("idle");
	const double Degrees = FMath::RadiansToDegrees(FMath::Atan2(Heading.X, Heading.Y));
	const int32 Facing = (FMath::RoundToInt(Degrees / 45.0) + 8) % 8;
	const int32 Direction = FMath::Min(Facing, 8 - Facing); // AnimationBase.DirectionToIndex.
	// The UE plane's U axis reverses the SpriteRenderer's horizontal flip.
	const bool bMirror = !(Facing > 0 && Facing < 4);
	const float UnitScale = FRokSpriteClip::EvaluateUnityUnitScale(Dxf) * Projection.GetSafeUeScale();
	const FVector2D Side(Heading.Y, -Heading.X);
	bHasVisibleCards = false;
	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		auto& Unit = Units[Index];
		const auto Clip = GetClip(Unit.Model, Pose);
		if (!Unit.Card.IsValid() || !Unit.Material.IsValid() || !Clip || Clip->Frames[Direction].IsEmpty()) continue;
		const auto& Frames = Clip->Frames[Direction];
		const int32 Frame = FMath::FloorToInt(AnimationElapsed / Clip->FrameSeconds + Index * 0.37) % Frames.Num();
		auto* Texture = Frames[Frame].Get();
		if (Unit.LastTexture.Get() != Texture)
		{
			Unit.Material->SetTextureParameterValue(TEXT("Sprite"), Texture);
			Unit.LastTexture = Texture;
		}
		const FVector2D Size = Clip->SizeInUnity * UnitScale;
		const FVector2D Offset = (Side * Unit.Offset.X + Heading * Unit.Offset.Y) * Projection.GetSafeUeScale();
		const double PivotX = bMirror ? 1.0 - Clip->Pivot.X : Clip->Pivot.X;
		Unit.Card->SetRelativeScale3D({(bMirror ? Size.X : -Size.X) / 100.0, -Size.Y / 100.0, 1});
		Unit.Card->SetRelativeLocation(FVector(-Offset.X, Offset.Y, 0) + SpriteRight * ((0.5 - PivotX) * Size.X)
			+ SpriteUp * FMath::Max(Size.Y * 0.5, (0.5 - Clip->Pivot.Y) * Size.Y));
		Unit.Card->SetVisibility(true);
		bHasVisibleCards = true;
	}
}
