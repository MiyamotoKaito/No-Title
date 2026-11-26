// ボス1体ぶんの定義（敵としてもプレイアブルとしても、このデータ1つで同じキャラになる）

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MockBossDefinition.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UGameplayAbility;
class UParticleSystem;
class USkeletalMesh;
class USoundBase;

/**
 * キャラのボイス（SoundCue を入れると、毎回ランダムな言い回しになる）
 * null のものは鳴らさない
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerVoiceSet
{
	GENERATED_BODY()

	// 登場（ボスとして出てきた時）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> IntroBoast;

	// 登場したボスへの返し（プレイヤーとして戦う時）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> IntroResponse;

	// コンボの掛け声（毎段ではなく確率で）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> AttackEffort;

	// コンボ開始時のセリフ（確率で）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> AttackLine;

	// スキルのセリフ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> SkillLine;

	// 必殺技のセリフ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> UltimateLine;

	// 必殺技ゲージが満タンになった時（プレイヤー操作時）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> UltimateReady;

	// 回避
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> DodgeEffort;

	// 被弾（小・大）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> Pain;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> PainHeavy;

	// ブレイクした時
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> Break;

	// 体力が少なくなった時（1回だけ）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> HealthLow;

	// 倒れる時
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> Death;

	// 相手を倒した時
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> Kill;

	// 獲得されて仲間になる時
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> Catchphrase;

	// 進化した時
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> LevelUp;

	// 勝った時
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundBase> Victory;
};

/**
 * 付与するアビリティと、それを発動する入力タグの組
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	// プレイヤーもAIもこのタグを「押して」発動する
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Input"))
	FGameplayTag InputTag;
};

/**
 * AI が選ぶ行動1つ（どの入力を、どの距離で、何回押すか）
 * ボスの個性はこの並びと数値で表す
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerAIAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	// この距離の範囲にプレイヤーがいる時だけ選ぶ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	float MaxRange = 300.f;

	// 選ばれやすさ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0"))
	float Weight = 1.f;

	// 押す回数（コンボなら段数）。PressCountMin〜PressCount の間でランダムに決める
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "1"))
	int32 PressCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "1"))
	int32 PressCountMin = 1;

	// 2回目以降を押す間隔（秒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0.05"))
	float PressInterval = 0.3f;
};

/**
 * 進化の形態1つ（見た目とステータスのボーナス）
 * レベルが RequiredLevel に達すると、この形態に進化する
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerEvolutionForm
{
	GENERATED_BODY()

	// 形態の名前（例：Frozen Hearth Aurora）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution")
	FText FormName;

	// この形態の見た目（キャラのスケルトンと共通のメッシュ）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution")
	TObjectPtr<USkeletalMesh> Mesh;

	// 進化に必要なレベル
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	// ステータスの追加ボーナス（0.1 なら +10%）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution")
	float StatBonus = 0.f;
};

/**
 * ボス定義 DataAsset
 * メッシュ・アニメ・ステータス・技・AIの行動・塊の設定を持つ
 */
UCLASS(BlueprintType)
class SLAYER_API UMockBossDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 表示名
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	// 操作キャラとして使えるようになるレベル（操作キャラの誰かがこのレベルに達したら解放）。1 なら最初から
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display", meta = (ClampMin = "1"))
	int32 UnlockLevel = 1;

	// ---- 見た目 ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> Mesh;

	// AnimBP。空なら下の移動アニメを速さで切り替える簡易アニメ（UMockSlayerSimpleAnimInstance）を使う
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSubclassOf<UAnimInstance> AnimClass;

	// ---- AnimBP が無いキャラ用の移動アニメ ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Locomotion")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Locomotion")
	TObjectPtr<UAnimSequenceBase> WalkAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Locomotion")
	TObjectPtr<UAnimSequenceBase> RunAnimation;

	// 歩き・走りのアニメがちょうど合う移動速度（等倍の時）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Locomotion")
	float WalkAnimationSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Locomotion")
	float RunAnimationSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector MeshLocation = FVector(0.f, 0.f, -96.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FRotator MeshRotation = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	float CapsuleRadius = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	float CapsuleHalfHeight = 96.f;

	// 体の大きさ（アクター全体の倍率）。技の当たり判定・踏み込み・エフェクトもこの倍率で大きくなる
	// 技の数値（Offset・Radius など）は等倍の時の値で書く
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (ClampMin = "0.1"))
	float BodyScale = 1.f;

	// ---- アニメーション ----
	// 登場演出（Intro フェーズで再生）。このモンタージュのスロット名を、他のアニメを動的モンタージュにする時にも使う
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> IntroMontage;

	// ブレイク中に再生するアニメ（モンタージュでもシーケンスでもよい）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> BreakAnimation;

	// 死亡時に再生するアニメ（最後のポーズで止まる）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> DeathAnimation;

	// IntroMontage が無い時に使うスロット名
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName FallbackSlotName = TEXT("DefaultSlot");

	// ---- エフェクト ----
	// 攻撃が当たった時に相手の位置に出す（技ごとの指定が無い時）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

	// 攻撃中の武器の軌跡（AnimTrail のパーティクル）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UParticleSystem> TrailEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	FName TrailSocketStart = TEXT("FX_Trail_R_01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	FName TrailSocketEnd = TEXT("FX_Trail_R_02");

	// 必殺技の溜めで体に出す
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UParticleSystem> UltimateWarmupEffect;

	// ---- ボイス ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	FMockSlayerVoiceSet Voices;

	// ---- レベル ----
	// ボスとして出てくる時のレベル（推奨レベル）。獲得するとこのレベルで仲間になる
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "1"))
	int32 BossLevel = 1;

	// 1レベルごとのステータスの伸び（0.12 なら +12%）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float StatGrowthPerLevel = 0.12f;

	// ---- 進化 ----
	// 進化の形態（RequiredLevel の小さい順）。空なら進化しない
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution")
	TArray<FMockSlayerEvolutionForm> EvolutionForms;

	// レベルに合う形態の番号（-1 なら形態なし＝基本の Mesh）
	int32 GetFormIndex(int32 Level) const
	{
		int32 Result = INDEX_NONE;
		for (int32 Index = 0; Index < EvolutionForms.Num(); ++Index)
		{
			if (Level >= EvolutionForms[Index].RequiredLevel)
			{
				Result = Index;
			}
		}
		return Result;
	}

	const FMockSlayerEvolutionForm* GetForm(int32 Level) const
	{
		const int32 Index = GetFormIndex(Level);
		return EvolutionForms.IsValidIndex(Index) ? &EvolutionForms[Index] : nullptr;
	}

	// 形態の名前（形態が無ければ DisplayName）
	FText GetFormName(int32 Level) const
	{
		const FMockSlayerEvolutionForm* Form = GetForm(Level);
		return (Form != nullptr && !Form->FormName.IsEmpty()) ? Form->FormName : DisplayName;
	}

	// レベルと形態ボーナスを合わせたステータス倍率
	float GetLevelScale(int32 Level) const
	{
		const FMockSlayerEvolutionForm* Form = GetForm(Level);
		return (1.f + StatGrowthPerLevel * FMath::Max(Level - 1, 0)) * (1.f + (Form ? Form->StatBonus : 0.f));
	}

	// ---- ステータス ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MaxHealth = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float AttackPower = 1.f;

	// ボス（AI）として戦う時のブレイク値。プレイヤー操作中はブレイクしない
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MaxBreak = 300.f;

	// ブレイクが続く秒数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BreakDuration = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float CrushPower = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float WalkSpeed = 600.f;

	// プレイヤーが操作する時の体力倍率（ボスとしての体力のままだと長すぎるため）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float PlayerHealthScale = 0.5f;

	// ---- 技（敵の時もプレイアブルの時も同じものを使う） ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<FMockSlayerAbilityGrant> Abilities;

	// ---- AI ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	TArray<FMockSlayerAIAction> AIActions;

	// 行動の後に次を考えるまでの間（秒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FVector2D AIThinkInterval = FVector2D(0.6f, 1.4f);

	// この距離まで近づいてから行動を選ぶ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	float AIPreferredRange = 250.f;

	// ---- 塊（砕くフェーズ） ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush")
	float CrushChunkHealth = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush")
	float CrushTimeLimit = 15.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BossDefinition"), GetFName());
	}
};
