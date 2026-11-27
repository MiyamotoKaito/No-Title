// Fill out your copyright notice in the Description page of Project Settings.


#include "Develop/AbilitySystem/BaseAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UBaseAttributeSet::InitAttributes() const
{
}

void UBaseAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  Power, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  CriticalMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  CriticalChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  Break, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  MaxBreak, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  Ultimate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBaseAttributeSet,  MaxUltimate, COND_None, REPNOTIFY_Always);
}

void UBaseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetBreakAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxBreak());
	}
	else if (Attribute == GetUltimateAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxUltimate());
	}
}

void UBaseAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& EffectData)
{
	Super::PostGameplayEffectExecute(EffectData);
	
	if (EffectData.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		
		if (GetHealth() <= 0.f)
		{
			// 死亡処理
		}
	}
	
	else if (EffectData.EvaluatedData.Attribute == GetBreakAttribute())
	{
		SetBreak(FMath::Clamp(GetBreak(), 0.f, GetMaxBreak()));
		if (GetBreak() <= 0.f)
		{
			// ブレイク処理(スタン)
		}
	}
	
	else if (EffectData.EvaluatedData.Attribute == GetUltimateAttribute())
	{
		SetUltimate(FMath::Clamp(GetUltimate(), 0.f, GetMaxUltimate()));
		if (GetUltimate() >= GetMaxUltimate())
		{
			// アルティメット使用可能状態にする
		}
	}
}

void UBaseAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, Health, OldHealth);
}

void UBaseAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, MaxHealth, OldMaxHealth);
}

void UBaseAttributeSet::OnRep_Power(const FGameplayAttributeData& OldPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, Power, OldPower);
}

void UBaseAttributeSet::OnRep_CriticalMultiplier(const FGameplayAttributeData& OldCriticalMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, CriticalMultiplier, OldCriticalMultiplier);
}

void UBaseAttributeSet::OnRep_CriticalChance(const FGameplayAttributeData& OldCriticalChance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, CriticalChance, OldCriticalChance);
}

void UBaseAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, Defense, OldDefense);
}

void UBaseAttributeSet::OnRep_Break(const FGameplayAttributeData& OldBreak)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, Break, OldBreak);
}

void UBaseAttributeSet::OnRep_MaxBreak(const FGameplayAttributeData& OldMaxBreak)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, MaxBreak, OldMaxBreak);
}

void UBaseAttributeSet::OnRep_Ultimate(const FGameplayAttributeData& OldUltimate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, Ultimate, OldUltimate);
}

void UBaseAttributeSet::OnRep_MaxUltimate(const FGameplayAttributeData& OldMaxUltimate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseAttributeSet, MaxUltimate, OldMaxUltimate)
}
