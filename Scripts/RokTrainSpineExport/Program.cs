using System.Security.Cryptography;
using System.Text.Json;
using Spine;
using Microsoft.VisualBasic.FileIO;
// Offline internal source evaluator. Reference runtime remains unmodified.
if (args.Length != 2) throw new ArgumentException("Usage: RokTrainSpineExport <Rok root> <--all|Arms ID>");
var root = Path.GetFullPath(args[0]);
using var csv = new TextFieldParser(Path.Combine(root,"ref/config-tables/编译客户端配置/CSV/Arms.csv"));
csv.SetDelimiters(","); csv.HasFieldsEnclosedInQuotes = true;
var headers = csv.ReadFields()!;
var rows = new List<Dictionary<string,string>>();
while (!csv.EndOfData) {
 var values=csv.ReadFields()!;
 if(values.Length != headers.Length) throw new InvalidDataException("Malformed Arms CSV row");
 var row=headers.Zip(values).ToDictionary(p=>p.First,p=>p.Second);
 if(int.TryParse(row["ID"],out _)) rows.Add(row);
}
var selected= args[1]=="--all" ? rows : rows.Where(r=>r["ID"]==args[1]).ToList();
if(selected.Count==0) throw new ArgumentException("Unknown source Arms ID");
var results=new List<object>(); int failures=0, exported=0, unsupported=0;
foreach(var row in selected) {
 int id=int.Parse(row["ID"]); string model=row["armsShow"];
 if(string.IsNullOrWhiteSpace(model)) { unsupported++; results.Add(new {arms_id=id,status="unsupported_empty_armsShow", arms_type=row["armsType"]}); continue; }
 try { results.Add(Export(root,id,model)); exported++; }
 catch(Exception e) { failures++; results.Add(new {arms_id=id,arms_show=model,status="failed",error=e.Message}); Console.Error.WriteLine($"{id} {model}: {e.Message}"); }
}
// Runtime data lives in Content/RokData; reports keep the pre-move name (see Scripts/rok_data_paths.py).
var outdir=Path.Combine(root,args[1]=="--all" ? "Content/RokData/TrainArmy" : "Saved/RokUnityMigration/TrainArmy"); Directory.CreateDirectory(outdir);
File.WriteAllText(Path.Combine(outdir,args[1]=="--all" ? "export_report.json" : $"export_report_{args[1]}.json"),JsonSerializer.Serialize(new {schema="rok.train_army.spine_export_report.v1", exported,unsupported,failures,results},new JsonSerializerOptions{WriteIndented=true}));
Console.WriteLine($"Train source export: {exported} exported, {unsupported} explicitly unsupported, {failures} failures");
Environment.ExitCode=failures>0 ? 1 : exported==0 ? 2 : 0;

static object Export(string root,int armsId,string model) {
if(!System.Text.RegularExpressions.Regex.IsMatch(model,@"^Arms(?:Special)?[0-9]+_[0-9]+_SD$")) throw new InvalidDataException("Unsupported armsShow resource name");
var prefabPath=Path.Combine(root,"ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_troops/UIArmyTrain/UI_Win_TrainArmy.prefab");
var prefabText=File.ReadAllText(prefabPath);
var rect=prefabText.Split("--- !u!224 &1845448039395248044")[1].Split("--- !u!")[0];
if(!rect.Contains("m_LocalScale: {x: 0.85, y: 0.85, z: 0.85}") || !rect.Contains("m_AnchoredPosition: {x: 66, y: 63}") || !rect.Contains("m_AnchorMin: {x: 0.5, y: 0}") || !rect.Contains("m_AnchorMax: {x: 0.5, y: 0}") || !rect.Contains("m_Pivot: {x: 0.5, y: 0.5}")) throw new InvalidDataException("Training source transform changed; update explicit schema contract first");
var graphic=prefabText.Split("--- !u!114 &5290344151149938783")[1].Split("--- !u!")[0];
foreach(var required in new[]{"startingAnimation: idle","startingLoop: 1","timeScale: 1","pmaVertexColors: 1","useClipping: 1","initialFlipX: 0","initialFlipY: 0"})
 if(!graphic.Contains(required)) throw new InvalidDataException("Training SkeletonGraphic contract changed: "+required);
var directory = Path.Combine(root, $"ref/client-unity/Assets/BundleAssets/Troop/spine_troop/{model}/Materials");
var skeletonPath = Directory.GetFiles(directory, "*.skel.bytes").Single();
var atlasPath = Directory.GetFiles(directory, "*.atlas.txt").Single();
var assetPath = Path.Combine(directory, $"{model}.asset");
var assetText = File.ReadAllText(assetPath);
var scaleMatch = System.Text.RegularExpressions.Regex.Match(assetText, @"(?m)^  scale: ([0-9.]+)\r?$");
if (!scaleMatch.Success) throw new InvalidDataException("Missing source skeleton scale");
var sourceScale = float.Parse(scaleMatch.Groups[1].Value, System.Globalization.CultureInfo.InvariantCulture);
var loader = new SourceTextureLoader();
var atlas = new Atlas(atlasPath, loader);
var data = new SkeletonBinary(atlas) { Scale = sourceScale }.ReadSkeletonData(skeletonPath);
var attachmentTypes=data.Skins.SelectMany(s=>s.Attachments.Values).Select(a=>a.GetType().Name).Distinct().OrderBy(s=>s).ToArray();
var slotBlends=data.Slots.Select(s=>s.BlendMode.ToString()).Distinct().OrderBy(s=>s).ToArray();
var animations=data.Animations.Select(a=>new {name=a.Name,duration=a.Duration}).ToArray();
if(attachmentTypes.Contains("ClippingAttachment")) throw new InvalidDataException("Unsupported source ClippingAttachment: cannot silently drop clipping geometry");
if(attachmentTypes.Any(t=>t!="RegionAttachment" && t!="MeshAttachment")) throw new InvalidDataException("Unsupported source attachment: "+string.Join(",",attachmentTypes));
if(slotBlends.Any(b=>b!="Normal")) throw new InvalidDataException("Unsupported widget blend mode: "+string.Join(",",slotBlends));
var animationName = "idle";
if (animationName != "idle" && animationName != "standby") throw new ArgumentException("Unsupported animation request");
var loop = animationName == "idle";
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
    skeleton.SetSkin(data.DefaultSkin ?? data.Skins.Items[0]);
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
            uv = quad.UVs; triangles = new[] { 0, 1, 2, 2, 3, 0 };
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
if (uniquePoses.Count < 2) throw new InvalidDataException("Idle has no observable mesh deformation");
var sources = new[] { skeletonPath, atlasPath, assetPath, Path.Combine(root,"ref/config-tables/编译客户端配置/CSV/Arms.csv"), Path.Combine(root,"ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_troops/UIArmyTrain/UI_Win_TrainArmy.prefab"), Path.Combine(root,"ref/client-unity/Assets/Third/Spine/spine-unity/Modules/SkeletonGraphic/Shaders/Spine-SkeletonGraphic.shader") }.Concat(loader.Paths).Concat(loader.Paths.Select(p => p+".meta")).Distinct().Select(path => new {
    path = Path.GetRelativePath(root, path).Replace('\\', '/'),
    sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))).ToLowerInvariant() });
var output = Path.Combine(root, $"Content/RokData/TrainArmy/arms_{armsId}_idle_mesh.json");
Directory.CreateDirectory(Path.GetDirectoryName(output)!);
File.WriteAllText(output, JsonSerializer.Serialize(new {
    schema = "rok.train_army.spine_mesh.v1", schema_version = 1, arms_id = armsId, arms_show = model, animation = animationName, loop,
    pma = new { texture = "source atlas pixels unchanged", vertex_colors = true, source_blend = "One OneMinusSrcAlpha", exported_rgba = "straight attachment*slot*skeleton tint; consumer must apply source PMA once" },
    source_transform = new { skeleton_scale = sourceScale, rect_scale = new[] {.85f,.85f,.85f}, anchored_position = new[] {66f,63f}, anchor = new[] {.5f,0f}, pivot = new[] {.5f,.5f}, canvas_multiplier = "canvas.referencePixelsPerUnit (resolve from actual UI canvas; not commander projection)" },
    attachment_types = attachmentTypes, slot_blends = slotBlends, animations,
    duration = animation.Duration, source_scale = sourceScale, frame_count = frameCount,
    sampling = "Spine source evaluation at uniform <=1/60 second intervals; not a raster flipbook",
    coordinate_system = "source XY y-up; UVs follow source AtlasAttachmentLoader",
    bounds = new[] { minX, minY, maxX, maxY }, unique_poses = uniquePoses.Count, sources, frames
}));
return new { arms_id = armsId, arms_show = model, status = "exported", duration = animation.Duration, frame_count = frameCount, unique_poses = uniquePoses.Count, attachment_types = attachmentTypes, slot_blends = slotBlends, animations, sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(output))).ToLowerInvariant(), output = $"Saved/RokUnityMigration/TrainArmy/arms_{armsId}_idle_mesh.json" };

}
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
