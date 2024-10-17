// Copyright 2024 Metagravity. All Rights Reserved.


#include "Schema/HScaleSchemaToolWidget.h"

#include "Engine/AssetManager.h"
#include "ReplicationLayer/Schema/HScaleSchemaDataAsset.h"

void UHScaleSchemaToolWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.LoadPrimaryAssetsWithType(UHScaleSchemaDataAsset::GetPrimaryAssetType(), TArray<FName>(), FStreamableDelegate::CreateWeakLambda(this, [=, this]()
	{
		TArray<FAssetData> AssetDataList;
		if (UAssetManager::Get().GetPrimaryAssetDataList(TEXT("HScaleSchemaDataAsset"), AssetDataList))
		{
			for (const FAssetData& AssetData : AssetDataList)
			{
				SchemaAssets.Add(Cast<UHScaleSchemaDataAsset>(AssetData.GetAsset()));
			}

			OnConstructed();
		}
	}));
}