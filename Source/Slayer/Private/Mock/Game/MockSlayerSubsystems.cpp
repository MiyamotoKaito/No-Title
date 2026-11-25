#include "Game/MockSlayerSubsystems.h"

#include "Data/MockBossDefinition.h"

bool UMockProgressSubsystem::AddCapturedBoss(UMockBossDefinition* Boss, int32 StartLevel)
{
	if (Boss == nullptr || IsCaptured(Boss))
	{
		return false;
	}
	CapturedBosses.Add(Boss);

	// 強かった時のレベルのまま仲間になる
	FMockSlayerCharacterProgress& Entry = Progress.FindOrAdd(Boss);
	Entry.Level = FMath::Max(Entry.Level, StartLevel);
	return true;
}

bool UMockProgressSubsystem::IsCaptured(const UMockBossDefinition* Boss) const
{
	return Boss != nullptr && CapturedBosses.ContainsByPredicate([Boss](const TObjectPtr<UMockBossDefinition>& Captured)
	{
		return Captured.Get() == Boss;
	});
}

FMockSlayerCharacterProgress UMockProgressSubsystem::GetProgress(const UMockBossDefinition* Character) const
{
	for (const TPair<TObjectPtr<UMockBossDefinition>, FMockSlayerCharacterProgress>& Pair : Progress)
	{
		if (Pair.Key.Get() == Character)
		{
			return Pair.Value;
		}
	}
	return FMockSlayerCharacterProgress();
}

int32 UMockProgressSubsystem::GetLevel(const UMockBossDefinition* Character) const
{
	return GetProgress(Character).Level;
}

int32 UMockProgressSubsystem::AddExp(UMockBossDefinition* Character, int32 Amount)
{
	if (Character == nullptr || Amount <= 0)
	{
		return 0;
	}

	FMockSlayerCharacterProgress& Entry = Progress.FindOrAdd(Character);
	Entry.Exp += Amount;

	int32 LevelsGained = 0;
	while (Entry.Exp >= GetExpToNextLevel(Entry.Level))
	{
		Entry.Exp -= GetExpToNextLevel(Entry.Level);
		++Entry.Level;
		++LevelsGained;
	}
	return LevelsGained;
}

bool UMockProgressSubsystem::MarkUltimateUsed(UMockBossDefinition* Character)
{
	if (Character == nullptr)
	{
		return false;
	}
	FMockSlayerCharacterProgress& Entry = Progress.FindOrAdd(Character);
	if (Entry.bUsedUltimate)
	{
		return false;
	}
	Entry.bUsedUltimate = true;
	return true;
}
