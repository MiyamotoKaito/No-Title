#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MockAbilityData.generated.h"

USTRUCT(BlueprintType)
struct SLAYER_API FMockAbilityData
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSubclassOf<UGameplayAbility> AbilityClass;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Categories = "Input"))
	FGameplayTag InputTag;
};
