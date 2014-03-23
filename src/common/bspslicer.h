/*
 * @file
 */

#pragma once

#include "../shared/shared.h"
#include "../shared/typedefs.h"
#include "../common/tracing.h"

void SL_BSPSlice(const TR_TILE_TYPE* model, float thickness, int scale, bool singleFile, bool multipleContour);
