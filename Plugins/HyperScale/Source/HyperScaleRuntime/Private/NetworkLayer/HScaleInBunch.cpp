#include "NetworkLayer/HScaleInBunch.h"

#include "NetworkLayer/HScaleConnection.h"


void FHScaleInBunch::SetChannel(UChannel* Chann)
{
	this->Channel = Chann;
	if (Channel == nullptr) { return; }
	this->ChName = Channel->ChName;
	this->ChIndex = Channel->ChIndex;
}

void FHScaleInBunch::CountMemory(FArchive& Ar) const
{
	for (const FInBunch* Current = this; Current; Current = Current->Next)
	{
		Current->FNetBitReader::CountMemory(Ar);
		const SIZE_T MemberSize = sizeof(*this) - sizeof(FNetBitReader);
		Ar.CountBytes(MemberSize, MemberSize);
	}
}

uint32 FHScaleInBunch::EngineNetVer() const
{
	const UHScaleConnection* TempConnection = Cast<UHScaleConnection>(Connection);
	check(TempConnection);

	return TempConnection->GetNetworkCustomVersion_HS(FEngineNetworkCustomVersion::Guid);
}

uint32 FHScaleInBunch::GameNetVer() const
{
	const UHScaleConnection* TempConnection = Cast<UHScaleConnection>(Connection);
	check(TempConnection);

	return TempConnection->GetNetworkCustomVersion_HS(FEngineNetworkCustomVersion::Guid);
}

void FHScaleInBunch::SetEngineNetVer(const uint32 InEngineNetVer)
{
	// FInBunch::SetEngineNetVer(InEngineNetVer);
}

void FHScaleInBunch::SetGameNetVer(const uint32 InGameNetVer)
{
	// FInBunch::SetGameNetVer(InGameNetVer);
}