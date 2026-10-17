# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Rok** is a UE 5.8 C++ project for a strategy/MMO game in the style of "Rise of Kingdoms" (万国觉醒). The project is being rebuilt from a Unity client (`D:\ueDemo\Rok-unity`) using UE-native C++ subsystems, DataAssets/DataTables, UMG widgets, and Blueprint presentation shells. The Unity source is preserved as reference material under `ref/` — never dump it directly into UE.

- **Engine target**: UE 5.8 — install at `D:\ue\UE_5.8`
- **Project root**: this directory (contains `Rok.uproject`)
- **Modules**: `Rok` (runtime, Default phase) and `RokRendering` (runtime, `PostConfigInit`; RDG/global-shader probes backed by `Shaders/Private/*.usf`). Plugins: `EnhancedInput`, `ProceduralMeshComponent`, `EditorScriptingUtilities`, `ModelContextProtocol`.
- **Default map**: `/Game/Maps/L_RokUnityGameAppCity` (editor + game)
- **Default GameMode**: `ARokGameModeBase`
- **Python for Scripts/**: `C:\Python312\python.exe` (3.12.1). Bare `python` on PATH is the Microsoft Store stub. Codex handoff runners were invoked with `C:/Users/13436/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -B -X utf8`.

The companion live Unity project is at `D:\ueDemo\Rok-unity` (do not commit its `Library/`, `Logs/`, `Temp/` caches). Paired Unity learning notes go in `UnityLearning/`.

## Start Here: Current State (Codex → Claude handoff, 2026-10-16)

Codex ran this migration until 2026-10-16 and stopped at the user's request. Before resuming any slice, read `Saved/RokUnityMigration/ClaudeHandoff_20261016T152648077944Z/HANDOFF.md` (Chinese) and its `EVIDENCE.json` / `MANIFEST.json` SHA-256 pins. The migration is **not** complete.

**Where the newest code lives — shadow project `Saved/UIB1/`.** On 2026-10-10 Codex byte-copied the project to `Saved/UIB1/Rok.uproject`, and every later build (Run1–Run30) ran there. Its `Source/` and `Shaders/` are strictly ahead of the live tree:
- 31 files exist only in UIB1: Alliance gift/ranking/shop/territory readonly audits and data, `RokAllianceUiWindowAnimationAuditSubsystem`, `RokGrass3SameUvAuditSubsystem`, `RokMailLocalUnreadState.h`, `RokWorkerUnknownAccountRuntimeAuditSubsystem`, 10 `Tests/` files (Mail unread, PowerGain clock/replay, Commander/AllianceTerritory synthetic Slate), and `RokRendering/{Public,Private}/RokGrass3SameUvProbe.*`. Also `Shaders/Private/RokGrass3SameUv.usf` and `RokGrass3SameUvPoints.ush`.
- 13 shared files are newer in UIB1: `RokMainHudWidget.cpp`, `RokPowerGainWidget.*`, `RokSpeedUpAuditSubsystem.*`, `RokUnityFeatureScreenWidget.*`, `RokUnityGameplayCatalogSubsystem.*`, `RokWorldLod2GeometryProbeLibrary.*`, `RokAllianceFlagLocalPreviewAuditSubsystem.*`.
- `Saved/` is git-ignored and not in P4, so this code is unprotected. Promote UIB1 → live `Source/`/`Shaders/` only with user approval, then rebuild `RokEditor` and commit. **Never copy UIB1 `Config/` back**: it deliberately sets MCP `bAutoStartServer=False`, trims `DefaultEngine.ini`, and has no `DefaultGame.ini`/`DefaultEditor.ini`.
- Until promotion, every Codex receipt describes UIB1, not the live tree.

**Last verified checkpoint.** The Run30 repair build of UIB1 (normal `Build.bat`, exit 0) passed independent physical review. Receipt: `Saved/PendingRootPowerMapRun30RepairBuildV1/RepairRun_20261016T151810559184Z/execution_result.json` (SHA `ac0604ac…`). Independent review: `Saved/PendingRootPowerMapRun30RepairCompileActualIndependentV1/READY.json` (`2c778e92…`). After the repair, **no runtime or visual acceptance has run** (`runtime_accepted=false`, `visual_accepted=false`).

**Resume order (from the handoff):**
1. Independently review the frozen UI/Power runtime adapter in `Saved/PendingRootRun30UiPowerRuntimeAdapterV1/`. Do this before switching its runner from `--preflight-only` to execute. Then run its five modes in sequence (`mail/local_fixture`, `mail/ordinary_unbound`, `shop`, `ranking`, `power`) and look at the real PNGs. Power keeps 17 captures and the manual-close boundary.
2. Grass3 same-UV GPU runner `Saved/PendingRootRokGrass3SameUvActualRunnerV2/` is on HOLD with no READY. Bind the new shared proof/helper, review it independently, and only then run the direct RHI readback (144 source UVs + 8 clamp controls). `material_evaluated` stays false: this is a texture probe, not material or full-map visual acceptance.
3. Unity offline HUD V3 draft `Saved/PendingRootUnityPowerHudOfflineIsolationV3/` (see `WIP_HANDOFF.md`) is uncompiled, with no READY. Fix V2's `GrayChildrens.LoadGrayMaterial` NRE with a key-scoped local `IAssetService` adapter for `UI_GrayWithMask`. Do not disable `GrayChildrens`, edit the original prefab, or start GameApp/real services.
4. Daily Activity Reward V4 (`Saved/PendingRootDailyActivityRewardPreviewV4/`) and runtime audit V2 both need independent review before a new Run31 stage/build. A Daily stage changes the closure, so Run30 proof no longer covers it.
- Blocked: Academy / age 2 (source map context and resource chain gaps). Do not approximate terrain to unblock it.

**Evidence conventions (keep them).**
- `Saved/Pending*/` (~1.35k dirs) are candidate/evidence staging areas, not the live project.
- `READY.json` = a frozen candidate passed its own gates. It does **not** mean accepted.
- Never delete failed receipts, flip HOLD constants, rerun old one-shot requests, or rewrite failed history. Historical Stage `88ebb11b…`, adopted inner `9da47986…` and outer `392d7451…` stay false.
- Verifier PASS ≠ visual parity: always inspect the PNG.

**Account / server boundary.** No real accounts, logins, reward claims, inventory actions, or server writes. UI, power and reward data are explicitly local fixtures/previews. Never copy auth payloads into reports.

**UI coverage** (`Saved/RokUnityMigration/unity_ui_runtime_coverage_verify.json`, 2026-09-12): 46 mappings / 49 entries checked, 1105 of 1150 Unity UI prefabs still unimplemented. The verifier only proves that declarations exist. Earlier UI handoff: `UnityLearning/UI_MIGRATION_HANDOFF_2026-08-14.md`.

## Version Control: Git + Perforce (verified 2026-10-17)

| | Git | Perforce |
|---|---|---|
| Server | `origin` `https://github.com/xuezchuang/Rok.git`, branch `main` | `home.snowsome.com:1666`, user `snow`, client `ueDemo`, root `D:\ueDemo`, view `//depot/... //ueDemo/...` |
| Owns | Text: `Source/`, `Shaders/`, `Scripts/`, `Config/`, `Rok.uproject`, `AGENTS.md`, `CLAUDE.md`, `UnityLearning/` notes | Binary content and reference snapshot: `//depot/Rok/Content/...` (~2.3k files), `//depot/Rok/ref/...` (~74k files) |
| Latest | HEAD `7b2bfce4` (2026-07-19); ~29 modified + ~1.9k untracked WIP files since | CL 76 (2026-09-12); 142 Content files opened in the default changelist (137 add / 5 edit), not submitted |

- **P4 holds stale text copies.** `//depot/Rok/Source` (6 files), `Scripts` (16), `Config` (2) and `AGENTS.md` are May 2026 snapshots (CL 46–49), and every one differs from the git copy. Never `p4 sync -f`, "Get Revision (force)", or `p4 reconcile` over those paths: git is authoritative for text.
- The large untracked set is real WIP: new subsystems, ~480 scripts, and the `UnityLearning/InitialCityLayoutUnity*` Unity projects. Never `git reset --hard`, `git clean`, `git checkout --`, or bulk revert/submit in P4. Commit or submit only when the user asks.
- `.gitignore` excludes `*.slnx` and the .NET `Scripts/*/bin/`, `Scripts/*/obj/` folders of the Spine exporters (`RokSpineExport`, `RokMailSpineExport`, `RokTrainSpineExport`). Keep `obj/project.assets.json` on disk: the handoff's offline `--no-restore` Spine builds depend on it.

## Build & Run Commands

All UE tooling lives under `D:\ue\UE_5.8\Engine\Binaries\Win64\` and `D:\ue\UE_5.8\Engine\Build\BatchFiles\`.

```bat
:: Launch the editor with the default map
"D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\ueDemo\Rok\Rok.uproject" /Game/Maps/L_RokUnityGameAppCity -log

:: Build the Editor target
"D:\ue\UE_5.8\Engine\Build\BatchFiles\Build.bat" RokEditor Win64 Development "D:\ueDemo\Rok\Rok.uproject" -waitmutex

:: Build the Game target
"D:\ue\UE_5.8\Engine\Build\BatchFiles\Build.bat" Rok Win64 Development "D:\ueDemo\Rok\Rok.uproject" -waitmutex

:: Run a map-generation commandlet
"D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\ueDemo\Rok\Rok.uproject" /Game/Maps/L_RokCityRuntime -ExecutePythonScript="<script>.py"
```

Convenience launcher: `RunRokInitialScene.bat` (sets the above editor + map).

**Never** use older UE installs such as `D:\ue\UE_5.2` or `C:\UnrealEngine` for this project; saving `.umap`/assets with the wrong engine can produce incompatible content.

## Official Unreal MCP

The project uses UE 5.8's built-in experimental `ModelContextProtocol` plugin. Its HTTP endpoint is `http://127.0.0.1:8010/mcp`, configured in `Config/DefaultEditorPerProjectUserSettings.ini`. No project-relative MCP plugin or legacy WebSocket transport is used.

**Before any MCP write**, run a read-only identity check:

```python
import unreal
print(unreal.Paths.project_dir())
print(unreal.EditorLevelLibrary.get_editor_world().get_name())
```

Proceed only when the returned project path exactly matches this project. If ambiguous, stop and report active UE processes/ports. Confirm the official MCP listener is bound to loopback port `8010` before writes.

`project_dir()` returning `D:/ueDemo/Rok/Saved/UIB1/` means you are attached to the shadow project, not the live one. UIB1 ships with MCP auto-start off, so this only happens if someone enabled it manually. Other workstation projects (MMOARPG, Metanoia, `D:\ueasset\Rok`, EasySurvivalRPG) may be open at the same time on legacy WebSocket ports 8090–8098 (see AGENTS.md).

## High-Level Architecture

The `Rok` module is organized into three layers, mirroring the AGENTS.md migration plan:

### 1. Camera & Input (Pawn/Controller)
- `ARokCameraPawn` — top-down orthographic strategy camera (legacy)
- `ARokStrategyCameraPawn` — **current** 2.5D perspective camera (TestUnity `RokCitySimpleCamera` model: Unity vertical FOV → UE horizontal FOV, `dxf = distance * fov` zoom curve, zoom-to-cursor, drag pan, keyboard pan, reset)
- `ARokPlayerController` — input routing
- `FRokCityViewTypes` / `FRokCityViewMath` — view-math helpers, `MakeDefaultCameraKeys()` mirrors Unity `CameraParamLod.csv`

### 2. Gameplay Subsystems (`UGameInstanceSubsystem`)
All state, validation, timers, and lookup live here. UI calls into these; they do not call back into UI directly.

| Subsystem | Responsibility |
|---|---|
| `URokServerSubsystem` | HTTP client to the dedicated backend. `PingHealth`, `ConnectAsGuest`, `FetchWorldState`, `StartTestMarch`. Configured via `URokServerSettings` (UObject, `Config=Game`, `DefaultConfig`). |
| `URokBuildingUpgradeSubsystem` | Building queue/timer, reads `Saved/RokUnityMigration/unity_initial_city_state.json`, prefers `UnityInitId_*` tags over actor-label inference. |
| `URokUnityConfigSubsystem` | Unity `InitBuilding` resolution (`unity_gameapp_city_init_building_resolution.json`). |
| `URokUnityGameplayCatalogSubsystem` | Read-only catalog: `BuildingTypeConfig`, `Arms`, `Hero`, `HeroSkill`, `HeroTalent`, `TaskMain`. Source for game-system lookups. |
| `URokUnityRuntimeStateSubsystem` | Local-sim bootstrap state: active task chain, starter hero/troop previews. Marked as bootstrap-only until server state is migrated. |
| `URokUnityUiCatalogSubsystem` | UI slice coverage + MainInterface module tracking. |
| `URokUnityEconomyCatalogSubsystem` | Read-only economy catalog: items, shop categories, charge pages, alliance-shop rows. |
| `URokWorldMapEntitySubsystem` | World-map entity data, incl. `m_unit_scale_curve` for formation sizing. |
| `URokAudioRouterSubsystem` | Local-only audio channel router (no server/account requests). |
| `URokClientSettingsSubsystem` | Client settings persisted in `GameUserSettings.ini` (`/Script/Rok.RokClientSettingsConfig`). |
| `URokWorldDayNightSubsystem`, `URokCityGammaLayerCensusSubsystem` | `UTickableWorldSubsystem`s for world day/night materials and a read-only census of GameApp city renderer inputs. |

Beyond these, `Source/Rok` holds ~65 `*AuditSubsystem` classes (more in UIB1). They are read-only, local-fixture audit/preview harnesses driven by `Scripts/Run*/Verify*` runners: they **observe** UI/visual state for acceptance evidence and are not product gameplay. `Source/Rok/Tests/` holds ~83 automation test files. `RokRendering` hosts the city gamma compositor/raster and source-geometry/camera-material probes.

### 3. World Presentation (Actors/Components)
- `ARokCitySpriteActor` — camera-facing sprite card with translucent alpha + mask material (render queue `2995`, `ZWrite Off`); the accepted runtime path for ordinary buildings, **not** imported FBX/static-mesh cards.
- `ARokBuildingActor` — building gameplay/upgrade actor
- `ARokWorldMapEntityActor` / `ARokWorldMapCameraPawn` — world-map formation cards and world camera (rules in AGENTS.md "Unity World Entity Presentation Slice")
- `URokMainHudWidget`, `URokBuildingPanelWidget`, `URokWorldMapHudWidget`, `URokUnityFeatureScreenWidget`, `URokUnityModalWidget`, `URokPowerGainWidget`, plus per-screen widgets (Research, Scout, Help, Hospital, Barracks, Commander/TrainArmy/MailAttachment mesh): presentation shells only, with business logic kept in subsystems

### 4. Maps & Content (`Content/`)
Generated by `Scripts/*.py` commandlets, not hand-authored. Key maps:
- `/Game/Maps/L_RokUnityGameAppCity` — default playable preview (62 tagged actors: 10 boundary, 2 ground, 24 initial runtime buildings, 24 supplemental interior preview buildings, 1 CameraActor, 1 PlayerStart)
- `/Game/Maps/L_RokUnityRuntimeCityAllBuildings` — diagnostic all-buildings slice
- `/Game/Maps/L_RokUnityRuntimeBoundaryOnly` — boundary-only acceptance map (wall/gate/tower only)
- `/Game/Maps/L_RokCitySingleBuilding` — single-building validation (`Building_Castle_02` from `Map_TTS.unity`)

## Migration Pipeline (Scripts/)

All Unity → UE work flows through Python scripts under `Scripts/`, each producing a `Saved/RokUnityMigration/*_verify.json` report. **Run generation + verify as a pair; never hand-edit the generated JSON.**

| Pattern | Generator | Verifier |
|---|---|---|
| Initial city state | `BuildRokUnityInitialCityState.py` | — |
| Gameplay catalog | `BuildRokUnityGameplayCatalog.py` | `VerifyRokUnityGameplayCatalog.py` |
| Runtime bootstrap | `BuildRokUnityRuntimeBootstrap.py` | `VerifyRokUnityRuntimeBootstrap.py` |
| UI catalog | `BuildRokUnityUiCatalog.py` | `VerifyRokUnityUiCatalog.py` |
| City boundary | `BuildRokUnityCityBoundary.py` | `VerifyRokUnityCityBoundary.py` |
| City visual foundation | `BuildRokUnityCityVisualFoundation.py` | `VerifyRokUnityCityVisualFoundation.py` |
| City visual layer stack | `BuildRokUnityCityVisualLayerStack.py` | `VerifyRokUnityCityVisualLayerStack.py` |
| Building visual layers | `BuildRokUnityBuildingVisualLayers.py` | `VerifyRokUnityBuildingVisualLayers.py` |
| Building body/ground | `BuildRokUnityBuildingBodyGroundLayers.py` | `VerifyRokUnityBuildingBodyGroundLayers.py` |
| Building visual integration | — | `VerifyRokUnityBuildingVisualIntegration.py` |
| Runtime city all buildings | `CreateRokUnityRuntimeCityAllBuildingsScene.py` | `VerifyRokUnityRuntimeCityAllBuildingsScene.py` |
| Single building cutout | `PrepareRokSingleBuildingCutout.py` | — |
| TestUnity view architecture | `BuildRokTestUnityViewArchitecture.py` | `VerifyRokTestUnityViewArchitecture.py` |
| City placement extraction | `ExtractRokUnityCityPlacement.py` | — |

Generated output goes to `Saved/RokUnityMigration/` (cities/UI/catalog) or `Saved/RokUnityPlacement/` (placement extract).

## Key Conventions

- **C++ owns behavior**, Blueprints are presentation shells. If a Blueprint contains business logic that affects save data, timers, combat, economy, or protocol — migrate it to C++.
- **Preserve Unity IDs**: `UnityInitId_*`, `UnityBuildingType_*`, `UnityGridX/Y_*`, `UnityMesh_*`, `UnityMaterial_*`, `UnitySourcePrefab_*`, `UnityPrefabObject_*` — rename UE classes freely, but keep these tags on actors for runtime trace-back.
- **Coordinate conversion**: Unity `(x, y, z)` → UE `(-x, z, y)` for camera right alignment. Unity vertical FOV `12` ≈ UE horizontal FOV `21.17` at 16:9. UE plane UV Y-flip is needed (`ROK_SINGLE_BUILDING_ENGINE_PLANE_FLIP_Y=1`); do not fix by rotating the camera.
- **Render queues**: `CitySurface` `2992`, `CityTranslucent` `2991`, normal building cards `2995`, town center night `2996` — all `ZWrite Off`. Translucent + sort by `material_render_queue`; never opaque ground that writes depth over sprite cards.
- **Map size tuning for `/Game/Maps/L_RokUnityGameAppCity`** uses `ROK_RUNTIME_CITY_ALL_BUILDINGS_ASSET_SUFFIX=GameAppRuntimeV5Dense`, `ROK_RUNTIME_BUILDING_CARD_MODE=sprite_actor`, `ROK_RUNTIME_EXTRA_INTERIOR_BUILDINGS=24`.
- **Forbidden patterns** (caught by verification scripts): `BasicShapes` plane cards as building visuals, cone spires on reference buildings, placeholder cone/cylinder trees, coplanar ground/terrain/sprite layers, hand-placed wall ribbons, PNG cards without mask channels, OBB-faithful scale on reference FBX.
- **Do not commit**: `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `Library/`, `Logs/`, `Temp/`, `.vs/`.
- **Decorative fallback rule**: type 52/53 must not fall back to `build_51_101_1_*` sprites; missing decoration rows stay recorded in `unity_gameapp_city_init_building_resolution.json`.

## Reference Material Layout (`ref/`)

- `ref/client-unity` — Unity client source (Hotfix/MVC mediators, prefabs, materials, scenes). **For city selection arrows**: `CityGlobalMediator.cs`, `building_move_arrow.prefab`, `building_move_bottom.prefab`, `customcitybuilding_move_arrow+21.shader`, `ui_hud` atlas (`Arrow_up/right/down/left`).
- `ref/server` — backend authority boundaries, protocol shape, admin/web.
- `ref/resources` — sprites, masks, atlases, terrain textures. **Mask semantics first**: `*_mask.png` red channel = cutout; full-alpha PNG ≠ cutout.
- `ref/config-tables` — buildings, resources, heroes, troops, technologies, buffs, timers, costs.

When changing reference-derived behavior, cite the `ref/` source path that informed the decision.

## Acceptance Gates

A migration batch is **not** accepted just because it compiles. It must:

1. Load in the editor or commandlet path.
2. Pass its `Verify*.py` script with `failures=[]`.
3. Produce visual evidence (screenshot) for scene/UI/material/camera changes, side-by-side against the Unity source.
4. For data slices: pass table import validation + deterministic local-sim checks where practical, and clearly mark values that are still UE bootstrap placeholders.
5. For UI slices: open through the real runtime path, not just isolated widget construction.

See `AGENTS.md` for the full current state per slice (prototype scene, city base visual restoration, initial city state, gameplay catalog, runtime bootstrap, UI catalog, city wall, visual layer stack, building visual layers, view architecture).
