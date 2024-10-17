#pragma once

class UHScalePackageMap;
class FRepLayoutCmd;

class FHScaleEventsSerializer
{
public:
	bool static Serialize(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap);
	
	bool static ReadObjectPtr(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap);
	bool static WriteObjectPtr(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap);
	
	bool static ReadSoftObjectPtr(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap);
	bool static WriteSoftObjectPtr(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap);

	static bool ReadDynArray(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap, const TArray<FRepLayoutCmd>::ElementType& Cmd);
	static bool WriteDynArray(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap, const TArray<FRepLayoutCmd>::ElementType& Cmd);

private:
	template<class T>
	static bool SerializeDataType(FBitWriter& Writer, FBitReader& Reader);

	template<class T>
	static bool SerializeNetSerializeTypes(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap);

	static bool SerializeNetSerializeStruct(FBitReader& Reader, FBitWriter& Writer, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap);

	static bool SerializeBoolProperty(FBitWriter& Writer, FBitReader& Reader);
	static bool SerializeByteProperty(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap);

	static bool SerializeDynArray(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap, const TArray<FRepLayoutCmd>::ElementType& Cmd, const bool bOnSend);
};
