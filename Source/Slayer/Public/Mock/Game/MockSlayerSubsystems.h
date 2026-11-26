// 画面（レベル）をまたいで持ち越すデータ
// GameMode や HUD から参照するだけにして、画面同士で直接データを渡し合わない

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MockSlayerSubsystems.generated.h"

class UMockBossDefinition;

/**
 * バトルで変身した姿（ボス）が得た経験値
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerFormExp
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TObjectPtr<UMockBossDefinition> Form;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 OldLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 NewLevel = 1;
};

/**
 * 直前のバトルの結果
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerBattleResult
{
	GENERATED_BODY()

	// 戦ったボス
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TObjectPtr<UMockBossDefinition> Boss;

	// 操作していたキャラ
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TObjectPtr<UMockBossDefinition> Character;

	// ボスを倒したか
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bVictory = false;

	// 塊を砕いて獲得できたか
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bCaptured = false;

	// 今回が初めての獲得か
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bNewCapture = false;

	// 得た経験値とレベルの変化
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 ExpGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 OldLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 NewLevel = 1;

	// 進化したか（形態が変わったか）と、進化前後の形態名
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bEvolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FText OldFormName;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FText NewFormName;

	// 変身した姿（ボス）も同じだけ経験値を得る
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FMockSlayerFormExp> FormResults;

	// このバトルのレベルアップで新しく使えるようになった操作キャラ
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<TObjectPtr<UMockBossDefinition>> NewlyUnlocked;
};

/**
 * キャラ1体ぶんの成長
 */
USTRUCT(BlueprintType)
struct SLAYER_API FMockSlayerCharacterProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 Level = 1;

	// 今のレベルで溜まっている経験値
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 Exp = 0;

	// このキャラで必殺技を撃ったことがあるか（初回の特別演出用）
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	bool bUsedUltimate = false;
};

/**
 * 1回のバトルの情報（選んだボス・選んだキャラ・直前の結果）
 */
UCLASS()
class SLAYER_API UMockBattleSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 挑むボス（null ならバトル側で決める）
	UPROPERTY(BlueprintReadWrite, Category = "Session")
	TObjectPtr<UMockBossDefinition> SelectedBoss;

	// 操作するキャラ（null ならバトル側の初期キャラ）
	UPROPERTY(BlueprintReadWrite, Category = "Session")
	TObjectPtr<UMockBossDefinition> SelectedCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FMockSlayerBattleResult LastResult;
};

/**
 * 進行状況（変身できるボスと、キャラごとのレベル・経験値）
 * 「獲得済みのボス」＝変身できるボス（最初から変身できるボスも含む）
 * Alpha ではゲーム起動中だけ保持する（セーブはロードマップ3で入れる）
 */
UCLASS()
class SLAYER_API UMockProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 獲得済みに追加する（StartLevel のレベルで仲間になる）。初めての獲得なら true
	bool AddCapturedBoss(UMockBossDefinition* Boss, int32 StartLevel = 1);

	UFUNCTION(BlueprintPure, Category = "Progress")
	bool IsCaptured(const UMockBossDefinition* Boss) const;

	const TArray<TObjectPtr<UMockBossDefinition>>& GetCapturedBosses() const { return CapturedBosses; }

	UFUNCTION(BlueprintPure, Category = "Progress")
	int32 GetLevel(const UMockBossDefinition* Character) const;

	FMockSlayerCharacterProgress GetProgress(const UMockBossDefinition* Character) const;

	// 経験値を足す。上がったレベル数を返す
	int32 AddExp(UMockBossDefinition* Character, int32 Amount);

	// 次のレベルまでに必要な経験値
	static int32 GetExpToNextLevel(int32 Level) { return 80 + 40 * Level; }

	// 必殺技を初めて撃った時に記録する。初めてなら true
	bool MarkUltimateUsed(UMockBossDefinition* Character);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMockBossDefinition>> CapturedBosses;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UMockBossDefinition>, FMockSlayerCharacterProgress> Progress;
};
