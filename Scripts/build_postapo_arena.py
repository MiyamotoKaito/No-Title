# -*- coding: utf-8 -*-
"""
PostApo レベルをベースに 1v1 円形アリーナ (Lvl_WastelandArena) を作り直すスクリプト。

  1. (手動) PostApo を開き File > Save Current Level As で /Game/Level/ に保存して開いておく
  2. ランドスケープをライントレースで調べ、平坦で周囲に建物が多い場所をアリーナ中心に自動選択
  3. 円内の障害物を削除し、岩の境界リング + 不可視の壁 + スポーン地点 + NavMesh + GameMode を配置

使い方: 上記レベルを開いた状態で py "C:/UnrealProjects/Slayer/Scripts/build_postapo_arena.py"
"""
import builtins
import importlib.util
import math
import os
import random

import unreal

builtins.WASTELAND_ARENA_IMPORT_ONLY = True
try:
    _path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build_wasteland_arena.py")
except NameError:
    _path = "C:/UnrealProjects/Slayer/Scripts/build_wasteland_arena.py"
_spec = importlib.util.spec_from_file_location("build_wasteland_arena", _path)
arena = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(arena)
builtins.WASTELAND_ARENA_IMPORT_ONLY = False

log, warn, load = arena.log, arena.warn, arena.load
eal, eas, les, ues = arena.eal, arena.eas, arena.les, arena.ues
rot, polar = arena.rot, arena.polar

SOURCE_LEVEL = "/Game/JustBStudios/PostApo/Levels/PostApo"
LEVEL_PATH = arena.LEVEL_PATH
BACKUP_PATH = LEVEL_PATH + "_Old"
NEUTRAL_LEVEL = "/Game/GenericShooter/Level/TestMap"
R = arena.ARENA_RADIUS
CLEAR_RADIUS = R + 200.0

# 削除・評価の対象外にするクラス
KEEP_CLASSES = (unreal.LandscapeProxy, unreal.Light, unreal.SkyAtmosphere, unreal.SkyLight,
                unreal.ExponentialHeightFog, unreal.Volume, unreal.Brush, unreal.WorldSettings,
                unreal.InstancedFoliageActor, unreal.VolumetricCloud, unreal.Info)
HUGE_FOOTPRINT = 4000.0  # これより大きいもの (山など) は削除しない


# ---------------------------------------------------------------------------
# レベルの準備
# ---------------------------------------------------------------------------
def use_current_level():
    """
    手動で PostApo を「名前を付けて保存」したレベルを開いた状態で実行する前提。
    (スクリプトからのマップ複製はエディタが固まったため使わない)
    """
    world = ues.get_editor_world()
    path = world.get_path_name().split(".")[0]
    if path.startswith("/Game/JustBStudios/"):
        raise RuntimeError("配布アセットのレベルは直接編集しません。File > Save Current Level As で /Game/Level/ に保存してから実行してください")
    if not any(isinstance(a, unreal.LandscapeProxy) for a in eas.get_all_level_actors()):
        raise RuntimeError("ランドスケープがありません。PostApo を保存し直したレベルを開いてから実行してください: " + path)
    log("現在のレベルでアリーナを構築します: " + path)
    return path


def prepare_level():
    """(未使用) 複製先のレベルパスを返す (既存レベルの退避に失敗した場合は別名)"""
    if not eal.does_asset_exist(SOURCE_LEVEL):
        raise RuntimeError("参考レベルが見つかりません: " + SOURCE_LEVEL)
    target = LEVEL_PATH
    # 対象レベルを閉じるため無関係なレベルを開き、メモリから解放する
    les.load_level(NEUTRAL_LEVEL)
    unreal.SystemLibrary.collect_garbage()
    if eal.does_asset_exist(LEVEL_PATH):
        backup = BACKUP_PATH
        n = 2
        while eal.does_asset_exist(backup):
            backup = "{}{}".format(BACKUP_PATH, n)
            n += 1
        if eal.rename_asset(LEVEL_PATH, backup):
            log("既存レベルを退避: " + backup)
        else:
            target = LEVEL_PATH + "_PostApo"
            warn("既存レベルの退避に失敗したため別名で作成します: " + target)
            if eal.does_asset_exist(target):
                raise RuntimeError("作成先が既に存在します。不要なら削除してから再実行してください: " + target)
    if eal.duplicate_asset(SOURCE_LEVEL, target) is None:
        raise RuntimeError("PostApo の複製に失敗しました")
    if not les.load_level(target):
        raise RuntimeError("複製したレベルを開けませんでした")
    log("PostApo を複製して開きました: " + target)
    return target


# ---------------------------------------------------------------------------
# 地形調査
# ---------------------------------------------------------------------------
def trace_ground(world, x, y):
    """(z, 地形に当たったか) を返す。何にも当たらなければ None"""
    start = unreal.Vector(x, y, 100000.0)
    end = unreal.Vector(x, y, -100000.0)
    res = unreal.SystemLibrary.line_trace_single(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
        unreal.DrawDebugTrace.NONE, True)
    if isinstance(res, tuple):
        res = res[-1] if res and res[0] else None
    if res is None:
        return None
    fields = res.to_tuple()
    loc = next((f for f in fields if isinstance(f, unreal.Vector)), None)
    actor = next((f for f in fields if isinstance(f, unreal.Actor)), None)
    if loc is None:
        return None
    return loc.z, isinstance(actor, unreal.LandscapeProxy)


def collect_footprints():
    items = []
    for a in eas.get_all_level_actors():
        if isinstance(a, KEEP_CLASSES) or isinstance(a, (unreal.DecalActor, unreal.PlayerStart)):
            continue
        try:
            o, ext = a.get_actor_bounds(False)
        except Exception:
            continue
        r = math.hypot(ext.x, ext.y)
        if r <= 1.0 or r > HUGE_FOOTPRINT:
            continue
        is_building = "BP_Building" in a.get_class().get_name()
        items.append((a, o.x, o.y, r, is_building))
    return items


def landscape_bounds():
    xs, ys = [], []
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.LandscapeProxy):
            o, ext = a.get_actor_bounds(False)
            xs += [o.x - ext.x, o.x + ext.x]
            ys += [o.y - ext.y, o.y + ext.y]
    if not xs:
        raise RuntimeError("ランドスケープが見つかりません")
    return min(xs), max(xs), min(ys), max(ys)


def sample_points(cx, cy):
    pts = [(cx, cy)]
    for ring_r, n in ((R * 0.35, 6), (R * 0.7, 10), (R, 14), (R + 400.0, 14)):
        for i in range(n):
            a = 2.0 * math.pi * i / n
            pts.append((cx + math.cos(a) * ring_r, cy + math.sin(a) * ring_r))
    return pts


def evaluate(world, cx, cy, footprints, map_cx, map_cy):
    heights, blocked = [], 0
    for x, y in sample_points(cx, cy):
        h = trace_ground(world, x, y)
        if h is None:
            return None
        if h[1]:
            heights.append(h[0])
        else:
            blocked += 1
    if len(heights) < 10:
        return None
    hrange = max(heights) - min(heights)
    inside_big = inside_small = near_buildings = near_props = 0
    for _, x, y, r, is_b in footprints:
        d = math.hypot(x - cx, y - cy)
        if d - r < CLEAR_RADIUS:
            if is_b or r > 500.0:
                inside_big += 1
            else:
                inside_small += 1
        elif d - r < R + 5000.0:
            if is_b:
                near_buildings += 1
            else:
                near_props += 1
    score = (hrange * 1.5 + blocked * 400.0 + inside_big * 900.0 + inside_small * 40.0
             - near_buildings * 350.0 - min(near_props, 60) * 12.0
             + math.hypot(cx - map_cx, cy - map_cy) * 0.03)
    heights.sort()
    return dict(score=score, z=heights[len(heights) // 2], zmin=heights[0], zmax=heights[-1],
                hrange=hrange, blocked=blocked, inside_big=inside_big, inside_small=inside_small,
                near_buildings=near_buildings, near_props=near_props)


def choose_center(world, footprints):
    x0, x1, y0, y1 = landscape_bounds()
    log("ランドスケープ範囲: X {:.0f}〜{:.0f}, Y {:.0f}〜{:.0f}".format(x0, x1, y0, y1))
    map_cx, map_cy = (x0 + x1) * 0.5, (y0 + y1) * 0.5
    inset = R + 5000.0
    if x1 - x0 < inset * 2 or y1 - y0 < inset * 2:
        inset = R + 800.0
    step = max((x1 - x0 - inset * 2) / 14.0, 1500.0)
    best = None
    x = x0 + inset
    while x <= x1 - inset:
        y = y0 + inset
        while y <= y1 - inset:
            e = evaluate(world, x, y, footprints, map_cx, map_cy)
            if e and (best is None or e["score"] < best[2]["score"]):
                best = (x, y, e)
            y += step
        x += step
    if best is None:
        raise RuntimeError("アリーナを置ける場所が見つかりませんでした")
    # 最良点の周辺を細かく探す
    bx, by, be = best
    for dx in (-step / 2, 0.0, step / 2):
        for dy in (-step / 2, 0.0, step / 2):
            e = evaluate(world, bx + dx, by + dy, footprints, map_cx, map_cy)
            if e and e["score"] < be["score"]:
                best = (bx + dx, by + dy, e)
    cx, cy, e = best
    log("アリーナ中心: ({:.0f}, {:.0f}, {:.0f}) 高低差 {:.0f}cm / 円内の大物 {} 小物 {} / 周囲の建物 {} 小物 {}".format(
        cx, cy, e["z"], e["hrange"], e["inside_big"], e["inside_small"], e["near_buildings"], e["near_props"]))
    return cx, cy, e


# ---------------------------------------------------------------------------
# アリーナ構築
# ---------------------------------------------------------------------------
def clear_circle(cx, cy, footprints):
    removed = 0
    for a, x, y, r, _ in footprints:
        if math.hypot(x - cx, y - cy) - r < CLEAR_RADIUS:
            eas.destroy_actor(a)
            removed += 1
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.PlayerStart):
            eas.destroy_actor(a)
            removed += 1
    log("円内の障害物と既存の PlayerStart を {} 個削除".format(removed))


def foliage_report(cx, cy, z):
    center = unreal.Vector(cx, cy, z)
    for ifa in eas.get_all_level_actors():
        if not isinstance(ifa, unreal.InstancedFoliageActor):
            continue
        for comp in ifa.get_components_by_class(unreal.InstancedStaticMeshComponent):
            try:
                idx = comp.get_instances_overlapping_sphere(center, R, True)
            except Exception:
                continue
            if idx:
                coll = comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
                mesh = comp.static_mesh.get_name() if comp.static_mesh else "?"
                (warn if coll else log)("円内のフォリッジ: {} x{} (コリジョン{})".format(
                    mesh, len(idx), "あり" if coll else "なし"))


def ground_z(world, x, y, fallback):
    h = trace_ground(world, x, y)
    return h[0] if h else fallback


def build_boundary(world, cx, cy, info):
    folder = "Arena/Boundary"
    center = unreal.Vector(cx, cy, 0)

    # 不可視の壁 (高低差分も覆う高さ)
    cube = load("/Engine/BasicShapes/Cube")
    segments = 64
    wall_r = R + 60.0
    seg_len = 2.0 * math.pi * wall_r / segments * 1.08
    height = info["hrange"] + 1500.0
    for i in range(segments):
        ang = 360.0 * i / segments
        p = center + polar(wall_r, ang)
        p.z = info["zmin"] - 200.0 + height * 0.5
        w = eas.spawn_actor_from_object(cube, p, rot(yaw=ang + 90.0))
        w.set_actor_scale3d(unreal.Vector(seg_len / 100.0, 0.4, height / 100.0))
        w.set_folder_path(folder + "/InvisibleWall")
        w.set_actor_label("ArenaWall_{:02d}".format(i))
        w.set_actor_hidden_in_game(True)
        c = w.static_mesh_component
        c.set_collision_profile_name("InvisibleWall")
        c.set_cast_shadow(False)

    # 岩のリング (コリジョン無し: 境界は不可視の壁が担当)
    rocks = ["SM_Rock_01", "SM_Rock_02", "SM_Rock_03", "SM_Rock_04", "SM_Rock_05", "SM_Rock_06"]
    ang = 0.0
    while ang < 360.0:
        name = random.choice(rocks)
        mesh = load(arena.MESH + name)
        s = arena.height_scale(mesh, random.uniform(180.0, 420.0)) * random.uniform(0.9, 1.15)
        r = R + random.uniform(120.0, 350.0)
        p = center + polar(r, ang)
        p.z = ground_z(world, p.x, p.y, info["z"])
        arena.spawn_mesh(name, p, yaw=random.uniform(0, 360), scale=s, folder=folder + "/Rocks",
                         sink=random.uniform(0.15, 0.3), collision=False,
                         roll=random.uniform(-8, 8), pitch=random.uniform(-8, 8))
        _, ext = arena.mesh_bounds(mesh)
        ang += max(math.degrees(max(ext.x, ext.y) * 2.0 * s * random.uniform(0.55, 0.85) / r), 2.0)

    # 小石と枯れ草 (円内は外周寄りに疎ら、すべてコリジョン無し)
    small = rocks + ["SM_DryGrass_01_1", "SM_DryBush_01_Short", "SM_DryBush_02"]
    for _ in range(40):
        name = random.choice(small)
        mesh = load(arena.MESH + name)
        s = arena.height_scale(mesh, random.uniform(30.0, 80.0)) if name.startswith("SM_Rock") else random.uniform(0.7, 1.2)
        p = center + polar(random.uniform(R * 0.5, R + 80.0), random.uniform(0, 360))
        p.z = ground_z(world, p.x, p.y, info["z"])
        arena.spawn_mesh(name, p, yaw=random.uniform(0, 360), scale=s, folder="Arena/Detail",
                         sink=0.25 if name.startswith("SM_Rock") else 0.0, collision=False)

    # 中央の焦げ跡
    charcoal = load(arena.MAT + "MI_Charcoal")
    if charcoal:
        d = arena.spawn_class(unreal.DecalActor, unreal.Vector(cx, cy, info["z"]), folder="Arena/Detail")
        d.set_actor_rotation(rot(pitch=-90.0), False)
        d.decal.set_decal_material(charcoal)
        d.decal.set_editor_property("decal_size", unreal.Vector(400.0, 450.0, 450.0))


def build_gameplay(world, cx, cy, info):
    folder = "Arena/Gameplay"
    p_human = unreal.Vector(cx - arena.SPAWN_DISTANCE, cy, 0)
    p_knight = unreal.Vector(cx + arena.SPAWN_DISTANCE, cy, 0)
    p_human.z = ground_z(world, p_human.x, p_human.y, info["z"]) + 100.0
    p_knight.z = ground_z(world, p_knight.x, p_knight.y, info["z"]) + 100.0

    arena.spawn_class(unreal.PlayerStart, p_human, yaw=0.0, folder=folder, label="PlayerStart_Human")
    enemy_cls = eal.load_blueprint_class(arena.ENEMY_CHARACTER)
    if enemy_cls:
        arena.spawn_class(enemy_cls, p_knight, yaw=180.0, folder=folder, label="Enemy_Knight")

    nav = arena.spawn_class(unreal.NavMeshBoundsVolume, unreal.Vector(cx, cy, info["z"]),
                            folder=folder, label="NavMeshBounds_Arena")
    side = (R + 300.0) * 2.0
    nav.set_actor_scale3d(unreal.Vector(side / 200.0, side / 200.0, (info["hrange"] + 1000.0) / 200.0))

    gm_cls = eal.load_blueprint_class(arena.PLAYER_GAME_MODE)
    if gm_cls:
        world.get_world_settings().set_editor_property("default_game_mode", gm_cls)


def strip_collision_inside(cx, cy):
    """円内にかかる (壁・ランドスケープ以外の) メッシュのコリジョンを外す"""
    count = 0
    for a in eas.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        if a.get_actor_label().startswith("ArenaWall_"):
            continue
        o, ext = a.get_actor_bounds(False)
        if math.hypot(o.x - cx, o.y - cy) - math.hypot(ext.x, ext.y) < R + 60.0:
            arena.disable_collision(a.static_mesh_component)
            count += 1
    log("円内のコリジョンを外したアクター: {}".format(count))


def main():
    random.seed(arena.RANDOM_SEED)
    with unreal.ScopedSlowTask(5, "PostApo ベースのアリーナを構築中...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "レベル確認")
        target = use_current_level()
        world = ues.get_editor_world()

        task.enter_progress_frame(1, "アリーナの場所を探索")
        footprints = collect_footprints()
        cx, cy, info = choose_center(world, footprints)

        task.enter_progress_frame(1, "円内を整理")
        clear_circle(cx, cy, footprints)
        foliage_report(cx, cy, info["z"])

        task.enter_progress_frame(1, "境界を配置")
        build_boundary(world, cx, cy, info)

        task.enter_progress_frame(1, "ゲームプレイ設定")
        build_gameplay(world, cx, cy, info)
        strip_collision_inside(cx, cy)

    les.save_current_level()
    log("PostApo アリーナ完了: " + target)


main()
