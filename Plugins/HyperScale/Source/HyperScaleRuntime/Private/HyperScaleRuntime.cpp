// Copyright 2024 Metagravity. All Rights Reserved.

#include "HyperScaleRuntime.h"

#define LOCTEXT_NAMESPACE "FHyperScaleRuntimeModule"

void FHyperScaleRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FHyperScaleRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHyperScaleRuntimeModule, HyperScaleRuntime)
