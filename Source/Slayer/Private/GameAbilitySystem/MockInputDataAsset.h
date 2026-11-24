// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MockInputData.h"
#include "Engine/DataAsset.h"
#include "MockInputDataAsset.generated.h"

/**
 * インプットアクションとタグを紐づけるDataAsset
 * Action発火時に対応するタグを探すため
 */
UCLASS(Blueprintable)
class SLAYER_API UMockInputDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (TitleProperty = "ActionTag"))
	TArray<FMockInputData> InputActionsData;
};
