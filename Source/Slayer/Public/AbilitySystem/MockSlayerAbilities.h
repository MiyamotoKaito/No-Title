// Alpha版の具体的なアビリティ
// 攻撃系（コンボ・スキル）はキャラごとに BP サブクラスを作り、アニメや当たり判定の数値だけ変える

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/MockSlayerGameplayAbility.h"
#include "MockSlayerAbilities.generated.h"

class UAnimSequenceBase;

/**
 * コンボの1段
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerComboStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	float PlayRate = 1.f;

	// この時間（秒）を過ぎてから押した入力を、次の段の予約として受け付ける
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	float InputOpenTime = 0.15f;

	// この時間（秒）を過ぎたら、予約があれば次の段へ移る
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	float AdvanceTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	TArray<FMockSlayerHitEvent> Hits;

	// 攻撃と同時に前へ踏み込む距離（相手の手前で止まる）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	float LungeDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	float LungeDuration = 0.15f;
};

/**
 * 通常攻撃のコンボ
 * 攻撃中にもう一度入力を押すと次の段へ進む（プレイヤーもAIも PressAbilityInput で同じように押す）
 * 受付は時間で決めるので、モンタージュに通知を置かなくてよい
 */
UCLASS()
class SLAYER_API UGA_MockSlayerComboAttack : public UMockSlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MockSlayerComboAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combo")
	TArray<FMockSlayerComboStage> Stages;

	virtual void OnAnimationEnded(bool bInterrupted) override;

private:
	void StartStage();
	void OnAdvanceReady();
	void AdvanceStage();

	int32 StageIndex = 0;
	float StageStartTime = 0.f;
	bool bNextQueued = false;
	bool bAdvanceReady = false;
	FTimerHandle AdvanceTimer;
};

/**
 * 1回のアニメで出す技（スキル・必殺技）
 * スキルはクールダウンで、必殺技（bUltimate）は必殺技ゲージで連発を防ぐ
 */
UCLASS()
class SLAYER_API UGA_MockSlayerMontageAttack : public UMockSlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MockSlayerMontageAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	bool IsUltimate() const { return bUltimate; }

protected:
	// 必殺技にする（ゲージが満タンの時だけ撃てて、撃つと演出が入る）
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	bool bUltimate = false;

	// 必殺技の名前（撃った時に画面に出す）
	UPROPERTY(EditDefaultsOnly, Category = "Ultimate")
	FText UltimateName;

	// 攻撃と同時に前へ踏み込む距離
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	float LungeDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	float LungeDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TArray<FMockSlayerHitEvent> Hits;

	virtual void OnAnimationEnded(bool bInterrupted) override;
};

/**
 * 回避（プレイヤーが操作している時だけ付与される共通技）
 * 移動入力の方向へ短く滑り、その間は無敵。攻撃をキャンセルして出せる
 */
UCLASS()
class SLAYER_API UGA_MockSlayerDodge : public UMockSlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MockSlayerDodge();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float DodgeDistance = 550.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float DodgeDuration = 0.3f;

private:
	FTimerHandle EndTimer;
};

/**
 * ブレイク（Event.Character.Break で発動）
 * 実行中の攻撃をキャンセルし、State.Break を付けてひるませる
 * 時間が終わったらブレイク値を最大まで戻す
 */
UCLASS()
class SLAYER_API UGA_MockSlayerBreak : public UMockSlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MockSlayerBreak();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void FinishBreak();

	FTimerHandle BreakTimer;
};

/**
 * 死亡（Event.Character.Death で発動）
 * すべての技を止め、State.Dead を付けたまま終わらない
 */
UCLASS()
class SLAYER_API UGA_MockSlayerDeath : public UMockSlayerGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MockSlayerDeath();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
