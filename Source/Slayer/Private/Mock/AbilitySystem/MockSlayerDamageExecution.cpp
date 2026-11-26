#include "Mock/AbilitySystem/MockSlayerDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "Mock/AbilitySystem/MockSlayerAttributeSet.h"
#include "SlayerGameplayTags.h"

namespace
{
	// 攻撃側から読み取る Attribute の定義
	struct FSlayerDamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CrushPower);

		FSlayerDamageStatics()
		{
			// 当たった瞬間の値を使う（スナップショットしない）
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMockSlayerAttributeSet, AttackPower, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMockSlayerAttributeSet, CrushPower, Source, false);
		}
	};

	const FSlayerDamageStatics& DamageStatics()
	{
		static FSlayerDamageStatics Statics;
		return Statics;
	}
}

UMockSlayerDamageExecution::UMockSlayerDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().CrushPowerDef);
}

void UMockSlayerDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	if (TargetASC == nullptr)
	{
		return;
	}

	// 無敵・死亡中の相手には当たらない
	if (TargetASC->HasMatchingGameplayTag(SlayerTags::State_Invulnerable) || TargetASC->HasMatchingGameplayTag(SlayerTags::State_Dead))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Damage] %s は無敵・死亡中なので当たらない"), *GetNameSafe(TargetASC->GetAvatarActor()));
		return;
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const float BaseDamage = Spec.GetSetByCallerMagnitude(SlayerTags::Data_Damage, false, 0.f);
	const float BaseBreakDamage = Spec.GetSetByCallerMagnitude(SlayerTags::Data_BreakDamage, false, 0.f);

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 塊を殴った時は「砕く力」で計算する
	if (TargetASC->HasMatchingGameplayTag(SlayerTags::Actor_CrushChunk))
	{
		float CrushPower = 1.f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CrushPowerDef, EvalParams, CrushPower);

		const float ChunkDamage = BaseDamage * FMath::Max(CrushPower, 0.f);
		if (ChunkDamage > 0.f)
		{
			OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UMockSlayerAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, ChunkDamage));
		}
		return;
	}

	float AttackPower = 1.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvalParams, AttackPower);

	float Damage = BaseDamage * FMath::Max(AttackPower, 0.f);
	float BreakDamage = BaseBreakDamage;

	// ブレイク中は攻撃し放題：ダメージを上げ、ブレイク値はもう減らさない
	if (TargetASC->HasMatchingGameplayTag(SlayerTags::State_Break))
	{
		Damage *= BreakDamageMultiplier;
		BreakDamage = 0.f;
	}

	if (Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UMockSlayerAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::AddBase, Damage));
	}
	if (BreakDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UMockSlayerAttributeSet::GetIncomingBreakDamageAttribute(), EGameplayModOp::AddBase, BreakDamage));
	}
}
