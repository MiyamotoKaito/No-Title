// Copyright (c) 2026 Kaito Miyamoto. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Develop/AbilitySystem/BaseAttributeSet.h"
#include "Develop/Data/BossAttributeInitData.h"
#include "GameFramework/Character.h"
#include "BossCharacterBase.generated.h"

/**
 * 
 */
UCLASS()
class SLAYER_API ABossCharacterBase : public ACharacter, public IAbilitySystemInterface
{
public:
	GENERATED_BODY()

public:
	ABossCharacterBase();
	
	// =====関数=====
	/**
	 * ASCを取得
	 * @return AbilitySystemComponent
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	/**
	 * Bossのパラメータを設定
	 * スポーン直後(FinishingSpwaningの前)にGameModeから呼ぶ
	 * @param InBossData Attributeなどの値
	 * @param InLevel レベル
	 */
	void SetBossData(const FBossAttributeInitData& InBossData, int32 InLevel);
protected:
	// =====変数=====
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "0_Slayer|Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UBaseAttributeSet> BaseAttributeSet;
	UPROPERTY(Transient)
	FBossAttributeInitData BossInitData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Slayer|Data")
	int32 CharacterLevel = 1;
	// =====関数=====
	virtual void PossessedBy(AController* EventController) override;
	virtual void OnRep_PlayerState() override;
	void InitAttributes() const;
};
