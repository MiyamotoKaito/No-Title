// エディタの Python スクリプトや BP からデータを組み立てる時の補助関数

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MockSlayerDataLibrary.generated.h"

class UComposableCameraTypeAsset;

UCLASS()
class SLAYER_API UMockSlayerDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 文字列から Gameplay Tag を作る（Python から unreal.SlayerDataLibrary.make_tag("Input.Attack") で使う）
	UFUNCTION(BlueprintCallable, Category = "Slayer|Data")
	static FGameplayTag MakeTag(FName TagName);

	// Composable Camera のカメラ定義の中身（ノードとピンの値）を文字列にする（調整用）
	UFUNCTION(BlueprintCallable, Category = "Slayer|Camera")
	static FString DescribeCameraType(UComposableCameraTypeAsset* CameraType);

	// カメラ定義の、指定したノード（クラス名に NodeClassContains を含む最初のもの）のピンの値を上書きする
	// 値は Unreal のテキスト形式（例：float は "350.0"、FVector は "(X=0,Y=60,Z=40)"）。成功したら true
	UFUNCTION(BlueprintCallable, Category = "Slayer|Camera")
	static bool SetCameraPinValue(UComposableCameraTypeAsset* CameraType, const FString& NodeClassContains, FName PinName, const FString& Value);

	// カメラ定義の、クラス名に NodeClassContains を含む最初のノードを返す（ピンではない設定値を Python から変える時に使う）
	UFUNCTION(BlueprintCallable, Category = "Slayer|Camera")
	static UObject* GetCameraNode(UComposableCameraTypeAsset* CameraType, const FString& NodeClassContains);
};
