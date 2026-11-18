# City/world travel investigation, 2026-11-18

Status: code change prepared; no build, runtime reproduction, post-change timing or visual acceptance has been performed. The TODO performance acceptance gate remains open.

## Evidence before the change

The two runtime entrypoints immediately called `UGameplayStatics::OpenLevelBySoftObjectPtr`: `ARokStrategyCameraPawn::RequestReturnToWorldMapInternal` and `ARokWorldMapPlayerController::RequestEnterPlayerCity`. Each call schedules a blocking map load on the following engine tick. This tears down the current map, performs GC, loads the destination and reconstructs its HUD.

Five existing navigation logs from 2026-11-17 contain both directions. They are historical automated server navigation runs, not measurements from the user's reported session or a controlled performance benchmark. The logs were read on 2026-11-18 without starting an editor or client:

| Run under `Saved/RokUi/Tests/` | City to world LoadMap (ms) | World to city LoadMap (ms) |
| --- | ---: | ---: |
| `task_chapter_server_20261117-legacy-navigation/run.log` | 229.965 | 158.282 |
| `task_chapter_server_final_20261117-legacy-navigation/run.log` | 199.938 | 220.472 |
| `task_chapter_server_final_20261117-wbp-navigation/run.log` | 292.833 | 232.168 |
| `task_chapter_server_hospital_gc_20261117-legacy-navigation/run.log` | 241.493 | 196.458 |
| `task_chapter_server_hospital_gc_20261117-wbp-navigation/run.log` | 330.949 | 251.938 |

The final WBP log shows the city-to-world request and LoadMap at 10:41:59.612, world creation at .721, BeginPlay at .771, HUD completion at .900 and LoadMap completion at .905. This establishes a blocking travel frame with loading and initialization work; it does not isolate disk I/O, GC or individual asset costs. First-frame work continues afterward, including binding 313 environment materials at .940. Merely moving package loading cannot remove teardown, HUD construction or that first-frame work.

A machine-readable extraction of these ten historical values is at `Saved/RokMapTravel/historical_baseline_20261118.json` (generated scratch evidence).

## Unity reference

`ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GlobalViewLevelMediator.cs`, `OnWorldViewChange`, changes camera/LOD state and sends `EnterCityShow` / `ExitCityHide` in the existing scene. This explains the architectural difference from UE's full map travel. Unity was not launched or timed during this investigation.

## Bounded change

`URokMapTravelSubsystem` retains the current game world while the destination map package loads through `FStreamableManager::RequestAsyncLoad`. Only an inactive, uninitialized world without a game instance may be committed. It then invokes the same OpenLevel API with the original destination and URL options. No gameplay or networking initialization is run manually, and no world actors are cached across travel.

The streamable handle and an explicit GC-tracked world reference keep the destination alive through old-world GC. Both are released after LoadMap completes. A generation guard, per-game-instance PreLoadMap callback, source-world cleanup callback and Deinitialize cancellation prevent a stale completion from initiating another travel. Failed preloads leave the current world active and reset the caller's duplicate-request flag. Duplicate pending requests are rejected. PIE retains the original OpenLevel path so its editor-world duplication and package renaming are unchanged. No claim is made that async package PostLoad is free of game-thread work.

The change moves destination package loading ahead of current-world teardown. It does not implement Unity's continuous camera transition, make OpenLevel asynchronous, or prove a latency improvement. The current city remains active during preload, so total request-to-loaded time may differ from the final blocking LoadMap time. Both are logged separately.

## Engine source checks

The installed UE 5.8 engine source was read from `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`; this workstation does not currently have the documented `D:/ue/UE_5.8` path. No engine was launched or built.

- `Runtime/Engine/Private/World.cpp`, `UWorld::PostLoad`: defaults a loaded world to `EWorldType::Inactive`; does not run `InitWorld` or gameplay BeginPlay. `UWorld::InitWorld` separately initializes world subsystems.
- `Runtime/Engine/Private/UnrealEngine.cpp`, `UEngine::LoadMap`: emits PreLoadMapWithContext, tears down the old world and trims memory, then reuses a destination found through FindPackage. It sets the new game instance before InitWorld and BeginPlay. PIE has its own duplication/renaming branch. Its scoped PostLoadMap caller also emits a null world on failure.
- `Runtime/Engine/Private/StreamableManager.cpp`, `FStreamableManager::AddReferencedObjects`: adds stable asset references for loaded targets. `FStreamableHandle::CancelHandle` cancels queued completion delegates and releases the handle.
- `Runtime/Engine/Classes/Engine/StreamableManager.h`: completion runs on a later tick even if the asset is already loaded. Neither WaitUntilComplete nor any synchronous fallback was added.
- No Rok actor defines a custom PostLoad or PostInitializeComponents override. Server population begins through game-mode BeginPlay or ticking world subsystems, after the destination is committed by the engine.

## Remaining acceptance

After the user permits a current build/runtime validation, compare identical accounts, maps, command lines, rendering modes and warm/cold asset conditions for both directions. Collect the new `Rok map travel preload ready` and `Rok map travel completed` records, the engine's LoadMap duration, request-to-first-interactive-frame time and maximum frame time. Exercise a failed destination, repeated clicks, quitting/logging out during loading, outside travel while loading and PIE travel. Measure Unity separately under comparable conditions. Until then this is a prepared mitigation with historical evidence, not an accepted performance fix.
