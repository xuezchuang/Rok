# Global Agent Rules

## Unreal MCP Target Safety

This workstation often has multiple Unreal Editor instances open at the same time. Never assume an MCP tool name maps to the intended project.

This project (`D:\ueDemo\Rok`) no longer uses the legacy McpAutomationBridge WebSocket transport (removed in git `7b2bfce4`). It uses UE 5.8's built-in `ModelContextProtocol` plugin over HTTP at `http://127.0.0.1:8010/mcp` (`Config/DefaultEditorPerProjectUserSettings.ini`). The shadow compile project `Saved/UIB1` sets `bAutoStartServer=False` so it never competes for port 8010.

Known fixed legacy MCP WebSocket listen ports for the other workstation projects (not this repo):

- `D:\ueDemo\MMOARPG`: `127.0.0.1:8090`
- `D:\ueasset\RPG Metanoia Project v12.8`: `127.0.0.1:8092`
- `D:\ueasset\Rok`: `127.0.0.1:8094`
- `D:\ueasset\Easy Survival RPG v5\EasySurvivalRPGv5`: `127.0.0.1:8096`
- `D:\ueasset\Easy Survival RPG v5\EasySurvivalRPGv5Cpp`: `127.0.0.1:8098`

Legacy-bridge projects' `DefaultGame.ini` files should keep `bMultiListen=False`, one `ListenPorts` value, matching `ClientPort`, and loopback-only `ListenHost=127.0.0.1`.

Before any MCP write operation, first run a read-only identity check against the MCP target:

```python
import unreal
print(unreal.Paths.project_dir())
print(unreal.EditorLevelLibrary.get_editor_world().get_name())
```

Proceed only when the returned project directory exactly matches the user-requested target project. If multiple UE windows are open and the MCP target is ambiguous, stop and report the active UE processes and ports instead of executing a write.

Do not use `mcp__unreal_engine` or `mcp__unreal_engine_metanoia__` for writes just because the tool name looks relevant. The tool must prove its project path first in the current turn.

If the wrong project is detected, do not save, spawn actors, modify assets, run Python scripts, or issue console commands through that MCP connection.

--- project-doc ---
# Rok UE5.8 Project Instructions

## Project Purpose

This repository is a UE 5.8 C++ project for building a strategy/MMO game in the style of "Rise of Kingdoms" / "万国觉醒": city construction, world map marching, heroes, troops, alliances, timed progression, battle resolution, and server-authoritative multiplayer systems.

The extracted reference material lives under `ref/`. Treat it as reference input for understanding feature shape, protocol boundaries, data tables, UI flows, and server/client responsibilities. Do not turn the project into a direct source dump; rebuild gameplay as UE-native systems with clean C++ foundations and Blueprint assets used mainly for presentation and editor-authored content.

## Current Initialization

- Engine target: UE 5.8.
- On this workstation, the UE 5.8 install for this project is `D:\ue\UE_5.8`. Use `D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe`, `UnrealEditor-Cmd.exe`, and `Engine\Build\BatchFiles\Build.bat` for editor launches, commandlets, map saves, and builds.
- Do not use older UE installs such as `D:\ue\UE_5.2` or `C:\UnrealEngine` for this project. Saving `.umap`/assets with the wrong engine can make content incompatible with the required UE 5.8 project.
- Project root: this repository root, the directory containing `Rok.uproject`.
- Main project file: `Rok.uproject`.
- Runtime module: `Rok`.
- Editor automation uses UE 5.8's built-in experimental `ModelContextProtocol` plugin. Its project defaults live in `Config/DefaultEditorPerProjectUserSettings.ini` and use loopback port `8010`.
- Reference sources: `ref/`, normalized after extraction:
  - `ref/client-unity`: Unity client source and assets for UI, gameplay flow, hotfix/package layout, and client-side system reference.
  - `ref/server`: server source and web/admin material for backend responsibility and protocol/system boundaries.
  - `ref/resources`: extracted art/resource material for visual and content reference.
  - `ref/config-tables`: generated config pipeline, spreadsheets, CSV/config output, and table naming reference.

## Working Rules

- Prefer small, staged UE-native systems over broad ports.
- Keep durable architecture decisions in this file as the project evolves.
- Keep C++ as the source of gameplay/framework behavior; use Blueprints for asset shells, layout, tuning, and visual scripting hooks where it improves iteration.
- When using MCP automation, keep generated Blueprint/content work reproducible through notes or scripts where practical.
- Before changing reference-derived behavior, identify which `ref` folder/file informed the decision.
- Avoid committing generated UE folders such as `Binaries/`, `Intermediate/`, `Saved/`, and `DerivedDataCache/`.

## Version Control and Workspace State (verified 2026-10-17)

- Git (`origin` `https://github.com/xuezchuang/Rok.git`, `main`) owns text: `Source/`, `Shaders/`, `Scripts/`, `Config/`, `Rok.uproject`, `AGENTS.md`, `CLAUDE.md`, `UnityLearning/` notes. Perforce (`home.snowsome.com:1666`, client `ueDemo`, root `D:\ueDemo`) owns binary content and the reference snapshot: `//depot/Rok/Content/...` and `//depot/Rok/ref/...`.
- `//depot/Rok/Source`, `Scripts`, `Config`, and `AGENTS.md` in P4 are stale May 2026 snapshots (CL 46-49) that differ from the git copies. Never force-sync or reconcile those paths from P4; git is authoritative for text.
- State on 2026-10-17: `main` at `3da3b629`, pushed. It holds the Jul-Oct WIP checkpoint and the promoted `Saved/UIB1` shadow code (31 new + 13 updated Source files, 2 shaders). The 142 opened P4 Content files are shelved in CL 80, not submitted. Do not `git reset --hard`, `git clean`, `git checkout --`, or bulk revert/submit P4. Commit/submit only on explicit user request.
- Work in the live tree, not a shadow copy. `Saved/UIB1` now keeps only `Content/` (8 extra Grass3 diagnostic assets), `Source/`, `Shaders/`, `Config/` and `Rok.uproject`. Never copy its `Config/` back.
- Codex evidence (`Saved/Pending*`, UIB1 `Run*` receipts, UPH receipts, `RokUnityMigration` evidence-only entries, old logs) is archived in verified tars under `F:\RokArchive\`, each with a `*.manifest.csv` plus `INDEX.csv`. Exception: `PendingUnityTileRippleRendererV8` and `PendingUnityTileRippleSimulationV2` stay because product code reads them. Handoff: `Saved/RokUnityMigration/ClaudeHandoff_20261016T152648077944Z/HANDOFF.md`. Its frozen runners no longer pass preflight; `CLAUDE.md` describes the current resume path.
- `UnityLearning/*/Assets/{BundleAssets,BundleAssetsTest,Mesh,Resources,Shader,T4M,T4MOBJ,RokUnityMigration}` are NTFS junctions into `ref/` and `Saved/`. Remove them only with `rmdir`, never with a recursive delete.

## Initial Technical Direction

- Start with a playable prototype loop before heavy content migration: camera, world map pawn/controller, city screen shell, resource counters, timed build queue, troop march command, and one server-shaped simulation boundary.
- Model data with UE-friendly assets first: DataAssets/DataTables for buildings, troops, heroes, skills, resources, technologies, and buffs.
- Keep network/server work behind clear interfaces so the project can later connect to a dedicated backend or local simulation without rewriting UI/gameplay.
- Build repeated Blueprint, asset, and level-generation workflows through the UE 5.8 built-in `ModelContextProtocol` toolsets.

## Full Unity-to-UE Migration Plan

- Treat the Unity project as the executable reference, not as code to dump directly into UE. The migration goal is to preserve gameplay behavior, data identity, visual composition, UI flow, and protocol semantics while rebuilding them as UE-native C++ systems, DataAssets/DataTables, UMG widgets, and Blueprint presentation shells.
- Every migrated feature must keep a traceable source chain: Unity source path, extracted intermediate data, UE target class/asset/map, and the verification command or screenshot used to accept it. Put hand-authored migration notes in repo docs; put generated manifests under `Saved/RokUnityMigration/` or the existing `Saved/RokUnityPlacement/` area and regenerate them instead of editing them by hand.
- Do not attempt a one-shot whole-project port. Migrate in playable vertical slices that leave the UE project openable and runnable after each batch.
- Preserve stable Unity IDs and table keys when they represent product data. Rename UE classes and assets only at the UE architecture boundary, not inside imported data rows where server/config compatibility matters.
### Dual Unity + UE Learning Workflow

- Treat `D:\ueDemo\Rok-unity` as the live Unity learning/source project, and treat this repository's `ref/` folder as the normalized in-repo snapshot refreshed from that Unity project when explicitly requested.
- When refreshing `ref/` from `D:\ueDemo\Rok-unity`, copy `client-unity`, `server`, `resources`, and `config-tables`, but do not copy Unity-generated local caches such as `Library/`, `Logs/`, `Temp/`, `Obj/`, `.vs/`, or build outputs.
- Use `UnityLearning/` in this repository for paired Unity-side learning notes, small experiments, and per-slice comparison records. Do not put Unity generated caches or packaged builds in that folder.
- For every new migration slice, work both sides together: identify or implement the Unity behavior first, then implement the UE counterpart, and record the Unity source path, Unity experiment path, UE target class/asset/map, and the observed Unity-versus-UE difference.
- Do not migrate by blindly copying Unity runtime code into UE. Keep the Unity side as the executable reference and learning implementation, while rebuilding the UE side with C++, UMG, DataAssets/DataTables, and reproducible editor automation.
- When a slice changes both sides, keep paired acceptance evidence under `Saved/RokUnityMigration/` or in a slice note under `UnityLearning/`, including the exact Unity scene/prefab/config and UE script/map touched.

### Source Inventory

- `ref/client-unity` is the source for client bootstrap, scene flow, UI layout, prefabs, materials, shaders, animations, address/package references, and client-only interaction behavior.
- `ref/resources` is the source for extracted sprites, masks, atlases, effects, icons, terrain textures, and other visual resources. Mask and shader channel semantics must be inspected before judging transparency or cutout behavior.
- `ref/config-tables` is the source for buildings, resources, heroes, troops, technologies, buffs, timers, costs, unlock rules, and localization/config naming. These become UE DataTables/DataAssets through repeatable import scripts.
- `ref/server` is the source for authority boundaries, protocol names, request/response shape, validation responsibility, admin/web feature expectations, and later dedicated-backend integration.

### Migration Order

- First restore the visual foundation: Unity camera rules, coordinate conversion, sorting/depth rules, terrain/city-ground layers, sprite mask semantics, reference mesh scale/pivots, and scene placement generated from Unity scenes/prefabs/configs.
- Then build the UE runtime shell: GameInstance/bootstrap, local simulation boundary, map/scene transition service, asset lookup service, strategy camera pawn/controller, UI root, resource HUD, and a minimal city screen.
- Then migrate core game systems in C++ behind clear interfaces: resources, buildings, construction queues, speedups, heroes, troops, march commands, world-map entities, alliance placeholders, technology, mail/notice stubs, and battle-resolution adapters.
- Then migrate UI screens feature by feature. UMG should own layout and presentation; C++ should own state, validation, timers, table lookup, and event routing. Do not port Unity MonoBehaviour/UI scripts as large Blueprint graphs.
- Finally connect server-shaped behavior. Local simulation should share the same command/query interfaces that a dedicated backend integration will use later, so replacing local simulation with network calls does not rewrite UI/gameplay code.

### UE Architecture Targets

- Use C++ subsystems or manager classes for durable behavior: resource state, build queues, city state, world map state, march state, data registry, asset reference mapping, and protocol/local-simulation adapters.
- Use actors/components for world presentation: strategy camera pawn, city sprite/mesh actors, placement components, march visual actors, selectable world objects, and diagnostic reference layers.
- Use DataTables/DataAssets for config-shaped content. Generated importers should validate row counts, key stability, enum compatibility, icon/sprite references, and missing localization.
- Keep Blueprints as shells for layout, tuning, animation hooks, material instances, and editor-authored presentation. If a Blueprint contains business logic that can affect save data, timers, combat, economy, or protocol behavior, migrate that logic to C++.

### Visual Migration Rules

- City and world placement must be derived from Unity scenes, prefabs, transforms, sorting layers, and config tables. Do not hand-place city buildings for visual matching except in explicitly named diagnostic maps.
- Preserve Unity-to-UE coordinate and camera semantics. Unity vertical FOV, screen handedness, sprite flip state, and render order must be converted intentionally instead of adjusted by viewport guesswork.
- Rebuild the Unity material/shader pipeline before widening content. For city visuals this means separating terrain, city surface, road, shadow/decal, ground patch, and building body layers when the Unity source does so.
- Do not use visible PNG rectangles, BasicShapes card fields, cone/cylinder trees, fake spires, or other placeholders as accepted final city visuals. Diagnostic placeholders may exist only in clearly named scratch maps and must be blocked from promoted maps by verification scripts.
- Current single-building validation remains pinned to `/Game/Maps/L_RokCitySingleBuilding` until `Building_Castle_02` matches the Unity source well enough. Broader GameApp/city scenes can be generated only after their source extraction and visual layer rules are reproducible.

### Required Automation

- Extraction scripts should parse Unity scenes, prefabs, sprite metadata, material/shader references, config CSVs, and server/protocol tables into explicit JSON/CSV manifests.
- UE creation scripts should import or update assets, create maps, spawn actors, assign materials, set camera/culling/sorting values, and write reproducible acceptance metadata.
- Verification scripts should reject known bad patterns: wrong map, missing camera, missing tagged source actors, coplanar layer fighting, placeholder meshes in promoted maps, missing source metadata, rectangular card artifacts where a cutout/mask is expected, and unvalidated reference FBX placement.
- Before any MCP write, prove the MCP target project path and current world exactly match this project and intended map. If identity is ambiguous, report processes/ports instead of modifying the editor.

### Acceptance Gates

- A migration batch is not accepted just because it compiles. It must also load in the editor or commandlet path, pass its verification script, and produce visual evidence when the change affects scenes, UI, materials, or camera.
- For visual work, keep side-by-side evidence against the Unity Game view or a Unity-source-derived reference image. If the UE result still reads as flat cards, wrong layer order, mirrored scene, wrong scale, or random terrain clutter, the batch is diagnostic only.
- For gameplay/data work, acceptance requires table import validation, deterministic local-simulation tests where practical, and a clear boundary showing which behavior is still stubbed.
- For UI migration, acceptance requires the target widget/screen opening through the real UE runtime path, not only being constructible as an isolated widget asset.

## Prototype Scene

- First scene: `/Game/Maps/L_RokUnityGameAppCity` is the default startup map (`EditorStartupMap`/`GameDefaultMap` in `Config/DefaultEngine.ini`, changed 2026-08-14) because it hosts the migrated runtime city and all UI slices. `/Game/Maps/L_RokPrototype_Lit` remains the older prototype baseline, and `/Game/Maps/L_RokUnityWorldMapPrototype` is the world-map slice map.
- Setup script: `Scripts/CreateRokPrototypeScene.py`.
- Lighting adjustment script for the current lit map: `Scripts/CreateRokPrototypeLitMap.py`.
- Verification script: `Scripts/VerifyRokPrototypeScene.py`.
- Imported reference meshes under `/Game/RokPrototype/Meshes` from `ref/client-unity/Assets/BundleAssets/Map`, `ref/client-unity/Assets/BundleAssets/land`, and related Unity client folders.
- Imported reference textures under `/Game/RokPrototype/Textures` from Unity land/building texture folders.
- The current prototype scene uses validated reference FBX models for the main visual anchors: `OriginalCityWall_101_1`, `OriginalCityGate_101_1`, `OriginalCityTower_101_1_*`, `OriginalTownCenter_CheckpointBig`, `OriginalMountain_*`, `OriginalBridge_Route`, and `OriginalAllianceFlag_Icon1`. UE-native blockout geometry remains only for terrain, roads, secondary buildings, resource markers, and fallback coverage.
- The active visual city layer reconstructs `ref/client-unity/Assets/BundleAssets/Map/Scenes/Map_TTS.unity` placement as UE volumetric layout actors named `RokLayout_*`. Do not use visible BasicShapes plane cards with building textures as the main scene representation; they read as texture sheets rather than RoK scene assets.
- Sprite PNGs under `/Game/RokPrototype/Textures/ReferenceSprites` and their materials are reference-only for future asset matching or cutout work unless a real masked/volumetric presentation path is added.
- Do not layer fake cone spires or triangular caps onto real/reference city buildings. `TownCenter_Spire`, `*_Spire`, and city-layer `/Engine/BasicShapes/Cone` placeholders are blocked by `Scripts/VerifyRokPrototypeScene.py`.
- Do not use cone/cylinder placeholder trees in the active terrain/resource tree layers. `WorldTree_*`, `*_Canopy`, and tree-layer `/Engine/BasicShapes/Cone` placeholders are blocked; use `OriginalWorldTreePatch_*` and `OriginalWoodGrove_*` reference mesh patches instead.
- Import PNG sprite assets with `TextureFactory` in `Scripts/CreateRokPrototypeScene.py`; the Interchange PNG import path has asserted in older MCP callback workflows and should be revalidated before replacing this route.
- Do not place imported Unity FBX assets directly into the active prototype map until their pivots, bounds, materials, and scale are validated. When using large reference meshes, place them through bounds-origin/bottom compensation so their visible geometry lands at the intended world position; unstable FBX placement can create floating fragments and non-RoK scene clutter.

## City Base Visual Restoration Gate

- Current priority for `/Game/Maps/L_RokCityRuntime` and related diagnostic maps is the RoK city-base visual match, not more camera tweaking, UI work, unit movement, or interaction expansion.
- The 2.5D runtime route is validated enough to continue: perspective `RokStrategyCameraPawn`, `ARokCitySpriteActor`, and camera-facing city sprites can render in-game without the old tilted-paper failure. Mouse edge scrolling should remain disabled by default while visual validation is in progress.
- Do not keep trying to solve the remaining city-base mismatch by only changing camera zoom, actor spacing, translucent alpha, or simple green-screen/flood-fill cutouts. Those attempts produced PNG-card / rectangular-tile artifacts and do not reach the target look.
- Do not hand-place city buildings for visual matching. City placement must be derived from Unity source data first, then applied to UE.
- The current placement extraction entrypoint is `Scripts/ExtractRokUnityCityPlacement.py`. It reads Unity scenes under `ref/client-unity/Assets/BundleAssets/Map/Scenes/` plus config CSVs under `ref/config-tables/编译客户端配置/CSV/`, and writes transient outputs under `Saved/RokUnityPlacement/`.
- Treat `Saved/RokUnityPlacement/unity_city_placement.json`, `Saved/RokUnityPlacement/unity_city_placement.csv`, and `Saved/RokUnityPlacement/unity_init_building_grid.csv` as generated evidence for the next UE map-generation step. Re-run the extractor instead of editing those files by hand.
- MCP automation may be used to create or update UE maps from parsed placement data, but MCP/editor viewport positions are not the source of truth.
- Old screenshots and derived PNGs under `Saved/` are scratch diagnostics only. Do not use them as authoritative placement, layering, or visual-match evidence after the Unity placement extractor exists.
- Before the next serious visual iteration, inspect the Unity reference rendering path and identify the real material/shader/layer rules for city sprites:
  - how `ref/resources/Sprite/*_mask.png` channels are used;
  - whether building sprites are intentionally combined with base tiles, shadows, grass edges, or decals;
  - whether separate floor, road, ground, shadow, decal, and building-body layers exist under `ref/client-unity` or `ref/resources`;
  - how Unity blends sprite ground edges into the city terrain.
- The next implementation target is to reproduce that resource-layer pipeline in UE: ground/floor/road/shadow/building layers must be generated from the correct source assets and shader semantics. A scene should not be considered visually improved just because it has more sprite actors if the result still reads as rectangular PNG cards.

### Confirmed Unity Single-Building Sprite Rules

- `ref/client-unity/Assets/BundleAssets/Map/Scenes/Map_TTS.unity` is the source of truth for the current single-building visual check.
- The `Map_TTS.unity` `Main Camera` is a perspective camera, not a top/orthographic camera: Unity position `(0.52, 15.82, -16.37)`, rotation X `45`, vertical field of view `12`, `orthographic: 0`.
- Unity `Camera.field of view` is vertical FOV. UE `CameraComponent.field_of_view` is horizontal FOV by default, so do not copy the value `12` directly into UE unless the UE camera is explicitly constrained to maintain Y/vertical FOV. For a 16:9 preview, Unity vertical FOV `12` corresponds to UE horizontal FOV about `21.17`.
- Preserve Unity screen handedness when converting positions and directions into UE. For this single-building camera, Unity screen-right is `+X`; after conversion it must align with UE camera right `(-1, 0, 0)`, so the current scene conversion is `(Unity x,y,z) -> (UE -x,z,y)`. The earlier `(x,z,y)` mapping matched camera forward/up but mirrored screen-left/screen-right at runtime.
- `Building_Castle_02` in `Map_TTS.unity` is a `SpriteRenderer` with default sprite material `{fileID: 10754}`, transform position `(-0.104, 0.334, 0.079)`, scale about `0.240767`, rotation X `45`, and sprite guid `e6124bd7f7a6dbd45a1fb2d9370d86a5`, mapped to `ref/resources/Sprite/Castle_6_5.png`.
- `ref/resources/Sprite/Castle_6_5.png` has fully opaque alpha; the transparent/cutout behavior is not carried by PNG alpha. Do not judge this sprite path by PNG alpha alone.
- For one-building diagnostics, generate a clean main-body cutout with `Scripts/PrepareRokSingleBuildingCutout.py` before rebuilding the UE map. The script uses the red channel of `ref/resources/Sprite/*_mask.png`, keeps the largest mask component plus nearby disconnected components inside the same owned visual island, fills enclosed mask holes, and writes `Saved/RokUnityPlacement/clean/<sprite>_main_clean_cropped.png` plus metadata. The raw masks contain disconnected components from neighboring scene content; using the full mask directly recreates dirty runtime images, while keeping only the largest component can drop legitimate disconnected roof/tower/detail pieces.
- The same cutout script may emit `Saved/RokUnityPlacement/clean/<sprite>_ground_patch.png` and `<sprite>_terrain_plate.png` for one-building diagnostics. The ground patch is only terrain-colored pixels inside the owned sprite island. The terrain plate is a temporary visual underlay generated from Unity grass texture material; it is useful for judging building/ground composition, but it is not the final Unity city-ground reproduction.
- For the Map_TTS single-building scene, the real Unity city ground is not an ellipse under the building. It is the enabled `CityWall_Ground_1_1_0` prefab instance under `LV_02`, using material `build_ground_6_101_1.mat` and texture `ref/client-unity/Assets/BundleAssets/Map/Textures/build_ground_6_101_1.psd`. `Scripts/PrepareRokSingleBuildingCutout.py` exports this PSD to `Saved/RokUnityPlacement/clean/UnityCityGround_build_ground_6_101_1.png` so UE can import it reproducibly.
- `build_ground_6_101_1` is the sand/city-yard variant. For the grassland country/terrain look, use Unity `I_TYPE_Grass_01.mat`: `_Splat=build_GrassMap_mask_1.png`, `_Tex0=build_SceneMap_2_lv.tif`, `_Tex1=build_SceneMap_4.tga`, `_Tex2=xijiecao.tga`, `_Tex3=nitudi.tga`, blended with `cm_mask_texture_lod.shader`. `Scripts/PrepareRokSingleBuildingCutout.py` bakes this to `Saved/RokUnityPlacement/clean/UnityGrassGround_I_TYPE_Grass_01.png`.
- The soft in-city green background seen in full city screenshots exists separately from the world/grassland ground: `city_splat_ground_alpha.mat` and `CitySurface_101_1_mat.mat` both reference `ref/client-unity/Assets/BundleAssets/Map/Env/Ground/Terrain_grass_08_3g2.png` as their base `_MainTex`; `CitySurface_101_1_mat` also uses the `map_incity*.png` family for city-yard surfaces. `Scripts/PrepareRokSingleBuildingCutout.py` exports `UnityCityBackground_Terrain_grass_08_3g2.png` and `UnityCitySurface_map_incity4.png` for UE diagnostics.
- In `/Game/Maps/L_RokCitySingleBuilding`, prefer the full Unity city background by default (`ROK_SINGLE_BUILDING_CITY_GROUND_STYLE=city_background`). Use `ROK_SINGLE_BUILDING_CITY_GROUND_STYLE=sand`, `grass`, `city_splat`, or `city_surface` only for explicit source comparison. The older generated ellipse `terrain_plate` is diagnostic-only and should stay disabled unless specifically testing cutout behavior; sprite-local `ground_patch` may be enabled as a lower visual layer, but it must not replace the main building sprite by default.
- City surface and road preview layers are diagnostics until the real Unity meshes/masks are imported. `CitySurface_101_1_mat` is used by prefab mesh objects such as `ground_1`, not as a full opaque square card; therefore `Scripts/PrepareRokSingleBuildingCutout.py` exports the UE preview `UnityCitySurface_map_incity4.png` with soft alpha. `dryDoad01.mat` uses `dirtRoad01.png`; the UE preview `UnityCityRoad_dirtRoad01.png` removes the white texture border into alpha.
- `Scripts/PrepareRokSingleBuildingCutout.py` can split sprite ground-colored pixels into `<sprite>_ground_patch.png` and `<sprite>_body_clean_cropped.png`, but color-only splitting can remove legitimate lower-building/foundation pixels. Keep `ROK_SINGLE_BUILDING_SPLIT_GROUND=0` by default; use the split only as a diagnostic when checking future layer extraction.
- Ground, terrain, and building sprite actors must not be coplanar in UE. Keep the ground patch and terrain plate slightly behind the building along the Unity camera forward vector (`ROK_SINGLE_BUILDING_GROUND_LAYER_OFFSET`, `ROK_SINGLE_BUILDING_TERRAIN_LAYER_OFFSET`) to avoid runtime depth/sort striping.
- Full Unity city ground is a horizontal mesh layer, not a camera-facing sprite. Building sprites must be shifted toward the camera along the Unity camera forward axis (`ROK_SINGLE_BUILDING_BUILDING_LAYER_OFFSET`, negative by default) so the horizontal ground depth does not clip the sprite body.
- UE `/Engine/BasicShapes/Plane` samples the cleaned texture with the vertical UV direction opposite to Unity `SpriteRenderer`. For the single-building diagnostic, keep Unity `m_FlipY: 0` as source truth, but compensate the UE plane by setting the sprite mesh relative Y scale negative through `ROK_SINGLE_BUILDING_ENGINE_PLANE_FLIP_Y=1`. Do not fix this by rotating or flipping the camera.
- For the single-building template, first make `/Game/Maps/L_RokCitySingleBuilding` visually correct with one Unity-derived `Building_Castle_02` before widening to multiple city buildings.

## Unity Initial City State Runtime Slice

- Current initial city runtime state is generated by `Scripts/BuildRokUnityInitialCityState.py` from `InitBuilding`, `BuildingLevelData`, `BuildingResourcesProduce`, and `Saved/RokUnityMigration/unity_gameapp_city_init_building_resolution.json`.
- Generated evidence lives at `Saved/RokUnityMigration/unity_initial_city_state.json`, `Saved/RokUnityMigration/unity_initial_city_state.md`, and `Saved/RokUnityMigration/unity_initial_city_state_verify.json`. Regenerate these files instead of hand-editing them.
- `/Game/Maps/L_RokUnityGameAppCity` actors spawned from `InitBuilding` must keep `RokUnityConfigInitBuilding`, `UnityInitId_*`, `UnityBuildingType_*`, `UnityBuildingLevel_*`, `UnityGridX_*`, and `UnityGridY_*` tags so runtime systems can map actors back to Unity source data.
- `URokBuildingUpgradeSubsystem` loads the initial city state manifest and uses `UnityInitId_*` as the preferred identity path before falling back to actor-label inference.
- `bootstrap_resources` in the initial city state manifest are temporary UE local-simulation values only. Do not treat them as accepted Unity economy data until login rewards, item packages, or server start-state data are migrated.
## Unity Gameplay Catalog Runtime Slice

- Current broad gameplay catalog is generated by `Scripts/BuildRokUnityGameplayCatalog.py` and verified by `Scripts/VerifyRokUnityGameplayCatalog.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_gameplay_catalog.json`, `Saved/RokUnityMigration/unity_gameplay_catalog.md`, and `Saved/RokUnityMigration/unity_gameplay_catalog_verify.json`.
- The catalog currently aggregates Unity `BuildingTypeConfig`, `BuildingMenuData`, `BuildingCountLimit`, `BuildingLevelData`, `BuildingResourcesProduce`, `Arms`, `Hero`, `HeroSkill`, `HeroTalent`, and `TaskMain` into one source-traceable manifest.
- `URokUnityGameplayCatalogSubsystem` is the read-only UE runtime catalog for building types, troop rows, heroes, hero skills/talents summary counts, and main tasks. It should remain config-derived; gameplay systems should reference Unity IDs from it instead of copying rows into ad hoc constants.
- The HUD builder bar is allowed to show a compact catalog acceptance marker such as `Catalog B30 T54 H46 Q205`; this is a runtime verification aid, not final UI design.
- Acceptance for this slice is: catalog script passes, catalog verification has no failures, `RokEditor` builds under `D:\ue\UE_5.8`, `/Game/Maps/L_RokUnityGameAppCity` verification passes, and runtime logs include `Loaded Rok Unity gameplay catalog`.
- `URokUnityGameplayCatalogSubsystem` also exposes `HeroSkill` and `HeroTalent` rows by Unity ID; treat those as the runtime source for later hero/skill/battle adapters.
## Unity Runtime Bootstrap State Slice

- Current local runtime bootstrap is generated by `Scripts/BuildRokUnityRuntimeBootstrap.py` and verified by `Scripts/VerifyRokUnityRuntimeBootstrap.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_runtime_bootstrap_state.json`, `Saved/RokUnityMigration/unity_runtime_bootstrap_state.md`, and `Saved/RokUnityMigration/unity_runtime_bootstrap_state_verify.json`.
- The runtime bootstrap derives current task progress from `unity_gameplay_catalog.json` plus `unity_initial_city_state.json`; the current active task is Unity `TaskMain` id `200900` with a building-level evaluator against city hall type `1`.
- `URokUnityRuntimeStateSubsystem` is the UE local-simulation runtime state layer for the active main task chain, starter hero preview state, starter troop stacks, and trainable troop IDs. It should remain a bootstrap/local-sim layer until real login, roster, inventory, and server state are migrated.
- Starter hero ownership and troop counts in this manifest are explicit UE bootstrap values. Do not treat them as accepted Unity economy/server start-state data.
- HUD no-selection status may show the runtime acceptance marker `Task 200900 ... 1/2 | Heroes 5 | Troops 200`; this is a migration/debug marker, not final UI design.
- Acceptance for this slice is: runtime bootstrap generation passes, runtime bootstrap verification has no failures, `RokEditor` builds under `D:\ue\UE_5.8`, `/Game/Maps/L_RokUnityGameAppCity` verification passes, and runtime logs include `Loaded Rok Unity runtime bootstrap`.
## Unity UI Catalog Runtime Slice

- Current Unity UI catalog evidence is generated by `Scripts/BuildRokUnityUiCatalog.py` and verified by `Scripts/VerifyRokUnityUiCatalog.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_ui_catalog.json`, `Saved/RokUnityMigration/unity_ui_catalog.md`, and `Saved/RokUnityMigration/unity_ui_catalog_verify.json`. Regenerate these files instead of hand-editing them.
- `URokUnityUiCatalogSubsystem` is the UE runtime catalog for Unity UI slice coverage, MainInterface module names, UI prefab/view/mediator source status, processed texture source/target status, and accepted runtime target names.
- The HUD catalog status may show a compact UI acceptance marker such as `UI M17 S5 Tex10/7`; this is a migration/debug marker, not final UI design.
- Acceptance for this slice is: UI catalog verification has no failures, `RokEditor` builds under `D:\ue\UE_5.8`, `/Game/Maps/L_RokUnityGameAppCity` still opens, and runtime logs include `Loaded Rok Unity UI catalog`.

## Unity GameApp City Wall and UI Polish Slice

- `/Game/Maps/L_RokUnityGameAppCity` should not present as an open floating grass card. Its generator `Scripts/CreateRokUnityGameAppCityScene.py` owns a city-boundary layer using the UE city meshes `SM_Rok_CityWall`, `SM_Rok_CityGate`, and `SM_Rok_CityTower` under the `RokUnityGameAppCity/Walls` folder.
- Do not use unrelated building sprites as accepted wall stand-ins in this map. `WALL_SPRITE_LAYOUT` is kept empty unless explicitly debugging sprite fallback behavior.
- HUD and building-panel text should avoid English engineering/debug wording in normal preview. Catalog and UI markers may remain visible as migration acceptance aids, but labels such as builders, build, march, alliance, mail, resources, level, power, queue, ready, upgrade, and max level should be localized for the current Chinese preview path.

## Unity City Boundary Source Migration Slice

- Current city-boundary source evidence is generated by `Scripts/BuildRokUnityCityBoundary.py` and verified by `Scripts/VerifyRokUnityCityBoundary.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_city_boundary.json`, `Saved/RokUnityMigration/unity_city_boundary.md`, and `Saved/RokUnityMigration/unity_city_boundary_verify.json`. Regenerate these files instead of hand-editing them.
- `unity_city_boundary.json` is the source of truth for city-wall and city-gate prefab GUIDs, source prefab paths, active state, transform overrides, material overrides, and Unity material main-texture chains.
- Hand-authored wall ribbons or simple 2D border planes are diagnostic/demo-only. They must not be treated as accepted migrated city-wall content when `unity_city_boundary.json` is available.
- The next accepted city-wall implementation must be driven by this manifest: import/use Unity CityWall and CityGate assets, preserve active/inactive state unless a documented runtime slice overrides it, and tag generated UE actors with source scene/name/prefab GUID/material path metadata.

## Unity World Entity Presentation Slice

- `Scripts/BuildRokUnityWorldEntityPresentation.py` (+`Verify*`) extracts the Unity world-entity presentation chain into `Saved/RokUnityMigration/unity_world_entity_presentation.json`: `BarbarianFormation_{modelId}` rows/dummies (level 14/16 -> 15 units each: 1 Monster_1000 leader + Arms2_1/Arms2_3/Monster_2004 soldiers), per-unit idle-frame crops (direction 1 frame 0, Color.gray baked into the crop because `Troops.InitFormationS` tints barbarian units), and the `ui_map.png` atlas nameplate sprites (`pl_map_levelBg`, `pl_map_buildingNameBg`, `img_map_state_back`).
- `ARokWorldMapEntityActor` renders formation units as UPRIGHT world-plane `/Engine/BasicShapes/Plane` cards and composite/far cards as 45-degree tilted planes, all with `M_RokWorldEntitySprite` (unlit translucent two-sided, `Sprite` texture parameter). Do not use `UBillboardComponent` for gameplay entities: it is gated by the editor `BillboardSprites` show flag and never renders in `-game` runs.
- Unit card world size comes from the unit prefab's serialized SpriteRenderer `m_Size` (1.28x1.28 Unity units = 640x640 cm for all four unit types), NOT the sprite's native pixel size — Unity stretches every idle frame into that fixed quad. Using pixels/PPU shrank the units ~2.5x and made the formation read as scattered confetti. Unity also multiplies units by `LevelDetailScalerManager.m_unit_scale_curve` (LOD-dependent, not yet extracted).
- The HUD shows Unity-style nameplates: `pl_map_levelBg` + level number above every levelled entity, and `UI_Tip_WorldObjectPlayerLevel` layout (green own-city name + level badge) over the player city. The old green self-marker pin, hero head bubble, and 7-man composite cluster card are removed.
- Idle animation is live: 8 cropped frames per unit (direction 1), 0.15s per frame from `AnimationBase m_state[0] m_update_rate`, random start frame per unit; frame textures are cached per path across entities.
- Close-LOD clone expansion from `MatrixRow.UpdateUnitPositionBarabarianLod` is live: soldier rows expand by 2 diagonal clones each (`m_expansivity=0.18`, ±1.414 diagonals, even/odd z alternation) only when `dxf < 900` (Unity `Troops m_lod_array = [501, 900, 99999]`, lod<=1) — 15 base units become 43 at close zoom; the hero row never expands. Do NOT key expansion off the CloseDetail band (1500): Unity's lod:1537 strategic view still shows the tight base formation.
- Formation-local offsets convert with a z negation (`formation_offset_to_ue`): the prefab puts the hero row at +z, and the Unity screenshots show the hero at screen-bottom (south), so an unflipped conversion renders the whole formation mirrored front-to-back.
- World-object nameplates anchor at the entity's ground position (`MapElementUI.SetPosition(pos.x, 0, pos.z)`), not lifted above the formation.
- Entity card sprites must negate both in-plane scale axes (`CardScale` in `RokWorldMapEntityActor.cpp`): the engine Plane's UVs are vertically opposite to Unity SpriteRenderer quads and the card's local +X faces world -X, so an unadjusted card renders every unit rotated 180 degrees on screen (same class of bug as the city slice's `ROK_SINGLE_BUILDING_ENGINE_PLANE_FLIP_Y`).
- Unit card world size = NATIVE sprite size (frame pixels / spritePixelsToUnits) x `LevelDetailScalerManager.m_unit_scale_curve` (extracted from GameApp.unity; dxf 500->1.0, 900->1.75, 1500->2.1, 3000->2.5). The SpriteRenderer drawMode is Simple (0), so the serialized m_Size (1.28x1.28) is IGNORED at runtime - sizing cards to m_Size made units ~2.5x too wide, and native size without the curve made them scattered. Verified against the Unity lod:1537 reference: formation mass = 19.6% of UE screen width vs Unity 16.9-21.7%. (The per-prefab serialized m_Size 0.32x0.68948674 is identical across all four unit prefabs - a template default, not per-unit authored data.)
- Formation unit cards must be UPRIGHT world-plane quads (`UnitCardRotation`/`UnitCardUp` in `RokWorldMapEntityActor.cpp`), NOT camera-facing 45-degree cards: the barbarian unit prefabs keep the body SpriteRenderer on an identity-rotation root (only the 0.29-scaled `shadow`/`shadow_lod1` children carry a 22.5-degree X tilt), so Unity's 45-degree world camera foreshortens the sprite by cos(45). Camera-facing cards removed that foreshortening and made every formation read too tall and too sparse. The upright basis reuses the tilted basis's U axis (rotation only about world X), so the verified horizontal mapping and face side are preserved.
- Unit card lift pins the art BOTTOM to the entity anchor (`Lift = HeightCm * UnitFactor * 0.5`): Unity hangs PivotY (~12-15%) of the art below the anchor and its terrain depth-test clips that part; the visible Unity result is feet + baked shadow sitting on the ground line. The pivot-based lift left a hard clip line through the leader's feet.
- `M_RokWorldEntitySprite` MUST stay `BLEND_TRANSLUCENT` with texture alpha as opacity (Unity SpriteRenderer semantics). The `monster_1000_idle` atlas is natively capped at 50% alpha in every direction strip - the ghostly translucent leader is source-faithful, not a bug - and soldier shield/head/leg pixels sit near 50% alpha (they blend to khaki over sand in Unity too). A masked clip 0.33 turned the leader into a moth-eaten silhouette. Note: the on-disk material asset predates the script's masked experiment and was always translucent (`REUSE_EXISTING_ASSETS` returns early without applying property changes); the script now creates translucent explicitly so regeneration agrees with the asset.
- When pixel-measuring formations in captures, exclude the village/farm composite cards (they sit within a few display units of the barbarians and are hundreds of px wide); use a strict desaturated gray filter and a +-200px window around the focused entity.
- Pending (recorded in the manifest): `UIZoomCurve` nameplate offset/scale curves.
- Auto-captures can aim the camera with `-RokWorldMapFocusX=... -RokWorldMapFocusY=...` (display coordinates; applied after a 0.5s defer so the camera config is resolved) and `-RokWorldMapDxf=...` to force a zoom level.
- The world-map wheel-zoom floor is the world tactical band: `ARokWorldMapCameraPawn::UserZoomMinimumCameraKeyId` clamps user zooming to `FTE_Tactics` (dxf 1110) unless the player city is in view (which keeps the hard `limit_min` floor so the zoom-into-city flow still triggers). Source chain: `CameraParamLod.csv` tactical states start at 1110 (`init` 1350, `map_tactical` 1509) and the `m_unit_scale_curve` X keys match the same dxf ladder; dxf 500-1100 is the city-exit transit band `WorldCameraImpl`/`GlobalViewLevelMediator` only passes through via its dynamic `setAdditionHeightForMinDxf` (raised toward `CityBoundDxf`=500). Wheel zooms that settle at 500-900 evaluate the curve at ~1.1-1.75 WITH clone expansion (43 ghost-blended units) - a sparse transient state the real game never presents as a resting view, which reads as "wrong" against Unity screenshots taken at the tactical band (curve ~2.1+, 15-unit dense mass).
- Regenerating the map while a GUI editor has the project open can fail: memory-mapped `.uasset` file locks make `rename_asset` report "destination path already exists" even after `delete_asset`. `purge_package_from_disk` hardens the mountain importer, but close the GUI editor before full map regeneration.

## Unity World Map Streaming Pipeline Notes

- World-map pipeline order: `BuildRokUnityWorldMapManifest.py` → `PrepareRokUnityWorldMapAssets.py` → `CreateRokUnityWorldMapPrototype.py` (UE editor) → `PrepareRokUnityWorldMapStreamingAssets.py` → `CompileRokUnityWorldMapStreamingData.py` → `BuildRokUnityWorldMapRuntimeData.py`, each with a matching `Verify*` script.
- UE mesh asset names must go through the same `safe_name()` in both `PrepareRokUnityWorldMapAssets.py` and `CreateRokUnityWorldMapPrototype.py` (dashes become underscores); a mismatch silently drops streaming bindings.
- UE 5.8 material-graph Python pin names differ from older engines: `ComponentMask`/`Saturate` input is `""` (not `"Input"`), `TextureSample` UV input is `"UVs"` (not `"Coordinates"`), `Power` exponent is `"Exp"` (not `"Exponent"`), and `MaterialStatistics` has no `errors` property.
- Binary FBX node truth comes from parsing `Model`/`Geometry` records in `parse_fbx_binary_nodes` (Prepare). Unity prefab child names are not guaranteed to exist as FBX nodes (Ocean_CO_A has no plain `_Collision` node; its geometry nodes have empty names, so UE imports them as `Mesh`/`Mesh_ncl_N` placeholders mapped back by `source_fbx_geometry_model_order`).
- Unity mirrors some ground transition tiles (`I_TYPE_TEST_guodu*`) with negative-determinant transforms. The streaming compiler keeps them by folding the reflection into a negative X scale (proper rotation + mirrored scale); affected ground materials are two-sided unlit. Do not re-add a positive-determinant rejection gate.
- `Rive_CO_H` (waterfall composite of Rive_CO_F + build_mountain_G + splash/ripple particle systems, exactly one instance in `Tile-map-pvp_2`) is migrated as representation `exact_source_fbx_composite_waterfall`: 6 static parts bind existing river/mountain meshes plus the dedicated `WaterfallLayers` meshes (`build_mountain_G_edge`/`_water`) and materials `M_WorldSource_build_mountain_T` / `M_WorldWaterfall_WaterLight` (the latter approximates Unity builtin shader fileID 10721, Particles additive family). Its 6 ParticleSystems (bowen/shuahua2/shuihua/xia-shui/shitou (1)) stay under the documented `particle_systems_pending_niagara_or_omission_policy`.

## Unity Runtime Visual Extraction Rule

- For wall, gate, tower, building, ground, and other promoted city visuals, prefer the Unity runtime extraction route when prefab/scene parsing leaves transform, mesh, or material ambiguity: create a minimal Unity check scene, export the active `MeshRenderer` geometry after runtime prefab transforms to an explicit manifest/OBJ set under `Saved/RokUnityMigration/`, then generate UE actors from that manifest.
- Do not hand-place or viewport-tune accepted city visuals. UE actor placement should come from exported Unity renderer bounds/vertices using the established coordinate conversion, and generated actors should align visible bounds back to Unity `Renderer.bounds.center` when the mesh is imported as a reusable UE static mesh.
- Material assignment must follow the Unity renderer material chain: renderer material GUID/path, material asset, shader properties, and `_MainTex`/texture references. Do not infer material solely from the mesh prefix. Known example: Unity runtime `CityTower_101_1_4` / `CityTower_1_4*` uses `CityTower_101_2.mat` and `CityTower_101_2.png`, while the other `CityTower_101_1_*` meshes use `CityTower_101_1_mat.png`.
- Generated actors must keep source/audit tags such as `UnityMesh_*`, `UnityMaterial_*`, `UnitySourcePrefab_*`, `UnityPrefabObject_*`, and a slice tag such as `RokUnityRuntimeBoundarySource`. Verification scripts must assert actor counts, role counts, material-variant counts, no forbidden layer bleed, and zero failures before the result is treated as accepted.
- Imported runtime OBJ assets should reuse the generated OBJ basename as the UE asset name so later regeneration can load the existing mesh instead of reimporting and fighting open-editor file locks. Use explicit `ROK_FORCE_REIMPORT=1` only when the runtime OBJ geometry has intentionally changed.
- Keep accepted diagnostic slices isolated. `/Game/Maps/L_RokUnityRuntimeBoundaryOnly` is the boundary-only acceptance map and must remain wall/gate/tower only; add ground, roads, buildings, or camera/player actors in a separate map or a clearly named next slice.
- Ordinary initial city buildings are runtime sprite cards, not FBX/static-mesh buildings. Unity `CityGlobalMediator.CreateCityBuilding` instantiates the prefab from `GetModelIdByType`, parents it under `CityBuildingContainer`, sets root local position from `ConvertCityObjTileToLocal`, zeroes root local rotation, and lets `GridCollideItem.SetScale` apply the runtime LOD scale. The visible child is normally a `SpriteRenderer` named `sprite`, with local rotation/position/scale from the prefab and `MaskSprite` supplying the cutout mask.
- For ordinary buildings, use `D:\ueDemo\Rok-unity\client-unity\Assets\Editor\CodexExportRuntimeCityBuildings.cs` to bake runtime `SpriteRenderer` geometry and `MaskSprite` red-channel alpha to `Saved/RokUnityMigration/unity_runtime_city_buildings_obj_manifest.json` plus OBJ/PNG outputs. In UE, the accepted preview path is the baked PNG/alpha material on `ARokCitySpriteActor` camera-facing cards aligned to the exported Unity `Renderer.bounds.center`; the generated OBJ cards are source evidence/fallback only because importing them as runtime `StaticMeshActor` cards can become edge-on or invisible in PIE.
- `/Game/Maps/L_RokUnityRuntimeCityAllBuildings` is the current all-building diagnostic slice, and `/Game/Maps/L_RokUnityGameAppCity` may be regenerated from the same runtime manifest for the default playable preview: 10 boundary actors, 2 ground actors, and 24 non-boundary initial-city building sprite cards. Type `6` CityWall and type `8` GuardTower stay in the runtime boundary mesh layer; do not duplicate them as ordinary building cards.
- Preserve Unity render queue and depth state for runtime city visual layers. Current source evidence: `CitySurface_101_1_mat` renderQueue `2992`, shader `cm_mask_texture_lod`, `ZWrite Off`; `CityTranslucent_101_1_mat` renderQueue `2991`, shader `cm_mask_texture_lod_alpha`, `ZWrite Off`; normal building cards use renderQueue `2995`, `ZWrite Off`; town center `city_building_night.mat` uses renderQueue `2996`, `ZWrite Off`. In UE these ground/building layers must be translucent and sorted by the exported Unity `material_render_queue`; do not use opaque ground materials that write depth over building sprite cards.
- Runtime/PIE camera for `/Game/Maps/L_RokUnityRuntimeCityAllBuildings` and the default `/Game/Maps/L_RokUnityGameAppCity` preview should use Unity main-city `CameraParamLod` key `city_default`, not the old top orthographic preview camera or the raw `GameApp.unity` WorldCamera startup FOV alone: local diagnostic center `(0,0)`, distance `20`, vertical FOV `7.5`, `dxf=150`, forward `(0,-0.7,0.7)`, UE horizontal FOV about `11.98` at aspect `16:10`. `FRokCityViewMath::MakeDefaultCameraKeys()` should mirror Unity `CameraParamLod.csv`, and generated maps should include one tagged CameraActor and one tagged PlayerStart for visibility while `ARokStrategyCameraPawn` uses the same source camera in Play mode.
- Accepted default-preview regeneration for this slice is currently: run `Scripts/CreateRokUnityRuntimeCityAllBuildingsScene.py` through `D:\ue\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` with `ROK_UNITY_RUNTIME_CITY_ALL_BUILDINGS_LEVEL=/Game/Maps/L_RokUnityGameAppCity`, `ROK_RUNTIME_CITY_ALL_BUILDINGS_ASSET_SUFFIX=GameAppRuntimeV5Dense`, `ROK_RUNTIME_BUILDING_CARD_MODE=sprite_actor`, and `ROK_RUNTIME_EXTRA_INTERIOR_BUILDINGS=24`, then run `Scripts/VerifyRokUnityRuntimeCityAllBuildingsScene.py` with the matching manifest/verify paths under `Saved/RokUnityMigration/`.
- Keep the identity split clear when adding density: the first 24 ordinary buildings come from `unity_runtime_city_buildings_obj_manifest.json` and keep `UnityInitId_*` plus `RokUnityConfigInitBuilding`; supplemental visual-density buildings must be tagged `RokUnityRuntimeInteriorPreview` and `UnityInteriorPreviewId_*`, must not get `UnityInitId_*`, and must source real Unity `build_*_101_1.png` plus `_mask.png` sprites from `client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/`.
- The current dense default preview acceptance target is `/Game/Maps/L_RokUnityGameAppCity` with 62 tagged actors: 10 boundary, 2 ground, 24 initial runtime buildings, 24 supplemental interior preview buildings, 1 tagged CameraActor, and 1 tagged PlayerStart. Verification should report `failures=[]`, `initial_buildings=24`, `extra_interior_buildings=24`, `buildings=48`, and the supplemental actors under `RokUnityRuntimeCityAllBuildings/InteriorPreviewBuildings`.

## Unity Migration Decomposition Goals

The full Unity-to-UE migration is a large multi-slice goal. Do not treat quick visual patches, demo placeholders, or hand-authored approximations as accepted migration results. Every slice must be source-driven, reproducible, and small enough to verify independently.

### Goal 1: Source Evidence Manifests

- For each feature area, first extract Unity evidence into a generated manifest under `Saved/RokUnityMigration/` or another documented generated folder.
- Each manifest must record Unity source paths, stable IDs or GUIDs, active/inactive state, transform/layout data, material/texture references, config keys, and UE target intent.
- Generated manifests must have matching verify scripts. Regenerate them instead of hand-editing them.
- A UE implementation may use temporary fallback visuals only when the manifest explicitly marks missing or unresolved source data.

### Goal 2: City Visual Foundation

- Migrate city visuals in source-backed layers: ground, city surface, road, shadow/decal, wall/gate, building body, decoration, tree/resource props, and selection/highlight.
- `L_RokUnityGameAppCity` must be generated from Unity scene/prefab/config manifests, not from manual placement.
- City-wall work must use `unity_city_boundary.json` or its successor. Demo 2D ribbons and hand-placed border planes are diagnostic-only.
- Acceptance requires source tags on generated actors, no placeholder wall/card artifacts in promoted maps, and visual comparison against Unity source evidence.

### Goal 3: Runtime Data and Local Simulation

- Continue migrating gameplay catalog data in small groups: buildings, resources, construction queues, heroes, troops, skills, talents, tasks, technologies, buffs, and rewards.
- C++ subsystems own state, validation, timers, lookup, and command handling. Blueprints and widgets remain presentation shells.
- Runtime systems must consume generated Unity manifests or config-derived catalogs rather than hardcoded sample values.
- Acceptance requires deterministic local-simulation behavior and a clear record of which values remain UE bootstrap placeholders.

### Goal 4: UI Flow Migration

- Migrate UI one screen or flow at a time from Unity prefab/view/mediator evidence.
- Start with real runtime paths: main HUD, building menu, build city window, worker queue, main task panel, wall UI, troop UI, hero UI, mail/notice stubs, and alliance placeholders.
- UE UMG should reproduce layout and presentation while C++ owns state and event routing.
- Debug acceptance markers are allowed during migration, but normal preview labels should not look like engineering diagnostics.

### Goal 5: Interaction and Scene Flow

- Migrate the real player flow in stages: bootstrap, main city entry, building selection, build/upgrade, task progress, training, hero/troop preview, march command, and world-map transition.
- Each interaction must start from Unity source behavior or config evidence and end in a UE runtime path that can be opened and exercised.
- If Unity disables or hides a source object by default, preserve that state until the corresponding enable/runtime rule is migrated.

### Goal 6: Server and Protocol Boundary

- Keep backend-shaped behavior behind interfaces so local simulation can later be replaced by dedicated-server or protocol calls.
- Use `ref/server` for authority boundaries, protocol names, validation responsibility, and admin/web feature expectations.
- Do not rewrite UI/gameplay when replacing local simulation with network-backed behavior; the command/query interface should stay stable.

### Goal 7: Acceptance and Regression Gates

- Each slice must define its source manifest, UE target classes/assets/maps, generation script, verify script, and runtime evidence.
- Visual slices must load the target map and provide visual evidence or actor/material/source-tag verification.
- Data/gameplay slices must pass manifest verification and any deterministic local-simulation checks that are practical.
- UI slices must open through the real UE runtime path, not just isolated widget construction.
- Keep the project openable and runnable after each batch.

## Unity City Visual Foundation Slice

- Current city visual foundation evidence is generated by `Scripts/BuildRokUnityCityVisualFoundation.py` and verified by `Scripts/VerifyRokUnityCityVisualFoundation.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_city_visual_foundation.json`, `Saved/RokUnityMigration/unity_city_visual_foundation.md`, and `Saved/RokUnityMigration/unity_city_visual_foundation_verify.json`.
- This slice ties together Unity-derived ground sources, `unity_city_boundary.json`, `unity_gameapp_city_init_building_resolution.json`, and `unity_initial_city_state.json` for `/Game/Maps/L_RokUnityGameAppCity`.
- Acceptance for Goal 2 first batch is: visual foundation verification passes, generated map has `RokUnityCityBoundarySource` actors, generated map has `RokUnityConfigInitBuilding` actors, and no accepted `UnityCityBoundary2D` demo actors remain.

## Unity City Visual Layer Stack Slice

- Current city visual layer-stack evidence is generated by `Scripts/BuildRokUnityCityVisualLayerStack.py` and verified by `Scripts/VerifyRokUnityCityVisualLayerStack.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_city_visual_layer_stack.json`, `Saved/RokUnityMigration/unity_city_visual_layer_stack.md`, and `Saved/RokUnityMigration/unity_city_visual_layer_stack_verify.json`.
- `/Game/Maps/L_RokUnityGameAppCity` should generate terrain, terrain blend, city ground, city surface, and road layers from this manifest before building sprites are spawned.
- Accepted layer actors must carry `RokUnityCityVisualLayer` plus role/source tags. Missing or manually painted layer substitutes are diagnostic-only unless the manifest explicitly records a missing source fallback.

## Unity Building Visual Layers Slice

- Current per-building visual-layer evidence is generated by `Scripts/BuildRokUnityBuildingVisualLayers.py` and verified by `Scripts/VerifyRokUnityBuildingVisualLayers.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_building_visual_layers.json`, `Saved/RokUnityMigration/unity_building_visual_layers.md`, `Saved/RokUnityMigration/unity_building_visual_layers_verify.json`, and generated textures under `Saved/RokUnityMigration/CityBuildingVisualLayers/`.
- `/Game/Maps/L_RokUnityGameAppCity` should spawn `RokUnityBuildingVisualLayer` actors before building body sprites when this manifest exists. The first accepted layer role is `UnityBuildingVisualLayer_Shadow`, generated from Unity building mask red channel when available.
- These building shadow layers are source-derived support layers, not final Unity shader reproduction. Future batches should split ground patch, body, mask, shadow, decal, and selection layers more tightly as Unity shader/material rules are extracted.

## Unity Building Body and Ground Patch Layers Slice

- Current building body/ground-patch evidence is generated by `Scripts/BuildRokUnityBuildingBodyGroundLayers.py` and verified by `Scripts/VerifyRokUnityBuildingBodyGroundLayers.py`.
- Generated evidence lives under `Saved/RokUnityMigration/unity_building_body_ground_layers.json`, `Saved/RokUnityMigration/unity_building_body_ground_layers.md`, `Saved/RokUnityMigration/unity_building_body_ground_layers_verify.json`, and generated textures under `Saved/RokUnityMigration/CityBuildingBodyGroundLayers/`.
- `/Game/Maps/L_RokUnityGameAppCity` should spawn `UnityBuildingVisualLayer_GroundPatch` actors before building shadow/body sprites when this manifest exists.
- Ground patches are estimated from Unity sprite pixels inside the Unity mask and remain source-derived diagnostics until exact Unity material/shader ground separation is migrated.

## Unity Building Visual Integration Slice

- Current building visual integration is verified by `Scripts/VerifyRokUnityBuildingVisualIntegration.py`, which checks that each Unity InitBuilding has generated shadow, body-clean, and ground-patch texture evidence.
- `/Game/Maps/L_RokUnityGameAppCity` should use `UnityBuildingVisualLayer_BodyClean` actors as the visible building body when `unity_building_body_ground_layers.json` exists, falling back to the older raw sprite+mask path only when the body layer is missing.
- Accepted building actors must keep gameplay/source tags such as `RokBuilding`, `CityBuilding`, `UnityInitId_*`, `UnityBuildingType_*`, and `UnityGridX/Y_*` even when their visible body comes from a generated clean layer.

## TestUnity 2.5D View Architecture Slice

- `D:\ueDemo\TestUnity` is the current compact Unity reference for the first accepted city-view architecture pass.
- Its generated evidence is produced by `Scripts/BuildRokTestUnityViewArchitecture.py` and verified by `Scripts/VerifyRokTestUnityViewArchitecture.py`.
- Generated evidence lives under `Saved/RokUnityMigration/testunity_city_view_architecture.json`, `Saved/RokUnityMigration/testunity_city_view_architecture.md`, and `Saved/RokUnityMigration/testunity_city_view_architecture_verify.json`.
- `RokStrategyCameraPawn` should follow the TestUnity `RokCitySimpleCamera` model: perspective 2.5D camera, Unity vertical FOV converted to UE horizontal FOV, `dxf = distance * fov` zoom curve, zoom-to-cursor, drag pan, keyboard pan, and reset.
- Treat view architecture as a separate layer from gameplay/data systems. Source-derived view layers are terrain, city ground, road, shadow, wall, building ground, building body, decoration, unit, HUD, and debug.
- Do not reintroduce the old orthographic top-down camera as the accepted city preview path unless a new Unity source view explicitly requires it.

## Unity Decoration Resolution Rule

- Do not substitute unrelated decoration sprites when Unity `InitBuilding` resolves to a missing decoration model. In particular, type 52/53 must not fall back to `build_51_101_1_*` just to fill space.
- Missing decoration rows should remain recorded in `unity_gameapp_city_init_building_resolution.json` and summarized as unresolved decorations in `unity_city_visual_foundation.json` until the real Unity source path is extracted.
- Type 51 variants under `build_51_101_1_*` are allowed because those files exist as Unity sprite variants for the same model prefix.


## UnityLearning Reference-Fidelity Rules

When editing anything under `UnityLearning/`, the implementation must be source-faithful to `ref/` before it is considered acceptable.

- First identify the exact owning reference chain in `ref/client-unity`: runtime script, prefab, material, shader, texture or atlas, config table, and scene/prefab transform data.
- Do not replace a reference asset or behavior with a hand-made approximation when the reference source exists. This specifically forbids temporary Mesh arrows, guessed icon geometry, guessed shader behavior, guessed positions, or visually similar placeholder objects.
- For Unity learning slices, mimic the reference object structure first. If ref uses `SpriteRenderer`, use `SpriteRenderer`; if ref uses a prefab hierarchy, reproduce that hierarchy; if ref uses atlas sprites, load those atlas sprites or copy the source atlas into the learning project.
- Keep reference constants traceable in code comments when practical: source prefab path, source material path, source shader path, atlas sprite names, child names, local transforms, scale, and offset formulas.
- If a required ref asset cannot be loaded or copied into `UnityLearning/`, stop and report the missing source path instead of inventing a fallback visual.
- For city selection and move feedback, the current source of truth is `ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/CityGlobalMediator.cs`, `ref/client-unity/Assets/BundleAssets/Scene/City/city_function/building_move_arrow.prefab`, `building_move_bottom.prefab`, `city_building_move_arrow.mat`, `customcitybuilding_move_arrow+21.shader`, and `ui_hud` atlas sprites `Arrow_up`, `Arrow_right`, `Arrow_down`, and `Arrow_left`.
- The reference arrow hierarchy is `building_move_arrow/arrow1..arrow4`; child transforms stay source-shaped and use the source arrow sprites. Do not draw replacement triangles or custom arrow meshes.
