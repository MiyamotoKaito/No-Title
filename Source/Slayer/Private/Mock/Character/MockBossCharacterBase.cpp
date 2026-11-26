#include "Mock/Character/MockBossCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Mock/AI/MockSlayerBossAIController.h"
#include "Mock/AbilitySystem/MockSlayerAbilities.h"
#include "Mock/AbilitySystem/MockSlayerAttributeSet.h"
#include "Mock/Character/MockSlayerSimpleAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Mock/Game/MockCrushChunk.h"
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

	// ボイス（キャラの位置から鳴らす）
	VoiceComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("Voice"));
	VoiceComponent->SetupAttachment(RootComponent);
	VoiceComponent->bAutoActivate = false;

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
	// 体の大きさ：アクターごと拡大する（カプセル・メッシュ・技の位置がまとめて大きくなる）
	SetActorScale3D(FVector(FMath::Max(Definition->BodyScale, 0.1f)));

	// 進化していればその形態の見た目にする
	USkeletalMeshComponent* MeshComponent = GetMesh();
	const FMockSlayerEvolutionForm* Form = Definition->GetForm(CharacterLevel);
	USkeletalMesh* TargetMesh = (Form != nullptr && Form->Mesh != nullptr) ? Form->Mesh.Get() : Definition->Mesh.Get();
	if (TargetMesh != nullptr && MeshComponent->GetSkeletalMeshAsset() != TargetMesh)
	{
		MeshComponent->SetSkeletalMesh(TargetMesh);
	}
	// AnimBP が無いキャラは、移動アニメを速さで切り替える簡易アニメを使う
	UClass* AnimClass = Definition->AnimClass.Get();
	if (AnimClass == nullptr && Definition->IdleAnimation != nullptr)
	{
		AnimClass = UMockSlayerSimpleAnimInstance::StaticClass();
	}
	if (AnimClass != nullptr && MeshComponent->GetAnimClass() != AnimClass)
	{
		MeshComponent->SetAnimInstanceClass(AnimClass);
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

	// AI はプレイヤーを狙う（変身中は変身した姿を狙う）
	if (!IsPlayerControlled())
	{
		AMockBossCharacterBase* Player = Cast<AMockBossCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0));
		return (Player != nullptr && !Player->IsDead() && !Player->IsDormant()) ? Player : nullptr;
	}

	// プレイヤーは一番近い敵か塊を狙う
	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(MaxDistance);
	const FVector Location = GetActorLocation();

	for (TActorIterator<AMockBossCharacterBase> It(World); It; ++It)
	{
		if (*It == this || It->IsDead() || It->IsDormant())
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

	if (const FMockSlayerVoiceSet* Voices = GetVoices())
	{
		PlayVoice(Voices->Death, EVoicePriority::Important);
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
	if (const FMockSlayerVoiceSet* Voices = GetVoices())
	{
		PlayVoice(Voices->LevelUp, EVoicePriority::Important);
	}
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

const FMockSlayerVoiceSet* AMockBossCharacterBase::GetVoices() const
{
	return Definition ? &Definition->Voices : nullptr;
}

void AMockBossCharacterBase::PlayVoice(USoundBase* Sound, EVoicePriority Priority, float Chance)
{
	if (Sound == nullptr || VoiceComponent == nullptr || FMath::FRand() > Chance)
	{
		return;
	}
	// 倒れた後は大事なセリフ（断末魔など）だけ
	if (IsDead() && Priority != EVoicePriority::Important)
	{
		return;
	}
	// 鳴っている声より優先度が低ければ鳴らさない。掛け声どうしも重ねない
	if (VoiceComponent->IsPlaying())
	{
		if (Priority < CurrentVoicePriority || (Priority == EVoicePriority::Effort && CurrentVoicePriority == EVoicePriority::Effort))
		{
			return;
		}
	}
	VoiceComponent->SetSound(Sound);
	VoiceComponent->Play();
	CurrentVoicePriority = Priority;
	UE_LOG(LogTemp, Verbose, TEXT("[Voice] %s : %s"), *GetNameSafe(Definition), *Sound->GetName());
}

void AMockBossCharacterBase::NotifyDamaged(float Damage)
{
	const FMockSlayerVoiceSet* Voices = GetVoices();
	if (Voices == nullptr || AttributeSet == nullptr || IsDead() || Damage <= 0.f)
	{
		return;
	}

	// 体力が3割を切ったら1回だけ
	const float MaxHealth = AttributeSet->GetMaxHealth();
	if (!bHealthLowVoicePlayed && MaxHealth > 0.f && AttributeSet->GetHealth() / MaxHealth < 0.3f && AttributeSet->GetHealth() > 0.f)
	{
		bHealthLowVoicePlayed = true;
		PlayVoice(Voices->HealthLow, EVoicePriority::Line);
		return;
	}

	// 被弾の声は間隔をあける。大きいダメージほど確実に鳴らす
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPainVoiceTime < 1.2)
	{
		return;
	}
	LastPainVoiceTime = Now;
	const bool bHeavy = MaxHealth > 0.f && Damage >= MaxHealth * 0.1f;
	PlayVoice(bHeavy ? Voices->PainHeavy : Voices->Pain, EVoicePriority::Effort, bHeavy ? 1.f : 0.6f);
}

void AMockBossCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 必殺技ゲージが満タンになった瞬間に声で知らせる（プレイヤー操作時）
	if (IsPlayerControlled() && AttributeSet != nullptr && !IsDead())
	{
		const bool bReady = AttributeSet->GetMaxUltimateCharge() > 0.f && AttributeSet->GetUltimateCharge() >= AttributeSet->GetMaxUltimateCharge();
		if (bReady && !bUltimateWasReady && GetVoices() != nullptr)
		{
			PlayVoice(GetVoices()->UltimateReady, EVoicePriority::Line);
		}
		bUltimateWasReady = bReady;
	}

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
	StartSlowMotion(bFirstTime ? 0.2f : 0.35f, bFirstTime ? 1.4f : 0.6f);

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

float AMockBossCharacterBase::GetBodyScale() const
{
	return Definition ? FMath::Max(Definition->BodyScale, 0.1f) : 1.f;
}

void AMockBossCharacterBase::NotifyDamageDealt(float Damage)
{
	OnDamageDealt.Broadcast(this, Damage);
}

void AMockBossCharacterBase::StartSlowMotion(float TimeDilation, float Duration)
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	UGameplayStatics::SetGlobalTimeDilation(this, TimeDilation);
	// Tick で実時間を見て戻す（長い方を優先）
	SlowMotionEndRealTime = FMath::Max(SlowMotionEndRealTime, World->GetRealTimeSeconds() + Duration);
}

void AMockBossCharacterBase::SetDormant(bool bInDormant)
{
	if (bDormant == bInDormant)
	{
		return;
	}
	bDormant = bInDormant;

	// 技やモンタージュは止めておく（戻った時に途中から再開しないように）
	if (bDormant && AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}
	StopWeaponTrail();

	SetActorHiddenInGame(bDormant);
	SetActorEnableCollision(!bDormant);
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bDormant)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	else
	{
		Movement->SetMovementMode(MOVE_Walking);
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
