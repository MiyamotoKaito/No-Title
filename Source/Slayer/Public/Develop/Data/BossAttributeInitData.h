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
	
	UPROPERTY(BlueprintReadOnly)
	FScalableFloat Health;
	UPROPERTY(BlueprintReadOnly)
	FScalableFloat Power;
	UPROPERTY(BlueprintReadOnly)
	float CriticalMultiplier;
	UPROPERTY(BlueprintReadOnly)
	float CriticalChance;
	UPROPERTY(BlueprintReadOnly)
	float Speed;
	UPROPERTY(BlueprintReadOnly)
	FScalableFloat Defence;
	UPROPERTY(BlueprintReadOnly)
	FScalableFloat Break;
	UPROPERTY(BlueprintReadOnly)
	FScalableFloat Ultimate;
};
