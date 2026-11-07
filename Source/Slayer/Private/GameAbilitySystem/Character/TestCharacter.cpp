// Fill out your copyright notice in the Description page of Project Settings.


#include "TestCharacter.h"
#include "EnhancedInputComponent.h"

// Sets default values
ATestCharacter::ATestCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// AbilitySystemComponentを追加する
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// ネットワークでのEffectの共有をできるように設定
	AbilitySystemComponent->SetIsReplicated(true);
	// 共有するタイプ
	AbilitySystemComponent->SetReplicationMode(ReplicationMode);
	// 属性セットをサブオブジェクトとして生成
	BasicAttributeSet = CreateDefaultSubobject<UBasicAttributeSet>(TEXT("BasicSubObjectSet"));
}

// Called when the game starts or when spawned
void ATestCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATestCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

UAbilitySystemComponent* ATestCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

TArray<FGameplayAbilitySpecHandle> ATestCharacter::GrantAbilities(const TArray<FAbilityData>& GrantToAbilities)
{
	// ASCが未生成、またはサーバー以外では付与しない（アビリティの付与はサーバーのみ）
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return TArray<FGameplayAbilitySpecHandle>();
	}
	
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;

	for (const auto& AbilityData : GrantToAbilities)
	{
		if (!AbilityData.AbilityClass)
		{
			continue;
		}
		// Spec(入れ子)を作成
		FGameplayAbilitySpec Spec;
		Spec = FGameplayAbilitySpec(AbilityData.AbilityClass, 1,-1, this);
		if (AbilityData.InputTag.IsValid())
		{
			// コンテナからSpecを取り出してタグを追加
			Spec.GetDynamicSpecSourceTags().AddTag(AbilityData.InputTag);
		}
		// Specの番号(Key)を作成
		// SpecのTagなどが紐づけられたアビリティを付与
		FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(Spec);
		AbilityHandles.Add(SpecHandle);
	}
	return AbilityHandles;
}

void ATestCharacter::RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove)
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}
	
	for (FGameplayAbilitySpecHandle Ability : AbilityHandlesToRemove)
	{
		AbilitySystemComponent->ClearAbility(Ability);
	}
}

void ATestCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
		
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this,this);
	}
}

void ATestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this,this);
	}
}

void ATestCharacter::OnPressed(FGameplayTag Tag)
{
	UE_LOG(LogTemp, Log, TEXT("%s : Pressed"), *Tag.ToString());
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}
	
	auto& SpecArray = AbilitySystemComponent->GetActivatableAbilities();
	for (auto& Spec : SpecArray)
	{
		FGameplayTagContainer GameplayTags = Spec.GetDynamicSpecSourceTags();
		if (!GameplayTags.HasTagExact(Tag))
		{
			continue;
		}
		if (Spec.IsActive())
		{
			AbilitySystemComponent->AbilitySpecInputPressed(Spec);
			AbilitySystemComponent->InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputPressed,
				Spec.Handle,
				Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
		else
		{
			AbilitySystemComponent->TryActivateAbility(Spec.Handle, true);
		}
	}
}

void ATestCharacter::OnReleased(FGameplayTag Tag)
{
	UE_LOG(LogTemp, Log, TEXT("%s : Released"), *Tag.ToString());
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}
	
	auto& SpecArray = AbilitySystemComponent->GetActivatableAbilities();
	for (auto& Spec : SpecArray)
	{
		FGameplayTagContainer GameplayTags = Spec.GetDynamicSpecSourceTags();
		if (!GameplayTags.HasTagExact(Tag))
		{
			continue;
		}
		if (Spec.IsActive())
		{
			AbilitySystemComponent->AbilitySpecInputReleased(Spec);
			AbilitySystemComponent->InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputReleased,
				Spec.Handle,
				Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

void ATestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* action = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (action == nullptr || InputDataAsset == nullptr)
	{
		return;
	}
	
	for (const auto& InputData : InputDataAsset->InputActionsData)
	{
		if (InputData.Action == nullptr || !InputData.ActionTag.IsValid())
		{
			continue;
		}
		
		action->BindAction(InputData.Action, 
			ETriggerEvent::Started,
			this,
			&ATestCharacter::OnPressed,
			InputData.ActionTag);
		
		action->BindAction(InputData.Action,
			ETriggerEvent::Completed,
			this,
			&ATestCharacter::OnReleased,
			InputData.ActionTag);
	}
}

