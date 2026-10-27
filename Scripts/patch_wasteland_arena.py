# -*- coding: utf-8 -*-
"""
既存の Lvl_WastelandArena に追加修正を当てるスクリプト (作り直さないので手作業の変更は残る)。

  - 円の外に建物クラスター・小物・即席バリケードを追加
  - 遠景の山を作り直し (正面を中心へ向ける + 両面マテリアル)
  - 行動範囲内の草・石・小物のコリジョンを除去

使い方: py "C:/UnrealProjects/Slayer/Scripts/patch_wasteland_arena.py"
"""
import builtins
import importlib.util
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


def ensure_level_open():
    world = arena.ues.get_editor_world()
    if world and world.get_path_name().startswith(arena.LEVEL_PATH):
        return
    if not arena.les.load_level(arena.LEVEL_PATH):
        raise RuntimeError("レベルを開けませんでした: " + arena.LEVEL_PATH)


def remove_folder(prefix):
    removed = 0
    for a in arena.eas.get_all_level_actors():
        if str(a.get_folder_path()).startswith(prefix):
            arena.eas.destroy_actor(a)
            removed += 1
    arena.log("{} のアクターを {} 個削除".format(prefix, removed))


def main():
    ensure_level_open()
    random.seed(arena.RANDOM_SEED + 1)

    with unreal.ScopedSlowTask(3, "荒野アリーナを修正中...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "山の向きを修正")
        remove_folder("Outskirts/Mountains")
        arena.build_mountains()
        task.enter_progress_frame(1, "建物を追加")
        arena.build_settlements()
        task.enter_progress_frame(1, "コリジョン整理")
        arena.strip_arena_collision()

    arena.les.save_current_level()
    arena.log("パッチ完了")


main()
