// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Module-wide log category.
 *
 * Painting runs per stroke and per fade pass, so its diagnostics need their own verbosity
 * control ("Log LogPaintSystem Verbose") rather than competing with everything else on
 * LogTemp.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogPaintSystem, Log, All);
