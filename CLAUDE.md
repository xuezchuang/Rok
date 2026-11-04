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

## Start Here: Current Plan (2026-10-18; status 2026-10-31)

**Read `UnityLearning/MIGRATION_PLAN_2026-10-18.md` first.** It's the active plan. It has:
- the verified state, with a 2026-10-31 snapshot at the top;
- the Unity-vs-UE gap table from the first side-by-side capture;
- Phases 1–6 (capture baseline → camera/framing → HUD correctness → city completeness → interaction + UI animation → breadth) and a progress table;
- the Unity reference capture procedure (view-only, ask before computer use).

The Codex open work below stays **parked** while Phases 5–6 have visible work left.

**Status on 2026-10-31.**
- **Phases:**
  - Phases 1–3 are done; the one exception (#10, one card per task) is a data gap.
  - Phase 4 is blocked: #7 needs a settled zoomed-out Unity shot, #8 the ground pipeline.
  - Phase 5 is mostly done.
  - Phase 6 is in progress.
- **Tests:** `Rok.*` has 221 tests: 190 pass, 25 pass with warnings, 6 fail. The 6 failures are the 2026-10-18 baseline set and are still untriaged (`Saved/ClaudeCapture/type18_full_automation`, 2026-10-31).
- **Repo:** `main` is at `cbcb8c91`, one commit ahead of `origin` and not pushed. P4 is unchanged since CL 110.
- **2026-10-31, uncommitted:** the commander hall (统帅厅, type 18) builds locally through the BuildCity flow, using an exported Unity tight `Sprite.mesh`. `FRokLocalConstructionVisualFactory::FindSourceVisual` is the per-type table for constructible source visuals. Its 4 new assets in `Content/RokUnity/City/SourceBuildings/build_18_101_1/` are not in P4. Details are in the plan's Phase 6 log.

**Phase 3 (HUD correctness) landed on 2026-10-25, except one-card-per-task.** The city HUD now follows the camera dxf the way `MainInterfaceMediator`/`OpenLodUi` do: at or below `city_bound` (250) the coordinate box is hidden and the hammer + world globe are shown; above it the box appears and the pair flips to the magnifier + castle (`URokMainHudWidget::ApplyCityHudBand`, fed by the pawn's dxf each tick; castle calls `ARokStrategyCameraPawn::FrameCityCenter()`). `rok.DebugBuildingGrid` defaults to 0 in every build and only placement/move force the selected footprint. The `本地预览` city marker is diagnostic-only (`-RokShowCityMarker`), the army-queue entry stays collapsed without a march snapshot, and the world-map nameplate anchors at the city card's visual centre. Panning now moves the zoom target, so a wheel notch no longer snaps the camera back to the city centre. One card per task is deferred: only the active task has runtime state.

**Phase 2 (camera and framing) landed on 2026-10-25.** The city opens at dxf 150 (`city_default`); one wheel notch multiplies dxf by 10/9 out and 10/11 in (`InputManager.cs:151`) with no `DeltaSeconds`, so it is frame-rate independent; the interactive clamp is 82.4551–250 (`min`…`city_bound`) from `MakeDefaultCameraKeys()`; the camera fills any window at 7.5° vertical FOV (`bConstrainAspectRatio=false`, `AspectRatio_MaintainYFOV`). `-RokCameraDistance=` is now read once in `BeginPlay` and accepted verbatim as a capture override (it bypasses the city clamp, e.g. dxf 313.6); `RokCityEntryDxf` stays clamped. Every run logs `Rok city camera initialized: distance=… dxf=… vertical_fov=… aspect=… constrain_aspect=0 axis=0`. Capture with `Saved/ClaudeCapture/tools/capture_ue.ps1` (PowerShell; Git Bash corrupts `/Game/...` into `C:/Program Files/Git/Game/...`) and compare with `pair.py`. Tests: `Rok.WorldMap.City.{Framing,WheelZoom,ZoomClamp,AspectFraming}`.

**Phase 5 started 2026-10-25: U4 and U5 parity.** The town hall's ring menu now shows all three source actions; the third one (`BuildingMenuData[129] → openUI 1001 → UI.s_GainEffect`) opens the new `URokUnityModalWidget::OpenGainEffect(tab)` window (`UI_Win_GainEffect`): 经济/军事 tabs, economy rows from `city_buff_reference.json`, military tab on its server boundary, footer "静态配置预览 · 非实时/未激活". Capture it with `-RokOpenModal=gain_effect` / `gain_effect_military`. The upgrade window was already verified (`-RokAutoSelectBuilding=UnityInitId_5 -RokOpenModal=building_upgrade`). The tab plates now have a selected/dim state. Animations are runtime-sampled with these flags:
- `-RokModalAnimationSample=` (window open curve);
- `-RokBuildingMenuSample=` (ring menu);
- `-RokTaskFoldSample=` / `-RokFeatureFoldSample=` (HUD folds).

Still open:
- the source tab sprite;
- a Unity reference capture of the gain-effect window;
- a runtime sample of `UA_Chat_*`;
- the ring menu renders about 10% smaller than Unity's (measure before changing it).

**Phase 6 (breadth) has been in progress since 2026-10-25.** `URokUnityModalWidget` has 19 new `UI_Win_*` windows, and each opens with its own `-RokOpenModal=` flag. The developer gallery (`-RokOpenModal=ui_gallery`, or F10) has 19 slots: the gain-effect window plus 18 of these. `Vip` (`vip_privilege`) has no slot yet. They are built on shared pieces:
- text: `FRokUiTextTable` (Language.csv ids, rich-text sanitizing);
- tables: `FRokCsv::ParseRecords` (quoted newlines);
- icons: `FRokSpriteAtlas` (Unity `.tpsheet` rects are bottom-origin);
- layout: `BuildDefinitionList` (scrolling list or grid, search box, chip filters);
- framing: a fixed 1456x910 modal canvas, centred at any aspect.

Read `UnityLearning/UI_PORTED_PARITY_2026-10-25.md` for status. Against the prefab structure: 3 match, 14 partial, 1 mismatch (`FuncGuide`, which is an overlay in Unity), 1 unknown. "Ported" means *data window*, not *screen parity*. `UnityLearning/UI_GAP_BACKLOG_2026-10-25.md` lists the 97 of 156 `UI_Win_*` prefabs that still have no counterpart.

City life (#9): the `建造 m:ss` construction bubble is done. Wandering citizens (`-RokCityCitizens`) stay opt-in until the worker sheet gets a single-cell material.

## Background: Codex → Claude handoff (2026-10-16)

Codex ran this migration until 2026-10-16 and stopped at the user's request. Before resuming any of its slices, read `Saved/RokUnityMigration/ClaudeHandoff_20261016T152648077944Z/HANDOFF.md` (Chinese) and its `EVIDENCE.json` / `MANIFEST.json` SHA-256 pins. The migration is **not** complete.

**Cleanup done on 2026-10-17 (Phases 0, 1 and 3 of the cleanup plan).**
- **Code promoted.** Codex built Run1–Run30 in a byte-copied shadow project, `Saved/UIB1/`. Its Source/Shaders deltas (31 new + 13 updated files, 2 shaders) are now in `main` (commit `3da3b629`). `RokEditor` builds with 0 errors. Runtime and visual acceptance have **not** been run on the live project.
- **What's left in `Saved/UIB1/`:** `Content/`, `Source/`, `Shaders/`, `Config/`, `Scripts/`, `ref/`, `Rok.uproject`.
  - `Content/` matches live `Content/` plus 8 Grass3 diagnostic assets (e.g. `M_WorldSource_Grass3SignedCenterV5`, used by `RokGrass3SameUvAuditSubsystem`).
  - **Never copy UIB1 `Config/` back**: it disables MCP auto-start and trims ini files.
- **Evidence archived to `F:\RokArchive\`:**
  - Tars: `rokunitymigration-evidence`, `pending`, `uib1-records`, `uph-receipts`, `logs-older-than-2026-10-03`.
  - Each tar has a `*.manifest.csv` (path, bytes, SHA-256) and was verified by re-reading. `INDEX.csv` lists every archived top-level entry with its `READY.json` status.
  - A copy of the Claude handoff folder is there too.
  - To restore a dir, extract it from the tar and check its SHA against the manifest.
- **Kept in `Saved/`:**
  - `RokUnityMigration` entries referenced by `Source/`, `Scripts/`, UnityLearning or the docs.
  - The handoff folder.
  - `Unity*Probe` outputs (Library/Temp caches removed).
  - Recent logs.
  - `PendingUnityTileRippleRendererV8` and `PendingUnityTileRippleSimulationV2`: product code (`RokUnityTileRippleRenderer.cpp`) reads them.
- **Deleted outright:** Unity project clones (UPH2–6, Pending HUD capture), `RefBackups`, loose `.tmp` files, UIB1 build caches.

**Consequences of the archive.**
- The handoff's frozen runners pin paths and hashes under `Saved/Pending*` and `Saved/UIB1/Run*`, so those preflights now fail by design. The "resume exactly where Codex stopped" route is closed; redo runtime and visual acceptance in the live layout instead.
- About 29 audit subsystems and tests hard-code archived `Saved/Pending*` / `UIB1/Run*` receipt paths. They fail provenance checks unless those files are restored. Product code is unaffected (checked 2026-10-17).

**Last verified checkpoint (historical).** The Run30 repair build of UIB1 passed independent review. Its receipt (SHA `ac0604ac…`) is now in `pending.tar` under `PendingRootPowerMapRun30RepairBuildV1/`. After the repair, no runtime or visual acceptance ran.

**Open work carried over from the handoff** (parked by the 2026-10-18 plan; candidate files are in `pending.tar`):
1. UI/Power runtime adapter (`PendingRootRun30UiPowerRuntimeAdapterV1`): five modes (`mail/local_fixture`, `mail/ordinary_unbound`, `shop`, `ranking`, `power`). Check real PNGs. Power keeps 17 captures and the manual-close boundary.
2. Grass3 same-UV GPU runner (`PendingRootRokGrass3SameUvActualRunnerV2`): was on HOLD. It's a direct RHI texture probe, not material or full-map visual acceptance.
3. Unity offline HUD V3 (`PendingRootUnityPowerHudOfflineIsolationV3`): fix the `GrayChildrens.LoadGrayMaterial` NRE with a key-scoped local `IAssetService` adapter for `UI_GrayWithMask`. Do not disable `GrayChildrens`, edit the prefab, or start GameApp/real services.
4. Daily Activity Reward V4 and runtime audit V2: unreviewed candidates, not in Source.
- Blocked: Academy / age 2 (source map context and resource chain gaps). Do not approximate terrain to unblock it.

**Evidence conventions (keep them).**
- `READY.json` = a frozen candidate passed its own gates. It does **not** mean accepted.
- Verifier PASS ≠ visual parity: always inspect the PNG.
- Don't rewrite failed history. Historical Stage `88ebb11b…`, adopted inner `9da47986…` and outer `392d7451…` stay false.

**Junction hazard.** Nine folders under `UnityLearning/*/Assets/` are NTFS junctions: `BundleAssets`, `BundleAssetsTest`, `Mesh`, `Resources`, `Shader`, `T4M`, `T4MOBJ` point into `ref/client-unity/Assets/`, and `RokUnityMigration` points into `Saved/RokUnityMigration`. They're listed in `.gitignore`. Remove a junction only with `cmd /c rmdir <path>` (or PowerShell `(Get-Item <path>).Delete()`). **Never use `rm -rf` or a recursive delete**: it deletes the real `ref/` / `Saved/` data behind the link. The Unity project also writes `.meta` files through the `RokUnityMigration` junction into `Saved/RokUnityMigration`.

**Account / server boundary.** No real accounts, logins, reward claims, inventory actions, or server writes. UI, power and reward data are explicitly local fixtures/previews. Never copy auth payloads into reports. One exception, which the user asked for on 2026-10-18: the original Unity client may auto-login to the user's own server for **view-only** reference screenshots (see the plan's section 4).

**UI coverage** (`Saved/RokUnityMigration/unity_ui_runtime_coverage_verify.json`, 2026-09-12): 46 mappings / 49 entries checked, 1105 of 1150 Unity UI prefabs still unimplemented. The verifier only proves that declarations exist, and it has not been re-run since. The window-level view is newer: on 2026-10-31, 97 of 156 `UI_Win_*` prefabs had no UE counterpart (`UnityLearning/UI_GAP_BACKLOG_2026-10-25.md`; parity in `UI_PORTED_PARITY_2026-10-25.md`). Earlier UI handoff: `UnityLearning/UI_MIGRATION_HANDOFF_2026-08-14.md`.

## Version Control: Git + Perforce (verified 2026-10-31)

| | Git | Perforce |
|---|---|---|
| Server | `origin` `https://github.com/xuezchuang/Rok.git`, branch `main` | `home.snowsome.com:1666`, user `snow`, client `ueDemo`, root `D:\ueDemo`, view `//depot/... //ueDemo/...` |
| Owns | Text: `Source/`, `Shaders/`, `Scripts/`, `Config/`, `Rok.uproject`, `AGENTS.md`, `CLAUDE.md`, `UnityLearning/` notes | Binary content, runtime data and reference snapshot: `//depot/Rok/Content/...` (~2.95k files, incl. `Content/RokData`), `//depot/Rok/ref/...` (~74k files) |
| Latest (2026-10-31) | `main` at `cbcb8c91` (plan Phases 2–6: camera, city HUD band, UI window ports, tests, docs), **one commit ahead of `origin/main` and not pushed**. Before it, `88818c95`/`b59d48c9` were pushed: the Jul–Oct WIP checkpoint, the UIB1 promotion, and Phases 2a/2b (runtime data moved to `Content/RokData`, runtime provenance gates removed). Only real UnityLearning project files remain untracked | Last submit CL 110 (2026-10-18): the 639 `Content/RokData` files, all filetype `binary`. The 142 opened Content files (137 add / 5 edit) are **shelved in CL 80**, still open locally, not submitted |

- **Add `Content/RokData` files as `binary`** (`p4 add -t binary`). They are LF-only JSON/CSV whose bytes C++ loaders pin by SHA-256; the client uses `LineEnd: local` and the typemap covers only `.uasset`/`.umap`, so a `text` add comes back CRLF on sync and fails those pins.
- **The client's default changelist is not Rok's.** It holds other projects' open files (`//depot/RPG`, `//depot/tx`, 347 on 2026-10-18). Put Rok adds in a numbered changelist and never submit the default one.
- **P4 holds stale text copies.** `//depot/Rok/Source` (6 files), `Scripts` (16), `Config` (2) and `AGENTS.md` are May 2026 snapshots (CL 46–49), and every one differs from the git copy. Never `p4 sync -f`, "Get Revision (force)", or `p4 reconcile` over those paths: git is authoritative for text.
- Never `git reset --hard`, `git clean`, `git checkout --`, or bulk revert/submit in P4. Commit or submit only when the user asks. `git clean` is doubly dangerous here: the UnityLearning junctions (see the junction hazard above) lead into `ref/` and `Saved/`.
- **P4 non-ASCII names:** this server stores Chinese and other non-ASCII path characters as `?` (e.g. `//depot/Rok/ref/config-tables/???????/…`). Content is intact, but names are lossy and may collide. Don't rely on P4 for restoring non-ASCII-named `ref/` files.
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
