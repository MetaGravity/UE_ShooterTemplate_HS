// Copyright 2023 Metagravity. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;
using UnrealBuildTool;

public class QuarkAPI : ModuleRules
{
	public QuarkAPI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		Type = ModuleType.External;
		bEnableExceptions = true;

		// Include .h files from C++ API
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

		// Link static quark libraries
		foreach (string staticLib in GetStaticLibraries())
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, staticLib));
		}

		CopyDlls(Target);
	}

	private void CopyDlls(ReadOnlyTargetRules Target)
	{
		var DllName = "quark.dll";
		var DllPath = Path.Combine(ModuleDirectory, "lib", DllName);
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RuntimeDependencies.Add($"$(BinaryOutputDir)/{DllName}", DllPath); //This tells Unreal to copy the DLL into the packaged build.

			string BinariesPath = Path.Combine(ModuleDirectory, "../../Binaries/Win64");

			string OutputDLLPath = Path.Combine(BinariesPath, DllName);

			if (!Directory.Exists(BinariesPath))
			{
				Directory.CreateDirectory(BinariesPath);
			}

			try
			{
				File.Copy(DllPath, OutputDLLPath, true);
			}
			catch (System.Exception ex)
			{
				Log.WriteException(ex, null);
			}
		}
	}

	private List<string> GetStaticLibraries()
	{
		List<string> output = new List<string>();

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			output.AddRange(new[]
			{
				Path.Combine("lib", "x64", "ntdll.lib"),
				Path.Combine("lib", "x64", "bcrypt.lib"),
				Path.Combine("lib", "x64", "userenv.lib")
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			output.AddRange(new[]
			{
				Path.Combine("lib", "amr64", "ntdll.lib"),
				Path.Combine("lib", "amr64", "bcrypt.lib"),
				Path.Combine("lib", "amr64", "userenv.lib")
			});
		}
		else
		{
			// Nothing to add, unsupported platform
		}

		output.AddRange(new[]
		{
			Path.Combine("lib", "quark.lib")
		});

		return output;
	}
}