// Fill out your copyright notice in the Description page of Project Settings.


#include "Develop/AbilitySystem/BaseAttributeSet.h"

void UBaseAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UBaseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
}

void UBaseAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& EffectData)
{
	Super::PostGameplayEffectExecute(EffectData);
}

void UBaseAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
}

void UBaseAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
}

void UBaseAttributeSet::OnRep_Power(const FGameplayAttributeData& OldPower)
{
}

void UBaseAttributeSet::OnRep_CriticalMultiplier(const FGameplayAttributeData& OldCriticalMultiplier)
{
}

void UBaseAttributeSet::OnRep_CriticalChance(const FGameplayAttributeData& OldCriticalChance)
{
}

void UBaseAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldDefense)
{
}

void UBaseAttributeSet::OnRep_Break(const FGameplayAttributeData& OldBreak)
{
}

void UBaseAttributeSet::OnRep_MaxBreak(const FGameplayAttributeData& OldMaxBreak)
{
}

void UBaseAttributeSet::OnRep_Ultimate(const FGameplayAttributeData& OldUltimate)
{
}

void UBaseAttributeSet::OnRep_MaxUltimate(const FGameplayAttributeData& OldMaxUltimate)
{
}
