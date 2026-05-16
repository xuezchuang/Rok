# Rok UE5.2 Project Instructions

## Project Purpose

This repository is a UE 5.2 C++ project for building a strategy/MMO game in the style of "Rise of Kingdoms" / "Íò¹ú¾õÐÑ": city construction, world map marching, heroes, troops, alliances, timed progression, battle resolution, and server-authoritative multiplayer systems.

The extracted reference material lives under `ref/`. Treat it as reference input for understanding feature shape, protocol boundaries, data tables, UI flows, and server/client responsibilities. Do not turn the project into a direct source dump; rebuild gameplay as UE-native systems with clean C++ foundations and Blueprint assets used mainly for presentation and editor-authored content.

## Current Initialization

- Engine target: UE 5.2.
- Project root: this repository root, the directory containing `Rok.uproject`.
- Main project file: `Rok.uproject`.
- Runtime module: `Rok`.
- Automation plugin: `Plugins/McpAutomationBridge`, kept as a project-relative plugin directory.
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

## Initial Technical Direction

- Start with a playable prototype loop before heavy content migration: camera, world map pawn/controller, city screen shell, resource counters, timed build queue, troop march command, and one server-shaped simulation boundary.
- Model data with UE-friendly assets first: DataAssets/DataTables for buildings, troops, heroes, skills, resources, technologies, and buffs.
- Keep network/server work behind clear interfaces so the project can later connect to a dedicated backend or local simulation without rewriting UI/gameplay.
- Build editor automation through `McpAutomationBridge` for repeated Blueprint, asset, and level-generation tasks.

## Prototype Scene

- First scene: `/Game/Maps/L_RokPrototype_Lit` as the current default map. `/Game/Maps/L_RokPrototype` remains a duplicate baseline for comparison.
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
- Import PNG sprite assets with `TextureFactory` in `Scripts/CreateRokPrototypeScene.py`; UE 5.2 Interchange PNG import can assert inside the MCP callback path.
- Do not place imported Unity FBX assets directly into the active prototype map until their pivots, bounds, materials, and scale are validated. When using large reference meshes, place them through bounds-origin/bottom compensation so their visible geometry lands at the intended world position; unstable FBX placement can create floating fragments and non-RoK scene clutter.

## City Base Visual Restoration Gate

- Current priority for `/Game/Maps/L_RokCityRuntime` and related diagnostic maps is the RoK city-base visual match, not more camera tweaking, UI work, unit movement, or interaction expansion.
- The 2.5D runtime route is validated enough to continue: orthographic `RokStrategyCameraPawn`, `ARokCitySpriteActor`, and billboard-style city sprites can render in-game without the old tilted-paper failure. Mouse edge scrolling should remain disabled by default while visual validation is in progress.
- Do not keep trying to solve the remaining city-base mismatch by only changing camera zoom, actor spacing, translucent alpha, or simple green-screen/flood-fill cutouts. Those attempts produced PNG-card / rectangular-tile artifacts and do not reach the target look.
- Do not hand-place city buildings for visual matching. City placement must be derived from Unity source data first, then applied to UE.
- The current placement extraction entrypoint is `Scripts/ExtractRokUnityCityPlacement.py`. It reads Unity scenes under `ref/client-unity/Assets/BundleAssets/Map/Scenes/` plus config CSVs under `ref/config-tables/±àÒë¿Í»§¶ËÅäÖÃ/CSV/`, and writes transient outputs under `Saved/RokUnityPlacement/`.
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
