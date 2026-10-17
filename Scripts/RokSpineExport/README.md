# Commander source-mesh export

Internal migration tool for `CaptainMediator.UpdateHero` → `LoadSpine(heroModel)` → looping `idle`.
Uses the reference `spine-csharp` sources in place; does not modify/copy them into the UE runtime. Existing Spine license and copyright notices still apply and are not superseded by this tool.

From the Rok project root:

```powershell
dotnet run --project Scripts/RokSpineExport/RokSpineExport.csproj -p:BaseIntermediateOutputPath=D:/ueDemo/Rok/Saved/RokUnityMigration/SpineTool/obj/ -p:BaseOutputPath=D:/ueDemo/Rok/Saved/RokUnityMigration/SpineTool/bin/ -- D:/ueDemo/Rok 2007
```

The tool reads the original SkeletonDataAsset scale, binary skeleton, atlas and texture page. It evaluates weighted/deformed attachment world vertices through the reference evaluator at uniform intervals no longer than 1/60 second, in source draw order. Unsupported non-mesh/non-region visible attachments fail explicitly. It records source hashes, UVs, triangles, slot colors, blend modes, fixed whole-clip bounds and unique-pose count. Re-running on unchanged input is deterministic.

Output: `Saved/RokUnityMigration/Commander/hero_2007_idle_mesh.json` (generated; regenerate rather than hand-edit).

Pass `--list` after the hero ID to inspect actual source animation names/durations without exporting. Pass `standby` to export that original non-looping click clip. Non-looping clips include the exact endpoint; idle sampling remains endpoint-exclusive. A missing requested source animation is an error, not a fabricated fallback.

UE `URokCommanderMeshWidget` draws the original atlas through native Slate vertices and interpolates adjacent compatible vertex samples. This is sampled skinned geometry, not a raster sprite sheet or runtime Spine solver. The current renderer supports Normal blend mode only. Other blend modes are rejected. `RokCommanderCamera.h` projects the mesh through the source prefab's fixed camera; clip bounds no longer resize each hero to fill a portrait box.

## Current scope and acceptance

- Connected idle clips: 2007 plus current local-preview roster 1006, 1007, 1009, 1010 and 1012. Each has 200 samples and duration 3.3333333 seconds. These IDs/models are resolved from `unity_runtime_bootstrap_state.json`; ownership there is local simulation, not server login state.
- Source inventory finds standby only in 1006 (3 seconds, 181 samples) and 1009 (1.8333334 seconds, 111 samples) among those six. Both are connected through `CommanderPortraitAction` (499). `CaptainMediator.OnCharClickEvent` is the source for non-restarting one-shot playback followed by looping idle. The idle coordinate frame stays fixed across the gesture so auto-fit cannot resize the whole character.
- The feature screen loads a generated clip for the selected ID. Same-hero sort/detail rebuilds reattach the existing widget, preserving the animation clock and avoiding repeated JSON/texture loads. Leaving the screen releases its retained portrait reference.
- Runtime entry: city `-RokOpenModal=commander`, or the real world-map Commander button. The world-map command-line modal dispatcher does not currently route the commander alias.
- Optional `-RokCommanderMeshAudit` logs the initial painted sample and a later sample after half a cycle, using real Slate ticks.
- Camera: `UI_IF_Captain.prefab` Hero local position `(0,-3.85,4)`, scale `1.3`; Camera `(0,0,-9.2)`, identity rotation, vertical perspective FOV `41.7`. `UIRenderTexture` targets the stretched full-screen RawImage, now represented by a 1440x900 native mesh canvas. Source `CaptainMediator.OnHeroSelected` enters from camera X -10/+10 by list direction, and Update settles using SwitchSpeed=20, CameraDamping=10+10*t. These are exact simplifications of the inspected prefab's unweighted linear/constant curves. Same-hero clicks do nothing. Native projective drawing covers the planar hero, not a restored 3D HeroShow environment.
- Automation: `Rok.Ui.Animation.Commander.SourceMesh` checks loading, source duration/frame count, actual vertex deformation and renderer reconstruction. This does not replace visual validation.
- Runtime evidence: `Saved/RokUnityMigration/Commander/MeshIdleCity20260908.png`, `MeshIdleCityLater20260908.png`; `Saved/Logs/CommanderMeshCityLater20260908.log` records actual samples 7 and 100 being painted.

## Remaining work (not production acceptance)

- Other heroes; source camera drag/subpanel transitions and HeroShow backgrounds. Click voice/talk tips, subpanel-specific click suppression, and source animation mixing are not yet migrated. Current gestures switch clips directly; this is not full Spine AnimationState mixing parity.
- Convert/stage generated mesh clips and source texture pages into a packaged content path. Current runtime loads migration JSON from Saved and atlas PNG from ref; a packaged build without these files does not have this animation.
- Add schema/hash validation at the packaged-data boundary; broaden renderer only with source examples for clipping, additive blends and attachment changes.
- Compare against Unity at identical animation timestamps. Fixed perspective hero projection and selection return are migrated, but source background parallax, runtime aspect changes and full HeroShow rendering still need acceptance.
