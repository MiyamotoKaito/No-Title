// C++ で中身を決めた GameplayEffect（アセットを作らずに使える）

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MockSlayerGameplayEffects.generated.h"

/**
 * ダメージ用の Instant GE
 * 量は SetByCaller（Data.Damage / Data.BreakDamage）で渡し、計算は UMockSlayerDamageExecution が行う
 */
UCLASS()
class SLAYER_API UGE_MockSlayerDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_MockSlayerDamage();
};

/**
 * クールダウン用の Duration GE
 * 秒数は SetByCaller（Data.Cooldown）、付けるタグは GA 側が DynamicGrantedTags で渡す
 * （アビリティごとに GE を作らなくて済むようにするため）
 */
UCLASS()
class SLAYER_API UGE_MockSlayerCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_MockSlayerCooldown();
};
