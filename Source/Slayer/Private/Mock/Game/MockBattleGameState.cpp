#include "Mock/Game/MockBattleGameState.h"

#include "Mock/Character/MockBossCharacterBase.h"
#include "Mock/Game/MockCrushChunk.h"
#include "Net/UnrealNetwork.h"

void AMockBattleGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMockBattleGameState, Phase);
	DOREPLIFETIME(AMockBattleGameState, PhaseEndTime);
	DOREPLIFETIME(AMockBattleGameState, Boss);
	DOREPLIFETIME(AMockBattleGameState, Chunk);
	DOREPLIFETIME(AMockBattleGameState, BossDefinition);
}

void AMockBattleGameState::SetPhase(EMockBattlePhase NewPhase, float Duration)
{
	Phase = NewPhase;
	PhaseEndTime = Duration > 0.f ? GetServerWorldTimeSeconds() + Duration : 0.f;
	OnPhaseChanged.Broadcast(Phase);
}

float AMockBattleGameState::GetPhaseTimeRemaining() const
{
	if (PhaseEndTime <= 0.f)
	{
		return 0.f;
	}
	return FMath::Max(PhaseEndTime - static_cast<float>(GetServerWorldTimeSeconds()), 0.f);
}

void AMockBattleGameState::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(Phase);
}
