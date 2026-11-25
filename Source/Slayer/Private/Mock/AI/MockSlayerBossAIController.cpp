#include "AI/MockSlayerBossAIController.h"

#include "Character/MockBossCharacterBase.h"
#include "Data/MockBossDefinition.h"
#include "Engine/World.h"
#include "Game/MockBattleGameState.h"
#include "TimerManager.h"

AMockSlayerBossAIController::AMockSlayerBossAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
}

void AMockSlayerBossAIController::OnUnPossess()
{
	StopPressing();
	Super::OnUnPossess();
}

void AMockSlayerBossAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AMockBossCharacterBase* Boss = Cast<AMockBossCharacterBase>(GetPawn());
	if (Boss == nullptr || Boss->IsDead() || Boss->GetDefinition() == nullptr)
	{
		return;
	}

	// Battle フェーズ以外は動かない（GameState が無いテスト用レベルでは常に動く）
	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	if (State != nullptr && State->GetPhase() != EMockBattlePhase::Battle)
	{
		StopPressing();
		return;
	}

	const UMockBossDefinition* Definition = Boss->GetDefinition();
	const float Now = GetWorld()->GetTimeSeconds();

	// ブレイク中・攻撃中は待つ。終わってから少し間を置く
	if (Boss->IsBroken() || Boss->IsAttacking())
	{
		NextThinkTime = Now + FMath::FRandRange(Definition->AIThinkInterval.X, Definition->AIThinkInterval.Y);
		return;
	}

	const AActor* Target = Boss->FindCombatTarget(0.f);
	if (Target == nullptr)
	{
		return;
	}

	const float Distance = FVector::Dist2D(Boss->GetActorLocation(), Target->GetActorLocation());

	if (Now >= NextThinkTime && TryStartAction(Boss, Distance))
	{
		return;
	}

	// 届く行動が無ければ近づく。近ければ相手の方を向いて構える
	if (Distance > Definition->AIPreferredRange)
	{
		MoveToward(Boss, Target);
	}
	else
	{
		FaceToward(Boss, Target, DeltaSeconds);
	}
}

bool AMockSlayerBossAIController::TryStartAction(AMockBossCharacterBase* Boss, float Distance)
{
	const UMockBossDefinition* Definition = Boss->GetDefinition();

	// 距離に合う行動を集める
	TArray<const FMockSlayerAIAction*> Candidates;
	float TotalWeight = 0.f;
	for (const FMockSlayerAIAction& Action : Definition->AIActions)
	{
		if (Action.Weight > 0.f && Distance >= Action.MinRange && Distance <= Action.MaxRange)
		{
			Candidates.Add(&Action);
			TotalWeight += Action.Weight;
		}
	}

	// 重み付きランダムで選ぶ。クールダウン中などで発動できなければ次の候補を試す
	while (Candidates.Num() > 0 && TotalWeight > 0.f)
	{
		float Pick = FMath::FRandRange(0.f, TotalWeight);
		int32 Index = 0;
		for (; Index < Candidates.Num() - 1; ++Index)
		{
			Pick -= Candidates[Index]->Weight;
			if (Pick <= 0.f)
			{
				break;
			}
		}

		const FMockSlayerAIAction* Action = Candidates[Index];
		if (Boss->PressAbilityInput(Action->InputTag))
		{
			Boss->ReleaseAbilityInput(Action->InputTag);

			// コンボなら残りの段を一定間隔で押す
			StopPressing();
			RemainingPresses = FMath::RandRange(FMath::Min(Action->PressCountMin, Action->PressCount), Action->PressCount) - 1;
			if (RemainingPresses > 0)
			{
				PendingInputTag = Action->InputTag;
				GetWorldTimerManager().SetTimer(PressTimer, this, &AMockSlayerBossAIController::PressNext, Action->PressInterval, true);
			}
			return true;
		}

		TotalWeight -= Action->Weight;
		Candidates.RemoveAt(Index);
	}
	return false;
}

void AMockSlayerBossAIController::PressNext()
{
	AMockBossCharacterBase* Boss = Cast<AMockBossCharacterBase>(GetPawn());
	if (Boss == nullptr || RemainingPresses <= 0 || !Boss->IsAbilityInputActive(PendingInputTag))
	{
		StopPressing();
		return;
	}

	--RemainingPresses;
	Boss->PressAbilityInput(PendingInputTag);
	Boss->ReleaseAbilityInput(PendingInputTag);

	if (RemainingPresses <= 0)
	{
		StopPressing();
	}
}

void AMockSlayerBossAIController::StopPressing()
{
	GetWorldTimerManager().ClearTimer(PressTimer);
	RemainingPresses = 0;
	PendingInputTag = FGameplayTag();
}

void AMockSlayerBossAIController::MoveToward(AMockBossCharacterBase* Boss, const AActor* Target) const
{
	FVector Direction = Target->GetActorLocation() - Boss->GetActorLocation();
	Direction.Z = 0.f;
	if (Direction.Normalize())
	{
		Boss->AddMovementInput(Direction, 1.f);
	}
}

void AMockSlayerBossAIController::FaceToward(AMockBossCharacterBase* Boss, const AActor* Target, float DeltaSeconds) const
{
	FVector Direction = Target->GetActorLocation() - Boss->GetActorLocation();
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		return;
	}
	const FRotator Desired = Direction.Rotation();
	Boss->SetActorRotation(FMath::RInterpTo(Boss->GetActorRotation(), Desired, DeltaSeconds, 6.f));
}
