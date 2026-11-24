// Alpha版のアビリティ共通の基底クラス

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MockSlayerGameplayAbility.generated.h"

class AMockBossCharacterBase;
class UAnimMontage;
class UAnimSequenceBase;
class UParticleSystem;

/**
 * 技の当たり判定1回ぶん
 * アニメの開始から Time 秒後に、キャラの前方 Offset の位置で球の判定を出す
 * Range が0より大きい時は、そこから前方へ Range だけ球を飛ばす（遠距離技）
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerHitEvent
{
	GENERATED_BODY()

	// アニメ開始からの秒数（再生速度1.0の時の時間）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float Time = 0.3f;

	// キャラから見た判定の位置（X が前方）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	FVector Offset = FVector(120.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float Radius = 120.f;

	// 0より大きければ、前方へこの距離だけ判定を伸ばす
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float Range = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float Damage = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float BreakDamage = 20.f;

	// 当たった相手に出すエフェクト（null ならキャラ定義の ImpactEffect）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UParticleSystem> ImpactEffect;

	// 判定を出す瞬間に出すエフェクト（範囲攻撃の爆発やマズルフラッシュなど）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UParticleSystem> CastEffect;

	// CastEffect を付けるソケット（None なら判定の位置に出す）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FName CastSocket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float EffectScale = 1.f;
};

/**
 * アビリティ共通の基底
 * - クールダウン：秒数とタグを設定するだけで使える（GE はアビリティごとに作らない）
 * - アニメ再生：モンタージュでもシーケンスでも再生できる（シーケンスは動的モンタージュにする）
 * - 当たり判定：FMockSlayerHitEvent を時間通りに出してダメージ GE を当てる
 */
UCLASS(Abstract)
class SLAYER_API UMockSlayerGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMockSlayerGameplayAbility();

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// HUD 表示用
	FGameplayTag GetCooldownTag() const { return CooldownTag; }

protected:
	// クールダウン秒数（0ならクールダウンなし）
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
	float CooldownDuration = 0.f;

	// クールダウン中に付くタグ（Cooldown.*）
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown", meta = (Categories = "Cooldown"))
	FGameplayTag CooldownTag;

	// 技の開始時に、この距離以内の相手の方を向く（0なら向かない）
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float FaceTargetDistance = 1000.f;

	// 攻撃系アビリティ共通のタグ設定（コンストラクタで呼ぶ）
	// 実行中は State.Attacking。ブレイク中・死亡中は発動しない
	// bIsSkill = false：コンボ（Ability.Attack.Combo）。攻撃中は新しく始めない
	// bIsSkill = true ：スキル・必殺技（Ability.Attack.Skill）。コンボを中断して出せる
	void InitAttackTags(bool bIsSkill);

	AMockBossCharacterBase* GetSlayerCharacter() const;

	// 近くの相手の方を向く
	void FaceTarget() const;

	// アニメを再生する。終わったら OnAnimationEnded が呼ばれる。戻り値は成功したか
	bool PlayAnimation(UAnimSequenceBase* Animation, float PlayRate);

	// 自分が再生したアニメが終わった時（別のアニメに上書きされた時は呼ばれない）
	virtual void OnAnimationEnded(bool bInterrupted) {}

	// 当たり判定を時間通りに出す予約をする（近接なら武器の軌跡も出す）
	void ScheduleHits(const TArray<FMockSlayerHitEvent>& Hits, float PlayRate);
	void ClearScheduledHits();

	// 攻撃と同時に前へ踏み込む（相手の手前で止まる）
	void ApplyLunge(float Distance, float Duration);

	// 当たり判定を1回出して、当たった相手にダメージを与える
	void ExecuteHit(const FMockSlayerHitEvent& Hit) const;

private:
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial);

	// 再生中のモンタージュ（動的モンタージュを GC させないために保持する）
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PlayingMontage;

	// 古いアニメの終了通知を無視するための通し番号
	int32 AnimSerial = 0;

	TArray<FTimerHandle> HitTimers;

	// GetCooldownTags が返すコンテナ（CooldownTag から作る）
	UPROPERTY(Transient)
	mutable FGameplayTagContainer CooldownTagContainer;
};
