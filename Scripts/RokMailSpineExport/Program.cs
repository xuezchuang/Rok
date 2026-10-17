using System.Security.Cryptography;
using System.Text.Json;
using Spine;
if(args.Length!=1)throw new ArgumentException("Usage: RokMailSpineExport <Rok root>");
var root=Path.GetFullPath(args[0]);
var directory=Path.Combine(root,"ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UE_TreasureBox");
var skeletonPath=Path.Combine(directory,"UE_TreasureBox.skel.bytes");
var atlasPath=Path.Combine(directory,"UE_TreasureBox.atlas.txt");
var assetPath=Path.Combine(directory,"UE_TreasureBox_SD.asset");
const float sourceScale=.01f;
var loader = new SourceTextureLoader();
var atlas = new Atlas(atlasPath, loader);
var data = new SkeletonBinary(atlas) { Scale = sourceScale }.ReadSkeletonData(skeletonPath);
var attachmentTypes=data.Skins.SelectMany(s=>s.Attachments.Values).Select(a=>a.GetType().Name).Distinct().OrderBy(s=>s).ToArray();
var slotBlends=data.Slots.Select(s=>s.BlendMode.ToString()).Distinct().OrderBy(s=>s).ToArray();
var animations=data.Animations.Select(a=>new {name=a.Name,duration=a.Duration}).ToArray();
var animationName = "UE_TreasureBox_stay";

var loop = true;
var animation = data.FindAnimation(animationName) ?? throw new InvalidDataException($"Source has no {animationName} animation");
const int fps = 60;
var frameCount = (int)Math.Ceiling(animation.Duration * fps) + (loop ? 0 : 1);
if (frameCount < 2) throw new InvalidDataException("Expected an animated idle clip");
var frames = new List<object>();
var minX = float.PositiveInfinity; var minY = float.PositiveInfinity;
var maxX = float.NegativeInfinity; var maxY = float.NegativeInfinity;
var uniquePoses = new HashSet<string>();
for (var frame = 0; frame < frameCount; ++frame)
{
    // A fresh setup pose makes sampling deterministic and independent of frame order.
    var skeleton = new Skeleton(data);
    skeleton.SetSkin("gift5"); skeleton.SetSlotsToSetupPose();
    var time = frame * animation.Duration / (loop ? frameCount : frameCount - 1);
    animation.Apply(skeleton, -1, time, false, null, 1, MixPose.Setup, MixDirection.In);
    skeleton.UpdateWorldTransform();
    var draws = new List<object>();
    var pose = new List<float>();
    foreach (var slot in skeleton.DrawOrder)
    {
        if (slot.Attachment == null) continue;
        float[] xy, uv; int[] triangles; AtlasRegion region;
        float red, green, blue, alpha;
        if (slot.Attachment is RegionAttachment quad)
        {
            xy = new float[8]; quad.ComputeWorldVertices(slot.Bone, xy, 0);
            // SpineMesh.cs optimized non-clipping BuildMesh path packs regions
            // BL, BR, UL, UR (not ComputeWorldVertices' BL, UL, UR, BR).
            // Preserve that exact native buffer/index order for independent comparison.
            var order = new[] {0,3,1,2}; var sourceXY = xy; var sourceUV = quad.UVs;
            xy = new float[8]; uv = new float[8];
            for(int v=0;v<4;v++) { xy[v*2]=sourceXY[order[v]*2];xy[v*2+1]=sourceXY[order[v]*2+1];uv[v*2]=sourceUV[order[v]*2];uv[v*2+1]=sourceUV[order[v]*2+1]; }
            triangles = new[] { 0, 2, 1, 2, 3, 1 };
            region = (AtlasRegion)quad.RendererObject;
            red = quad.R; green = quad.G; blue = quad.B; alpha = quad.A;
        }
        else if (slot.Attachment is MeshAttachment mesh)
        {
            xy = new float[mesh.WorldVerticesLength]; mesh.ComputeWorldVertices(slot, xy);
            uv = mesh.UVs; triangles = mesh.Triangles;
            region = (AtlasRegion)mesh.RendererObject;
            red = mesh.R; green = mesh.G; blue = mesh.B; alpha = mesh.A;
        }
        else throw new InvalidDataException($"Unsupported visible attachment {slot.Attachment.GetType().Name}: {slot.Data.Name}");
        if (xy.Length != uv.Length || triangles.Any(i => i < 0 || i >= xy.Length / 2)
            || xy.Any(v => !float.IsFinite(v)) || uv.Any(v => !float.IsFinite(v)))
            throw new InvalidDataException($"Invalid mesh for {slot.Data.Name}");
        for (var i = 0; i < xy.Length; i += 2)
        {
            minX = Math.Min(minX, xy[i]); maxX = Math.Max(maxX, xy[i]);
            minY = Math.Min(minY, xy[i + 1]); maxY = Math.Max(maxY, xy[i + 1]);
        }
        pose.AddRange(xy);
        draws.Add(new { slot = slot.Data.Name, attachment = slot.Attachment.Name,
            texture = Path.GetRelativePath(root, (string)region.page.rendererObject).Replace('\\', '/'),
            blend = slot.Data.BlendMode.ToString(), xy, uv, triangles,
            rgba = new[] { red * slot.R * skeleton.R, green * slot.G * skeleton.G,
                blue * slot.B * skeleton.B, alpha * slot.A * skeleton.A } });
    }
    if (draws.Count == 0) throw new InvalidDataException("Empty sampled pose");
    uniquePoses.Add(JsonSerializer.Serialize(pose));
    frames.Add(new { time, draws });
}

var sourcePaths=new[]{skeletonPath,atlasPath,assetPath,Path.Combine(directory,"UE_TreasureBox_Atlas.asset"),Path.Combine(directory,"UE_TreasureBox_Material.mat"),Path.Combine(root,"ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/UI_Item_MailBtn.prefab"),Path.Combine(root,"ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AnimationBox.prefab"),Path.Combine(root,"ref/client-unity/Assets/Third/Spine/spine-unity/Modules/SkeletonGraphic/Shaders/Spine-SkeletonGraphic.shader")}.Concat(loader.Paths).ToArray();
var sources=sourcePaths.Concat(sourcePaths.Select(p=>p+".meta")).Select(path=>new{path=Path.GetRelativePath(root,path).Replace('\\','/'),sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))).ToLowerInvariant()});
var outdir=Path.Combine(root,"Saved/RokUnityMigration/MailSpine");Directory.CreateDirectory(outdir);
var output=Path.Combine(outdir,"gift5_stay_mesh.json");
File.WriteAllText(output,JsonSerializer.Serialize(new{schema="rok.mail_attachment.spine_mesh.v1",schema_version=1,animation=animationName,skin="gift5",loop,duration=animation.Duration,source_scale=sourceScale,frame_count=frameCount,attachment_types=attachmentTypes,slot_blends=slotBlends,animations,source_transform=new{skeleton_scale=.01,rect_scale=new[]{.06,.06,.06},anchored_position=new[]{0,-5.1},anchor=new[]{.5,.5},pivot=new[]{.5,.5}},pma=new{texture="source atlas pixels unchanged",vertex_colors=true,source_blend="One OneMinusSrcAlpha",exported_rgba="straight attachment*slot*skeleton tint; consumer must apply source PMA once"},bounds=new[]{minX,minY,maxX,maxY},unique_poses=uniquePoses.Count,sources,frames}));
var report=new{animation=animationName,skin="gift5",duration=animation.Duration,frame_count=frameCount,unique_poses=uniquePoses.Count,attachment_types=attachmentTypes,slot_blends=slotBlends,animations,output=Path.GetRelativePath(root,output).Replace('\\','/'),sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(output))).ToLowerInvariant()};
File.WriteAllText(Path.Combine(outdir,"export_report.json"),JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine(JsonSerializer.Serialize(report));
sealed class SourceTextureLoader : TextureLoader
{
    public List<string> Paths { get; } = new();
    public void Load(AtlasPage page, string path)
    {
        if (!File.Exists(path)) throw new FileNotFoundException("Missing source atlas page", path);
        page.rendererObject = Path.GetFullPath(path); Paths.Add(Path.GetFullPath(path));
    }
    public void Unload(object texture) { }
}
