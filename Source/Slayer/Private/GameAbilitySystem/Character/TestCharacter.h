// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Slayer/GameAbilitySystem/Attribute/BasicAttributeSet.h"
#include "TestCharacter.generated.h"

UCLASS()
class SLAYER_API ATestCharacter : public ACharacter,public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATestCharacter();
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	TArray<FGameplayAbilitySpecHandle> GrantAtlibites(TArray<TSubclassOf<UGameplayAbility>> GrantToAbilities); 
	
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove);
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UBasicAttributeSet> BasicAttributeSet;
protected:

	virtual void BeginPlay() override;
	/*
	 * プレイヤーを操作するときに使う関数
	 */
	virtual void OnRep_PlayerState() override;
	/*
	 * 主に、敵がコントローラーに依存されている場合に使う関数
	 */
	virtual void PossessedBy(AController* NewController) override;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Abilities")
	EGameplayEffectReplicationMode ReplicationMode = EGameplayEffectReplicationMode:: Mixed;
};
