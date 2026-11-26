// プレイヤーの入力を受けて、操作中のボス（ポーン）の入力タグを「押す」 PlayerController

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "MockSlayerPlayerController.generated.h"

class AMockBossCharacterBase;
class UMockBossDefinition;
class UComposableCameraTransitionDataAsset;
class UComposableCameraTypeAsset;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * - 移動・カメラ・技の入力
 * - ボスチェンジ（変身）：獲得したボスにバトル中に変身する。ポーンごと乗り換え、時間が来たら元のキャラに戻る
 * - リザルト画面でのキャラ選択と「次のバトルへ」
 *
 * Alpha では Input Action / IMC を C++ で組み立てる（アセットが無くても動くように）
 * DefaultMappingContext にアセットを設定すれば、そちらが優先される
 */
UCLASS()
class SLAYER_API AMockSlayerPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMockSlayerPlayerController();

	// リザルト画面で選んでいるキャラの番号（HUD 表示用）
	int32 GetSelectedCharacterIndex() const { return SelectedCharacterIndex; }

	// ---- ボスチェンジ（変身） ----
	// 変身ゲージ（0〜1）。満タンで変身できる
	float GetChangeChargePercent() const;
	bool IsChangeReady() const;

	// 変身中か。変身中の残り時間（0〜1）
	bool IsBossChanged() const { return ChangedPawn != nullptr; }
	float GetChangeRemainingPercent() const;

	// 変身できるキャラ（獲得済みで、もとのキャラ以外）。数字キーの番号順
	TArray<UMockBossDefinition*> GetChangeCandidates() const;

	// 変身した瞬間のカットイン用。表示中なら true（Age は表示してからの実時間）
	bool GetChangeBanner(FText& OutName, int32& OutLevel, float& OutAge) const;

	// 変身を解いて元のキャラに戻す（獲得演出・リザルトの前に GameMode から呼ぶ）
	void EndBossChange(bool bWithEffect);

	// このバトルで変身した姿（経験値を分けるため）
	TArray<UMockBossDefinition*> GetUsedChangeForms() const { return ObjectPtrDecay(UsedChangeForms); }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

	// 設定されていればこちらを使う（無ければ C++ で作る）
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// ---- カメラ（Composable Camera System） ----
	// 狙う相手がいない時のカメラ
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TObjectPtr<UComposableCameraTypeAsset> FreeCamera;

	// ボス（砕くフェーズでは塊）を画面に入れ続けるカメラ。AimActor パラメータに相手を渡す
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TObjectPtr<UComposableCameraTypeAsset> LockOnCamera;

	// LockOnCamera が相手を受け取るパラメータ名
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FName LockOnTargetParameter = TEXT("AimActor");

	// 狙う相手が変わった時のカメラの切り替え（null ならカメラ定義の既定）
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TObjectPtr<UComposableCameraTransitionDataAsset> TargetSwitchTransition;

	// 体の大きいキャラ（BodyScale が LargeBodyScale 以上）を操作する時のカメラ（引きが大きい）
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TObjectPtr<UComposableCameraTypeAsset> FreeCameraLarge;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TObjectPtr<UComposableCameraTypeAsset> LockOnCameraLarge;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LargeBodyScale = 1.5f;

	// ---- ボスチェンジ ----
	UPROPERTY(EditDefaultsOnly, Category = "BossChange")
	float MaxChangeCharge = 100.f;

	// ボスの最大HPのこの割合だけダメージを与えるとゲージが満タンになる
	UPROPERTY(EditDefaultsOnly, Category = "BossChange")
	float ChangeFullDamageRatio = 0.2f;

	// バトル中は何もしなくても少しずつ溜まる（1秒あたり）
	UPROPERTY(EditDefaultsOnly, Category = "BossChange")
	float ChangeChargePerSecond = 1.5f;

	// 変身していられる秒数
	UPROPERTY(EditDefaultsOnly, Category = "BossChange")
	float ChangeDuration = 25.f;

	// 変身の衝撃で近くの敵を吹き飛ばす距離（等倍の時。体の大きさを掛ける）と強さ
	UPROPERTY(EditDefaultsOnly, Category = "BossChange")
	float ChangeShockwaveRadius = 450.f;

	UPROPERTY(EditDefaultsOnly, Category = "BossChange")
	float ChangeShockwaveStrength = 1400.f;

	// カットインを出す長さ（実時間）
	UPROPERTY(EditDefaultsOnly, Category = "BossChange")
	float ChangeBannerDuration = 2.2f;

private:
	void CreateDefaultInput();
	void AddMappingContext();
	bool IsGameplayInputAllowed() const;
	AMockBossCharacterBase* GetSlayerPawn() const;

	void OnMove(const FInputActionValue& Value);

	// ボスチェンジ
	void OnChangePressed();
	void TryBossChange(int32 CandidateIndex);
	void BeginBossChange(UMockBossDefinition* Target);
	void UpdateBossChange(float DeltaTime);
	void OnChangedPawnDied(AMockBossCharacterBase* DeadPawn);
	void OnPawnDealtDamage(AMockBossCharacterBase* Source, float Damage);
	void PlayChangeShockwave(AMockBossCharacterBase* Center);
	void OnAbilityPressed(FGameplayTag InputTag);
	void OnAbilityReleased(FGameplayTag InputTag);

	// リザルト画面
	bool IsInResult() const;
	void OnChooseCharacter(int32 Index);
	void OnSelectStep(int32 Step);
	void OnConfirm();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SelectPrevAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SelectNextAction;

	// 技の入力（入力タグごと）
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UInputAction>> AbilityActions;

	// 数字キーでのキャラ選択
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ChooseActions;

	// 開発用コンソール変数（slayer.AutoPlay / slayer.God）の処理
	void UpdateDebugCheats(float DeltaTime);

	// フェーズに合わせて、狙う相手（ボス・塊）が変わったらカメラを切り替える
	void UpdateCamera();
	void ActivateCameraFor(AActor* Target);

	// 今のカメラが狙っている相手（カメラを付け直すか判断するため）
	TWeakObjectPtr<AActor> CameraTarget;
	bool bCameraActivated = false;

	// ロックオンの切り替え（Tab／マウスホイールクリック／右スティック押し込み）
	void OnToggleLockOn();
	bool bLockOnEnabled = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LockOnAction;

	// 変身する／変身を解く（F・LB）
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ChangeAction;

	// 変身中のもとのキャラ（眠らせておき、戻る時にまた操作する）と、変身した姿
	UPROPERTY(Transient)
	TObjectPtr<AMockBossCharacterBase> BasePawn;

	UPROPERTY(Transient)
	TObjectPtr<AMockBossCharacterBase> ChangedPawn;

	float ChangeCharge = 0.f;
	float ChangeRemaining = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMockBossDefinition>> UsedChangeForms;

	// 戻った時に元のキャラの体力と必殺技ゲージを戻すため（Possess でステータスが初期化されるので）
	float SavedBaseHealth = 0.f;
	float SavedBaseUltimate = 0.f;

	FTimerHandle ChangeRevertTimer;
	FTimerHandle ChangeIntroStopTimer;

	FText ChangeBannerName;
	int32 ChangeBannerLevel = 1;
	double ChangeBannerStartRealTime = -100.0;

	// 与ダメージの通知を受けているポーン
	TWeakObjectPtr<AMockBossCharacterBase> DamageBoundPawn;
	FDelegateHandle DamageDealtHandle;

	// 視点操作の入力アクション（アセット）。カメラ定義の ControlRotate ノードと同じものを使う
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookActionTemplate;

public:
	bool IsLockOnEnabled() const { return bLockOnEnabled; }

private:

	int32 SelectedCharacterIndex = 0;
	bool bSelectionInitialized = false;

	float AutoPlayTimer = 0.f;
	bool bGodApplied = false;
};
