#pragma once
#include <map>

/**
 * Serves as a common iterator for std::map and unreal TSet with template T as the
 * iterator value
 */
template<typename T, typename K>
class THScaleUSetSMapIterator
{
public:
	THScaleUSetSMapIterator(typename std::map<T, K>::const_iterator MapIter, typename std::map<T, K>::const_iterator MapEndIter, typename TSet<T>::TConstIterator SetIter, const bool bIsSet)
		: StdMapIter(MapIter), StdMapEndIter(MapEndIter), UnrealSetIter(SetIter), bIsSet(bIsSet) {}

	T operator*() const
	{
		if (bIsSet) { return *UnrealSetIter; }
		return StdMapIter->first;
	}

	THScaleUSetSMapIterator& operator++()
	{
		if (bIsSet) { ++UnrealSetIter; }
		else { ++StdMapIter; }
		return *this;
	}

	bool IsEnd()
	{
		if (bIsSet) { return !UnrealSetIter; }
		return StdMapIter == StdMapEndIter;
	}

private:
	typename std::map<T, K>::const_iterator StdMapIter;
	typename std::map<T, K>::const_iterator StdMapEndIter;
	typename TSet<T>::TConstIterator UnrealSetIter;
	bool bIsSet;
};

template<typename K, typename V>
class THScaleStdMapPairIterator
{
public:
	THScaleStdMapPairIterator(const typename std::map<K, V>::const_iterator& StdMapIter, const typename std::map<K, V>::const_iterator& StdMapEndIter)
		: StdMapIter(StdMapIter),
		  StdMapEndIter(StdMapEndIter) {}

	K operator*() const
	{
		return StdMapIter->first;
	}

	V Value() const
	{
		return StdMapIter->second;
	}

	typename std::map<K, V>::const_iterator Pair() const
	{
		return StdMapIter;
	}

	THScaleStdMapPairIterator& operator++()
	{
		++StdMapIter;
		return *this;
	}

	bool IsEnd() const
	{
		return StdMapIter == StdMapEndIter;
	}

private:
	typename std::map<K, V>::const_iterator StdMapIter;
	const typename std::map<K, V>::const_iterator StdMapEndIter;
};