#include "Mock/AbilitySystem/MockSlayerAbilities.h"

#include "AbilitySystemComponent.h"
#include "Mock/AbilitySystem/MockSlayerAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Mock/Character/MockBossCharacterBase.h"
#include "Mock/Data/MockBossDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Mock/Game/MockSlayerSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SlayerGameplayTags.h"
#include "TimerManager.h"

// =====================================================================
// コンボ
// =====================================================================

UGA_MockSlayerComboAttack::UGA_MockSlayerComboAttack()
{
	InitAttackTags(false);
}

void UGA_MockSlayerComboAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (Stages.Num() == 0 || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StageIndex = 0;
	StartStage();
}

void UGA_MockSlayerComboAttack::StartStage()
{
	bNextQueued = false;
	bAdvanceReady = false;

	const FMockSlayerComboStage& Stage = Stages[StageIndex];
	FaceTarget();

	if (!PlayAnimation(Stage.Animation, Stage.PlayRate))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	StageStartTime = GetWorld()->GetTimeSeconds();
	ScheduleHits(Stage.Hits, Stage.PlayRate);

	// 掛け声（最初の段はたまにセリフ）
	if (AMockBossCharacterBase* Character = GetSlayerCharacter())
	{
		if (const FMockSlayerVoiceSet* Voices = Character->GetVoices())
		{
			if (StageIndex == 0 && FMath::FRand() < 0.3f)
			{
				Character->PlayVoice(Voices->AttackLine, AMockBossCharacterBase::EVoicePriority::Line);
			}
			else
			{
				Character->PlayVoice(Voices->AttackEffort, AMockBossCharacterBase::EVoicePriority::Effort, 0.6f);
			}
		}
	}
	ApplyLunge(Stage.LungeDistance, Stage.LungeDuration);

	const float Rate = FMath::Max(Stage.PlayRate, 0.01f);
	GetWorld()->GetTimerManager().SetTimer(AdvanceTimer, this, &UGA_MockSlayerComboAttack::OnAdvanceReady, FMath::Max(Stage.AdvanceTime / Rate, 0.01f), false);
}

void UGA_MockSlayerComboAttack::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!Stages.IsValidIndex(StageIndex))
	{
		return;
	}

	// 受付開始より前の連打は無視する
	const FMockSlayerComboStage& Stage = Stages[StageIndex];
	const float Elapsed = GetWorld()->GetTimeSeconds() - StageStartTime;
	if (Elapsed < Stage.InputOpenTime / FMath::Max(Stage.PlayRate, 0.01f))
	{
		return;
	}

	bNextQueued = true;
	if (bAdvanceReady)
	{
		AdvanceStage();
	}
}

void UGA_MockSlayerComboAttack::OnAdvanceReady()
{
	bAdvanceReady = true;
	if (bNextQueued)
	{
		AdvanceStage();
	}
}

void UGA_MockSlayerComboAttack::AdvanceStage()
{
	// 最後の段なら次はない（アニメが終わったら終了）
	if (!Stages.IsValidIndex(StageIndex + 1))
	{
		bNextQueued = false;
		return;
	}

	++StageIndex;
	ClearScheduledHits();
	StartStage();
}

void UGA_MockSlayerComboAttack::OnAnimationEnded(bool bInterrupted)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bInterrupted);
}

void UGA_MockSlayerComboAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AdvanceTimer);
	}
	if (bWasCancelled && IsActive())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Combo] %s cancelled at stage %d"), *GetName(), StageIndex + 1);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =====================================================================
// 単発技
// =====================================================================

UGA_MockSlayerMontageAttack::UGA_MockSlayerMontageAttack()
{
	InitAttackTags(true);
	CooldownDuration = 5.f;
	CooldownTag = SlayerTags::Cooldown_Skill1;
}

void UGA_MockSlayerMontageAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FaceTarget();

	if (!PlayAnimation(Animation, PlayRate))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ScheduleHits(Hits, PlayRate);
	ApplyLunge(LungeDistance, LungeDuration);

	AMockBossCharacterBase* Character = GetSlayerCharacter();

	// スキル・必殺技のセリフ
	if (Character != nullptr)
	{
		if (const FMockSlayerVoiceSet* Voices = Character->GetVoices())
		{
			Character->PlayVoice(bUltimate ? Voices->UltimateLine : Voices->SkillLine,
				bUltimate ? AMockBossCharacterBase::EVoicePriority::Important : AMockBossCharacterBase::EVoicePriority::Line);
		}
	}

	// 必殺技の演出。プレイヤーがそのキャラで初めて撃つ時は特別に長くする
	if (bUltimate && Character != nullptr)
	{
		bool bFirstTime = false;
		if (Character->IsPlayerControlled())
		{
			if (UMockProgressSubsystem* Progress = Character->GetGameInstance()->GetSubsystem<UMockProgressSubsystem>())
			{
				bFirstTime = Progress->MarkUltimateUsed(Character->GetDefinition());
			}
		}
		Character->PlayUltimateCinematic(UltimateName, bFirstTime);
	}
}

bool UGA_MockSlayerMontageAttack::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	if (!bUltimate)
	{
		return true;
	}

	// 必殺技はゲージが満タンの時だけ
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC == nullptr)
	{
		return false;
	}
	const float Charge = ASC->GetNumericAttribute(UMockSlayerAttributeSet::GetUltimateChargeAttribute());
	const float MaxCharge = ASC->GetNumericAttribute(UMockSlayerAttributeSet::GetMaxUltimateChargeAttribute());
	return MaxCharge > 0.f && Charge >= MaxCharge;
}

void UGA_MockSlayerMontageAttack::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	if (!bUltimate)
	{
		return;
	}

	// ゲージを使い切る
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->SetNumericAttributeBase(UMockSlayerAttributeSet::GetUltimateChargeAttribute(), 0.f);
	}
}

void UGA_MockSlayerMontageAttack::OnAnimationEnded(bool bInterrupted)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bInterrupted);
}

// =====================================================================
// 回避
// =====================================================================

UGA_MockSlayerDodge::UGA_MockSlayerDodge()
{
	SetAssetTags(FGameplayTagContainer(SlayerTags::Ability_Dodge));
	ActivationOwnedTags.AddTag(SlayerTags::State_Invulnerable);
	ActivationBlockedTags.AddTag(SlayerTags::State_Break);
	ActivationBlockedTags.AddTag(SlayerTags::State_Dead);
	// 攻撃の途中でも回避で抜けられる
	CancelAbilitiesWithTag.AddTag(SlayerTags::Ability_Attack);

	CooldownDuration = 0.6f;
	CooldownTag = SlayerTags::Cooldown_Dodge;
	FaceTargetDistance = 0.f;
}

void UGA_MockSlayerDodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AMockBossCharacterBase* Character = GetSlayerCharacter();
	if (Character == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 移動入力の方向へ。入力が無ければ後ろへ下がる
	FVector Direction = Character->GetLastMovementInputVector();
	Direction.Z = 0.f;
	if (!Direction.Normalize())
	{
		Direction = -Character->GetActorForwardVector();
	}
	else
	{
		Character->SetActorRotation(Direction.Rotation());
	}

	if (const FMockSlayerVoiceSet* Voices = Character->GetVoices())
	{
		Character->PlayVoice(Voices->DodgeEffort, AMockBossCharacterBase::EVoicePriority::Effort);
	}

	const float Duration = FMath::Max(DodgeDuration, 0.05f);
	UAbilityTask_ApplyRootMotionConstantForce* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Dodge"), Direction, DodgeDistance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, true);
	if (Task != nullptr)
	{
		Task->ReadyForActivation();
	}

	GetWorld()->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}), Duration, false);
}

void UGA_MockSlayerDodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =====================================================================
// ブレイク
// =====================================================================

UGA_MockSlayerBreak::UGA_MockSlayerBreak()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	FaceTargetDistance = 0.f;

	ActivationOwnedTags.AddTag(SlayerTags::State_Break);
	ActivationBlockedTags.AddTag(SlayerTags::State_Dead);
	// 実行中の技を止める
	CancelAbilitiesWithTag.AddTag(SlayerTags::Ability_Attack);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = SlayerTags::Event_Break;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_MockSlayerBreak::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AMockBossCharacterBase* Character = GetSlayerCharacter();
	const UMockBossDefinition* Definition = Character ? Character->GetDefinition() : nullptr;
	if (Character == nullptr || Definition == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->PlayVoice(Definition->Voices.Break, AMockBossCharacterBase::EVoicePriority::Line);

	if (Definition->BreakAnimation != nullptr)
	{
		PlayAnimation(Definition->BreakAnimation, 1.f);
	}

	GetWorld()->GetTimerManager().SetTimer(BreakTimer, this, &UGA_MockSlayerBreak::FinishBreak, FMath::Max(Definition->BreakDuration, 0.1f), false);
}

void UGA_MockSlayerBreak::FinishBreak()
{
	// ブレイク値を最大まで戻す
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		const float MaxBreak = ASC->GetNumericAttribute(UMockSlayerAttributeSet::GetMaxBreakAttribute());
		ASC->SetNumericAttributeBase(UMockSlayerAttributeSet::GetBreakAttribute(), MaxBreak);
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_MockSlayerBreak::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BreakTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =====================================================================
// 死亡
// =====================================================================

UGA_MockSlayerDeath::UGA_MockSlayerDeath()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	FaceTargetDistance = 0.f;

	ActivationOwnedTags.AddTag(SlayerTags::State_Dead);
	ActivationBlockedTags.AddTag(SlayerTags::State_Dead);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = SlayerTags::Event_Death;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_MockSlayerDeath::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AMockBossCharacterBase* Character = GetSlayerCharacter();
	if (ASC == nullptr || Character == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 自分以外のアビリティ（攻撃・ブレイク）をすべて止める
	ASC->CancelAbilities(nullptr, nullptr, this);

	// このアビリティは終わらせない（State.Dead を付けたままにする）
	Character->HandleDeath();
}
