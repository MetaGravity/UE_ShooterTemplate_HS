#include "Utils/HScaleConversionUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStringConvUtilsTest, "HyperScale.Utils.Conversion.String",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStringConvUtilsTest::RunTest(const FString& Parameters)
{
	const FString Input1 = "ABC";
	std::vector<std::string> SplitStrings1;
	FHScaleConversionUtils::SplitFStringToHScaleStrings(Input1, SplitStrings1);
	const FString CombinedString1 = FHScaleConversionUtils::CombinePartStrings(SplitStrings1);
	TestEqual(TEXT("Initial and final string does not match"), CombinedString1, Input1);

	const FString Input2 = TEXT(
		"The first way to divide sentences into groups was the original paragraphos, similar to an underscore at the beginning of the new group.[2] The Greek parágraphos evolved into the pilcrow (¶)");
	std::vector<std::string> SplitStrings2;
	FHScaleConversionUtils::SplitFStringToHScaleStrings(Input2, SplitStrings2);
	TestEqual(TEXT("Sizes of string splits does not match"), static_cast<int>(SplitStrings2.size()), 4);
	for (int32 i = 0; i < 3; i++)
	{
		constexpr uint32 MaxLength = HSCALE_MAX_STRING_LENGTH;
		TestEqual(TEXT("Split String should be of max possible size"), static_cast<int32>(SplitStrings2[i].size()), MaxLength);
	}
	const FString CombinedString2 = FHScaleConversionUtils::CombinePartStrings(SplitStrings2);
	TestEqual(TEXT("Initial and final string does not match"), CombinedString2, Input2);

	return true;
}

#endif