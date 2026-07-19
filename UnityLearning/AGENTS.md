# UnityLearning Agent Rules

## Scope

These rules apply to work under `UnityLearning/` in this repository, especially `UnityLearning/InitialCityLayoutUnity`.

The UnityLearning project is a learning and comparison project for understanding the Unity source behavior before rebuilding it in UE. It must stay source-driven and auditable. Do not make visual changes that only hide the source problem.

## Required Work Log For Each Change

Before changing files, state these four items in the chat:

- Source: the Unity/runtime/export file that drives the change.
- Action: the exact generated object, script, material, or scene behavior being changed.
- Expected: what should visibly or structurally change after rebuild.
- Verify: what screenshot, hierarchy object, generated file, or log line will prove it worked.

If any item is unknown, stop and inspect first. Do not patch by guessing.

## Source Of Truth

- UnityLearning city wall, gate, and tower visuals must come from the original Unity prefab assets under `Assets/BundleAssets/Scene/City_building/city_building_1/` when rebuilding the Unity learning scene.
- UE-oriented runtime OBJ exports under `Saved/RokUnityMigration/UnityRuntimeBoundaryObj/` are evidence for UE migration, not the preferred source to rehydrate the UnityLearning scene.
- Runtime-exported city building visuals must come from `Saved/RokUnityMigration/UnityRuntimeCityBuildingsObj/` and the Unity runtime city manifest.
- Baked terrain preview may use `Saved/RokUnityMigration/UnityRuntimeGroundObj/*_BakedGround.png` as a visible learning-scene ground layer.
- `Saved/RokUnityMigration/unity_runtime_boundary_reference.png` is visual evidence only. It may be used for comparison, but it must not be used as the accepted visible city wall implementation.

## Prohibited Shortcuts

- Do not use screenshot cards, reference PNG cutouts, or black-background alpha tricks as the main visible city wall.
- Do not hide broken source meshes and replace them with a flat image while calling the city wall fixed.
- Do not hand-place wall, gate, tower, or building actors for acceptance.
- Do not use large hard-edged OBJ ground planes as the accepted visible terrain when they create square-card artifacts.
- Do not call a result fixed from Scene view alone; the Game view must also be checked because this slice is about runtime camera presentation.

## City Wall Rules

- The visible wall in UnityLearning must be generated from the original Unity ref prefabs `build_6_101_1.prefab` and `build_8_101_1.prefab`, plus their Unity material texture chain.
- Do not use UE-coordinate OBJ exports as the main UnityLearning wall placement source when original Unity prefabs are available.
- If the wall appears broken, inspect mesh import, transform, material shader, culling, depth, render queue, and camera clipping in that order.
- Keep source boundary objects visible unless the current step is explicitly a diagnostic isolation test.
- If a diagnostic hides the source boundary, name the parent with `Diagnostic` and restore the accepted source path before finishing.

## Ground Rules

- `UnityRuntimeCityAllBuildings_ground_1_0` and `UnityRuntimeCityAllBuildings_ground_2_1` are source OBJ evidence. They should not be the default visible ground in `UL_InitialCityLayout` if they read as large square planes.
- Keep those ground OBJ meshes in a hidden audit parent when needed.
- The visible learning-scene terrain should use the baked terrain preview layer until the real ground mesh/material pipeline is solved.

## Rebuild And Verification

- The rebuild entrypoint is `UnityLearning -> Rebuild Initial City From Runtime Export`.
- If changing generated scene behavior, remove `Assets/Scenes/UL_InitialCityLayout.startup_built_v4` so Unity can rebuild after scripts compile.
- After rebuild, verify these hierarchy expectations:
  - `RuntimeBoundary_From_RefUnityPrefabs` exists for visible source-derived wall/gate/tower prefabs.
  - `RuntimeGroundObj_SourceMeshes_HiddenForAudit` may exist for hidden OBJ ground evidence.
  - No `BoundaryReferencePreview`, `SourceCutout`, or screenshot-card object is used as the main wall.
- Ask the user for a Game view screenshot when local window automation is not being used.

## Communication

When user verification is required, ask for one concrete thing only, for example:

`请点 UnityLearning -> Rebuild Initial City From Runtime Export，然后发 Game 视图截图。`

Do not spend time operating Unity through slow UI automation unless the user explicitly asks for it.
