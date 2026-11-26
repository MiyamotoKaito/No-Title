#include "Mock/AbilitySystem/MockSlayerGameplayEffects.h"

#include "Mock/AbilitySystem/MockSlayerDamageExecution.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "SlayerGameplayTags.h"

UGE_MockSlayerDamage::UGE_MockSlayerDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UMockSlayerDamageExecution::StaticClass();
	Executions.Add(Execution);
}

UGE_MockSlayerCooldown::UGE_MockSlayerCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SlayerTags::Data_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	// クールダウン GE はタグを付与する必要がある（エディタの検証）
	// 共通の親タグ「Cooldown」を付け、アビリティごとの Cooldown.* は GA 側が DynamicGrantedTags で足す
	UTargetTagsGameplayEffectComponent* TagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(SlayerTags::Cooldown);
	TagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TagsComponent);
}
