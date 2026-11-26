// バトル（アリーナ）の流れを管理する GameMode
// Intro → Battle → BossDown → Crush → Result を SetPhase() だけで進める

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Mock/Game/MockBattleGameState.h"
#include "MockBattleGameMode.generated.h"

class AMockBossCharacterBase;
class AMockCrushChunk;
class UMockBossDefinition;
class UParticleSystem;
class USoundBase;
struct FMockSlayerVoiceSet;

UCLASS()
class SLAYER_API AMockBattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMockBattleGameMode();

	virtual void StartPlay() override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	// リザルト画面から呼ぶ：選んだキャラで次のバトルを始める（レベルを開き直す）
	void RequestNextBattle(UMockBossDefinition* CharacterChoice);

	// 使える操作キャラの一覧（リザルトのキャラ選択用）
	TArray<UMockBossDefinition*> GetSelectableCharacters() const;

	// まだ使えない操作キャラ（解放レベルの低い順）
	TArray<UMockBossDefinition*> GetLockedCharacters() const;

	// 操作キャラの中で一番高いレベル（操作キャラの解放に使う）
	int32 GetHighestCharacterLevel() const;

	UMockBossDefinition* GetStarterCharacter() const { return StarterCharacter; }

protected:
	// 敵として出てくるボスの一覧（勝つたびに順番に入れ替わる）
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	TArray<TObjectPtr<UMockBossDefinition>> BossRoster;

	// 操作キャラの一覧（ボスとしては出てこない）。定義の UnlockLevel で解放される
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	TArray<TObjectPtr<UMockBossDefinition>> PlayableCharacters;

	// 最初に使う操作キャラ
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	TObjectPtr<UMockBossDefinition> StarterCharacter;

	// 最初から変身できるボス（倒して獲得しなくても変身できる）
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	TArray<TObjectPtr<UMockBossDefinition>> DefaultChangeForms;

	// 登場演出の長さ（IntroMontage が無い時）
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	float IntroDuration = 2.f;

	// 登場演出の最大の長さ
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	float MaxIntroDuration = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	float BossDownDuration = 2.5f;

	// プレイヤーの開始位置から、ボスを出す距離
	UPROPERTY(EditDefaultsOnly, Category = "Battle")
	float BossSpawnDistance = 1100.f;

	// 獲得演出の長さ
	UPROPERTY(EditDefaultsOnly, Category = "Capture")
	float CaptureDuration = 5.f;

	// 塊が砕けた時のエフェクト
	UPROPERTY(EditDefaultsOnly, Category = "Capture")
	TObjectPtr<UParticleSystem> ChunkBreakEffect;

	// 獲得したボスが現れる時のエフェクト
	UPROPERTY(EditDefaultsOnly, Category = "Capture")
	TObjectPtr<UParticleSystem> CaptureEffect;

	// 進化演出の長さとエフェクト
	UPROPERTY(EditDefaultsOnly, Category = "Evolution")
	float EvolutionDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Evolution")
	TObjectPtr<UParticleSystem> EvolutionEffect;

	// ---- 経験値 ----
	// 勝った時：Base + PerBossLevel × ボスのレベル
	UPROPERTY(EditDefaultsOnly, Category = "Exp")
	int32 VictoryExpBase = 50;

	UPROPERTY(EditDefaultsOnly, Category = "Exp")
	int32 VictoryExpPerBossLevel = 25;

	// 獲得できた時の追加
	UPROPERTY(EditDefaultsOnly, Category = "Exp")
	int32 CaptureExpBonus = 30;

	// 負けた時も少しだけ入る（再挑戦で少しずつ強くなれるように）
	UPROPERTY(EditDefaultsOnly, Category = "Exp")
	int32 DefeatExpBase = 15;

	UPROPERTY(EditDefaultsOnly, Category = "Exp")
	int32 DefeatExpPerBossLevel = 5;

private:
	void StartCaptureCinematic(const FVector& Location);
	void StartEvolutionCinematic(int32 NewLevel);

	// 少し遅らせてキャラにボイスを言わせる（Voice は FMockSlayerVoiceSet のどの声か）
	void PlayVoiceLater(AMockBossCharacterBase* Character, TObjectPtr<USoundBase> FMockSlayerVoiceSet::* Voice, float Delay);

	// 演出用のカメラに切り替える（前のカメラは消す）
	void ViewFromCamera(const FVector& CameraLocation, const FVector& Focus, float BlendTime);
	void BeginBattle();
	void SpawnBoss();
	void SetPhase(EMockBattlePhase NewPhase, float Duration = 0.f);

	void OnBossDied(AMockBossCharacterBase* DeadBoss);
	void OnPlayerDied(AMockBossCharacterBase* DeadPlayer);
	void StartCrush();
	void OnChunkBroken(AMockCrushChunk* Chunk);
	void OnCrushTimeout();
	void FinishBattle(bool bVictory, bool bCaptured);

	// 次に戦うボス（直前に勝ったボスの次。初めてなら一覧の先頭）
	UMockBossDefinition* ChooseNextBoss() const;
	UMockBossDefinition* GetPlayerDefinition() const;
	AMockBattleGameState* GetBattleGameState() const;

	UPROPERTY(Transient)
	TObjectPtr<UMockBossDefinition> CurrentBossDefinition;

	// 獲得演出で出すボスとカメラ
	UPROPERTY(Transient)
	TObjectPtr<AMockBossCharacterBase> CaptureDisplayBoss;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CaptureCamera;

	FTimerHandle PhaseTimer;
};
