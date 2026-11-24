// プレイヤーの入力を受けて、操作中のボス（ポーン）の入力タグを「押す」 PlayerController

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "MockSlayerPlayerController.generated.h"

class AMockBossCharacterBase;
class UComposableCameraTransitionDataAsset;
class UComposableCameraTypeAsset;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * - 移動・カメラ・技の入力
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

private:
	void CreateDefaultInput();
	void AddMappingContext();
	bool IsGameplayInputAllowed() const;
	AMockBossCharacterBase* GetSlayerPawn() const;

	void OnMove(const FInputActionValue& Value);
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
