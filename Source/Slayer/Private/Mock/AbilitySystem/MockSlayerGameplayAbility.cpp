#include "Mock/AbilitySystem/MockSlayerGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Mock/AbilitySystem/MockSlayerGameplayEffects.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Mock/Character/MockBossCharacterBase.h"
#include "Mock/Data/MockBossDefinition.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "SlayerGameplayTags.h"
#include "TimerManager.h"

namespace
{
	// 当たり判定を画面に表示する（コンソールで slayer.DebugHits 1）
	TAutoConsoleVariable<int32> CVarSlayerDebugHits(TEXT("slayer.DebugHits"), 0, TEXT("1 で技の当たり判定を表示する"));
}

UMockSlayerGameplayAbility::UMockSlayerGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	CooldownGameplayEffectClass = UGE_MockSlayerCooldown::StaticClass();
}

const FGameplayTagContainer* UMockSlayerGameplayAbility::GetCooldownTags() const
{
	CooldownTagContainer.Reset();
	if (CooldownTag.IsValid())
	{
		CooldownTagContainer.AddTag(CooldownTag);
	}
	return &CooldownTagContainer;
}

void UMockSlayerGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownDuration <= 0.f || !CooldownTag.IsValid())
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UGE_MockSlayerCooldown::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
	SpecHandle.Data->SetSetByCallerMagnitude(SlayerTags::Data_Cooldown, CooldownDuration);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

void UMockSlayerGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearScheduledHits();

	// 古いアニメの終了通知を無効にする
	++AnimSerial;

	// キャンセルされた時は自分のアニメを止める（ブレイクや回避で中断された時など）
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (bWasCancelled && ASC != nullptr && PlayingMontage != nullptr && ASC->GetCurrentMontage() == PlayingMontage)
	{
		ASC->CurrentMontageStop(0.2f);
	}
	PlayingMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMockSlayerGameplayAbility::InitAttackTags(bool bIsSkill)
{
	ActivationOwnedTags.AddTag(SlayerTags::State_Attacking);
	ActivationBlockedTags.AddTag(SlayerTags::State_Break);
	ActivationBlockedTags.AddTag(SlayerTags::State_Dead);

	if (!bIsSkill)
	{
		// コンボ：攻撃中（スキル中も含む）は新しく始めない
		SetAssetTags(FGameplayTagContainer(SlayerTags::Ability_Attack_Combo));
		ActivationBlockedTags.AddTag(SlayerTags::State_Attacking);
		return;
	}

	// スキル・必殺技：コンボの途中でも出せる（コンボを中断して出る）。スキル同士は重ねない
	SetAssetTags(FGameplayTagContainer(SlayerTags::Ability_Attack_Skill));
	ActivationOwnedTags.AddTag(SlayerTags::State_UsingSkill);
	ActivationBlockedTags.AddTag(SlayerTags::State_UsingSkill);
	CancelAbilitiesWithTag.AddTag(SlayerTags::Ability_Attack_Combo);
}

AMockBossCharacterBase* UMockSlayerGameplayAbility::GetSlayerCharacter() const
{
	return Cast<AMockBossCharacterBase>(GetAvatarActorFromActorInfo());
}

void UMockSlayerGameplayAbility::FaceTarget() const
{
	AMockBossCharacterBase* Character = GetSlayerCharacter();
	if (Character == nullptr || FaceTargetDistance <= 0.f)
	{
		return;
	}

	const AActor* Target = Character->FindCombatTarget(FaceTargetDistance);
	if (Target == nullptr)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - Character->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}
	Character->SetActorRotation(ToTarget.Rotation());
}

bool UMockSlayerGameplayAbility::PlayAnimation(UAnimSequenceBase* Animation, float PlayRate)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AMockBossCharacterBase* Character = GetSlayerCharacter();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (Animation == nullptr || ASC == nullptr || Character == nullptr || AnimInstance == nullptr)
	{
		return false;
	}

	// シーケンスならキャラのスロットで動的モンタージュにする
	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	if (Montage == nullptr)
	{
		Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Animation, Character->GetMontageSlotName(), 0.1f, 0.2f);
	}
	if (Montage == nullptr)
	{
		return false;
	}

	const float Duration = ASC->PlayMontage(this, GetCurrentActivationInfo(), Montage, FMath::Max(PlayRate, 0.01f));
	if (Duration <= 0.f)
	{
		return false;
	}

	PlayingMontage = Montage;
	const int32 Serial = ++AnimSerial;

	FOnMontageEnded EndDelegate = FOnMontageEnded::CreateUObject(this, &UMockSlayerGameplayAbility::HandleMontageEnded, Serial);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void UMockSlayerGameplayAbility::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial)
{
	// 次の段のアニメで上書きされた時など、古い通知は無視する
	if (Serial != AnimSerial || !IsActive())
	{
		return;
	}
	OnAnimationEnded(bInterrupted);
}

void UMockSlayerGameplayAbility::ScheduleHits(const TArray<FMockSlayerHitEvent>& Hits, float PlayRate)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float Rate = FMath::Max(PlayRate, 0.01f);
	float FirstMeleeTime = TNumericLimits<float>::Max();
	float LastMeleeTime = -1.f;
	for (const FMockSlayerHitEvent& Hit : Hits)
	{
		FTimerHandle& Handle = HitTimers.AddDefaulted_GetRef();
		const FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(this, [this, Hit]()
		{
			ExecuteHit(Hit);
		});
		World->GetTimerManager().SetTimer(Handle, Delegate, FMath::Max(Hit.Time / Rate, 0.001f), false);

		// 近接の判定（前に伸ばさないもの）の前後だけ武器の軌跡を出す
		if (Hit.Range <= 0.f)
		{
			FirstMeleeTime = FMath::Min(FirstMeleeTime, Hit.Time);
			LastMeleeTime = FMath::Max(LastMeleeTime, Hit.Time);
		}
	}

	if (LastMeleeTime < 0.f)
	{
		return;
	}

	TWeakObjectPtr<AMockBossCharacterBase> WeakCharacter = GetSlayerCharacter();
	const float TrailStart = FMath::Max((FirstMeleeTime - 0.2f) / Rate, 0.001f);
	const float TrailEnd = (LastMeleeTime + 0.12f) / Rate;

	World->GetTimerManager().SetTimer(HitTimers.AddDefaulted_GetRef(), FTimerDelegate::CreateWeakLambda(this, [WeakCharacter]()
	{
		if (AMockBossCharacterBase* Character = WeakCharacter.Get())
		{
			Character->StartWeaponTrail();
		}
	}), TrailStart, false);

	World->GetTimerManager().SetTimer(HitTimers.AddDefaulted_GetRef(), FTimerDelegate::CreateWeakLambda(this, [WeakCharacter]()
	{
		if (AMockBossCharacterBase* Character = WeakCharacter.Get())
		{
			Character->StopWeaponTrail();
		}
	}), FMath::Max(TrailEnd, TrailStart + 0.01f), false);
}

void UMockSlayerGameplayAbility::ClearScheduledHits()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : HitTimers)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	HitTimers.Reset();

	if (AMockBossCharacterBase* Character = GetSlayerCharacter())
	{
		Character->StopWeaponTrail();
	}
}

void UMockSlayerGameplayAbility::ApplyLunge(float Distance, float Duration)
{
	AMockBossCharacterBase* Character = GetSlayerCharacter();
	if (Character == nullptr || Distance <= 0.f || Duration <= 0.f)
	{
		return;
	}

	// 体の大きいキャラは、踏み込みも止まる距離も大きくする
	const float BodyScale = Character->GetBodyScale();

	// 目の前に相手がいれば、相手の手前で止まるように距離を縮める
	const float StopDistance = 130.f * BodyScale;
	float LungeDistance = Distance * BodyScale;
	if (const AActor* Target = Character->FindCombatTarget(LungeDistance + StopDistance + 200.f))
	{
		const FVector ToTarget = Target->GetActorLocation() - Character->GetActorLocation();
		const float Ahead = FVector::DotProduct(ToTarget.GetSafeNormal2D(), Character->GetActorForwardVector());
		if (Ahead > 0.5f)
		{
			LungeDistance = FMath::Min(LungeDistance, ToTarget.Size2D() - StopDistance);
		}
	}
	if (LungeDistance <= 10.f)
	{
		return;
	}

	UAbilityTask_ApplyRootMotionConstantForce* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Lunge"), Character->GetActorForwardVector(), LungeDistance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, true);
	if (Task != nullptr)
	{
		Task->ReadyForActivation();
	}
}

void UMockSlayerGameplayAbility::ExecuteHit(const FMockSlayerHitEvent& Hit) const
{
	AMockBossCharacterBase* Character = GetSlayerCharacter();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();
	if (Character == nullptr || SourceASC == nullptr || World == nullptr || !IsActive())
	{
		return;
	}

	// Offset はアクターの拡大に合わせて自動で伸びる。半径・射程・エフェクトは体の大きさを掛ける
	const float BodyScale = Character->GetBodyScale();
	const float Radius = Hit.Radius * BodyScale;
	const float EffectScale = Hit.EffectScale * BodyScale;

	const FTransform& Transform = Character->GetActorTransform();
	const FVector Start = Transform.TransformPosition(Hit.Offset);
	const FVector End = Hit.Range > 0.f ? Start + Character->GetActorForwardVector() * Hit.Range * BodyScale : Start;

	// 技のエフェクト（爆発・マズルフラッシュなど）
	if (Hit.CastEffect != nullptr)
	{
		if (Hit.CastSocket.IsNone())
		{
			Character->SpawnEffectAtLocation(Hit.CastEffect, Start, Character->GetActorRotation(), EffectScale);
		}
		else
		{
			Character->SpawnEffectAttached(Hit.CastEffect, Hit.CastSocket, EffectScale);
		}
	}

	// ダメージはサーバーでだけ与える
	if (!Character->HasAuthority())
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlayerHit), false, Character);

	TArray<FHitResult> HitResults;
	World->SweepMultiByObjectType(HitResults, Start, End + FVector(0.f, 0.f, 0.01f), FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	if (CVarSlayerDebugHits.GetValueOnGameThread() > 0)
	{
		DrawDebugCapsule(World, (Start + End) * 0.5f, (End - Start).Size() * 0.5f + Radius, Radius,
			FRotationMatrix::MakeFromZ(End - Start + FVector(0.f, 0.f, 0.01f)).ToQuat(), HitResults.Num() > 0 ? FColor::Red : FColor::Green, false, 0.5f);
	}

	// 同じ相手に複数回当たらないようにする
	TSet<const AActor*> AlreadyHit;
	for (const FHitResult& Result : HitResults)
	{
		AActor* TargetActor = Result.GetActor();
		if (TargetActor == nullptr || TargetActor == Character || AlreadyHit.Contains(TargetActor))
		{
			continue;
		}
		AlreadyHit.Add(TargetActor);

		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
		if (TargetASC == nullptr)
		{
			continue;
		}

		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(UGE_MockSlayerDamage::StaticClass(), GetAbilityLevel());
		if (!SpecHandle.IsValid())
		{
			continue;
		}
		SpecHandle.Data->SetSetByCallerMagnitude(SlayerTags::Data_Damage, Hit.Damage);
		SpecHandle.Data->SetSetByCallerMagnitude(SlayerTags::Data_BreakDamage, Hit.BreakDamage);
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

		// ヒットのエフェクト（技の指定が無ければキャラ共通のもの）
		UParticleSystem* Impact = Hit.ImpactEffect;
		if (Impact == nullptr && Character->GetDefinition() != nullptr)
		{
			Impact = Character->GetDefinition()->ImpactEffect;
		}
		const FVector ImpactLocation = Result.bBlockingHit || !Result.ImpactPoint.IsZero() ? FVector(Result.ImpactPoint) : TargetActor->GetActorLocation();
		Character->SpawnEffectAtLocation(Impact, ImpactLocation, (Character->GetActorLocation() - ImpactLocation).Rotation(), EffectScale);

		Character->NotifyHitLanded(TargetActor);
	}
}
