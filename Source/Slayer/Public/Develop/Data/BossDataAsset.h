// Copyright (c) 2026 Kaito Miyamoto. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BossAttributeInitData.h"
#include "Develop/Character/BossCharacterBase.h"
#include "Engine/DataAsset.h"
#include "BossDataAsset.generated.h"

/**
 *  ボスのデータをまとめたアセットデータクラス。
 */
UCLASS()
class SLAYER_API UBossDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "0_Slayer|Character") 
	TSoftClassPtr<ABossCharacterBase> BossCharacterBase;
	UPROPERTY(EditDefaultsOnly, Category = "0_Slayer|Stats")
	FBossAttributeInitData BossAttributeInitData;
};
