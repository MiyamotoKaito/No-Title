// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BaseAttributeSet.generated.h"

/**
 * ユーザーとBoss共通の属性セット
 */
UCLASS()
class SLAYER_API UBaseAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	// =====変数=====
	
	// Health関連
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;	
	UPROPERTY(BluePrintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	
	// Power関連
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Power)
	FGameplayAttributeData Power;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_CriticalMultiplier)
	FGameplayAttributeData CriticalMultiplier;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_CriticalChance)
	FGameplayAttributeData CriticalChance;
	
	// Deffense関連
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	
	// Break値関連
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Break)
	FGameplayAttributeData Break;
	UPROPERTY(BluePrintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxBreak)
	FGameplayAttributeData MaxBreak;
	
	// Ultimate関連
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Ultimate)
	FGameplayAttributeData Ultimate;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxUltimate)
	FGameplayAttributeData MaxUltimate;
	
	// =====関数=====
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& EffectData) override;
	
	// =====マクロ=====
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, Power);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, CriticalMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, CriticalChance);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, Defense);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, Break);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, MaxBreak);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, Ultimate);
	ATTRIBUTE_ACCESSORS_BASIC(UBaseAttributeSet, MaxUltimate);
protected:
	// =====関数=====
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	UFUNCTION()
	void OnRep_Power(const FGameplayAttributeData& OldPower);
	UFUNCTION()
	void OnRep_CriticalMultiplier(const FGameplayAttributeData& OldCriticalMultiplier);
	UFUNCTION()
	void OnRep_CriticalChance(const FGameplayAttributeData& OldCriticalChance);
	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldDefense);
	UFUNCTION()
	void OnRep_Break(const FGameplayAttributeData& OldBreak);
	UFUNCTION()
	void OnRep_MaxBreak(const FGameplayAttributeData& OldMaxBreak);
	UFUNCTION()
	void OnRep_Ultimate(const FGameplayAttributeData& OldUltimate);
	UFUNCTION()
	void OnRep_MaxUltimate(const FGameplayAttributeData& OldMaxUltimate);
};
