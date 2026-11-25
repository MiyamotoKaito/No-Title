#include "UI/MockSlayerHUD.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/MockSlayerAttributeSet.h"
#include "Character/MockBossCharacterBase.h"
#include "Data/MockBossDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Game/MockBattleGameMode.h"
#include "Game/MockBattleGameState.h"
#include "Game/MockCrushChunk.h"
#include "Game/MockSlayerSubsystems.h"
#include "Player/MockSlayerPlayerController.h"
#include "SlayerGameplayTags.h"

namespace
{
	const FLinearColor ColorBack(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor ColorHealth(0.85f, 0.12f, 0.12f);
	const FLinearColor ColorPlayerHealth(0.2f, 0.8f, 0.35f);
	const FLinearColor ColorBreak(1.f, 0.75f, 0.1f);
	const FLinearColor ColorUltimate(0.75f, 0.35f, 1.f);
	const FLinearColor ColorChunk(0.35f, 0.75f, 1.f);
	const FLinearColor ColorGold(1.f, 0.85f, 0.35f);
	const FLinearColor ColorText(1.f, 1.f, 1.f);
	const FLinearColor ColorDim(0.7f, 0.7f, 0.7f);

	FString GetDisplayName(const UMockBossDefinition* Definition)
	{
		if (Definition == nullptr)
		{
			return TEXT("???");
		}
		return Definition->DisplayName.IsEmpty() ? Definition->GetName() : Definition->DisplayName.ToString();
	}

	// クールダウンの残り秒数（0なら使える）
	float GetCooldownRemaining(const UAbilitySystemComponent* ASC, const FGameplayTag& CooldownTag)
	{
		if (ASC == nullptr)
		{
			return 0.f;
		}
		const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
		float Remaining = 0.f;
		for (const float Time : ASC->GetActiveEffectsTimeRemaining(Query))
		{
			Remaining = FMath::Max(Remaining, Time);
		}
		return Remaining;
	}

	// キャラの表示名（進化していれば形態名）
	FString GetCharacterName(const AMockBossCharacterBase* Character)
	{
		if (Character == nullptr)
		{
			return TEXT("???");
		}
		const FText FormName = Character->GetFormName();
		return FormName.IsEmpty() ? GetDisplayName(Character->GetDefinition()) : FormName.ToString();
	}

	// 定義とレベルからの表示名
	FString GetFormNameAt(const UMockBossDefinition* Definition, int32 Level)
	{
		if (Definition == nullptr)
		{
			return TEXT("???");
		}
		const FText FormName = Definition->GetFormName(Level);
		return FormName.IsEmpty() ? GetDisplayName(Definition) : FormName.ToString();
	}

	float GetUltimatePercent(const UMockSlayerAttributeSet* Attributes)
	{
		const float Max = Attributes->GetMaxUltimateCharge();
		return Max > 0.f ? Attributes->GetUltimateCharge() / Max : 0.f;
	}
}

void AMockSlayerHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}

	UIScale = FMath::Max(Canvas->SizeY / 1080.f, 0.5f);

	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	const AMockBossCharacterBase* Player = Cast<AMockBossCharacterBase>(GetOwningPawn());
	const AMockBossCharacterBase* Boss = State ? State->Boss.Get() : nullptr;

	if (State != nullptr && State->GetPhase() == EMockBattlePhase::Result)
	{
		DrawResult();
		return;
	}

	if (State != nullptr)
	{
		DrawPhaseBanner(State, Player);
		// 獲得・進化の演出中は余計な表示を出さない
		if (State->GetPhase() == EMockBattlePhase::Capture || State->GetPhase() == EMockBattlePhase::Evolution)
		{
			return;
		}
		DrawBossPanel(Boss);
	}
	DrawPlayerPanel(Player);
	DrawUltimateCallouts(Player, Boss);
	DrawControls();
}

void AMockSlayerHUD::DrawBossPanel(const AMockBossCharacterBase* Boss)
{
	if (Boss == nullptr || Boss->GetAttributeSet() == nullptr)
	{
		return;
	}

	const UMockSlayerAttributeSet* Attributes = Boss->GetAttributeSet();
	const float Width = Canvas->SizeX * 0.5f;
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = 40.f * UIScale;

	DrawTextAt(FString::Printf(TEXT("%s   Lv.%d"), *GetCharacterName(Boss), Boss->GetCharacterLevel()), X, Y - 30.f * UIScale, 1.2f, ColorText);

	const float MaxHealth = Attributes->GetMaxHealth();
	DrawBar(X, Y, Width, 18.f * UIScale, MaxHealth > 0.f ? Attributes->GetHealth() / MaxHealth : 0.f, ColorHealth);

	// ブレイクゲージ（HP バーの下）
	const float MaxBreak = Attributes->GetMaxBreak();
	const float BreakY = Y + 24.f * UIScale;
	if (MaxBreak > 0.f)
	{
		if (Boss->IsBroken())
		{
			DrawBar(X, BreakY, Width, 8.f * UIScale, 1.f, ColorBreak);
			DrawTextCentered(TEXT("BREAK!"), BreakY + 30.f * UIScale, 1.6f, ColorBreak);
		}
		else
		{
			DrawBar(X, BreakY, Width, 8.f * UIScale, Attributes->GetBreak() / MaxBreak, ColorBreak);
		}
	}

	// ボスの必殺技ゲージ（満タンが近いほど危ない）
	DrawBar(X, BreakY + 12.f * UIScale, Width * 0.3f, 5.f * UIScale, GetUltimatePercent(Attributes), ColorUltimate);
}

void AMockSlayerHUD::DrawPlayerPanel(const AMockBossCharacterBase* Player)
{
	if (Player == nullptr || Player->GetAttributeSet() == nullptr)
	{
		return;
	}

	const UMockSlayerAttributeSet* Attributes = Player->GetAttributeSet();
	const float X = 40.f * UIScale;
	const float Y = Canvas->SizeY - 150.f * UIScale;
	const float Width = Canvas->SizeX * 0.25f;

	DrawTextAt(FString::Printf(TEXT("%s   Lv.%d"), *GetCharacterName(Player), Player->GetCharacterLevel()), X, Y - 30.f * UIScale, 1.1f, ColorText);
	const float MaxHealth = Attributes->GetMaxHealth();
	DrawBar(X, Y, Width, 16.f * UIScale, MaxHealth > 0.f ? Attributes->GetHealth() / MaxHealth : 0.f, ColorPlayerHealth);

	// 必殺技ゲージ
	const float UltPercent = GetUltimatePercent(Attributes);
	const bool bUltReady = UltPercent >= 1.f;
	const float Pulse = 0.6f + 0.4f * FMath::Abs(FMath::Sin(GetWorld()->GetRealTimeSeconds() * 5.f));
	DrawBar(X, Y + 22.f * UIScale, Width, 8.f * UIScale, UltPercent, bUltReady ? ColorUltimate * Pulse : ColorUltimate * 0.7f);

	const UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	float RowY = Y + 38.f * UIScale;

	const float Skill1 = GetCooldownRemaining(ASC, SlayerTags::Cooldown_Skill1);
	DrawTextAt(Skill1 > 0.f ? FString::Printf(TEXT("RMB  Skill    %.1fs"), Skill1) : FString(TEXT("RMB  Skill    READY")), X, RowY, 0.9f, Skill1 > 0.f ? ColorDim : ColorText);
	RowY += 20.f * UIScale;

	DrawTextAt(bUltReady ? FString(TEXT("Q    ULTIMATE READY!")) : FString::Printf(TEXT("Q    Ultimate  %d%%"), FMath::FloorToInt(UltPercent * 100.f)), X, RowY, bUltReady ? 1.f : 0.9f, bUltReady ? ColorUltimate : ColorDim);
	RowY += 20.f * UIScale;

	const float Dodge = GetCooldownRemaining(ASC, SlayerTags::Cooldown_Dodge);
	DrawTextAt(Dodge > 0.f ? FString::Printf(TEXT("SPC  Dodge    %.1fs"), Dodge) : FString(TEXT("SPC  Dodge    READY")), X, RowY, 0.9f, Dodge > 0.f ? ColorDim : ColorText);
}

void AMockSlayerHUD::DrawUltimateCallouts(const AMockBossCharacterBase* Player, const AMockBossCharacterBase* Boss)
{
	FText Name;
	bool bFirstTime = false;

	// 自分の必殺技：技名を大きく
	if (Player != nullptr && Player->GetUltimateCallout(Name, bFirstTime))
	{
		const float Y = Canvas->SizeY * 0.62f;
		if (bFirstTime)
		{
			DrawTextCentered(FString::Printf(TEXT("FIRST ULTIMATE as %s!"), *GetDisplayName(Player->GetDefinition())), Y - 50.f * UIScale, 1.4f, ColorGold);
		}
		DrawTextCentered(Name.ToString().ToUpper(), Y, bFirstTime ? 3.2f : 2.6f, ColorUltimate);
	}

	// ボスの必殺技：警告
	if (Boss != nullptr && Boss->GetUltimateCallout(Name, bFirstTime))
	{
		const float Y = Canvas->SizeY * 0.2f;
		DrawTextCentered(FString::Printf(TEXT("!!  %s : %s  !!"), *GetDisplayName(Boss->GetDefinition()).ToUpper(), *Name.ToString().ToUpper()), Y, 2.f, ColorHealth);
	}
}

void AMockSlayerHUD::DrawPhaseBanner(const AMockBattleGameState* State, const AMockBossCharacterBase* Player)
{
	const float CenterY = Canvas->SizeY * 0.3f;

	switch (State->GetPhase())
	{
	case EMockBattlePhase::Intro:
	{
		const AMockBossCharacterBase* Boss = State->Boss;
		const int32 BossLevel = Boss ? Boss->GetCharacterLevel() : 1;
		const int32 PlayerLevel = Player ? Player->GetCharacterLevel() : 1;
		DrawTextCentered(FString::Printf(TEXT("VS  %s"), *GetCharacterName(Boss)), CenterY, 2.5f, ColorText);
		DrawTextCentered(FString::Printf(TEXT("Boss Lv.%d     Your Lv.%d"), BossLevel, PlayerLevel), CenterY + 60.f * UIScale, 1.3f, PlayerLevel >= BossLevel ? ColorText : ColorHealth);
		break;
	}

	case EMockBattlePhase::BossDown:
		DrawTextCentered(TEXT("BOSS DOWN"), CenterY, 2.5f, ColorBreak);
		break;

	case EMockBattlePhase::Crush:
	{
		DrawTextCentered(FString::Printf(TEXT("CRUSH IT!  %.1f"), State->GetPhaseTimeRemaining()), CenterY * 0.5f, 2.f, ColorChunk);
		if (const AMockCrushChunk* Chunk = State->Chunk)
		{
			const float Width = Canvas->SizeX * 0.4f;
			DrawBar((Canvas->SizeX - Width) * 0.5f, CenterY * 0.5f + 50.f * UIScale, Width, 14.f * UIScale, Chunk->GetHealthPercent(), ColorChunk);
		}
		break;
	}

	case EMockBattlePhase::Capture:
	{
		const UMockBossDefinition* Boss = State->BossDefinition;
		DrawTextCentered(TEXT("CAPTURED"), Canvas->SizeY * 0.12f, 3.5f, ColorGold);
		if (Boss != nullptr)
		{
			DrawTextCentered(FString::Printf(TEXT("%s  Lv.%d  joins you!"), *GetFormNameAt(Boss, Boss->BossLevel).ToUpper(), Boss->BossLevel), Canvas->SizeY * 0.8f, 2.f, ColorText);
		}
		break;
	}

	case EMockBattlePhase::Evolution:
	{
		const UMockBattleSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UMockBattleSessionSubsystem>();
		DrawTextCentered(TEXT("EVOLUTION!"), Canvas->SizeY * 0.12f, 3.5f, ColorGold);
		if (Session != nullptr)
		{
			const FMockSlayerBattleResult& Result = Session->LastResult;
			DrawTextCentered(FString::Printf(TEXT("%s   >>>   %s"), *Result.OldFormName.ToString(), *Result.NewFormName.ToString().ToUpper()), Canvas->SizeY * 0.8f, 1.8f, ColorText);
		}
		break;
	}

	default:
		break;
	}
}

void AMockSlayerHUD::DrawResult()
{
	DrawRect(ColorBack, 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const UMockBattleSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UMockBattleSessionSubsystem>();
	const UMockProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UMockProgressSubsystem>();
	const FMockSlayerBattleResult Result = Session ? Session->LastResult : FMockSlayerBattleResult();
	const FString BossName = GetDisplayName(Result.Boss);

	float Y = Canvas->SizeY * 0.12f;
	if (!Result.bVictory)
	{
		DrawTextCentered(TEXT("DEFEAT"), Y, 3.f, ColorHealth);
		DrawTextCentered(FString::Printf(TEXT("%s was too strong... grow stronger and try again."), *BossName), Y + 70.f * UIScale, 1.2f, ColorDim);
	}
	else if (Result.bCaptured)
	{
		DrawTextCentered(FString::Printf(TEXT("CAPTURED  %s"), *BossName), Y, 3.f, ColorGold);
		if (Result.bNewCapture)
		{
			DrawTextCentered(TEXT("NEW!  You can now fight as this boss."), Y + 70.f * UIScale, 1.3f, ColorText);
		}
	}
	else
	{
		DrawTextCentered(TEXT("CAPTURE FAILED"), Y, 3.f, ColorChunk);
		DrawTextCentered(TEXT("Not enough crush power... level up to break it in time."), Y + 70.f * UIScale, 1.2f, ColorDim);
	}

	// 経験値とレベルアップ
	Y = Canvas->SizeY * 0.3f;
	DrawTextCentered(FString::Printf(TEXT("%s   EXP +%d"), *GetDisplayName(Result.Character), Result.ExpGained), Y, 1.4f, ColorText);
	if (Result.NewLevel > Result.OldLevel)
	{
		DrawTextCentered(FString::Printf(TEXT("LEVEL UP!   Lv.%d  ->  Lv.%d"), Result.OldLevel, Result.NewLevel), Y + 40.f * UIScale, 1.8f, ColorGold);
	}
	if (Result.bEvolved)
	{
		DrawTextCentered(FString::Printf(TEXT("EVOLVED!   %s  ->  %s"), *Result.OldFormName.ToString(), *Result.NewFormName.ToString()), Y + 80.f * UIScale, 1.5f, ColorUltimate);
	}

	// キャラ選択
	const AMockBattleGameMode* GameMode = GetWorld()->GetAuthGameMode<AMockBattleGameMode>();
	const AMockSlayerPlayerController* PC = Cast<AMockSlayerPlayerController>(GetOwningPlayerController());
	if (GameMode == nullptr || PC == nullptr)
	{
		return;
	}

	Y = Canvas->SizeY * 0.48f;
	DrawTextCentered(TEXT("Choose your character"), Y, 1.4f, ColorText);
	Y += 50.f * UIScale;

	const TArray<UMockBossDefinition*> Characters = GameMode->GetSelectableCharacters();
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		const bool bSelected = Index == PC->GetSelectedCharacterIndex();
		FString ExpText;
		if (Progress != nullptr)
		{
			const FMockSlayerCharacterProgress CharacterProgress = Progress->GetProgress(Characters[Index]);
			ExpText = FString::Printf(TEXT("Lv.%d  (%d/%d)"), CharacterProgress.Level, CharacterProgress.Exp, UMockProgressSubsystem::GetExpToNextLevel(CharacterProgress.Level));
		}
		const int32 Level = Progress ? Progress->GetLevel(Characters[Index]) : 1;
		const FString Line = FString::Printf(TEXT("%s [%d] %s   %s %s"), bSelected ? TEXT(">") : TEXT(" "), Index + 1, *GetFormNameAt(Characters[Index], Level), *ExpText, bSelected ? TEXT("<") : TEXT(" "));
		DrawTextCentered(Line, Y, bSelected ? 1.5f : 1.2f, bSelected ? ColorBreak : ColorDim);
		Y += 40.f * UIScale;
	}

	DrawTextCentered(TEXT("1-4 / Left Right : choose    Enter : next battle"), Canvas->SizeY * 0.88f, 1.1f, ColorText);
}

void AMockSlayerHUD::DrawControls()
{
	const float X = Canvas->SizeX - 330.f * UIScale;
	const float Y = Canvas->SizeY - 110.f * UIScale;
	DrawTextAt(TEXT("WASD Move   Mouse Camera"), X, Y, 0.8f, ColorDim);
	DrawTextAt(TEXT("LMB Combo   RMB Skill   Q Ultimate"), X, Y + 20.f * UIScale, 0.8f, ColorDim);
	DrawTextAt(TEXT("Space Dodge   Tab / Wheel Lock-on"), X, Y + 40.f * UIScale, 0.8f, ColorDim);

	// ロックオン中の表示
	const AMockSlayerPlayerController* PC = Cast<AMockSlayerPlayerController>(GetOwningPlayerController());
	if (PC != nullptr && PC->IsLockOnEnabled())
	{
		DrawTextCentered(TEXT("[ LOCK ON ]"), Canvas->SizeY * 0.9f, 1.f, ColorBreak);
	}
}

void AMockSlayerHUD::DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& Color)
{
	DrawRect(ColorBack, X - 2.f, Y - 2.f, Width + 4.f, Height + 4.f);
	DrawRect(Color, X, Y, Width * FMath::Clamp(Percent, 0.f, 1.f), Height);
}

void AMockSlayerHUD::DrawTextAt(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color)
{
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale * UIScale);
}

void AMockSlayerHUD::DrawTextCentered(const FString& Text, float Y, float Scale, const FLinearColor& Color)
{
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, GEngine->GetMediumFont(), Scale * UIScale);
	DrawText(Text, Color, (Canvas->SizeX - Width) * 0.5f, Y, GEngine->GetMediumFont(), Scale * UIScale);
}
