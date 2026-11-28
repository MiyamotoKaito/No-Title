// Copyright (c) 2026 Kaito Miyamoto. All rights reserved.


#include "Develop/Character/BossCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Develop/Data/BossDataAsset.h"

ABossCharacterBase::ABossCharacterBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AbilitySystemComponent->SetIsReplicated(true);
	
	BaseAttributeSet = CreateDefaultSubobject<UBaseAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ABossCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent == nullptr ? nullptr : AbilitySystemComponent;
}

void ABossCharacterBase::SetBossData(const FBossAttributeInitData& InBossData, int32 InLevel)
{
	BossInitData = InBossData;
	CharacterLevel = InLevel;
}

void ABossCharacterBase::PossessedBy(AController* EventController)
{
	Super::PossessedBy(EventController);
	
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	InitAttributes();
}

void ABossCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void ABossCharacterBase::InitAttributes() const
{
	auto ASC = GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}
	// HP
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetMaxHealthAttribute(),          
		BossInitData.Health.GetValueAtLevel(CharacterLevel));
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetHealthAttribute(), 
		ASC->GetNumericAttributeBase(UBaseAttributeSet::GetMaxHealthAttribute()));
	
	// Power
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetPowerAttribute(),             
		BossInitData.Power.GetValueAtLevel(CharacterLevel));
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetCriticalChanceAttribute(),
		BossInitData.CriticalChance);
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetCriticalMultiplierAttribute(),
		BossInitData.CriticalMultiplier);
	
	// Break
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetMaxBreakAttribute(),
		BossInitData.Break.GetValueAtLevel(CharacterLevel));
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetBreakAttribute(),
		ASC->GetNumericAttributeBase(UBaseAttributeSet::GetMaxBreakAttribute()));

	// Defence
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetDefenseAttribute(),
		BossInitData.Defence.GetValueAtLevel(CharacterLevel));
	
	// Ultimate
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetMaxUltimateAttribute(),
		BossInitData.Ultimate.GetValueAtLevel(CharacterLevel));
	ASC->SetNumericAttributeBase(UBaseAttributeSet::GetUltimateAttribute(),
		0.0f);
}
