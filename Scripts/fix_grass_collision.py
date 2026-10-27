# -*- coding: utf-8 -*-
"""
開いているレベルの草・低木メッシュと小石 (Boundary/SmallRocks) の当たり判定を外すスクリプト。

コンポーネントの「Use Default Collision」が有効だとメッシュ側のコリジョン設定に戻ってしまうため、
それを切ったうえで BodyInstance を NoCollision に書き換える (set_editor_property 経由で保存対象にする)。

使い方: 対象レベルを開いた状態で py "C:/UnrealProjects/Slayer/Scripts/fix_grass_collision.py"
"""
import unreal

# メッシュ名にこれらを含むものを「草」とみなす
GRASS_KEYWORDS = ("Grass", "Bush")
# このアウトライナーフォルダ配下のアクターも対象 (小石)
TARGET_FOLDERS = ("Boundary/SmallRocks",)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def is_grass(comp):
    mesh = comp.get_editor_property("static_mesh")
    return mesh is not None and any(k in mesh.get_name() for k in GRASS_KEYWORDS)


def strip(comp):
    comp.modify()
    comp.set_editor_property("use_default_collision", False)
    body = comp.get_editor_property("body_instance")
    body.set_editor_property("collision_profile_name", "NoCollision")
    body.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("body_instance", body)
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)


def main():
    fixed = 0
    remaining = []
    with unreal.ScopedEditorTransaction("草の当たり判定を外す"):
        for actor in eas.get_all_level_actors():
            in_folder = str(actor.get_folder_path()).startswith(TARGET_FOLDERS)
            for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
                if not (in_folder or is_grass(comp)):
                    continue
                strip(comp)
                fixed += 1
                if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                    remaining.append(actor.get_actor_label())
    les.save_current_level()
    unreal.log("[GrassCollision] 当たり判定を外した草・小石: {} 個".format(fixed))
    if remaining:
        unreal.log_warning("[GrassCollision] まだコリジョンが残っている: {}".format(", ".join(remaining[:20])))
    else:
        unreal.log("[GrassCollision] すべての対象でコリジョン無しを確認")


main()
