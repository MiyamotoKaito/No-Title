// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MockBasicAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class SLAYER_API UMockBasicAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	// 体力関連のAttribute
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Abilities", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UMockBasicAttributeSet, Health);
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Abilities", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UMockBasicAttributeSet, MaxHealth);
	
	// スタミナ関連のAttribute
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Abilities", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UMockBasicAttributeSet, Stamina);
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Abilities", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(UMockBasicAttributeSet, MaxStamina);
	
public:
	UMockBasicAttributeSet();
	
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue) const;

	/**
	 * サーバーの同期リストに設定する
	 * @param OutLifetimeProps 
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 *  AttributeのCurrentValueが変更される前に呼ぶ
	 * @param Attribute 変更される Attribute
	 * @param NewValue 変更後の値
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/**
	 * BaseValueが変更された時に呼ばれる
	 * @param Data 実行された Effect、対象 Attribute、変化量などの情報
	 */
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
};
