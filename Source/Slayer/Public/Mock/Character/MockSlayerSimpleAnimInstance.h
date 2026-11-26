// AnimBP を持たないキャラ（SciFi_Beast06 など）用の簡易アニメ
// 待機・歩き・走りを移動速度でブレンドし、その上に技のモンタージュ（スロット）を重ねる

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MockSlayerSimpleAnimInstance.generated.h"

class UAnimSequenceBase;

UCLASS(Transient, NotBlueprintable)
class SLAYER_API UMockSlayerSimpleAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 技のモンタージュを流すスロット名（ボス定義の FallbackSlotName と合わせる）
	static const FName SlotName;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FMockSlayerSimpleAnimInstanceProxy;

	// キャラの定義から読む
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> WalkAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> RunAnimation;

	float WalkAnimationSpeed = 200.f;
	float RunAnimationSpeed = 600.f;

	// 今の移動速度（体の大きさで割った、等倍の時の速さ）
	float Speed = 0.f;
};
