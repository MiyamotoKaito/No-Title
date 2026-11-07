#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilityData.generated.h"

USTRUCT(BlueprintType)
struct SLAYER_API FAbilityData
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<UGameplayAbility> AbilityClass;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Categories = "Input"))
	FGameplayTag InputTag;
};
