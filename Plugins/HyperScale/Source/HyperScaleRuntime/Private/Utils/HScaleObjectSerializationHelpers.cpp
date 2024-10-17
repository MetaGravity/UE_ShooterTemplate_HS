#include "Utils/HScaleObjectSerializationHelpers.h"

#include "MemoryLayer/HScaleNetworkBibliothec.h"
#include "NetworkLayer/HScalePackageMap.h"

void FHScaleObjectSerializationHelper::LoadObject(UObject*& Object, TArray<FHScaleOuterChunk>& Chunks, FNetworkGUID& NetGUID, UHScalePackageMap* PkgMap)
{
	check(PkgMap)

	for (int8 i = 0; i < Chunks.Num(); i++)
	{
		FHScaleOuterChunk Ele = Chunks[i];
		if (Ele.HS_NetGUID.IsValid())
		{
			NetGUID = PkgMap->FindNetGUIDFromHSNetGUID(Ele.HS_NetGUID);
			Object = PkgMap->FindObjectFromEntityID(Ele.HS_NetGUID);
			FString ObjVa = Object ? Object->GetName() : FString();
			UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Chunk After load is flags: %d HS_NETGUID: %llu AttrId: %d Name: %s  ObjVa: %s	NetGUID:%s"), Ele.ExportFlags, Ele.HS_NetGUID.Get(), Ele.AttributeId, *Ele.ObjectName, *ObjVa, *NetGUID.ToString())
			if (NetGUID.IsValid()) { continue; }
		}
		Object = StaticFindObject(UObject::StaticClass(), Object, *Ele.ObjectName, false);
		FString ObjVa = Object ? Object->GetName() : FString();
		PkgMap->AssignNetGUID(NetGUID, Object);
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Chunk After load is flags: %d HS_NETGUID: %llu AttrId: %d Name: %s  ObjVa: %s	NetGUID:%s"), Ele.ExportFlags, Ele.HS_NetGUID.Get(), Ele.AttributeId, *Ele.ObjectName, *ObjVa, *NetGUID.ToString())
	}
}

void FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(FBitReader& Ar, TArray<FHScaleOuterChunk>& OuterChunks, UHScalePackageMap* PackageMap)
{
	if (Ar.IsError() || Ar.AtEnd())
	{
		UE_LOG(Log_HyperScaleMemory, Error, TEXT("Archive errored out while reading outer data"))
		return;
	}

	const bool bIsValid = !!Ar.ReadBit();

	FHScaleOuterChunk Chunk;

	if (bIsValid)
	{
		FHScaleNetGUID HScaleGUID;
		Ar << HScaleGUID;
		Chunk.HS_NetGUID = HScaleGUID;

		Chunk.NetGUID = PackageMap->FindNetGUIDFromHSNetGUID(Chunk.HS_NetGUID);
	}
	else
	{
		Ar << Chunk.NetGUID;
	}

	if (!bIsValid && !Chunk.NetGUID.IsValid())
	{
		return;
	}
	FHScaleExportFlags ExportFlags;
	Ar << ExportFlags.Value;
	Chunk.ExportFlags = ExportFlags.Value;

	if (ExportFlags.bHasPath && !Chunk.bIsCachedAttrId)
	{
		ReadOuterChunkFromBunch(Ar, OuterChunks, PackageMap);
		Ar << Chunk.ObjectName;
	}

	OuterChunks.Add(Chunk);
}