// Fill out your copyright notice in the Description page of Project Settings.


#include "MockCharacter.h"
#include "EnhancedInputComponent.h"

// Sets default values
AMockCharacter::AMockCharacter()
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
	BasicAttributeSet = CreateDefaultSubobject<UMockBasicAttributeSet>(TEXT("BasicSubObjectSet"));
}

// Called when the game starts or when spawned
void AMockCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMockCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

UAbilitySystemComponent* AMockCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

TArray<FGameplayAbilitySpecHandle> AMockCharacter::GrantAbilities(const TArray<FMockAbilityData>& GrantToAbilities)
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

void AMockCharacter::RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove)
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

void AMockCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
		
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this,this);
	}
}

void AMockCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this,this);
	}
}

TArray<FGameplayAbilitySpec*> AMockCharacter::FindSpecsByInputTag(FGameplayTag InputGameplayTag) const
{
	TArray<FGameplayAbilitySpec*> Specs;
	if (AbilitySystemComponent == nullptr || !InputGameplayTag.IsValid())
	{
		return Specs;
	}
	// タグが一致する Spec だけを集める
	for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputGameplayTag))
		{
			Specs.Add(&Spec);
		}
	}
	return Specs;
}

void AMockCharacter::OnPressed(FGameplayTag InputGameplayTag)
{
	UE_LOG(LogTemp, Log, TEXT("%s : Pressed"), *InputGameplayTag.ToString());
	PressAbilityInput(InputGameplayTag);
}

void AMockCharacter::OnReleased(FGameplayTag InputGameplayTag)
{
	UE_LOG(LogTemp, Log, TEXT("%s : Released"), *InputGameplayTag.ToString());
	ReleaseAbilityInput(InputGameplayTag);
}

void AMockCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
			&AMockCharacter::OnPressed,
			InputData.ActionTag);
		
		action->BindAction(InputData.Action,
			ETriggerEvent::Completed,
			this,
			&AMockCharacter::OnReleased,
			InputData.ActionTag);
	}
}

bool AMockCharacter::PressAbilityInput(FGameplayTag InputGameplayTag)
{
	if (AbilitySystemComponent == nullptr || !InputGameplayTag.IsValid())
	{
		return false;
	}
	
	bool bHandled = false;
	
	// ループ中にアビリティの追加、削除が起きても配列が崩れないようにする。
	FScopedAbilityListLock AbilityListLock(*AbilitySystemComponent);
	// 発動可能なアビリティの配列の参照を取得
	for (const auto& Spec : FindSpecsByInputTag(InputGameplayTag))
	{
		if (!Spec->IsActive())
		{
			// 未発動:発動出来た時だけTrue
			bHandled |= AbilitySystemComponent->TryActivateAbility(Spec->Handle);
			continue;
		}
		
		bHandled = true;
		AbilitySystemComponent->AbilitySpecInputPressed(*Spec);
		
		if (Spec->GetPrimaryInstance() == nullptr)
		{
			continue;
		}
		AbilitySystemComponent->InvokeReplicatedEvent(
		EAbilityGenericReplicatedEvent::InputPressed,
		Spec->Handle,
		Spec->GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
	return bHandled;
}

void AMockCharacter::ReleaseAbilityInput(FGameplayTag InputGameplayTag)
{
	if (AbilitySystemComponent == nullptr || !InputGameplayTag.IsValid())
	{
		return;
	}
	// ループ中にアビリティの追加、削除が起きても配列が崩れないようにする。
	FScopedAbilityListLock AbilityListLock(*AbilitySystemComponent);
	for (const auto& Spec : FindSpecsByInputTag(InputGameplayTag))
	{
		if (Spec->GetPrimaryInstance() == nullptr)
		{
			continue;
		}
		if (Spec->IsActive())
		{
			AbilitySystemComponent->AbilitySpecInputReleased(*Spec);
			AbilitySystemComponent->InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputReleased,
				Spec->Handle,
				Spec->GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

bool AMockCharacter::IsAbilityInputActive(FGameplayTag InputGameplayTag) const
{
	for (const auto& Spec : FindSpecsByInputTag(InputGameplayTag))
	{
		if (Spec->IsActive())
		{
			return true;
		}
	}
	return false;
}