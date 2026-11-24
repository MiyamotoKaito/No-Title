#include "AbilitySystem/MockSlayerAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "SlayerGameplayTags.h"

UMockSlayerAttributeSet::UMockSlayerAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitBreak(0.f);
	InitMaxBreak(0.f);
	InitAttackPower(1.f);
	InitCrushPower(1.f);
	InitUltimateCharge(0.f);
	InitMaxUltimateCharge(100.f);
	InitIncomingDamage(0.f);
	InitIncomingBreakDamage(0.f);
}

void UMockSlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, Break, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, MaxBreak, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, CrushPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, UltimateCharge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMockSlayerAttributeSet, MaxUltimateCharge, COND_None, REPNOTIFY_Always);
}

void UMockSlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
		return;
	}
	if (Attribute == GetBreakAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxBreak());
		return;
	}
	if (Attribute == GetUltimateChargeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxUltimateCharge());
	}
}

void UMockSlayerAttributeSet::AddUltimateCharge(UAbilitySystemComponent* ASC, float Amount)
{
	if (ASC == nullptr || Amount <= 0.f)
	{
		return;
	}
	const float Current = ASC->GetNumericAttributeBase(GetUltimateChargeAttribute());
	const float Max = ASC->GetNumericAttribute(GetMaxUltimateChargeAttribute());
	ASC->SetNumericAttributeBase(GetUltimateChargeAttribute(), FMath::Min(Current + Amount, Max));
}

void UMockSlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	UAbilitySystemComponent& TargetASC = Data.Target;
	AActor* Instigator = Data.EffectSpec.GetContext().GetOriginalInstigator();

	// 死んでいる相手には何もしない
	if (TargetASC.HasMatchingGameplayTag(SlayerTags::State_Dead))
	{
		SetIncomingDamage(0.f);
		SetIncomingBreakDamage(0.f);
		return;
	}

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage <= 0.f)
		{
			return;
		}

		const float NewHealth = FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth());
		SetHealth(NewHealth);

		// 必殺技ゲージ：受けた側と与えた側の両方に溜まる（塊を殴った時は溜めない）
		if (!TargetASC.HasMatchingGameplayTag(SlayerTags::Actor_CrushChunk))
		{
			AddUltimateCharge(&TargetASC, Damage * UltimateChargePerDamageTaken);
			AddUltimateCharge(Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent(), Damage * UltimateChargePerDamageDealt);
		}

		if (NewHealth <= 0.f)
		{
			SendEventToOwner(SlayerTags::Event_Death, Instigator, Damage);
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetIncomingBreakDamageAttribute())
	{
		const float BreakDamage = GetIncomingBreakDamage();
		SetIncomingBreakDamage(0.f);

		// ブレイクしないキャラ（MaxBreak が0）や、ブレイク中は減らさない
		if (BreakDamage <= 0.f || GetMaxBreak() <= 0.f || TargetASC.HasMatchingGameplayTag(SlayerTags::State_Break))
		{
			return;
		}

		const float NewBreak = FMath::Clamp(GetBreak() - BreakDamage, 0.f, GetMaxBreak());
		SetBreak(NewBreak);

		if (NewBreak <= 0.f)
		{
			SendEventToOwner(SlayerTags::Event_Break, Instigator, BreakDamage);
		}
	}
}

void UMockSlayerAttributeSet::SendEventToOwner(const FGameplayTag& EventTag, AActor* Instigator, float Magnitude) const
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Instigator;
	Payload.Target = GetOwningActor();
	Payload.EventMagnitude = Magnitude;
	ASC->HandleGameplayEvent(EventTag, &Payload);
}

void UMockSlayerAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, Health, OldValue);
}

void UMockSlayerAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, MaxHealth, OldValue);
}

void UMockSlayerAttributeSet::OnRep_Break(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, Break, OldValue);
}

void UMockSlayerAttributeSet::OnRep_MaxBreak(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, MaxBreak, OldValue);
}

void UMockSlayerAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, AttackPower, OldValue);
}

void UMockSlayerAttributeSet::OnRep_CrushPower(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, CrushPower, OldValue);
}

void UMockSlayerAttributeSet::OnRep_UltimateCharge(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, UltimateCharge, OldValue);
}

void UMockSlayerAttributeSet::OnRep_MaxUltimateCharge(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMockSlayerAttributeSet, MaxUltimateCharge, OldValue);
}
