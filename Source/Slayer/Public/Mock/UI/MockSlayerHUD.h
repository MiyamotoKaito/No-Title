// Alpha版の HUD（Widget アセットを作らずに Canvas で描く）

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MockSlayerHUD.generated.h"

class AMockBattleGameState;
class AMockBossCharacterBase;

/**
 * - ボスの HP とブレイクゲージ（上）
 * - 操作キャラの HP・必殺技ゲージ・クールダウン（左下）
 * - フェーズの表示（登場・撃破・砕く残り時間・獲得演出）
 * - 必殺技の技名（自分が撃った時は大きく、ボスが撃つ時は警告）
 * - リザルト（獲得の成否・経験値・レベルアップ・キャラ選択）
 */
UCLASS()
class SLAYER_API AMockSlayerHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawBossPanel(const AMockBossCharacterBase* Boss);
	void DrawPlayerPanel(const AMockBossCharacterBase* Player);
	void DrawPhaseBanner(const AMockBattleGameState* State, const AMockBossCharacterBase* Player);
	void DrawUltimateCallouts(const AMockBossCharacterBase* Player, const AMockBossCharacterBase* Boss);
	void DrawResult();
	void DrawControls();

	void DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& Color);
	void DrawTextAt(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color);
	void DrawTextCentered(const FString& Text, float Y, float Scale, const FLinearColor& Color);

	// 画面の高さに合わせた文字の倍率
	float UIScale = 1.f;
};
