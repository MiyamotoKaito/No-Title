// ダメージ計算（ExecutionCalculation）

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "MockSlayerDamageExecution.generated.h"

/**
 * 技の基本ダメージ（SetByCaller の Data.Damage / Data.BreakDamage）と
 * 攻撃側の AttackPower / CrushPower から、受ける側の IncomingDamage / IncomingBreakDamage を計算する
 *
 * - 相手が塊（Actor.CrushChunk）なら CrushPower を使い、ブレイク値は減らさない
 * - 相手がブレイク中ならダメージ倍率をかける（攻撃し放題の時間）
 * - 相手が無敵（State.Invulnerable）なら何もしない
 */
UCLASS()
class SLAYER_API UMockSlayerDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UMockSlayerDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

	// ブレイク中の相手へのダメージ倍率
	static constexpr float BreakDamageMultiplier = 1.5f;
};
