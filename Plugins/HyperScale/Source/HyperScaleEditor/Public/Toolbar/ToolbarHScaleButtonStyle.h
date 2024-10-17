// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FHScaleToolbarStyle
{
public:

	static void Create();

	static FName GetStyleSetName() { return "HScaleToolbarStyle"; }

	static FSlateIcon GetQuarkStatusIcon();

	static const ISlateStyle &Get()
	{
		if (!IsInitialized())
			Create();

		return *(StyleInstance.Get());
	}

	static bool IsInitialized() { return StyleInstance.IsValid(); }

private:

	static TSharedPtr< class ISlateStyle > StyleInstance;
};
