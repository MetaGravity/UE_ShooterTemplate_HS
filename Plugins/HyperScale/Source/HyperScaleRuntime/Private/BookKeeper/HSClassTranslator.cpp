#include "BookKeeper/HSClassTranslator.h"

#include "Utils/HScaleConversionUtils.h"

UClass* FHSClassTranslator::GetClass(const uint64 ClassId)
{
	// first check in cache
	if (ClassIdPtrCache.Contains(ClassId))
	{
		return *ClassIdPtrCache.Find(ClassId);
	}
	// if no class path mapping exists, there is nothing to do, return nullptr
	if (!ClassIdPathCache.Contains(ClassId)) { return nullptr; }

	// resolve class path, store in cache and return class ptr
	const FString ClassName = *ClassIdPathCache.Find(ClassId);
	const FSoftClassPath ClassPath(ClassName);
	UClass* LoadedClassPtr = ClassPath.ResolveClass();
	ClassIdPtrCache.Add(ClassId, LoadedClassPtr);
	return LoadedClassPtr;
}

uint64 FHSClassTranslator::GetClassId(const UClass* Class)
{
	const FTopLevelAssetPath TopLevelAssetPath = Class->GetClassPathName();
	const FString ClassPathString = TopLevelAssetPath.ToString();

	// if not found in class id mapping cache, will return 0 and stores the result
	return ReverseClassIdPathCache.FindOrAdd(ClassPathString);
}

FHScaleClassInfoHolder FHSClassTranslator::GetClassId(const UObject* Object)
{
	FHScaleClassInfoHolder Holder{};
	if (Object == nullptr) { return Holder; }
	if (Object->IsFullNameStableForNetworking())
	{
		const FString PathName = Object->GetPathName();
		UClass* StaticClass = Object->GetClass();
		const FString ClassPathName = StaticClass->GetPathName();
		Holder.ClassId = FHScaleConversionUtils::HashFString(ClassPathName);
		Holder.UEFullPath = PathName;
		Holder.bIsStatic = true;
		Holder.Clazz = StaticClass;
	}
	else
	{
		UClass* StaticClass = Object->GetClass();
		FString PathName = StaticClass->GetPathName();
		Holder.ClassId = FHScaleConversionUtils::HashFString(PathName);
		Holder.UEFullPath = PathName;
		Holder.Clazz = StaticClass;
	}
	return Holder;
}

UClass* FHSClassTranslator::GetClassFromPath(const FString& Path, const bool bIsStatic)
{
	if (bIsStatic)
	{
		const UObject* LoadObject = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
		return LoadObject->GetClass();
	}
	return StaticLoadClass(UObject::StaticClass(), nullptr, *Path);
}