#include "RokMapTravelSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

void URokMapTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
}

void URokMapTravelSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	ResetRequest(false);
	Super::Deinitialize();
}

bool URokMapTravelSubsystem::RequestTravel(UWorld* Source, TSoftObjectPtr<UWorld> Destination,
	const FString& Options, FSimpleDelegate OnFailure)
{
	if (bPending || !IsValid(Source) || Source != GetWorld() || Destination.IsNull()
		|| Source->GetGameInstance() != GetGameInstance())
	{
		return false;
	}

	bPending = true;
	SourceWorld = Source;
	Target = Destination;
	TravelOptions = Options;
	FailureDelegate = MoveTemp(OnFailure);
	RequestStartSeconds = FPlatformTime::Seconds();
	const uint64 Generation = ++RequestGeneration;

	if (Source->WorldType != EWorldType::Game)
	{
		// PIE owns package duplication/renaming in UEngine::LoadMap. Preserve
		// that path rather than preload an editor world into its game context.
		bCommitted = true;
		UGameplayStatics::OpenLevelBySoftObjectPtr(Source, Target, true, TravelOptions);
		return true;
	}

	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Target.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this,
			[this, Generation]() { CompletePreload(Generation); }),
		FStreamableManager::DefaultAsyncLoadPriority, false, false, TEXT("RokCityWorldTravel"));
	if (!LoadHandle.IsValid())
	{
		ResetRequest(true);
		return false;
	}
	return true;
}

void URokMapTravelSubsystem::CompletePreload(uint64 Generation)
{
	if (!bPending || Generation != RequestGeneration)
	{
		return;
	}
	UWorld* Source = SourceWorld.Get();
	PreloadedWorld = Target.Get();
	if (!IsValid(Source) || Source != GetWorld() || !IsValid(PreloadedWorld)
		|| PreloadedWorld->IsInitialized() || PreloadedWorld->GetGameInstance()
		|| PreloadedWorld->WorldType != EWorldType::Inactive)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok map travel preload failed or source changed: target=%s; retaining current world."),
			*Target.ToString());
		ResetRequest(true);
		return;
	}

	// UWorld::PostLoad leaves an asynchronously loaded map Inactive; it has
	// no game instance, InitWorld, BeginPlay or server population at this point.
	// LoadMap reuses FindPackage and initializes it after cleaning up Source.
	UE_LOG(LogTemp, Display, TEXT("Rok map travel preload ready: target=%s async_wait_ms=%.3f."),
		*Target.ToString(), (FPlatformTime::Seconds() - RequestStartSeconds) * 1000.0);
	bCommitted = true;
	UGameplayStatics::OpenLevelBySoftObjectPtr(Source, Target, true, TravelOptions);
}

void URokMapTravelSubsystem::HandlePreLoadMap(const FWorldContext& Context, const FString& MapName)
{
	if (!bPending || Context.OwningGameInstance != GetGameInstance())
	{
		return;
	}
	if (!bCommitted || MapName != Target.ToSoftObjectPath().GetLongPackageName())
	{
		// Another travel superseded this request while its source was active.
		ResetRequest(true);
		return;
	}
	bLoadMapStarted = true;
	LoadMapStartSeconds = FPlatformTime::Seconds();
}

void URokMapTravelSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!bPending || !bLoadMapStarted
		|| (LoadedWorld && LoadedWorld->GetGameInstance() != GetGameInstance()))
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("Rok map travel completed: target=%s success=%d load_map_ms=%.3f request_to_loaded_ms=%.3f."),
		*Target.ToString(), LoadedWorld ? 1 : 0,
		(Now - LoadMapStartSeconds) * 1000.0, (Now - RequestStartSeconds) * 1000.0);
	ResetRequest(LoadedWorld == nullptr);
}

void URokMapTravelSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (bPending && !bCommitted && SourceWorld.Get() == World)
	{
		ResetRequest(true);
	}
}

void URokMapTravelSubsystem::ResetRequest(bool bFailed)
{
	++RequestGeneration;
	FSimpleDelegate OnFailure = MoveTemp(FailureDelegate);
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
	PreloadedWorld = nullptr;
	SourceWorld.Reset();
	Target.Reset();
	TravelOptions.Reset();
	bPending = false;
	bCommitted = false;
	bLoadMapStarted = false;
	if (bFailed)
	{
		OnFailure.ExecuteIfBound();
	}
}
