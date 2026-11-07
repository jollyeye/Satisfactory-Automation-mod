# Automated Machines: creates the mod's few assets from the files in Tools/.
#   /AutomatedMachines/Icons/T_AM_TabIcon     the Automation tab button icon (UI texture)
#   /AutomatedMachines/Icons/T_AM_DropArrow   dropdown arrow, T_AM_Trash: remove button (UI textures)
#   /AutomatedMachines/Icons/T_AM_MarkerIcon  the Wi-Fi picture of the marker
#   /AutomatedMachines/Marker/M_AM_Marker     marker material: unlit, turns to the camera in the shader
#   /AutomatedMachines/Marker/SM_AM_Marker    marker mesh: a flat 1 m square
# Run inside the editor: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
# Safe to re-run: existing assets are replaced / rebuilt, not duplicated.
import os

import unreal

LOG_PATH = r"D:\SFMods\am-assets-log.txt"
_log = open(LOG_PATH, "w", encoding="utf-8")


def log(msg):
    _log.write(msg + "\n")
    _log.flush()
    unreal.log("[AutomatedMachines] " + msg)


ROOT = "/AutomatedMachines"
TOOLS = os.path.dirname(os.path.abspath(__file__))
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty

unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT], True)


def import_file(filename, destination, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths") or [])
    log("Imported %s -> %s" % (os.path.basename(filename), paths))
    return paths


def load(path):
    try:
        return unreal.load_asset(path)
    except Exception as e:
        log("  ! cannot load %s: %s" % (path, e))
        return None


def save(asset):
    eal.save_loaded_asset(asset, only_if_is_dirty=False)


# --- Textures ----------------------------------------------------------------------------------
UI_ICONS = ["T_AM_TabIcon", "T_AM_DropArrow", "T_AM_Trash", "T_AM_TrashLarge", "T_AM_Locate", "T_AM_SwitchOff", "T_AM_SwitchOn", "T_AM_SwitchOffDisabled", "T_AM_SwitchOnDisabled"]
for name in UI_ICONS + ["T_AM_MarkerIcon"]:
    import_file(os.path.join(TOOLS, "Icons", name + ".png"), ROOT + "/Icons")

for name in UI_ICONS:
    icon = load(ROOT + "/Icons/" + name)
    if icon:
        icon.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        icon.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        icon.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        icon.set_editor_property("srgb", True)
        save(icon)
        log("  %s: UI texture, no mips" % name)

marker_icon = load(ROOT + "/Icons/T_AM_MarkerIcon")
if marker_icon:
    marker_icon.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    marker_icon.set_editor_property("srgb", True)
    save(marker_icon)


# --- Marker material ---------------------------------------------------------------------------
def node(mat, cls, x, y, **props):
    n = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, out, b, inp):
    MEL.connect_material_expressions(a, out, b, inp)


def vec3(mat, x, y, v):
    return node(mat, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(v[0], v[1], v[2], 1.0))


mat_path = ROOT + "/Marker/M_AM_Marker"
if eal.does_asset_exist(mat_path):
    mat = load(mat_path)
    MEL.delete_all_material_expressions(mat)
    log("Rebuilding " + mat_path)
else:
    mat = asset_tools.create_asset("M_AM_Marker", ROOT + "/Marker", unreal.Material, unreal.MaterialFactoryNew())
    log("Created " + mat_path)

mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("used_with_instanced_static_meshes", True)

# Picture: emissive white (times Color and Brightness), cut out by the texture's alpha.
tex = node(mat, unreal.MaterialExpressionTextureSample, -700, 0)
if marker_icon:
    tex.set_editor_property("texture", marker_icon)
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
colour = node(mat, unreal.MaterialExpressionVectorParameter, -700, 250, parameter_name="Color",
              default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
brightness = node(mat, unreal.MaterialExpressionScalarParameter, -700, 400, parameter_name="Brightness", default_value=2.0)
tinted = node(mat, unreal.MaterialExpressionMultiply, -400, 100)
link(tex, "RGB", tinted, "A")
link(colour, "", tinted, "B")
emissive = node(mat, unreal.MaterialExpressionMultiply, -250, 150)
link(tinted, "", emissive, "A")
link(brightness, "", emissive, "B")
MEL.connect_material_property(emissive, "", MP.MP_EMISSIVE_COLOR)
MEL.connect_material_property(tex, "A", MP.MP_OPACITY_MASK)

# Facing the camera on every axis: each corner is placed at
#   (centre of the instance) + camera right * x + camera up * y
# where x, y come from the corner's UV. The offset is that minus where the corner is now.
uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 700)
right = node(mat, unreal.MaterialExpressionTransform, -1400, 850,
             transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW,
             transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
link(vec3(mat, -1650, 850, (1.0, 0.0, 0.0)), "", right, "")
up = node(mat, unreal.MaterialExpressionTransform, -1400, 1000,
          transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW,
          transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
link(vec3(mat, -1650, 1000, (0.0, 1.0, 0.0)), "", up, "")
size = node(mat, unreal.MaterialExpressionScalarParameter, -1400, 1150, parameter_name="Size", default_value=100.0)

corner = node(mat, unreal.MaterialExpressionCustom, -1100, 850)
corner.set_editor_property("code", "float a = (UV.x - 0.5) * Size;\nfloat b = (0.5 - UV.y) * Size;\nreturn normalize(R) * a + normalize(U) * b;")
corner.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
corner.set_editor_property("description", "BillboardCorner")
inputs = []
for name in ("UV", "R", "U", "Size"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", name)
    inputs.append(ci)
corner.set_editor_property("inputs", inputs)
link(uv, "", corner, "UV")
link(right, "", corner, "R")
link(up, "", corner, "U")
link(size, "", corner, "Size")

# The instance's own origin: "Instance & Particle space" if this engine has it, else local space.
PTS = unreal.MaterialPositionTransformSource
instance_space = getattr(PTS, "TRANSFORMPOSSOURCE_INSTANCE", None) or PTS.TRANSFORMPOSSOURCE_LOCAL
log("  pivot from %s" % instance_space)
pivot = node(mat, unreal.MaterialExpressionTransformPosition, -1100, 1100,
             transform_source_type=instance_space,
             transform_type=PTS.TRANSFORMPOSSOURCE_WORLD)
link(vec3(mat, -1350, 1300, (0.0, 0.0, 0.0)), "", pivot, "")
world_pos = node(mat, unreal.MaterialExpressionWorldPosition, -1100, 1300,
                 world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
to_pivot = node(mat, unreal.MaterialExpressionSubtract, -850, 1150)
link(pivot, "", to_pivot, "A")
link(world_pos, "", to_pivot, "B")
offset = node(mat, unreal.MaterialExpressionAdd, -600, 950)
link(to_pivot, "", offset, "A")
link(corner, "", offset, "B")
MEL.connect_material_property(offset, "", MP.MP_WORLD_POSITION_OFFSET)

MEL.recompile_material(mat)
save(mat)
log("  material: unlit, masked, two-sided, camera-facing")


# --- Underglow material (decal under automated machines) ---------------------------------------
# Projected down onto the floor around a machine: cyan light, full from the centre to FadeStart (per
# axis, as a part of the way to the decal's edge: R along U, G along V; set per machine by AAMMarkers
# so the fade is 1.5 m wide), then a smooth fade to nothing at the edge, with rounded corners.
# Inside it, a circuit-board pattern (7 Nov 2026): glowing traces on a 40 cm grid (and a finer 20 cm
# grid), with 45-degree bends and round pads at their ends, over a faint glow; sized in cm through
# HalfSize (per machine) so it is the same on every machine. The decal reaches 1.3 m above the floor
# and fades out between 40 and 110 cm, so the traces climb a little onto the machine (also on its
# sides: no wall limit any more). It fades a little with depth below the floor (slopes). Only emissive
# and opacity are connected, so the decal adds light and leaves the surface's own colour alone.
glow_path = ROOT + "/Marker/M_AM_Underglow"
if eal.does_asset_exist(glow_path):
    glow = load(glow_path)
    MEL.delete_all_material_expressions(glow)
    log("Rebuilding " + glow_path)
else:
    glow = asset_tools.create_asset("M_AM_Underglow", ROOT + "/Marker", unreal.Material, unreal.MaterialFactoryNew())
    log("Created " + glow_path)

glow.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
glow.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

g_uv = node(glow, unreal.MaterialExpressionTextureCoordinate, -900, 0)
# Height of the lit pixel above the machine's anchor (BaseZ, per machine), and its position
# relative to the camera (small numbers, for the surface's slope from screen derivatives).
g_world = node(glow, unreal.MaterialExpressionWorldPosition, -1300, 600)
g_world_z = node(glow, unreal.MaterialExpressionComponentMask, -1100, 600, r=False, g=False, b=True, a=False)
link(g_world, "", g_world_z, "")
g_base_z = node(glow, unreal.MaterialExpressionScalarParameter, -1100, 700, parameter_name="BaseZ", default_value=0.0)
g_height = node(glow, unreal.MaterialExpressionSubtract, -900, 650)
link(g_world_z, "", g_height, "A")
link(g_base_z, "", g_height, "B")
g_fade_start = node(glow, unreal.MaterialExpressionVectorParameter, -900, 150, parameter_name="FadeStart",
                    default_value=unreal.LinearColor(0.75, 0.75, 0.0, 0.0))
g_half = node(glow, unreal.MaterialExpressionVectorParameter, -900, 300, parameter_name="HalfSize",
              default_value=unreal.LinearColor(300.0, 300.0, 0.0, 0.0))
g_shape = node(glow, unreal.MaterialExpressionCustom, -600, 50)
CIRCUIT = r"""
float2 P = abs(UV * 2.0 - 1.0);
float2 F = clamp(FadeStart.xy, 0.0, 0.98);
float2 D = max(P - F, 0.0) / (1.0 - F);
float G = saturate(1.0 - length(D));
G = G * G;
float Up = 1.0 - smoothstep(40.0, 110.0, H);
float Down = 1.0 - saturate(-H / 400.0);
float2 L = (UV * 2.0 - 1.0) * HS.xy;
float Line = 0.0;
for (int K = 0; K < 2; K++)
{
    float Cell = K == 0 ? 40.0 : 20.0;
    float Dens = K == 0 ? 0.42 : 0.22;
    float HalfW = K == 0 ? 1.2 : 0.7;
    float Seed = K == 0 ? 0.0 : 17.0;
    float2 Pos = L / Cell + (K == 0 ? float2(0.0, 0.0) : float2(0.5, 0.5));
    float2 C = floor(Pos);
    float2 Q = Pos - C - 0.5;
    float2 S = float2(127.1 + Seed, 311.7 - Seed);
    float R = step(frac(sin(dot(C * 2.0 + float2(1.0, 0.0), S)) * 43758.5453), Dens);
    float Lf = step(frac(sin(dot(C * 2.0 - float2(1.0, 0.0), S)) * 43758.5453), Dens);
    float U = step(frac(sin(dot(C * 2.0 + float2(0.0, 1.0), S)) * 43758.5453), Dens);
    float Dn = step(frac(sin(dot(C * 2.0 - float2(0.0, 1.0), S)) * 43758.5453), Dens);
    float N = R + Lf + U + Dn;
    float Dist = 10.0;
    bool Corner = N == 2.0 && R * Lf == 0.0 && U * Dn == 0.0;
    if (Corner)
    {
        float2 A = float2(R - Lf, 0.0);
        float2 B = float2(0.0, U - Dn);
        float2 P0 = A * 0.5; float2 P1 = A * 0.18; float2 P2 = B * 0.18; float2 P3 = B * 0.5;
        float2 E = P1 - P0; float2 W = Q - P0;
        Dist = min(Dist, length(W - E * saturate(dot(W, E) / dot(E, E))));
        E = P2 - P1; W = Q - P1;
        Dist = min(Dist, length(W - E * saturate(dot(W, E) / dot(E, E))));
        E = P3 - P2; W = Q - P2;
        Dist = min(Dist, length(W - E * saturate(dot(W, E) / dot(E, E))));
    }
    else
    {
        float AX = abs(Q.y) + max(Q.x - 0.5, 0.0) + max(-Q.x, 0.0);
        float BX = abs(Q.y) + max(-Q.x - 0.5, 0.0) + max(Q.x, 0.0);
        float AY = abs(Q.x) + max(Q.y - 0.5, 0.0) + max(-Q.y, 0.0);
        float BY = abs(Q.x) + max(-Q.y - 0.5, 0.0) + max(Q.y, 0.0);
        Dist = min(Dist, AX + (1.0 - R) * 10.0);
        Dist = min(Dist, BX + (1.0 - Lf) * 10.0);
        Dist = min(Dist, AY + (1.0 - U) * 10.0);
        Dist = min(Dist, BY + (1.0 - Dn) * 10.0);
        if (N == 1.0 || N >= 3.0)
        {
            float Pad = 0.1 * (20.0 / Cell) * (K == 0 ? 1.0 : 0.8);
            Dist = min(Dist, abs(length(Q) - Pad));
            Dist = min(Dist, max(length(Q) - Pad * 0.45, 0.0));
        }
    }
    float Cm = Dist * Cell;
    float Core = 1.0 - smoothstep(HalfW, HalfW * 1.8, Cm);
    float Halo = exp(-Cm / 5.0) * 0.4;
    Line = max(Line, max(Core, Halo) * (K == 0 ? 1.0 : 0.75));
}
float Pattern = saturate(0.15 + Line);
return G * Up * Down * Pattern;
"""
g_shape.set_editor_property("code", CIRCUIT)
g_shape.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
g_shape.set_editor_property("description", "UnderglowShape")
g_inputs = []
for name in ("UV", "FadeStart", "H", "HS"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", name)
    g_inputs.append(ci)
g_shape.set_editor_property("inputs", g_inputs)
link(g_uv, "", g_shape, "UV")
link(g_fade_start, "", g_shape, "FadeStart")
link(g_height, "", g_shape, "H")
link(g_half, "", g_shape, "HS")

g_colour = node(glow, unreal.MaterialExpressionVectorParameter, -600, 250, parameter_name="Color",
                default_value=unreal.LinearColor(0.12, 0.7, 1.0, 1.0))
g_brightness = node(glow, unreal.MaterialExpressionScalarParameter, -600, 400, parameter_name="Brightness", default_value=1.6)
g_strength = node(glow, unreal.MaterialExpressionScalarParameter, -600, 500, parameter_name="Strength", default_value=0.7)
g_tint = node(glow, unreal.MaterialExpressionMultiply, -350, 250)
link(g_colour, "", g_tint, "A")
link(g_brightness, "", g_tint, "B")
g_emissive = node(glow, unreal.MaterialExpressionMultiply, -150, 150)
link(g_tint, "", g_emissive, "A")
link(g_shape, "", g_emissive, "B")
g_opacity = node(glow, unreal.MaterialExpressionMultiply, -150, 400)
link(g_shape, "", g_opacity, "A")
link(g_strength, "", g_opacity, "B")
MEL.connect_material_property(g_emissive, "", MP.MP_EMISSIVE_COLOR)
MEL.connect_material_property(g_opacity, "", MP.MP_OPACITY)
MEL.recompile_material(glow)
save(glow)
log("  underglow: deferred decal, translucent, emissive cyan circuit lines")


# --- Marker mesh -------------------------------------------------------------------------------
mesh_options = unreal.FbxImportUI()
mesh_options.set_editor_property("import_mesh", True)
mesh_options.set_editor_property("import_as_skeletal", False)
mesh_options.set_editor_property("import_materials", False)
mesh_options.set_editor_property("import_textures", False)
mesh_options.set_editor_property("import_animations", False)
import_file(os.path.join(TOOLS, "Marker", "SM_AM_Marker.obj"), ROOT + "/Marker", mesh_options)

mesh = load(ROOT + "/Marker/SM_AM_Marker")
if mesh:
    mesh.set_material(0, mat)
    # The material moves the corners, up to the square's size away: keep the bounds big enough.
    mesh.set_editor_property("positive_bounds_extension", unreal.Vector(80.0, 80.0, 80.0))
    mesh.set_editor_property("negative_bounds_extension", unreal.Vector(80.0, 80.0, 80.0))
    save(mesh)
    log("  mesh ready, material assigned")
else:
    log("  ! marker mesh missing after import")

log("Done")
