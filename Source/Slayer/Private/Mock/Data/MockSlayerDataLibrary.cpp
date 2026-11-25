#include "Data/MockSlayerDataLibrary.h"

#include "DataAssets/ComposableCameraTypeAsset.h"
#include "Nodes/ComposableCameraCameraNodeBase.h"
#include "Nodes/ComposableCameraNodePinTypes.h"

FGameplayTag UMockSlayerDataLibrary::MakeTag(FName TagName)
{
	return FGameplayTag::RequestGameplayTag(TagName, true);
}

FString UMockSlayerDataLibrary::DescribeCameraType(UComposableCameraTypeAsset* CameraType)
{
	if (CameraType == nullptr)
	{
		return TEXT("(null)");
	}

	FString Result;
	for (int32 Index = 0; Index < CameraType->NodeTemplates.Num(); ++Index)
	{
		const UComposableCameraCameraNodeBase* Node = CameraType->NodeTemplates[Index];
		if (Node == nullptr)
		{
			continue;
		}
		Result += FString::Printf(TEXT("[%d] %s\n"), Index, *Node->GetClass()->GetName());

		// 宣言されているピンと既定値
		TArray<FComposableCameraNodePinDeclaration> Pins;
		Node->GetPinDeclarations(Pins);
		for (const FComposableCameraNodePinDeclaration& Pin : Pins)
		{
			if (Pin.Direction != EComposableCameraPinDirection::Input)
			{
				continue;
			}
			FString Value = Pin.DefaultValueString;
			if (CameraType->NodePinOverrides.IsValidIndex(Index))
			{
				for (const FComposableCameraPinOverride& Override : CameraType->NodePinOverrides[Index].Overrides)
				{
					if (Override.PinName == Pin.PinName && Override.bHasDefaultOverride)
					{
						Value = Override.DefaultValueOverride + TEXT("  (override)");
					}
				}
			}
			Result += FString::Printf(TEXT("    %s = %s\n"), *Pin.PinName.ToString(), *Value);
		}
	}
	return Result;
}

UObject* UMockSlayerDataLibrary::GetCameraNode(UComposableCameraTypeAsset* CameraType, const FString& NodeClassContains)
{
	if (CameraType == nullptr)
	{
		return nullptr;
	}
	for (UComposableCameraCameraNodeBase* Node : CameraType->NodeTemplates)
	{
		if (Node != nullptr && Node->GetClass()->GetName().Contains(NodeClassContains))
		{
			return Node;
		}
	}
	return nullptr;
}

bool UMockSlayerDataLibrary::SetCameraPinValue(UComposableCameraTypeAsset* CameraType, const FString& NodeClassContains, FName PinName, const FString& Value)
{
	if (CameraType == nullptr)
	{
		return false;
	}

	for (int32 Index = 0; Index < CameraType->NodeTemplates.Num(); ++Index)
	{
		const UComposableCameraCameraNodeBase* Node = CameraType->NodeTemplates[Index];
		if (Node == nullptr || !Node->GetClass()->GetName().Contains(NodeClassContains))
		{
			continue;
		}

		if (CameraType->NodePinOverrides.Num() < CameraType->NodeTemplates.Num())
		{
			CameraType->NodePinOverrides.SetNum(CameraType->NodeTemplates.Num());
		}

		CameraType->Modify();
		TArray<FComposableCameraPinOverride>& Overrides = CameraType->NodePinOverrides[Index].Overrides;
		FComposableCameraPinOverride* Existing = Overrides.FindByPredicate([PinName](const FComposableCameraPinOverride& Override)
		{
			return Override.PinName == PinName;
		});
		if (Existing == nullptr)
		{
			Existing = &Overrides.AddDefaulted_GetRef();
			Existing->PinName = PinName;
		}
		Existing->bHasDefaultOverride = true;
		Existing->DefaultValueOverride = Value;
		CameraType->MarkPackageDirty();
		return true;
	}
	return false;
}
