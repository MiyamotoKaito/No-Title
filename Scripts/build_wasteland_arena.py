# -*- coding: utf-8 -*-
"""
荒野の1v1円形バトルステージ (Lvl_WastelandArena) を自動生成するエディタ用スクリプト。

使い方:
  1. Edit > Plugins で「Python Editor Script Plugin」を有効化してエディタを再起動
  2. Tools > Execute Python Script... でこのファイルを選択
     (またはコンソールで: py "C:/UnrealProjects/Slayer/Scripts/build_wasteland_arena.py")

生成物:
  /Game/Level/Lvl_WastelandArena          レベル本体
  /Game/Level/Materials/M_ArenaGround     ワールド座標タイリングの地面マテリアル

既にレベルが存在する場合は上書きせずに中断する。
"""
import math
import random

import unreal

# ---------------------------------------------------------------------------
# 設定
# ---------------------------------------------------------------------------
LEVEL_PATH = "/Game/Level/Lvl_WastelandArena"
MATERIAL_DIR = "/Game/Level/Materials"
GROUND_MATERIAL_NAME = "M_ArenaGround"

ARENA_RADIUS = 4000.0          # 行動範囲の半径 (cm) = 40m
BUILD_OUTSKIRTS = False        # 岩壁で外が見えないので外周の建物・山・砂煙は作らない (軽量化)
SPAWN_DISTANCE = 1500.0        # 中心からの開始位置距離
RANDOM_SEED = 20261027

PA = "/Game/JustBStudios/PostApo"
MESH = PA + "/Meshes/"
MAT = PA + "/Materials/"
TEX = PA + "/Textures/"
BP = PA + "/Blueprints/"

PLAYER_GAME_MODE = "/Game/GenericShooter/GameMode/BP_TestGameMode"
ENEMY_CHARACTER = "/Game/GenericShooter/Characters/BP_TestCharacter_Enemy"  # 騎士アセット導入までの仮置き

random.seed(RANDOM_SEED)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log("[WastelandArena] " + msg)


def warn(msg):
    unreal.log_warning("[WastelandArena] " + msg)


def load(path):
    asset = eal.load_asset(path)
    if asset is None:
        warn("アセットが見つかりません: " + path)
    return asset


def rot(yaw=0.0, pitch=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def polar(radius, angle_deg, z=0.0):
    a = math.radians(angle_deg)
    return unreal.Vector(math.cos(a) * radius, math.sin(a) * radius, z)


def yaw_to_center(loc):
    return math.degrees(math.atan2(-loc.y, -loc.x))


# ---------------------------------------------------------------------------
# スポーンヘルパー
# ---------------------------------------------------------------------------
def mesh_bounds(mesh):
    b = mesh.get_bounds()
    return b.origin, b.box_extent


def height_scale(mesh, target_height):
    """メッシュの高さが target_height (cm) になる等倍スケールを返す"""
    _, ext = mesh_bounds(mesh)
    return target_height / max(ext.z * 2.0, 1.0)


def size_scale(mesh, target_size):
    """メッシュの水平方向最大幅が target_size (cm) になる等倍スケールを返す"""
    _, ext = mesh_bounds(mesh)
    return target_size / max(ext.x * 2.0, ext.y * 2.0, 1.0)


def spawn_mesh(mesh_name, loc, yaw=0.0, scale=1.0, folder="Props", sink=0.0,
               collision=True, pitch=0.0, roll=0.0, label=None):
    """メッシュを接地させてスポーンする。sink は埋め込む割合(高さに対する比率)"""
    mesh = load(MESH + mesh_name)
    if mesh is None:
        return None
    origin, ext = mesh_bounds(mesh)
    scale_v = scale if isinstance(scale, unreal.Vector) else unreal.Vector(scale, scale, scale)
    bottom = (origin.z - ext.z) * scale_v.z
    height = ext.z * 2.0 * scale_v.z
    loc = unreal.Vector(loc.x, loc.y, loc.z - bottom - height * sink)

    actor = eas.spawn_actor_from_object(mesh, loc, rot(yaw, pitch, roll))
    actor.set_actor_scale3d(scale_v)
    actor.set_folder_path(folder)
    if label:
        actor.set_actor_label(label)
    if not collision:
        disable_collision(actor.static_mesh_component)
    return actor


def disable_collision(comp):
    """メッシュ側のデフォルトコリジョンに戻らないよう use_default_collision を切ってから NoCollision にする"""
    comp.modify()
    set_props(comp, use_default_collision=False)
    body = comp.get_editor_property("body_instance")
    set_props(body, collision_profile_name="NoCollision", collision_enabled=unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("body_instance", body)
    comp.set_collision_profile_name("NoCollision")
    set_props(comp, can_character_step_up_on=unreal.CanBeCharacterBase.ECB_NO)


def spawn_class(cls, loc, yaw=0.0, folder="", label=None):
    actor = eas.spawn_actor_from_class(cls, loc, rot(yaw))
    if folder:
        actor.set_folder_path(folder)
    if label:
        actor.set_actor_label(label)
    return actor


def set_props(obj, **props):
    """存在しないプロパティ名は警告だけ出して無視する(エンジンバージョン差吸収用)"""
    for k, v in props.items():
        try:
            obj.set_editor_property(k, v)
        except Exception as e:  # noqa
            warn("プロパティ設定失敗 {}.{}: {}".format(obj.get_class().get_name(), k, e))


# ---------------------------------------------------------------------------
# 地面マテリアル (ワールド座標タイリング + マクロバリエーションで砂地/岩混じりの砂をブレンド)
# ---------------------------------------------------------------------------
def sampler_type_for(tex):
    cs = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    TC = unreal.TextureCompressionSettings
    ST = unreal.MaterialSamplerType
    if cs == TC.TC_NORMALMAP:
        return ST.SAMPLERTYPE_NORMAL
    if cs == TC.TC_MASKS:
        return ST.SAMPLERTYPE_MASKS
    if cs in (TC.TC_GRAYSCALE, TC.TC_ALPHA):
        return ST.SAMPLERTYPE_GRAYSCALE if srgb else ST.SAMPLERTYPE_LINEAR_GRAYSCALE
    return ST.SAMPLERTYPE_COLOR if srgb else ST.SAMPLERTYPE_LINEAR_COLOR


def build_ground_material():
    full_path = MATERIAL_DIR + "/" + GROUND_MATERIAL_NAME
    if eal.does_asset_exist(full_path):
        log("地面マテリアルは既存のものを使用: " + full_path)
        return load(full_path)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(GROUND_MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def expr(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)

    def connect(a, a_out, b, b_in):
        mel.connect_material_expressions(a, a_out, b, b_in)

    def scalar(name, value, x, y):
        e = expr(unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    def tex_sample(tex_path, uv, x, y):
        tex = load(tex_path)
        e = expr(unreal.MaterialExpressionTextureSample, x, y)
        e.set_editor_property("texture", tex)
        e.set_editor_property("sampler_type", sampler_type_for(tex))
        connect(uv, "", e, "UVs")
        return e

    def lerp(a, a_out, b, b_out, alpha, alpha_out, x, y):
        e = expr(unreal.MaterialExpressionLinearInterpolate, x, y)
        connect(a, a_out, e, "A")
        connect(b, b_out, e, "B")
        connect(alpha, alpha_out, e, "Alpha")
        return e

    # ワールド座標 XY
    wp = expr(unreal.MaterialExpressionWorldPosition, -2200, 0)
    xy = expr(unreal.MaterialExpressionComponentMask, -2000, 0)
    set_props(xy, r=True, g=True, b=False, a=False)
    connect(wp, "", xy, "")

    # 細部用UV (約3m周期) / マクロ用UV (約60m周期) / 色ムラ用UV (約150m周期)
    def uv_scaled(param_name, value, y):
        s = scalar(param_name, value, -2000, y)
        m = expr(unreal.MaterialExpressionMultiply, -1800, y)
        connect(xy, "", m, "A")
        connect(s, "", m, "B")
        return m

    uv_detail = uv_scaled("DetailTiling", 1.0 / 300.0, 200)
    uv_macro = uv_scaled("MacroTiling", 1.0 / 6000.0, 600)
    uv_color = uv_scaled("ColorVariationTiling", 1.0 / 15000.0, 900)

    sand_b = tex_sample(TEX + "T_Sand_01_B", uv_detail, -1500, -600)
    sand_n = tex_sample(TEX + "T_Sand_01_N", uv_detail, -1500, -350)
    sand_r = tex_sample(TEX + "T_Sand_01_R", uv_detail, -1500, -100)
    rock_b = tex_sample(TEX + "T_Rocky_Sand_B", uv_detail, -1500, 150)
    rock_n = tex_sample(TEX + "T_Rocky_Sand_N", uv_detail, -1500, 400)
    rock_orm = tex_sample(TEX + "T_Rocky_Sand_ORM", uv_detail, -1500, 650)
    macro = tex_sample(TEX + "T_Noise_01", uv_macro, -1500, 900)
    color_var = tex_sample(PA + "/Demo/T_Default_MacroVariation", uv_color, -1500, 1150)

    # ブレンド係数: ノイズのコントラストを上げて砂と岩場の境界をはっきりさせる
    contrast = scalar("BlendContrast", 3.0, -1200, 1000)
    sub = expr(unreal.MaterialExpressionSubtract, -1100, 900)
    connect(macro, "R", sub, "A")
    set_props(sub, const_b=0.5)
    mul_c = expr(unreal.MaterialExpressionMultiply, -1000, 900)
    connect(sub, "", mul_c, "A")
    connect(contrast, "", mul_c, "B")
    add_h = expr(unreal.MaterialExpressionAdd, -900, 900)
    connect(mul_c, "", add_h, "A")
    set_props(add_h, const_b=0.5)
    alpha = expr(unreal.MaterialExpressionSaturate, -800, 900)
    connect(add_h, "", alpha, "")

    base = lerp(sand_b, "RGB", rock_b, "RGB", alpha, "", -600, -300)

    # 色ムラ (0.75〜1.1倍) と全体のティント
    var_lerp = expr(unreal.MaterialExpressionLinearInterpolate, -600, 1150)
    set_props(var_lerp, const_a=0.75, const_b=1.1)
    connect(color_var, "R", var_lerp, "Alpha")
    tint = expr(unreal.MaterialExpressionVectorParameter, -600, -500)
    tint.set_editor_property("parameter_name", "GroundTint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 0.86, 0.72, 1.0))
    m1 = expr(unreal.MaterialExpressionMultiply, -400, -300)
    connect(base, "", m1, "A")
    connect(var_lerp, "", m1, "B")
    m2 = expr(unreal.MaterialExpressionMultiply, -250, -300)
    connect(m1, "", m2, "A")
    connect(tint, "", m2, "B")

    normal = lerp(sand_n, "RGB", rock_n, "RGB", alpha, "", -600, 200)
    rough = lerp(sand_r, "R", rock_orm, "G", alpha, "", -600, 450)
    # AO: 砂側は1.0固定、岩混じり側はORMのR
    ao = expr(unreal.MaterialExpressionLinearInterpolate, -600, 650)
    set_props(ao, const_a=1.0)
    connect(rock_orm, "R", ao, "B")
    connect(alpha, "", ao, "Alpha")

    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("地面マテリアルを作成: " + full_path)
    return mat


# ---------------------------------------------------------------------------
# レベル構築
# ---------------------------------------------------------------------------
def create_level():
    if eal.does_asset_exist(LEVEL_PATH):
        raise RuntimeError("既にレベルが存在します。上書きを避けるため中断しました: " + LEVEL_PATH)
    try:
        ok = les.new_level(LEVEL_PATH, False)  # 小規模アリーナなので World Partition 無し
    except TypeError:
        ok = les.new_level(LEVEL_PATH)
    if not ok:
        raise RuntimeError("レベル作成に失敗しました: " + LEVEL_PATH)
    log("レベルを作成: " + LEVEL_PATH)


def build_lighting():
    folder = "Lighting"

    # 低い夕日 (乾いたオレンジ)
    sun = spawn_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), folder=folder, label="Sun")
    sun.set_actor_rotation(rot(yaw=-35.0, pitch=-14.0), False)
    lc = sun.light_component
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc.set_intensity(7.0)
    lc.set_light_color(unreal.LinearColor(1.0, 0.70, 0.45, 1.0))
    set_props(lc, atmosphere_sun_light=True, cast_shadows=True,
              dynamic_shadow_distance_movable_light=12000.0)

    sky = spawn_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), folder=folder, label="SkyAtmosphere")
    sac = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    if sac:
        # 砂塵で霞んだ空: ミー散乱を強め、黄土色寄りに
        set_props(sac,
                  mie_scattering_scale=0.012,
                  mie_absorption_scale=0.004,
                  mie_anisotropy=0.75,
                  mie_scattering=unreal.LinearColor(0.85, 0.72, 0.55, 1.0))

    skylight = spawn_class(unreal.SkyLight, unreal.Vector(0, 0, 500), folder=folder, label="SkyLight")
    slc = skylight.light_component
    slc.set_mobility(unreal.ComponentMobility.MOVABLE)
    set_props(slc, real_time_capture=True, intensity=1.0)

    fog = spawn_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), folder=folder, label="DustFog")
    fc = fog.component
    set_props(fc, fog_density=0.035, fog_height_falloff=0.12, start_distance=800.0,
              enable_volumetric_fog=True, volumetric_fog_scattering_distribution=0.4,
              volumetric_fog_albedo=unreal.Color(r=230, g=200, b=160, a=255))
    # 5.1以降は *_luminance、古い版は *_color
    for name in ("fog_inscattering_luminance", "fog_inscattering_color"):
        try:
            fc.set_editor_property(name, unreal.LinearColor(0.55, 0.40, 0.26, 1.0))
            break
        except Exception:
            continue
    for name in ("directional_inscattering_luminance", "directional_inscattering_color"):
        try:
            fc.set_editor_property(name, unreal.LinearColor(0.9, 0.55, 0.3, 1.0))
            break
        except Exception:
            continue

    try:
        clouds = spawn_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0), folder=folder, label="VolumetricCloud")
    except Exception as e:  # noqa
        warn("VolumetricCloud の生成に失敗: {}".format(e))

    ppv = spawn_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), folder=folder, label="PP_Wasteland")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    pp_values = dict(
        override_color_saturation=True, color_saturation=unreal.Vector4(1.0, 1.0, 1.0, 0.82),
        override_color_contrast=True, color_contrast=unreal.Vector4(1.0, 1.0, 1.0, 1.12),
        override_color_gain=True, color_gain=unreal.Vector4(1.06, 0.98, 0.86, 1.0),
        override_vignette_intensity=True, vignette_intensity=0.55,
        override_film_grain_intensity=True, film_grain_intensity=0.12,
        override_bloom_intensity=True, bloom_intensity=0.8,
        override_auto_exposure_min_brightness=True, auto_exposure_min_brightness=0.8,
        override_auto_exposure_max_brightness=True, auto_exposure_max_brightness=2.0,
    )
    for k, v in pp_values.items():
        try:
            s.set_editor_property(k, v)
        except Exception as e:  # noqa
            warn("PostProcess 設定失敗 {}: {}".format(k, e))
    ppv.set_editor_property("settings", s)


def ground_size_for(radius):
    """岩壁の外側 (+30m) まで覆う地面キューブのスケール"""
    return (radius + 3000.0) * 2.0 / 100.0


def build_ground(ground_mat):
    # キューブを地面に (上面が Z=0)。外周を作らない場合は岩壁の外側まで覆える大きさに抑える
    cube = load("/Engine/BasicShapes/Cube")
    ground = eas.spawn_actor_from_object(cube, unreal.Vector(0, 0, -50), rot())
    size = 1000.0 if BUILD_OUTSKIRTS else ground_size_for(ARENA_RADIUS)
    ground.set_actor_scale3d(unreal.Vector(size, size, 1))
    ground.set_actor_label("Ground")
    ground.set_folder_path("Ground")
    ground.static_mesh_component.set_material(0, ground_mat)

    # 地面の汚れ・焦げ跡デカール
    decal_mats = ["MI_Road_Debris_01", "MI_Forest_Compost_01", "MI_Charcoal",
                  "MI_Grunge_01", "MI_Grunge_02", "MI_Grunge_03",
                  "MI_Decal_Grunge_01", "MI_Decal_Grunge_01_dark", "MI_Decal_Grunge_02_dark"]
    decal_mats = [m for m in (load(MAT + n) for n in decal_mats) if m]

    def add_decal(mat, loc, size, yaw):
        d = spawn_class(unreal.DecalActor, loc, folder="Ground/Decals")
        d.set_actor_rotation(rot(yaw=yaw, pitch=-90.0), False)
        d.decal.set_decal_material(mat)
        d.decal.set_editor_property("decal_size", unreal.Vector(200.0, size, size))

    # 中央: 戦いの痕跡っぽい焦げ跡
    charcoal = load(MAT + "MI_Charcoal")
    if charcoal:
        add_decal(charcoal, unreal.Vector(0, 0, 0), 450.0, 0.0)

    for _ in range(38):
        r = math.sqrt(random.random()) * ARENA_RADIUS
        loc = polar(r, random.uniform(0, 360))
        add_decal(random.choice(decal_mats), loc, random.uniform(250, 700), random.uniform(0, 360))


ROCKS = ["SM_Rock_01", "SM_Rock_02", "SM_Rock_03", "SM_Rock_04", "SM_Rock_05", "SM_Rock_06"]

# 岩壁の列: (内径オフセット最小, 最大, 高さ最小, 最大, 隣との重なり係数)
# 内側の列で視線を遮り、外側の高い列で隙間と稜線を埋める
ROCK_WALL_ROWS = (
    (150.0, 450.0, 900.0, 1400.0, 0.45),
    (700.0, 1100.0, 1300.0, 1900.0, 0.5),
)


def build_invisible_wall(radius, folder="Boundary/InvisibleWall", height=1000.0, base_z=0.0):
    cube = load("/Engine/BasicShapes/Cube")
    segments = max(64, int(radius / 40.0))
    wall_r = radius + 60.0
    seg_len = 2.0 * math.pi * wall_r / segments * 1.08
    for i in range(segments):
        ang = 360.0 * i / segments
        loc = polar(wall_r, ang, base_z + height * 0.5)
        wall = eas.spawn_actor_from_object(cube, loc, rot(yaw=ang + 90.0))
        wall.set_actor_scale3d(unreal.Vector(seg_len / 100.0, 0.4, height / 100.0))
        wall.set_folder_path(folder)
        wall.set_actor_label("ArenaWall_{:03d}".format(i))
        wall.set_actor_hidden_in_game(True)
        c = wall.static_mesh_component
        c.set_collision_profile_name("InvisibleWall")
        c.set_cast_shadow(False)
        set_props(c, visible_in_ray_tracing=False, affect_distance_field_lighting=False)


def build_rock_wall(radius, folder="Boundary/Rocks"):
    """プレイヤー目線で外が見えない高さの岩壁を2列で円周に並べる (コリジョンは不可視の壁が担当)"""
    for row, (rmin, rmax, hmin, hmax, overlap) in enumerate(ROCK_WALL_ROWS):
        ang = random.uniform(0, 4) + row * 3.0
        end = ang + 360.0
        while ang < end:
            name = random.choice(ROCKS)
            mesh = load(MESH + name)
            if mesh is None:
                return
            s = height_scale(mesh, random.uniform(hmin, hmax))
            r = radius + random.uniform(rmin, rmax)
            spawn_mesh(name, polar(r, ang), yaw=random.uniform(0, 360), scale=s,
                       folder=folder, sink=random.uniform(0.12, 0.22), collision=False,
                       roll=random.uniform(-6, 6), pitch=random.uniform(-6, 6))
            _, ext = mesh_bounds(mesh)
            width = min(ext.x, ext.y) * 2.0 * s  # 細い向きで計算して隙間を作らない
            ang += max(math.degrees(width * overlap / r), 1.0)


def build_small_rocks(radius, count=70, folder="Boundary/SmallRocks"):
    """岩壁の足元の小石 (内側に少しはみ出させて自然に)"""
    for _ in range(count):
        name = random.choice(ROCKS)
        mesh = load(MESH + name)
        s = height_scale(mesh, random.uniform(30.0, 120.0))
        r = radius + random.uniform(-350.0, 120.0)
        spawn_mesh(name, polar(r, random.uniform(0, 360)), yaw=random.uniform(0, 360), scale=s,
                   folder=folder, sink=0.3, collision=False)


def build_boundary():
    """円形の行動範囲: 不可視のコリジョン壁 + 外が見えない岩壁 + 足元の小石"""
    build_invisible_wall(ARENA_RADIUS)
    build_rock_wall(ARENA_RADIUS)
    build_small_rocks(ARENA_RADIUS)


def build_outskirts():
    """アリーナ外の荒廃した風景"""
    # 廃墟の建物 (北東側に集落跡)
    buildings = ["BP_Building_01", "BP_Building_03", "BP_Building_05", "BP_Building_07",
                 "BP_Building_02", "BP_Building_06"]
    for i, name in enumerate(buildings):
        cls = eal.load_blueprint_class(BP + name)
        if cls is None:
            warn("BP が見つかりません: " + name)
            continue
        a = 20.0 + i * 22.0 + random.uniform(-5, 5)
        r = random.uniform(5500.0, 8000.0)
        loc = polar(r, a)
        spawn_class(cls, loc, yaw=yaw_to_center(loc) + random.uniform(-25, 25), folder="Outskirts/Buildings")

    # 放棄された車・トレーラー・給水塔など (南〜西側)
    junk = [
        ("SM_Car", 3600.0, 200.0), ("SM_Car", 4400.0, 228.0), ("SM_Trailer", 5200.0, 250.0),
        ("SM_WaterTankMerged", 6000.0, 285.0), ("SM_Satellite_01", 4300.0, 165.0),
        ("SM_Solar_Panel", 3900.0, 150.0), ("SM_Solar_Panel", 4000.0, 143.0),
        ("SM_PaletteBoxes_01", 3300.0, 310.0), ("SM_PaletteBoxes_02", 3400.0, 318.0),
        ("SM_Barrel", 3100.0, 120.0), ("SM_Barrel", 3150.0, 123.0), ("SM_Barrel", 3050.0, 335.0),
        ("SM_Rubbish_Bin", 3200.0, 95.0), ("SM_Electric_Cabinet", 3500.0, 100.0),
        ("SM_Wheel", 3000.0, 205.0), ("SM_Box_01", 3250.0, 300.0), ("SM_Box_03", 3300.0, 302.0),
        ("SM_Gate_01", 4800.0, 60.0), ("SM_ConcreteBarrier", 3400.0, 190.0),
        ("SM_ConcreteBarrier", 3450.0, 196.0),
    ]
    for name, r, a in junk:
        loc = polar(r, a + random.uniform(-3, 3))
        spawn_mesh(name, loc, yaw=random.uniform(0, 360), folder="Outskirts/Junk", sink=0.02)

    # 倒れかけた電柱の列
    for i in range(7):
        loc = polar(4200.0 + i * 1400.0, -60.0 + i * 4.0)
        spawn_mesh("SM_Electric_Pole_01", loc, yaw=random.uniform(-10, 10) - 60.0,
                   pitch=random.uniform(-6, 6), roll=random.uniform(-9, 9), folder="Outskirts/Poles")

    # 大きな岩で地平線の平坦さを崩す
    rocks = ["SM_Rock_01", "SM_Rock_02", "SM_Rock_03", "SM_Rock_04", "SM_Rock_05", "SM_Rock_06"]
    for _ in range(45):
        name = random.choice(rocks)
        mesh = load(MESH + name)
        s = height_scale(mesh, random.uniform(500.0, 2200.0))
        loc = polar(random.uniform(4500.0, 22000.0), random.uniform(0, 360))
        spawn_mesh(name, loc, yaw=random.uniform(0, 360), scale=s, folder="Outskirts/BigRocks",
                   sink=random.uniform(0.2, 0.45))

    build_mountains()


# ---------------------------------------------------------------------------
# 遠景の山 (片面の背景メッシュなので正面を中心に向ける + 念のため両面化)
# ---------------------------------------------------------------------------
_front_cache = {}


def mesh_front_yaw(mesh):
    """
    メッシュの正面方向 (ローカルのヨー角, 度) を推定する。
    面法線を面積で重み付けして合計し、上向き成分が負なら巻き順が逆とみなして反転。
    XY成分が正面 (法線が向いている = 見える側)。推定できなければ None。
    """
    key = mesh.get_path_name()
    if key in _front_cache:
        return _front_cache[key]
    result = None
    try:
        desc = mesh.get_static_mesh_description(0)
        count = desc.get_triangle_count()
        stride = max(1, count // 8000)
        sx = sy = sz = 0.0
        for i in range(0, count, stride):
            tid = unreal.TriangleID()
            tid.set_editor_property("value", i)
            vids = desc.get_triangle_vertices(tid)
            if len(vids) != 3:
                continue
            p0, p1, p2 = [desc.get_vertex_position(v) for v in vids]
            ax, ay, az = p1.x - p0.x, p1.y - p0.y, p1.z - p0.z
            bx, by, bz = p2.x - p0.x, p2.y - p0.y, p2.z - p0.z
            sx += ay * bz - az * by
            sy += az * bx - ax * bz
            sz += ax * by - ay * bx
        if sz < 0.0:
            sx, sy = -sx, -sy
        if abs(sx) + abs(sy) > 1e-3:
            result = math.degrees(math.atan2(sy, sx))
            log("{} の正面方向を推定: {:.1f}度".format(mesh.get_name(), result))
    except Exception as e:  # noqa
        warn("{} の正面推定に失敗 (長辺を接線方向に合わせます): {}".format(mesh.get_name(), e))
    _front_cache[key] = result
    return result


def two_sided_mountain_material():
    name = "MI_Mountains_TwoSided"
    path = MATERIAL_DIR + "/" + name
    if eal.does_asset_exist(path):
        return load(path)
    parent = load(MAT + "MI_Mountains")
    if parent is None:
        return None
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mic = tools.create_asset(name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
                             unreal.MaterialInstanceConstantFactoryNew())
    mic.set_editor_property("parent", parent)
    ov = mic.get_editor_property("base_property_overrides")
    set_props(ov, override_two_sided=True, two_sided=True)
    mic.set_editor_property("base_property_overrides", ov)
    mel.update_material_instance(mic)
    eal.save_loaded_asset(mic)
    log("両面の山マテリアルを作成: " + path)
    return mic


def orient_mountain(actor, mesh, mat):
    loc = actor.get_actor_location()
    to_center = yaw_to_center(loc)
    front = mesh_front_yaw(mesh)
    if front is None:
        # 推定できない場合は長辺を円の接線方向に
        _, ext = mesh_bounds(mesh)
        yaw = to_center + (90.0 if ext.x > ext.y else 0.0)
    else:
        yaw = to_center - front
    actor.set_actor_rotation(rot(yaw=yaw + random.uniform(-10, 10)), False)
    if mat:
        comp = actor.static_mesh_component
        for i in range(comp.get_num_materials()):
            comp.set_material(i, mat)


def build_mountains():
    mountains = ["SM_Mountain_01", "SM_Mountain_02", "SM_Mountain_03"]
    mat = two_sided_mountain_material()
    count = 16
    for i in range(count):
        name = mountains[i % len(mountains)]
        mesh = load(MESH + name)
        if mesh is None:
            continue
        s = size_scale(mesh, random.uniform(26000.0, 42000.0))
        loc = polar(random.uniform(42000.0, 52000.0), 360.0 * i / count + random.uniform(-6, 6))
        actor = spawn_mesh(name, loc, scale=s, folder="Outskirts/Mountains", sink=0.08, collision=False)
        if actor:
            orient_mountain(actor, mesh, mat)


# ---------------------------------------------------------------------------
# 外周の集落跡 (建物クラスター + 周辺の小物 + 間に合わせのバリケード)
# ---------------------------------------------------------------------------
BUILDINGS = ["BP_Building_0{}".format(i) for i in range(1, 9)]
CLUTTER = ["SM_Barrel", "SM_Barrel", "SM_Box_01", "SM_Box_02", "SM_Box_03", "SM_Box_04",
           "SM_PaletteBoxes_01", "SM_PaletteBoxes_02", "SM_AirConditioner", "SM_Electric_Cabinet",
           "SM_Rubbish_01", "SM_Rubbish_02", "SM_Rubbish_03", "SM_Rubbish_Bin", "SM_Wheel",
           "SM_Plank_01", "SM_Plank_04", "SM_Plank_08", "SM_MetalPlate_02", "SM_MetalPlate_07"]
LANDMARKS = ["SM_WaterTankMerged", "SM_Water_Tank_Three_Pieces", "SM_Trailer", "SM_Car",
             "SM_Satellite_01", "SM_Solar_Panel", "SM_Electric_Pole_01"]
BARRICADE = ["SM_MetalPlate_01", "SM_MetalPlate_03", "SM_MetalPlate_04", "SM_MetalPlate_05",
             "SM_MetalPlate_06", "SM_MetalPlate_08", "SM_MetalPlate_10", "SM_MetalPlate_12",
             "SM_ConcreteBarrier", "SM_Railing_01", "SM_Railing_02"]


def actor_footprint(actor):
    origin, ext = actor.get_actor_bounds(False)
    return origin, ext, math.hypot(ext.x, ext.y)


def existing_footprints(folder_prefixes):
    result = []
    for a in eas.get_all_level_actors():
        folder = str(a.get_folder_path())
        if any(folder.startswith(p) for p in folder_prefixes):
            o, _, r = actor_footprint(a)
            result.append((o.x, o.y, r))
    return result


def overlaps(x, y, r, placed, margin=300.0):
    return any(math.hypot(x - px, y - py) < r + pr + margin for px, py, pr in placed)


def snap_actor_to_ground(actor, sink=0.0):
    origin, ext = actor.get_actor_bounds(False)
    bottom = origin.z - ext.z
    loc = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z - bottom - ext.z * 2.0 * sink), False, False)


def build_settlements(building_count=18, min_gap=900.0):
    placed = existing_footprints(["Outskirts/Buildings"])
    inner_limit = ARENA_RADIUS + min_gap
    spawned = 0
    for attempt in range(building_count * 12):
        if spawned >= building_count:
            break
        cls = eal.load_blueprint_class(BP + random.choice(BUILDINGS))
        if cls is None:
            continue
        a = random.uniform(0, 360)
        r = random.uniform(3600.0, 8500.0)
        loc = polar(r, a)
        actor = spawn_class(cls, loc, yaw=yaw_to_center(loc) + random.choice([0, 90, 180, 270]) + random.uniform(-12, 12),
                            folder="Outskirts/Buildings")
        origin, _, fr = actor_footprint(actor)
        # 円に近すぎたら外へ押し出す
        dist = math.hypot(origin.x, origin.y)
        if dist - fr < inner_limit:
            push = inner_limit - (dist - fr)
            d = polar(push, a)
            l2 = actor.get_actor_location()
            actor.set_actor_location(unreal.Vector(l2.x + d.x, l2.y + d.y, l2.z), False, False)
            origin, _, fr = actor_footprint(actor)
        if overlaps(origin.x, origin.y, fr, placed):
            eas.destroy_actor(actor)
            continue
        snap_actor_to_ground(actor, 0.01)
        placed.append((origin.x, origin.y, fr))
        spawned += 1

        # 建物まわりの小物
        for _ in range(random.randint(4, 8)):
            pa = random.uniform(0, 360)
            pr = fr + random.uniform(100.0, 700.0)
            p = unreal.Vector(origin.x, origin.y, 0) + polar(pr, pa)
            if math.hypot(p.x, p.y) < inner_limit:
                continue
            spawn_mesh(random.choice(CLUTTER), p, yaw=random.uniform(0, 360),
                       folder="Outskirts/Clutter", sink=0.02, collision=False)
        # 給水塔や車などの目印をたまに
        if random.random() < 0.6:
            pa = random.uniform(0, 360)
            p = unreal.Vector(origin.x, origin.y, 0) + polar(fr + random.uniform(500.0, 1100.0), pa)
            if math.hypot(p.x, p.y) > inner_limit + 300.0 and not overlaps(p.x, p.y, 400.0, placed, 100.0):
                spawn_mesh(random.choice(LANDMARKS), p, yaw=random.uniform(0, 360),
                           folder="Outskirts/Landmarks", sink=0.02)
    log("建物を {} 棟追加".format(spawned))

    # 円のすぐ外側にトタンやバリアを並べた即席のバリケード (いくつかの区画)
    sectors = 7
    for sct in range(sectors):
        start = 360.0 * sct / sectors + random.uniform(0, 20)
        span = random.uniform(18.0, 32.0)
        r = ARENA_RADIUS + random.uniform(650.0, 950.0)
        ang = start
        while ang < start + span:
            name = random.choice(BARRICADE)
            mesh = load(MESH + name)
            if mesh is None:
                break
            _, ext = mesh_bounds(mesh)
            # 長辺を円の接線方向にそろえる
            yaw = ang + (90.0 if ext.x >= ext.y else 0.0) + random.uniform(-10, 10)
            spawn_mesh(name, polar(r + random.uniform(-60, 60), ang), yaw=yaw,
                       roll=random.uniform(-6, 6), folder="Outskirts/Barricades", sink=0.02)
            width = max(ext.x, ext.y) * 2.0
            ang += max(math.degrees(width * 0.9 / r), 1.0)


# ---------------------------------------------------------------------------
# 行動範囲内のコリジョン除去
# ---------------------------------------------------------------------------
NO_COLLISION_FOLDERS = ("Vegetation", "Boundary/SmallRocks", "Boundary/Rocks", "Boundary/Barriers",
                        "Outskirts/Clutter")


def strip_arena_collision():
    """
    行動範囲内にかかる草・石・小物のコリジョンを外す (境界は不可視の壁が担当)。
    地面 (Ground) と不可視の壁 (ArenaWall_*) はそのまま。
    """
    wall_r = ARENA_RADIUS + 60.0
    count = 0
    for a in eas.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        label = a.get_actor_label()
        if label == "Ground" or label.startswith("ArenaWall_"):
            continue
        folder = str(a.get_folder_path())
        origin, _, fr = actor_footprint(a)
        inside = math.hypot(origin.x, origin.y) - fr < wall_r
        if inside or folder.startswith(NO_COLLISION_FOLDERS):
            disable_collision(a.static_mesh_component)
            count += 1
    log("コリジョンを外したアクター: {}".format(count))


def build_vegetation():
    """枯れ草・低木・サボテン。戦闘の邪魔にならないよう中央は薄く、コリジョン無し"""
    dry = ["SM_DryGrass_01_1", "SM_DryBush_01_Short", "SM_DryBush_02",
           "SM_WildGrass_S_01", "SM_WildGrass_S_02", "SM_WildGrass_S_03",
           "SM_WildGrass_M_01", "SM_WildGrass_M_02", "SM_WildGrass_L_01"]
    # アリーナ内 (外周寄りに疎らに)
    for _ in range(45):
        r = random.uniform(ARENA_RADIUS * 0.45, ARENA_RADIUS - 100.0)
        spawn_mesh(random.choice(dry[:6]), polar(r, random.uniform(0, 360)), yaw=random.uniform(0, 360),
                   scale=random.uniform(0.7, 1.2), folder="Vegetation/Inner", collision=False)
    # アリーナ外 (濃いめ)
    for _ in range(160 if BUILD_OUTSKIRTS else 0):
        r = random.uniform(ARENA_RADIUS + 200.0, ARENA_RADIUS + 6000.0)
        spawn_mesh(random.choice(dry), polar(r, random.uniform(0, 360)), yaw=random.uniform(0, 360),
                   scale=random.uniform(0.8, 1.6), folder="Vegetation/Outer", collision=False)
    for _ in range(12 if BUILD_OUTSKIRTS else 0):
        r = random.uniform(ARENA_RADIUS + 600.0, ARENA_RADIUS + 5000.0)
        spawn_mesh("SM_Cactus", polar(r, random.uniform(0, 360)), yaw=random.uniform(0, 360),
                   scale=random.uniform(0.8, 1.3), folder="Vegetation/Cactus")


def build_dust_cards():
    """MI_Fog_01/02 (加算・DepthFade) を縦向きの板に貼った砂煙カード"""
    plane = load("/Engine/BasicShapes/Plane")
    fogs = [m for m in (load(MAT + "MI_Fog_01"), load(MAT + "MI_Fog_02")) if m]
    if not fogs:
        return
    for i in range(16):
        a = 360.0 * i / 16 + random.uniform(-8, 8)
        r = random.uniform(ARENA_RADIUS + 800.0, ARENA_RADIUS + 4500.0)
        loc = polar(r, a, random.uniform(150.0, 400.0))
        card = eas.spawn_actor_from_object(plane, loc, rot(yaw=yaw_to_center(loc) + 90.0, roll=90.0))
        card.set_actor_scale3d(unreal.Vector(random.uniform(25, 45), random.uniform(6, 10), 1))
        card.set_folder_path("FX/DustCards")
        c = card.static_mesh_component
        c.set_material(0, random.choice(fogs))
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_cast_shadow(False)


def build_gameplay():
    folder = "Gameplay"

    # プレイヤー開始位置 (西側、中心=騎士の方を向く)
    ps = spawn_class(unreal.PlayerStart, unreal.Vector(-SPAWN_DISTANCE, 0, 100), yaw=0.0,
                     folder=folder, label="PlayerStart_Human")

    # 騎士 (敵) — 騎士BPが用意できたら差し替え
    enemy_cls = eal.load_blueprint_class(ENEMY_CHARACTER)
    if enemy_cls:
        spawn_class(enemy_cls, unreal.Vector(SPAWN_DISTANCE, 0, 100), yaw=180.0,
                    folder=folder, label="Enemy_Knight")
    else:
        warn("敵キャラクターBPが見つかりません: " + ENEMY_CHARACTER)

    # AI 用ナビメッシュ (アリーナ全体)
    nav = spawn_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 0), folder=folder, label="NavMeshBounds_Arena")
    side = (ARENA_RADIUS + 300.0) * 2.0
    nav.set_actor_scale3d(unreal.Vector(side / 200.0, side / 200.0, 5.0))

    # GameMode オーバーライド
    world = ues.get_editor_world()
    gm_cls = eal.load_blueprint_class(PLAYER_GAME_MODE)
    if gm_cls:
        world.get_world_settings().set_editor_property("default_game_mode", gm_cls)


def main():
    create_level()
    ground_mat = build_ground_material()

    with unreal.ScopedSlowTask(9, "荒野アリーナを構築中...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "ライティング")
        build_lighting()
        task.enter_progress_frame(1, "地面")
        build_ground(ground_mat)
        task.enter_progress_frame(1, "円形の境界")
        build_boundary()
        task.enter_progress_frame(1, "外周の風景")
        if BUILD_OUTSKIRTS:
            build_outskirts()
        task.enter_progress_frame(1, "集落跡")
        if BUILD_OUTSKIRTS:
            build_settlements()
        task.enter_progress_frame(1, "植生")
        build_vegetation()
        task.enter_progress_frame(1, "砂煙")
        if BUILD_OUTSKIRTS:
            build_dust_cards()
        task.enter_progress_frame(1, "ゲームプレイ")
        build_gameplay()
        task.enter_progress_frame(1, "コリジョン整理")
        strip_arena_collision()

    les.save_current_level()
    log("完了: " + LEVEL_PATH)


# パッチスクリプトから import されたときは実行しない
import builtins  # noqa: E402
if not getattr(builtins, "WASTELAND_ARENA_IMPORT_ONLY", False):
    main()
