#include "Character/MockBossCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "AI/MockSlayerBossAIController.h"
#include "AbilitySystem/MockSlayerAbilities.h"
#include "AbilitySystem/MockSlayerAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Game/MockCrushChunk.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "SlayerGameplayTags.h"
#include "TimerManager.h"

AMockBossCharacterBase::AMockBossCharacterBase()
{
	// 必殺技のカメラ演出とスローの解除に使う
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMockSlayerAttributeSet>(TEXT("AttributeSet"));

	// カメラの当たり判定（Camera チャンネル）はキャラを無視する
	// （自分やボスの体にカメラが押し込まれて、頭の中に入ってしまうのを防ぐ）
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 移動方向を向く三人称の操作
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->MaxWalkSpeed = 600.f;
	Movement->BrakingDecelerationWalking = 2400.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 480.f;
	CameraBoom->SocketOffset = FVector(0.f, 40.f, 80.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	CommonAbilities.Add(UGA_MockSlayerBreak::StaticClass());
	CommonAbilities.Add(UGA_MockSlayerDeath::StaticClass());

	FMockSlayerAbilityGrant Dodge;
	Dodge.AbilityClass = UGA_MockSlayerDodge::StaticClass();
	Dodge.InputTag = SlayerTags::Input_Dodge;
	PlayerOnlyAbilities.Add(Dodge);

	// AI の付け外しは GameMode が明示的に行う（プレイヤー用に Spawn した時に AI が付かないようにする）
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
	AIControllerClass = AMockSlayerBossAIController::StaticClass();
}

void AMockBossCharacterBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDefinitionVisuals();
}

void AMockBossCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefinitionVisuals();
}

void AMockBossCharacterBase::SetDefinition(UMockBossDefinition* InDefinition)
{
	Definition = InDefinition;
	ApplyDefinitionVisuals();
}

void AMockBossCharacterBase::ApplyDefinitionVisuals()
{
	if (Definition == nullptr)
	{
		return;
	}

	GetCapsuleComponent()->SetCapsuleSize(Definition->CapsuleRadius, Definition->CapsuleHalfHeight);

	// 進化していればその形態の見た目にする
	USkeletalMeshComponent* MeshComponent = GetMesh();
	const FMockSlayerEvolutionForm* Form = Definition->GetForm(CharacterLevel);
	USkeletalMesh* TargetMesh = (Form != nullptr && Form->Mesh != nullptr) ? Form->Mesh.Get() : Definition->Mesh.Get();
	if (TargetMesh != nullptr && MeshComponent->GetSkeletalMeshAsset() != TargetMesh)
	{
		MeshComponent->SetSkeletalMesh(TargetMesh);
	}
	if (Definition->AnimClass != nullptr && MeshComponent->GetAnimClass() != Definition->AnimClass)
	{
		MeshComponent->SetAnimInstanceClass(Definition->AnimClass);
	}
	MeshComponent->SetRelativeLocationAndRotation(Definition->MeshLocation, Definition->MeshRotation);

	GetCharacterMovement()->MaxWalkSpeed = Definition->WalkSpeed;
}

UAbilitySystemComponent* AMockBossCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMockBossCharacterBase::InitAbilityActorInfo()
{
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AMockBossCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// AI（敵）にもプレイヤーにも対応するため、ここと OnRep_PlayerState の両方で初期化する
	InitAbilityActorInfo();
	GrantDefaultAbilities();
	ApplyControllerSettings(NewController != nullptr && NewController->IsPlayerController());
}

void AMockBossCharacterBase::UnPossessed()
{
	// プレイヤー専用アビリティは操作をやめたら外す
	if (HasAuthority() && AbilitySystemComponent != nullptr)
	{
		for (const FGameplayAbilitySpecHandle& Handle : PlayerAbilityHandles)
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}
	PlayerAbilityHandles.Reset();

	Super::UnPossessed();
}

void AMockBossCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void AMockBossCharacterBase::GrantDefaultAbilities()
{
	if (!HasAuthority() || AbilitySystemComponent == nullptr || bDefaultAbilitiesGranted)
	{
		return;
	}
	bDefaultAbilitiesGranted = true;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : CommonAbilities)
	{
		if (AbilityClass != nullptr)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}

	if (Definition == nullptr)
	{
		return;
	}

	for (const FMockSlayerAbilityGrant& Grant : Definition->Abilities)
	{
		if (Grant.AbilityClass == nullptr)
		{
			continue;
		}
		FGameplayAbilitySpec Spec(Grant.AbilityClass, 1, INDEX_NONE, this);
		if (Grant.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
		}
		AbilitySystemComponent->GiveAbility(Spec);
	}
}

void AMockBossCharacterBase::ApplyControllerSettings(bool bPlayerControlled)
{
	if (!HasAuthority() || AbilitySystemComponent == nullptr)
	{
		return;
	}

	// AI だけのボスは GE の情報をクライアントへ最小限だけ送る
	AbilitySystemComponent->SetReplicationMode(bPlayerControlled ? EGameplayEffectReplicationMode::Mixed : EGameplayEffectReplicationMode::Minimal);

	// ステータス（Max を先に入れてから現在値を入れる。現在値は Max でクランプされるため）
	// HP・攻撃力・砕く力・ブレイク値はレベルで伸びる
	const float LevelScale = Definition ? Definition->GetLevelScale(CharacterLevel) : 1.f;
	const float BaseHealth = (Definition ? Definition->MaxHealth : 1000.f) * LevelScale;
	const float MaxHealth = bPlayerControlled && Definition ? BaseHealth * Definition->PlayerHealthScale : BaseHealth;
	// プレイヤー操作中はブレイクしない
	const float MaxBreak = (!bPlayerControlled && Definition) ? Definition->MaxBreak * LevelScale : 0.f;

	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetMaxBreakAttribute(), MaxBreak);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetBreakAttribute(), MaxBreak);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetAttackPowerAttribute(), (Definition ? Definition->AttackPower : 1.f) * LevelScale);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetCrushPowerAttribute(), (Definition ? Definition->CrushPower : 1.f) * LevelScale);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetUltimateChargeAttribute(), 0.f);

	// プレイヤー専用アビリティ
	if (!bPlayerControlled || PlayerAbilityHandles.Num() > 0)
	{
		return;
	}
	for (const FMockSlayerAbilityGrant& Grant : PlayerOnlyAbilities)
	{
		if (Grant.AbilityClass == nullptr)
		{
			continue;
		}
		FGameplayAbilitySpec Spec(Grant.AbilityClass, 1, INDEX_NONE, this);
		if (Grant.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
		}
		PlayerAbilityHandles.Add(AbilitySystemComponent->GiveAbility(Spec));
	}
}

// ---------------------------------------------------------------------
// 入力
// ---------------------------------------------------------------------

bool AMockBossCharacterBase::PressAbilityInput(FGameplayTag InputTag)
{
	if (AbilitySystemComponent == nullptr || IsDead())
	{
		return false;
	}

	// ループ中にアビリティの追加・削除が起きても Spec のポインタが壊れないようにする
	FScopedAbilityListLock AbilityListLock(*AbilitySystemComponent);

	bool bHandled = false;
	for (FGameplayAbilitySpec* Spec : FindSpecsByInputTag(InputTag))
	{
		if (!Spec->IsActive())
		{
			// 未発動なら発動（クールダウン中・ブレイク中などは失敗する）
			bHandled |= AbilitySystemComponent->TryActivateAbility(Spec->Handle);
			continue;
		}

		// 発動中ならコンボ継続などの入力として通知する
		bHandled = true;
		AbilitySystemComponent->AbilitySpecInputPressed(*Spec);
		SendInputEvent(*Spec, EAbilityGenericReplicatedEvent::InputPressed);
	}
	return bHandled;
}

void AMockBossCharacterBase::ReleaseAbilityInput(FGameplayTag InputTag)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}

	FScopedAbilityListLock AbilityListLock(*AbilitySystemComponent);
	for (FGameplayAbilitySpec* Spec : FindSpecsByInputTag(InputTag))
	{
		if (!Spec->IsActive())
		{
			continue;
		}
		AbilitySystemComponent->AbilitySpecInputReleased(*Spec);
		SendInputEvent(*Spec, EAbilityGenericReplicatedEvent::InputReleased);
	}
}

bool AMockBossCharacterBase::IsAbilityInputActive(FGameplayTag InputTag) const
{
	for (const FGameplayAbilitySpec* Spec : FindSpecsByInputTag(InputTag))
	{
		if (Spec->IsActive())
		{
			return true;
		}
	}
	return false;
}

TArray<FGameplayAbilitySpec*> AMockBossCharacterBase::FindSpecsByInputTag(FGameplayTag InputTag) const
{
	TArray<FGameplayAbilitySpec*> Result;
	if (AbilitySystemComponent == nullptr || !InputTag.IsValid())
	{
		return Result;
	}

	for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			Result.Add(&Spec);
		}
	}
	return Result;
}

void AMockBossCharacterBase::SendInputEvent(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType)
{
	// インスタンスが無いGAには送れない（このプロジェクトのGAは InstancedPerActor）
	const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
	if (Instance == nullptr)
	{
		return;
	}
	AbilitySystemComponent->InvokeReplicatedEvent(EventType, Spec.Handle, Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
}

// ---------------------------------------------------------------------
// 状態
// ---------------------------------------------------------------------

bool AMockBossCharacterBase::IsDead() const
{
	return AbilitySystemComponent != nullptr && AbilitySystemComponent->HasMatchingGameplayTag(SlayerTags::State_Dead);
}

bool AMockBossCharacterBase::IsBroken() const
{
	return AbilitySystemComponent != nullptr && AbilitySystemComponent->HasMatchingGameplayTag(SlayerTags::State_Break);
}

bool AMockBossCharacterBase::IsAttacking() const
{
	return AbilitySystemComponent != nullptr && AbilitySystemComponent->HasMatchingGameplayTag(SlayerTags::State_Attacking);
}

AActor* AMockBossCharacterBase::FindCombatTarget(float MaxDistance) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// AI はプレイヤーを狙う
	if (!IsPlayerControlled())
	{
		AMockBossCharacterBase* Player = Cast<AMockBossCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0));
		return (Player != nullptr && !Player->IsDead()) ? Player : nullptr;
	}

	// プレイヤーは一番近い敵か塊を狙う
	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(MaxDistance);
	const FVector Location = GetActorLocation();

	for (TActorIterator<AMockBossCharacterBase> It(World); It; ++It)
	{
		if (*It == this || It->IsDead())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Location, It->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	for (TActorIterator<AMockCrushChunk> It(World); It; ++It)
	{
		if (It->IsBroken())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Location, It->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	return Best;
}

FName AMockBossCharacterBase::GetMontageSlotName() const
{
	if (Definition != nullptr)
	{
		const UAnimMontage* Intro = Definition->IntroMontage;
		if (Intro != nullptr && Intro->SlotAnimTracks.Num() > 0)
		{
			return Intro->SlotAnimTracks[0].SlotName;
		}
		return Definition->FallbackSlotName;
	}
	return TEXT("DefaultSlot");
}

float AMockBossCharacterBase::PlayIntro()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (Definition == nullptr || Definition->IntroMontage == nullptr || AnimInstance == nullptr)
	{
		return 0.f;
	}
	return AnimInstance->Montage_Play(Definition->IntroMontage);
}

void AMockBossCharacterBase::HandleDeath()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	// 倒れた後は他のキャラの邪魔をしない
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Definition != nullptr && Definition->DeathAnimation != nullptr)
	{
		// 最後のポーズで止まるように、ループしない単体再生にする
		GetMesh()->PlayAnimation(Definition->DeathAnimation, false);
	}

	OnDied.Broadcast(this);
}

void AMockBossCharacterBase::SetCharacterLevel(int32 InLevel)
{
	CharacterLevel = FMath::Max(InLevel, 1);
	ApplyDefinitionVisuals();
}

FText AMockBossCharacterBase::GetFormName() const
{
	return Definition ? Definition->GetFormName(CharacterLevel) : FText::GetEmpty();
}

void AMockBossCharacterBase::Evolve(int32 NewLevel, UParticleSystem* Effect)
{
	SetCharacterLevel(NewLevel);
	SpawnEffectAttached(Effect, NAME_None, 1.5f);
	PlayIntro();
}

void AMockBossCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// スロー中に消えた時（ボスが塊になった時など）に、スローが戻らなくならないようにする
	if (SlowMotionEndRealTime > 0.0)
	{
		SlowMotionEndRealTime = 0.0;
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}
	Super::EndPlay(EndPlayReason);
}

void AMockBossCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	// スロー中もカメラは実時間で動かす
	const float RealDelta = FApp::GetDeltaTime();

	// スローを戻す
	if (SlowMotionEndRealTime > 0.0 && Now >= SlowMotionEndRealTime)
	{
		SlowMotionEndRealTime = 0.0;
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}

	// 必殺技中はカメラを寄せる
	const float TargetArm = Now < CameraZoomEndRealTime ? DefaultArmLength * 0.55f : DefaultArmLength;
	if (!FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetArm, 1.f))
	{
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, RealDelta, 5.f);
	}
}

void AMockBossCharacterBase::PlayUltimateCinematic(const FText& UltimateName, bool bFirstTime)
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("[Ultimate] %s : %s (first=%d)"), *GetNameSafe(Definition), *UltimateName.ToString(), bFirstTime ? 1 : 0);

	// 技名の表示
	CalloutName = UltimateName;
	bCalloutFirstTime = bFirstTime;
	CalloutEndRealTime = Now + (bFirstTime ? 3.0 : 1.8);

	// 溜めのエフェクト
	if (Definition != nullptr)
	{
		SpawnEffectAttached(Definition->UltimateWarmupEffect, NAME_None);
	}

	// 一瞬スローにして「撃った」瞬間を見せる（初めての時は長め）
	UGameplayStatics::SetGlobalTimeDilation(this, bFirstTime ? 0.2f : 0.35f);
	SlowMotionEndRealTime = Now + (bFirstTime ? 1.4 : 0.6);

	// プレイヤーが撃つ時はカメラを寄せる
	if (IsPlayerControlled())
	{
		CameraZoomEndRealTime = Now + (bFirstTime ? 2.2 : 1.2);
	}
}

bool AMockBossCharacterBase::GetUltimateCallout(FText& OutName, bool& bOutFirstTime) const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetRealTimeSeconds() >= CalloutEndRealTime)
	{
		return false;
	}
	OutName = CalloutName;
	bOutFirstTime = bCalloutFirstTime;
	return true;
}

void AMockBossCharacterBase::StartWeaponTrail()
{
	if (Definition == nullptr || Definition->TrailEffect == nullptr)
	{
		return;
	}
	if (TrailComponent == nullptr)
	{
		TrailComponent = UGameplayStatics::SpawnEmitterAttached(Definition->TrailEffect, GetMesh(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
	}
	if (TrailComponent != nullptr)
	{
		TrailComponent->BeginTrails(Definition->TrailSocketStart, Definition->TrailSocketEnd, ETrailWidthMode_FromCentre, 1.f);
	}
}

void AMockBossCharacterBase::StopWeaponTrail()
{
	if (TrailComponent != nullptr)
	{
		TrailComponent->EndTrails();
	}
}

void AMockBossCharacterBase::SpawnEffectAtLocation(UParticleSystem* Effect, const FVector& Location, const FRotator& Rotation, float Scale) const
{
	if (Effect != nullptr)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Effect, Location, Rotation, FVector(Scale));
	}
}

void AMockBossCharacterBase::SpawnEffectAttached(UParticleSystem* Effect, FName SocketName, float Scale)
{
	if (Effect != nullptr)
	{
		UGameplayStatics::SpawnEmitterAttached(Effect, GetMesh(), SocketName, FVector::ZeroVector, FRotator::ZeroRotator, FVector(Scale), EAttachLocation::SnapToTarget, true);
	}
}

void AMockBossCharacterBase::NotifyHitLanded(AActor* HitActor)
{
	if (HitStopDuration <= 0.f || GetWorld() == nullptr)
	{
		return;
	}

	// 当てた側と当たった側を一瞬だけ止めて、攻撃の重さを出す
	CustomTimeDilation = 0.05f;
	if (HitActor != nullptr)
	{
		HitActor->CustomTimeDilation = 0.05f;
	}

	TWeakObjectPtr<AActor> WeakHitActor = HitActor;
	GetWorldTimerManager().SetTimer(HitStopTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakHitActor]()
	{
		CustomTimeDilation = 1.f;
		if (AActor* Actor = WeakHitActor.Get())
		{
			Actor->CustomTimeDilation = 1.f;
		}
	}), HitStopDuration, false);
}
