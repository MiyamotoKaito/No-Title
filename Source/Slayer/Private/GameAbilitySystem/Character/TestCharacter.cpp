// Fill out your copyright notice in the Description page of Project Settings.


#include "TestCharacter.h"


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

TArray<FGameplayAbilitySpecHandle> ATestCharacter::GrantAtlibites(TArray<TSubclassOf<UGameplayAbility>> GrantToAbilities)
{
	// ASCが未生成、またはサーバー以外では付与しない（アビリティの付与はサーバーのみ）
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return TArray<FGameplayAbilitySpecHandle>();
	}
	
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	for (TSubclassOf<UGameplayAbility> Ability : GrantToAbilities)
	{
		FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(Ability, 1, -1, this));
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
// Called to bind functionality to input
void ATestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

