# Rok UE5.2 Project Instructions

## Project Purpose

This repository is a UE 5.2 C++ project for building a strategy/MMO game in the style of "Rise of Kingdoms" / "万国觉醒": city construction, world map marching, heroes, troops, alliances, timed progression, battle resolution, and server-authoritative multiplayer systems.

The extracted reference material lives under `ref/`. Treat it as reference input for understanding feature shape, protocol boundaries, data tables, UI flows, and server/client responsibilities. Do not turn the project into a direct source dump; rebuild gameplay as UE-native systems with clean C++ foundations and Blueprint assets used mainly for presentation and editor-authored content.

## Current Initialization

- Engine target: UE 5.2.
- Project root: `D:\ueasset\Rok`.
- Main project file: `Rok.uproject`.
- Runtime module: `Rok`.
- Automation plugin: `Plugins/McpAutomationBridge`, copied from `D:\ueasset\McpAutomationBridge`.
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
- Before the next serious visual iteration, inspect the Unity reference rendering path and identify the real material/shader/layer rules for city sprites:
  - how `ref/resources/Sprite/*_mask.png` channels are used;
  - whether building sprites are intentionally combined with base tiles, shadows, grass edges, or decals;
  - whether separate floor, road, ground, shadow, decal, and building-body layers exist under `ref/client-unity` or `ref/resources`;
  - how Unity blends sprite ground edges into the city terrain.
- The next implementation target is to reproduce that resource-layer pipeline in UE: ground/floor/road/shadow/building layers must be generated from the correct source assets and shader semantics. A scene should not be considered visually improved just because it has more sprite actors if the result still reads as rectangular PNG cards.
- Keep the latest useful diagnostic baseline screenshots in mind:
  - `Saved/RokCityLocalBlockCiv6_RuntimeOrtho2250.png`: closest current UE runtime baseline for composition and camera.
  - `Saved/RokCityLocalCompositeCiv6.png`: offline composite showing that actor sorting alone is not the only issue; resource layering itself must be solved.
