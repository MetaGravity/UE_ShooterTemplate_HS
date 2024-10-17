// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#ifndef HYPERSCALE_PROFILER_ENABLE
#	if (UE_BUILD_SHIPPING)
#		define HYPERSCALE_PROFILER_ENABLE 0
#	else
#		define HYPERSCALE_PROFILER_ENABLE 1
#	endif
#endif

// When true this adds dynamic protocol names in profile captures. The downside is a noticeable cpu cost overhead but only while cpu trace recording is occurring.
#ifndef UE_HYPERSCALE_PROFILER_ENABLE_PROTOCOL_NAMES
#	define UE_HYPERSCALE_PROFILER_ENABLE_PROTOCOL_NAMES !UE_BUILD_SHIPPING
#endif

// When true this adds low-level cpu trace captures of operations in hyperscale. Adds a little cpu overhead but only while cpu trace recording is occurring.
#ifndef UE_HYPERSCALE_PROFILER_ENABLE_VERBOSE
#	if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#		define UE_HYPERSCALE_PROFILER_ENABLE_VERBOSE 0
#	else
#		define UE_HYPERSCALE_PROFILER_ENABLE_VERBOSE 1
#	endif
#endif

#if HYPERSCALE_PROFILER_ENABLE
#		include "ProfilingDebugging/CpuProfilerTrace.h"
#		define HYPERSCALE_PROFILER_SCOPE(x) TRACE_CPUPROFILER_EVENT_SCOPE(x)
#		define HYPERSCALE_PROFILER_SCOPE_TEXT(X) TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(X)
#else
#	define PERFORMANCEAPI_ENABLED 0
#	define HYPERSCALE_PROFILER_SCOPE(x)
#	define HYPERSCALE_PROFILER_SCOPE_TEXT(X)
#endif

#if UE_HYPERSCALE_PROFILER_ENABLE_PROTOCOL_NAMES
#	define HYPERSCALE_PROFILER_PROTOCOL_NAME(x) HYPERSCALE_PROFILER_SCOPE_TEXT(x)
#else
#	define HYPERSCALE_PROFILER_PROTOCOL_NAME(x)
#endif

#if UE_HYPERSCALE_PROFILER_ENABLE_VERBOSE
#	define HYPERSCALE_PROFILER_SCOPE_VERBOSE(x) HYPERSCALE_PROFILER_SCOPE(x);
#else
#	define HYPERSCALE_PROFILER_SCOPE_VERBOSE(x)
#endif
