// Copyright (c) 2026 Kaito Miyamoto. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScalableFloat.h"
#include "Engine/DataAsset.h"
#include "BossAttributeInitData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct SLAYER_API FBossAttributeInitData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FScalableFloat Health;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FScalableFloat Power;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float CriticalMultiplier;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float CriticalChance;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Speed;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FScalableFloat Defence;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FScalableFloat Break;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FScalableFloat Ultimate;
};
