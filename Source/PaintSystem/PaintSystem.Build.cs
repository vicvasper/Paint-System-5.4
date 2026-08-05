// Copyright (c) Victor Rivas Perez. All Rights Reserved.

using UnrealBuildTool;

public class PaintSystem : ModuleRules
{
	public PaintSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		// RenderCore and RHI back the vertex colour buffer work, which lives entirely in the
		// implementation - the public header only forward-declares.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
		});

		// Removed: "InputCore" and "EnhancedInput". This module reads no input; painting is
		// driven by callers passing a hit location.
	}
}
