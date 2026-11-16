using System.Security.Cryptography;
using System.Text.Json;
using Spine;

// Offline internal migration tool. Uses the reference runtime, including its
// Spine license requirements; does not redistribute that runtime into UE.
// CaptainMediator.UpdateHero loads heroModel and plays idle, loop=true.
if (args.Length < 2 || args.Length > 3)
    throw new ArgumentException("Usage: RokSpineExport <Rok project root> <--roster|hero id|--quest-npc> [--list|idle|standby|NPC key]");
var root = Path.GetFullPath(args[0]);
if (args[1] == "--quest-npc")
{
    if (args.Length != 3 || args[2] is not ("NPC_01_SD" or "NPC_07_SD"))
        throw new ArgumentException("Expected an NPC key from TaskChapterData.imgAgeRes");
    var key = args[2];
    var directory = Path.Combine(root, $"ref/client-unity/Assets/BundleAssets/Monster_SD/{key}/Materials");
    var skeleton = Directory.GetFiles(directory, "*.skel.bytes").Single();
    var atlas = Directory.GetFiles(directory, "*.atlas.txt").Single();
    var asset = Path.Combine(directory, $"{key}.asset");
    var loader = new SourceTextureLoader();
    var scale = ReadScale(asset);
    var data = new SkeletonBinary(new Atlas(atlas, loader)) { Scale = scale }.ReadSkeletonData(skeleton);
    ExportClip(0, "idle", data, scale, loader, skeleton, atlas, asset, "QuestGuide", key, 30);
    return;
}

using var config = JsonDocument.Parse(File.ReadAllBytes(Path.Combine(root, "Content/RokData/ConfigTables/Hero.json")));
var rows = config.RootElement.GetProperty("rows").EnumerateArray();
var roster = args[1] == "--roster";
var requestedId = roster ? 0 : int.Parse(args[1]);
var request = args.Length == 3 ? args[2] : "idle";
if ((roster && args.Length != 2) || request is not ("--list" or "idle" or "standby"))
    throw new ArgumentException("Unsupported animation request");

foreach (var row in rows)
{
    var heroId = row.GetProperty("ID").GetInt32();
    if (roster ? row.GetProperty("listDisplay").GetInt32() == 1 : heroId != requestedId)
        continue;
    var model = row.GetProperty("heroModel").GetString();
    var directory = Path.Combine(root, $"ref/client-unity/Assets/BundleAssets/Troop/spine_hero/{model}/Materials");
    var skeletonPath = Directory.GetFiles(directory, "*.skel.bytes").Single();
    var atlasPath = Directory.GetFiles(directory, "*.atlas.txt").Single();
    var assetPath = Path.Combine(directory, $"{model}.asset");
    var sourceScale = ReadScale(assetPath);
    var loader = new SourceTextureLoader();
    var atlas = new Atlas(atlasPath, loader);
    var data = new SkeletonBinary(atlas) { Scale = sourceScale }.ReadSkeletonData(skeletonPath);
    if (request == "--list")
    {
        Console.WriteLine(JsonSerializer.Serialize(new {
            hero_id = heroId,
            animations = data.Animations.Select(a => new { name = a.Name, duration = a.Duration })
        }));
        return;
    }
    if (roster)
    {
        ExportClip(heroId, "idle", data, sourceScale, loader, skeletonPath, atlasPath, assetPath);
        if (data.FindAnimation("standby") != null)
            ExportClip(heroId, "standby", data, sourceScale, loader, skeletonPath, atlasPath, assetPath);
    }
    else
    {
        ExportClip(heroId, request, data, sourceScale, loader, skeletonPath, atlasPath, assetPath);
        return;
    }
}
if (!roster) throw new ArgumentException($"Hero {requestedId} is missing from Hero.json");

float ReadScale(string assetPath)
{
    var match = System.Text.RegularExpressions.Regex.Match(File.ReadAllText(assetPath), @"(?m)^  scale: ([0-9.]+)\r?$");
    if (!match.Success) throw new InvalidDataException("Missing source skeleton scale");
    return float.Parse(match.Groups[1].Value, System.Globalization.CultureInfo.InvariantCulture);
}

void ExportClip(int heroId, string animationName, SkeletonData data, float sourceScale,
    SourceTextureLoader loader, string skeletonPath, string atlasPath, string assetPath,
    string destination = "Commander", string sourceKey = null, int fps = 60)
{
    var key = sourceKey ?? $"hero_{heroId}";
    var loop = animationName == "idle";
    var animation = data.FindAnimation(animationName) ?? throw new InvalidDataException($"Source has no {animationName} animation");
    var textureFolder = $"Content/RokData/{destination}/Textures/{key}";
    var texturePaths = loader.Paths.Distinct().ToDictionary(path => path,
        path => $"{textureFolder}/{Path.GetFileName(path)}");
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
                texture = texturePaths[(string)region.page.rendererObject],
                blend = slot.Data.BlendMode.ToString(), xy, uv, triangles,
                rgba = new[] { red * slot.R * skeleton.R, green * slot.G * skeleton.G,
                    blue * slot.B * skeleton.B, alpha * slot.A * skeleton.A } });
        }
        if (draws.Count == 0) throw new InvalidDataException("Empty sampled pose");
        uniquePoses.Add(JsonSerializer.Serialize(pose));
        frames.Add(new { time, draws });
    }
    if (uniquePoses.Count < 2) throw new InvalidDataException("Idle has no observable mesh deformation");
    var sources = new[] { skeletonPath, atlasPath, assetPath }.Concat(loader.Paths).Distinct().Select(path => new {
        path = Path.GetRelativePath(root, path).Replace('\\', '/'),
        sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))).ToLowerInvariant() });
    var output = Path.Combine(root, $"Content/RokData/{destination}/{key}_{animationName}_mesh.json");
    Directory.CreateDirectory(Path.Combine(root, textureFolder));
    foreach (var (source, target) in texturePaths)
        File.Copy(source, Path.Combine(root, target), overwrite: true);
    File.WriteAllText(output, JsonSerializer.Serialize(new {
        schema_version = 1, hero_id = heroId, animation = animationName, loop,
        duration = animation.Duration, source_scale = sourceScale, frame_count = frameCount,
        sampling = $"Spine source evaluation at uniform <=1/{fps} second intervals",
        coordinate_system = "source XY y-up; UVs follow source AtlasAttachmentLoader",
        bounds = new[] { minX, minY, maxX, maxY }, unique_poses = uniquePoses.Count, sources, frames
    }));
    Console.WriteLine($"Exported {heroId}: duration={animation.Duration}, frames={frameCount}, distinct poses={uniquePoses.Count}, {output}");
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
