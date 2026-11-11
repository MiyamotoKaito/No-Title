// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameAbilitySystem/AbilityData.h"
#include "GameAbilitySystem/InputDataAsset.h"
#include "GameFramework/Character.h"
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
	TArray<FGameplayAbilitySpecHandle> GrantAbilities(const TArray<FAbilityData>& GrantToAbilities); 
	
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void RemoveAbilities(TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove);
	
	/*
	 * 入力タグに対応するアビリティを外部から呼ぶ関数(AIController)
	 *OnPressed
	 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem|Input")
	bool PressAbilityInput(FGameplayTag InputGameplayTag);
	/*
	 * 入力タグに対応するアビリティを外部から呼ぶ関数(AIController)
	 *OnReleased
	 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem|Input")
	void ReleaseAbilityInput(FGameplayTag InputGameplayTag);
	/*
	 * 入力タグのGAが1つでも発動中なら true
	 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem|Input")
	bool IsAbilityInputActive(FGameplayTag InputTag) const;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AbilitySystem")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AbilitySystem")
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AbilitySystem")
	EGameplayEffectReplicationMode ReplicationMode = EGameplayEffectReplicationMode:: Mixed;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AbilitySystem|Input")
	TObjectPtr<UInputDataAsset> InputDataAsset;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AbilitySystem")
	TArray<FAbilityData> ConstructAbilities;
private:
	void OnPressed(FGameplayTag Tag);
	void OnReleased(FGameplayTag Tag);
	// 入力タグが付いた Spec をすべて集める
	TArray<FGameplayAbilitySpec*> FindSpecsByInputTag(FGameplayTag InputTag) const;
	void SendInputEvent(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType);
};