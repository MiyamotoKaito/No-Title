// ボスを敵として動かす AIController
// UMockBossDefinition::AIActions の中から、距離と重みで行動を選び、プレイヤーと同じ入力タグを「押す」

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "MockSlayerBossAIController.generated.h"

class AMockBossCharacterBase;
struct FMockSlayerAIAction;

/**
 * Alpha版の簡易AI（Behavior Tree の代わり）
 * 1. プレイヤーに近づく
 * 2. 距離に合う行動を重み付きランダムで選んで入力を押す（コンボは段数ぶん押す）
 * 3. 技が終わったら少し間を置いて次を考える
 * ブレイク中・攻撃中・Battle フェーズ以外は何もしない
 */
UCLASS()
class SLAYER_API AMockSlayerBossAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMockSlayerBossAIController();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnUnPossess() override;

private:
	// 距離に合う行動を選んで発動する。発動できたら true
	bool TryStartAction(AMockBossCharacterBase* Boss, float Distance);
	void MoveToward(AMockBossCharacterBase* Boss, const AActor* Target) const;
	void FaceToward(AMockBossCharacterBase* Boss, const AActor* Target, float DeltaSeconds) const;
	void PressNext();
	void StopPressing();

	// コンボの続きで押す入力と残り回数
	FGameplayTag PendingInputTag;
	int32 RemainingPresses = 0;
	FTimerHandle PressTimer;

	// 次に行動を考えてよい時刻
	float NextThinkTime = 0.f;
};
