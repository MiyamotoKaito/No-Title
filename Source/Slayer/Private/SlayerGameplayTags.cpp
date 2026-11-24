#include "SlayerGameplayTags.h"

namespace SlayerTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Attack, "Input.Attack", "通常攻撃（コンボ）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Skill1, "Input.Skill1", "スキル1");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Skill2, "Input.Skill2", "スキル2");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Dodge, "Input.Dodge", "回避（プレイヤー操作時のみ）");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "攻撃アビリティの実行中");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Break, "State.Break", "ブレイク中（行動不能・攻撃し放題）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "死亡");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "無敵（回避中など）");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack, "Ability.Attack", "攻撃系アビリティ");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Combo, "Ability.Attack.Combo", "通常攻撃のコンボ");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Skill, "Ability.Attack.Skill", "スキル・必殺技");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dodge, "Ability.Dodge", "回避アビリティ");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_UsingSkill, "State.UsingSkill", "スキル・必殺技の実行中");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Character.Death", "Health が0になった");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Break, "Event.Character.Break", "Break が0になった");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller：ダメージ量");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_BreakDamage, "Data.BreakDamage", "SetByCaller：ブレイク値の減少量");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Cooldown, "Data.Cooldown", "SetByCaller：クールダウン秒数");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown, "Cooldown", "クールダウン中（共通の親）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill1, "Cooldown.Skill1", "スキル1のクールダウン");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill2, "Cooldown.Skill2", "スキル2のクールダウン");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dodge, "Cooldown.Dodge", "回避のクールダウン");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Actor_CrushChunk, "Actor.CrushChunk", "砕く対象の塊");
}
