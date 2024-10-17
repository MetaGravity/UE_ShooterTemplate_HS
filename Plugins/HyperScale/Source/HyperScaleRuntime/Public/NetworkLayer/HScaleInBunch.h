#pragma once

class FHScaleInBunch : public FInBunch
{
public:
	FHScaleInBunch(UNetConnection* InConnection, uint8* Src, int64 CountBits)
		: FInBunch(InConnection, Src, CountBits),Channel(nullptr)
	{
	}

	UChannel* Channel;

	void SetChannel(UChannel* Chann);

	virtual void CountMemory(FArchive& Ar) const override;

	virtual uint32 EngineNetVer() const override;
	virtual uint32 GameNetVer() const override;
	virtual void SetEngineNetVer(const uint32 InEngineNetVer) override;
	virtual void SetGameNetVer(const uint32 InGameNetVer) override;
};