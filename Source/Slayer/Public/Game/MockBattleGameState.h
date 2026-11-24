// バトルの現在フェーズを持つ GameState（レイドでマルチにした時もクライアントへそのまま複製できる）

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MockBattleGameState.generated.h"

class AMockBossCharacterBase;
class AMockCrushChunk;
class UMockBossDefinition;

UENUM(BlueprintType)
enum class EMockBattlePhase : uint8
{
	None,
	Intro,    // 戦闘開始の演出（操作不可）
	Battle,   // ボス戦本体
	BossDown, // 撃破演出（操作不可）
	Crush,    // 塊を砕く
	Capture,  // 獲得演出（砕けた塊からボスが現れる。操作不可）
	Evolution,// 進化演出（レベルが上がって形態が変わった時だけ。操作不可）
	Result    // リザルト
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMockBattlePhaseChanged, EMockBattlePhase, NewPhase);

UCLASS()
class SLAYER_API AMockBattleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// サーバー（GameMode）から呼ぶ。Duration が0より大きければ残り時間を表示する
	void SetPhase(EMockBattlePhase NewPhase, float Duration = 0.f);

	UFUNCTION(BlueprintPure, Category = "Battle")
	EMockBattlePhase GetPhase() const { return Phase; }

	// フェーズの残り時間（秒）。時間制限が無ければ0
	UFUNCTION(BlueprintPure, Category = "Battle")
	float GetPhaseTimeRemaining() const;

	// プレイヤーが操作できるフェーズか
	UFUNCTION(BlueprintPure, Category = "Battle")
	bool IsPlayerInputAllowed() const { return Phase == EMockBattlePhase::Battle || Phase == EMockBattlePhase::Crush; }

	UPROPERTY(BlueprintAssignable, Category = "Battle")
	FOnMockBattlePhaseChanged OnPhaseChanged;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<AMockBossCharacterBase> Boss;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<AMockCrushChunk> Chunk;

	// 今戦っているボスの定義（ボスが塊になった後も残る）
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Battle")
	TObjectPtr<UMockBossDefinition> BossDefinition;

protected:
	UFUNCTION()
	void OnRep_Phase();

	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "Battle")
	EMockBattlePhase Phase = EMockBattlePhase::None;

	// フェーズが終わるサーバー時刻（0なら時間制限なし）
	UPROPERTY(Replicated)
	float PhaseEndTime = 0.f;
};
