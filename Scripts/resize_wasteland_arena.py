# -*- coding: utf-8 -*-
"""
既存の Lvl_WastelandArena の行動範囲を広げ、外が見えない岩壁で囲んで円の外の物を削除する (軽量化)。

  - 行動範囲: 半径 build_wasteland_arena.ARENA_RADIUS (40m)
  - 境界: 高さ 9〜19m の岩を2列 + 不可視の壁 + 足元の小石 (すべて作り直し、岩はコリジョン無し)
  - 円の外の建物・山・小物・植生・砂煙・デカールを削除、地面キューブも縮小
  - 広がった範囲に草と汚れデカールを少し追加 (草はコリジョン無し)
  - NavMesh 範囲を新しい半径に合わせる

使い方: Lvl_WastelandArena を開いた状態で py "C:/UnrealProjects/Slayer/Scripts/resize_wasteland_arena.py"
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
eas, les, ues = arena.eas, arena.les, arena.ues

R = arena.ARENA_RADIUS
OLD_R = 2500.0  # 以前の行動範囲
DELETE_FOLDERS = ("Boundary", "Outskirts", "FX", "Vegetation/Outer", "Vegetation/Cactus")
KEEP_FOLDERS = ("Lighting", "Gameplay")


def ensure_level():
    world = ues.get_editor_world()
    if not world.get_path_name().startswith(arena.LEVEL_PATH + "."):
        raise RuntimeError("Lvl_WastelandArena を開いてから実行してください (現在: {})".format(world.get_path_name()))


def delete_outside():
    removed = 0
    for a in eas.get_all_level_actors():
        folder = str(a.get_folder_path())
        if folder.startswith(KEEP_FOLDERS) or a.get_actor_label() == "Ground":
            continue
        if folder.startswith(DELETE_FOLDERS):
            eas.destroy_actor(a)
            removed += 1
            continue
        if isinstance(a, (unreal.StaticMeshActor, unreal.DecalActor)) or "BP_Building" in a.get_class().get_name():
            loc = a.get_actor_location()
            if math.hypot(loc.x, loc.y) > R + 200.0:
                eas.destroy_actor(a)
                removed += 1
    log("円の外・旧境界のアクターを {} 個削除".format(removed))


def resize_ground():
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "Ground":
            s = arena.ground_size_for(R)
            a.set_actor_scale3d(unreal.Vector(s, s, 1))
            log("地面を {:.0f}m 四方に縮小".format(s))
            return
    warn("Ground が見つかりません")


def fill_new_area():
    """広がった帯 (旧半径〜新半径) に草と汚れデカールを追加"""
    dry = ["SM_DryGrass_01_1", "SM_DryBush_01_Short", "SM_DryBush_02",
           "SM_WildGrass_S_01", "SM_WildGrass_S_02", "SM_WildGrass_S_03"]
    for _ in range(45):
        r = random.uniform(OLD_R, R - 150.0)
        arena.spawn_mesh(random.choice(dry), arena.polar(r, random.uniform(0, 360)),
                         yaw=random.uniform(0, 360), scale=random.uniform(0.7, 1.2),
                         folder="Vegetation/Inner", collision=False)

    names = ["MI_Road_Debris_01", "MI_Forest_Compost_01", "MI_Grunge_01", "MI_Grunge_02", "MI_Decal_Grunge_01_dark"]
    mats = [m for m in (load(arena.MAT + n) for n in names) if m]
    for _ in range(18):
        loc = arena.polar(random.uniform(OLD_R - 300.0, R), random.uniform(0, 360))
        d = arena.spawn_class(unreal.DecalActor, loc, folder="Ground/Decals")
        d.set_actor_rotation(arena.rot(yaw=random.uniform(0, 360), pitch=-90.0), False)
        d.decal.set_decal_material(random.choice(mats))
        size = random.uniform(300, 750)
        d.decal.set_editor_property("decal_size", unreal.Vector(200.0, size, size))


def resize_navmesh():
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.NavMeshBoundsVolume):
            side = (R + 300.0) * 2.0
            a.set_actor_location(unreal.Vector(0, 0, 0), False, False)
            a.set_actor_scale3d(unreal.Vector(side / 200.0, side / 200.0, 5.0))
            log("NavMesh 範囲を {:.0f}m 四方に変更".format(side / 100.0))
            return
    warn("NavMeshBoundsVolume が見つかりません")


def main():
    ensure_level()
    random.seed(arena.RANDOM_SEED + 2)
    with unreal.ScopedSlowTask(5, "行動範囲を広げて岩壁で囲み中...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "円の外を削除")
        delete_outside()
        resize_ground()
        task.enter_progress_frame(1, "不可視の壁")
        arena.build_invisible_wall(R)
        task.enter_progress_frame(1, "岩壁")
        arena.build_rock_wall(R)
        arena.build_small_rocks(R)
        task.enter_progress_frame(1, "広がった範囲の装飾")
        fill_new_area()
        task.enter_progress_frame(1, "NavMesh / コリジョン整理")
        resize_navmesh()
        arena.strip_arena_collision()

    les.save_current_level()
    log("行動範囲の拡張が完了 (半径 {:.0f}m)".format(R / 100.0))


main()
