// 通常ボス共通のキャラクター
// 同じクラスを AIController（敵）と PlayerController（プレイアブル）の Possess 切り替えで使い回す

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Mock/Data/MockBossDefinition.h"
#include "MockBossCharacterBase.generated.h"

class UAbilitySystemComponent;
class UAudioComponent;
class UCameraComponent;
class USoundBase;
class UParticleSystem;
class UParticleSystemComponent;
class UMockSlayerAttributeSet;
class USpringArmComponent;
class AMockBossCharacterBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMockSlayerCharacterDied, AMockBossCharacterBase* /*DeadCharacter*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMockSlayerDamageDealt, AMockBossCharacterBase* /*Source*/, float /*Damage*/);

UCLASS()
class SLAYER_API AMockBossCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMockBossCharacterBase();

	//~ AActor / APawn
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Spawn 時（FinishSpawning の前）に呼んで、どのボスになるかを決める
	void SetDefinition(UMockBossDefinition* InDefinition);
	UMockBossDefinition* GetDefinition() const { return Definition; }

	UMockSlayerAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// ---- 入力（プレイヤーとAIの共通の入口） ----

	// 入力タグのGAすべてに「押した」を伝える。未発動なら発動、発動中なら InputPressed を送る
	// 戻り値：1つでも発動できた、または発動中のGAに入力を送れたら true
	UFUNCTION(BlueprintCallable, Category = "Slayer|Input")
	bool PressAbilityInput(FGameplayTag InputTag);

	// 入力タグのGAすべてに「離した」を伝える
	UFUNCTION(BlueprintCallable, Category = "Slayer|Input")
	void ReleaseAbilityInput(FGameplayTag InputTag);

	// 入力タグのGAが1つでも発動中なら true
	UFUNCTION(BlueprintPure, Category = "Slayer|Input")
	bool IsAbilityInputActive(FGameplayTag InputTag) const;

	// ---- 状態 ----
	UFUNCTION(BlueprintPure, Category = "Slayer|State")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Slayer|State")
	bool IsBroken() const;

	UFUNCTION(BlueprintPure, Category = "Slayer|State")
	bool IsAttacking() const;

	// 技の向き先：AI ならプレイヤー、プレイヤーなら近くの敵か塊
	AActor* FindCombatTarget(float MaxDistance) const;

	// シーケンスを動的モンタージュにする時のスロット名（キャラの AnimBP に合わせる）
	FName GetMontageSlotName() const;

	// 登場演出を再生する。戻り値は長さ（秒）
	float PlayIntro();

	// GA_SlayerDeath から呼ばれる
	void HandleDeath();

	// 自分の攻撃が当たった時（ヒットストップ）
	void NotifyHitLanded(AActor* HitActor);

	FOnMockSlayerCharacterDied OnDied;

	// 自分の攻撃が相手（塊以外）にダメージを与えた時（AttributeSet から呼ばれる）。変身ゲージに使う
	void NotifyDamageDealt(float Damage);
	FOnMockSlayerDamageDealt OnDamageDealt;

	// 体の大きさ（定義の BodyScale）
	float GetBodyScale() const;

	// ---- 変身中のもとのキャラ ----
	// 眠らせる：姿と当たり判定と動きを止め、誰の標的にもならないようにする（変身中のもとのキャラ用）
	void SetDormant(bool bInDormant);
	bool IsDormant() const { return bDormant; }

	// 実時間で Duration 秒だけ、ゲーム全体をスローにする
	void StartSlowMotion(float TimeDilation, float Duration);

	UCameraComponent* GetCamera() const { return Camera; }

	// ---- レベル ----
	// Possess の前（Spawn 直後）に設定する。ステータスはレベルで伸び、レベルに合った形態の見た目になる
	void SetCharacterLevel(int32 InLevel);

	UFUNCTION(BlueprintPure, Category = "Slayer|Level")
	int32 GetCharacterLevel() const { return CharacterLevel; }

	// 今の形態の名前（進化していれば形態名）
	FText GetFormName() const;

	// 進化演出：レベルを上げて新しい形態の見た目に変え、エフェクトと登場モーションを出す
	void Evolve(int32 NewLevel, UParticleSystem* Effect);

	// ---- 演出 ----
	// 必殺技の演出：スロー・カメラ寄り（プレイヤー操作時）・技名の表示。初めて撃つ時はさらに長く
	void PlayUltimateCinematic(const FText& UltimateName, bool bFirstTime);

	// HUD 用：表示中の必殺技名。表示中なら true
	bool GetUltimateCallout(FText& OutName, bool& bOutFirstTime) const;

	// 攻撃中の武器の軌跡
	void StartWeaponTrail();
	void StopWeaponTrail();

	// エフェクトを出す（null なら何もしない）
	void SpawnEffectAtLocation(UParticleSystem* Effect, const FVector& Location, const FRotator& Rotation, float Scale = 1.f) const;
	void SpawnEffectAttached(UParticleSystem* Effect, FName SocketName, float Scale = 1.f);

	// ---- ボイス ----
	// 声の優先度。高いものは低いものを遮って鳴る（掛け声 < セリフ < 大事なセリフ）
	enum class EVoicePriority : uint8
	{
		Effort,
		Line,
		Important,
	};

	// ボイスを鳴らす（Chance の確率で）。キャラごとに1本の声だけ鳴らし、重ならないようにする
	void PlayVoice(USoundBase* Sound, EVoicePriority Priority, float Chance = 1.f);

	// 定義のボイス（無ければ null）
	const FMockSlayerVoiceSet* GetVoices() const;

	// ダメージを受けた時（AttributeSet から呼ばれる）。被弾や体力わずかの声を出す
	void NotifyDamaged(float Damage);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slayer")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slayer")
	TObjectPtr<UMockSlayerAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slayer|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slayer|Camera")
	TObjectPtr<UCameraComponent> Camera;

	// どのボスか（レベルに直接置く時はここで選ぶ）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slayer", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UMockBossDefinition> Definition;

	// どのキャラにも付ける共通アビリティ（ブレイク・死亡）
	UPROPERTY(EditDefaultsOnly, Category = "Slayer|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

	// プレイヤーが操作している時だけ付ける共通アビリティ（回避など）
	UPROPERTY(EditDefaultsOnly, Category = "Slayer|Abilities")
	TArray<FMockSlayerAbilityGrant> PlayerOnlyAbilities;

	// ヒットストップの長さ（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Slayer|Feel")
	float HitStopDuration = 0.06f;

private:
	// 定義のメッシュ・アニメ・カプセルを反映する
	void ApplyDefinitionVisuals();

	// ASC の初期化（PossessedBy と OnRep_PlayerState の両方から呼ぶ）
	void InitAbilityActorInfo();

	// 共通アビリティと定義のアビリティを付与する（1回だけ）
	void GrantDefaultAbilities();

	// 操作者（プレイヤー／AI）に合わせてステータスとプレイヤー専用アビリティを切り替える
	void ApplyControllerSettings(bool bPlayerControlled);

	TArray<FGameplayAbilitySpec*> FindSpecsByInputTag(FGameplayTag InputTag) const;
	void SendInputEvent(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType);

	TArray<FGameplayAbilitySpecHandle> PlayerAbilityHandles;
	bool bDefaultAbilitiesGranted = false;
	FTimerHandle HitStopTimer;

	int32 CharacterLevel = 1;
	bool bDormant = false;

	// ボイス用（キャラごとに1本）
	UPROPERTY(VisibleAnywhere, Category = "Slayer|Voice")
	TObjectPtr<UAudioComponent> VoiceComponent;

	EVoicePriority CurrentVoicePriority = EVoicePriority::Effort;
	double LastPainVoiceTime = -100.0;
	bool bHealthLowVoicePlayed = false;
	bool bUltimateWasReady = false;

	// 武器の軌跡（使い回す）
	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> TrailComponent;

	// 必殺技の演出の状態（ゲーム時間はスローになるので実時間で管理する）
	FText CalloutName;
	bool bCalloutFirstTime = false;
	double CalloutEndRealTime = 0.0;
	double SlowMotionEndRealTime = 0.0;
	double CameraZoomEndRealTime = 0.0;
	float DefaultArmLength = 480.f;
};
