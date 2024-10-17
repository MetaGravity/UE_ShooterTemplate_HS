#pragma once


struct FHScaleClassInfoHolder
{
	uint64 ClassId = 0;
	FString UEFullPath;
	bool bIsStatic = false;
	UClass* Clazz;
};


class FHSClassTranslator
{
public:
	static FHSClassTranslator& GetInstance()
	{
		static FHSClassTranslator Instance;
		return Instance;
	}

private:
	FHSClassTranslator() {}

	~FHSClassTranslator() {}

	FHSClassTranslator(const FHSClassTranslator&) = delete;

	FHSClassTranslator& operator=(const FHSClassTranslator&) = delete;

	TMap<uint64, UClass*> ClassIdPtrCache;
	TMap<uint64, FString> ClassIdPathCache;
	TMap<FString, uint64> ReverseClassIdPathCache;

	UClass* GetClassFromPath(const FString& Path, const bool bIsStatic);

public:
	UClass* GetClass(const uint64 ClassId);
	uint64 GetClassId(const UClass* Class);
	FHScaleClassInfoHolder GetClassId(const UObject* Object);

	UClass* GetClassFromObjectPath(const FString& ObjectPath)
	{
		return GetClassFromPath(ObjectPath, true);
	}

	UClass* GetClassFromClassPath(const FString& ClassPath)
	{
		return GetClassFromPath(ClassPath, false);
	}
};