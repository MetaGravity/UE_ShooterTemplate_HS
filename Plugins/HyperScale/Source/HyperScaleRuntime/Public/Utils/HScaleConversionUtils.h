#pragma once
#include "Core/HScaleCommons.h"

class FHScaleConversionUtils
{
public:
	/**
	 * Converts given FString into c++ string and if the size of string is greater than HScale max string size.
	 * Splits the string into multiple parts with last character signifying if there's a continuation or not.
	 * @param UnrealString Input Unreal FString
	 * @return 
	 */
	static void SplitFStringToHScaleStrings(const FString& UnrealString, std::vector<std::string>& Result)
	{
		Result.clear();
		const std::string CppString(TCHAR_TO_UTF8(*UnrealString));
		constexpr uint32 MaxLength = HSCALE_MAX_STRING_LENGTH;
		constexpr uint32 MaxSplitLength = MaxLength - 1;

		const size_t TotalLength = CppString.length();

		for (size_t i = 0; i < TotalLength; i += MaxSplitLength)
		{
			std::string Part = CppString.substr(i, MaxSplitLength);
			if (i + MaxSplitLength < TotalLength) { Part += '1'; }
			else { Part += '0'; }
			Result.push_back(Part);
		}
	}

	/**
	 * 
	 */
	static FString CombinePartStrings(const std::vector<std::string>& Parts)
	{
		std::string CombinedString;
		for (const std::string& Part : Parts)
		{
			// Remove the last character ('0' or '1') which is the indicator
			const std::string ActualPart = Part.substr(0, Part.size() - 1);
			CombinedString += ActualPart;
		}
		return FString(UTF8_TO_TCHAR(CombinedString.c_str()));
	}

	static uint32 HashFString(const FString& Input)
	{
		return FCrc::StrCrc32(*Input);
	}

	static bool IsHScaleSplitStringContd(const std::string& Str)
	{
		if (Str.empty()) { return false; }
		const char LastChar = Str.back();
		return LastChar == '1';
	}

	static bool IsValidSplitString(const std::string& Str)
	{
		if (Str.empty()) { return false; }
		const char LastChar = Str.back();
		return LastChar == '1' || LastChar == '0';
	}

	static bool IsReceivedHasIdValid(const uint8 CurrentHashId, const uint8 ReceivedHashId)
	{
		uint8 ACurHash = CurrentHashId >> 1;
		uint8 AReceivedHash = ReceivedHashId >> 1;

		if (AReceivedHash >= ACurHash) { return true; } // if new hash is greater than it's a valid hash

		// if difference between new hash and old hash crosses certain range, we can consider it also as valid hash
		if (ACurHash - AReceivedHash > 16) { return true; }

		return false;
	}

	static uint8 FetchHashIdFromBuffer(const quark::vector<uint8_t>& Buffer)
	{
		if (Buffer.empty()) { return 0; }
		return Buffer.back();
	}

	// HashIds ranges are maintained in last 7 bits of uint8
	static uint8 FetchNextHashIdForSend(uint8 CurrentSendHashId)
	{
		uint8 HashId = CurrentSendHashId >> 1;
		HashId++;
		if (HashId > 127) return 0;
		return HashId << 1;
	}


	static void SplitBufferToHScaleBuffers(const std::vector<uint8_t>& Buffer, std::vector<std::vector<uint8_t>>& Result, uint8 HashAppend)
	{
		Result.clear();
		constexpr uint32_t MaxLength = HSCALE_MAX_BUFFER_LENGTH;
		constexpr uint32_t MaxSplitLength = MaxLength - 1;

		const size_t TotalLength = Buffer.size();

		for (size_t i = 0; i < TotalLength; i += MaxSplitLength)
		{
			std::vector<uint8_t> Part(Buffer.begin() + i, Buffer.begin() + i + MaxSplitLength);
			if (i + MaxSplitLength < TotalLength)
			{
				HashAppend |= 1; // Set the first bit to 1 to signify continuity
			}
			else
			{
				HashAppend &= ~1; // set the first bit to 0 to signify end
			}

			Part.push_back(HashAppend);
			Result.push_back(Part);
		}
	}

	static void CombinePartBuffers(const std::vector<std::vector<uint8_t>>& Parts, std::vector<uint8_t>& CombinedBuffer)
	{
		CombinedBuffer.clear();
		for (const std::vector<uint8_t>& Part : Parts)
		{
			// Remove the last character which is the indicator
			CombinedBuffer.insert(CombinedBuffer.end(), Part.begin(), Part.end() - 1);
		}
	}

	static bool IsHScaleSplitBufferContd(const std::vector<uint8_t>& Buffer)
	{
		if (Buffer.empty()) { return false; }
		const uint8_t LastByte = Buffer.back();
		return (LastByte & 1) == 1;
	}

	static bool IsValidSplitBuffer(const std::vector<uint8_t>& Buffer)
	{
		if (Buffer.empty()) { return false; }
		return true;
	}
};