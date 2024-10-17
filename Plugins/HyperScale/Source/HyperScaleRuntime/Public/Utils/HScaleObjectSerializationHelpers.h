#pragma once
#include "Core/HScaleResources.h"

class UHScalePackageMap;

struct FHScaleOuterChunk
{
	FHScaleNetGUID HS_NetGUID;
	FNetworkGUID NetGUID;
	uint8 ExportFlags = 0;
	FString ObjectName;
	bool bIsCachedAttrId = false;
	uint16 AttributeId = 0;
};

class FHScaleObjectSerializationHelper
{
public:
	static void LoadObject(UObject*& Object, TArray<FHScaleOuterChunk>& Chunks, FNetworkGUID& NetGUID, UHScalePackageMap* PkgMap);

	static void ReadOuterChunkFromBunch(FBitReader& Ar, TArray<FHScaleOuterChunk>& OuterChunks, UHScalePackageMap* PackageMap);
};


