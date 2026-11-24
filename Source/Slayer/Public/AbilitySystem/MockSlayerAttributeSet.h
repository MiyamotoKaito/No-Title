// Alpha版のキャラクター（ボス／プレイヤー）と塊が共通で使う AttributeSet

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MockSlayerAttributeSet.generated.h"

#define SLAYER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 体力・ブレイク値・攻撃力・砕く力を持つ AttributeSet
 * ダメージは IncomingDamage / IncomingBreakDamage（メタ属性）経由で受け取り、
 * PostGameplayEffectExecute で Health / Break に反映する
 * Health が0になったら Event.Character.Death、Break が0になったら Event.Character.Break を自分に送る
 */
UCLASS()
class SLAYER_API UMockSlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMockSlayerAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// 体力
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, MaxHealth);

	// ブレイク値（0になるとブレイク。MaxBreak が0ならブレイクしない＝プレイヤー操作中など）
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Break)
	FGameplayAttributeData Break;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, Break);

	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxBreak)
	FGameplayAttributeData MaxBreak;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, MaxBreak);

	// 攻撃力の倍率（1.0 = 技の基本ダメージそのまま）
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, AttackPower);

	// 砕く力の倍率（塊へのダメージにだけ使う。将来はレベルで伸ばす）
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_CrushPower)
	FGameplayAttributeData CrushPower;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, CrushPower);

	// 必殺技ゲージ（攻撃を当てる・受けると溜まる。満タンで必殺技を撃てる）
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_UltimateCharge)
	FGameplayAttributeData UltimateCharge;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, UltimateCharge);

	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxUltimateCharge)
	FGameplayAttributeData MaxUltimateCharge;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, MaxUltimateCharge);

	// ダメージ1あたりに溜まる必殺技ゲージ（与えた側・受けた側）
	static constexpr float UltimateChargePerDamageDealt = 0.08f;
	static constexpr float UltimateChargePerDamageTaken = 0.12f;

	// メタ属性：受けるダメージ（サーバーでだけ使う一時的な値なので複製しない）
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, IncomingDamage);

	// メタ属性：受けるブレイクダメージ
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingBreakDamage;
	SLAYER_ATTRIBUTE_ACCESSORS(UMockSlayerAttributeSet, IncomingBreakDamage);

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_Break(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_MaxBreak(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_CrushPower(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_UltimateCharge(const FGameplayAttributeData& OldValue) const;
	UFUNCTION()
	void OnRep_MaxUltimateCharge(const FGameplayAttributeData& OldValue) const;

private:
	// ASC の必殺技ゲージを増やす（最大値で止める）
	static void AddUltimateCharge(UAbilitySystemComponent* ASC, float Amount);

	// 自分の ASC に GameplayEvent を送る
	void SendEventToOwner(const FGameplayTag& EventTag, AActor* Instigator, float Magnitude) const;
};
