// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct SLAYER_API FInputData
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> Action;
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Input"))
	FGameplayTag ActionTag;
};
