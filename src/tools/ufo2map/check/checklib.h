/**
 * @file checklib.h
 * @brief Performs check on a loaded mapfile, and makes changes
 * that can be saved back to the source map.
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef _CHECKLIB_H
#define _CHECKLIB_H

#include "../common/shared.h"

/** constants that may be passed to Check_Printf in lieu of entity/brush numbers */
#define NUM_NONE -1
#define NUM_DIFF -2
#define NUM_SAME -3

void Check_Printf(const verbosityLevel_t msgVerbLevel, qboolean change, int entnum, int brushnum, const char *format, ...) __attribute__((format(printf, 5, 6)));
void Check_InitEntityDefs(void);
void Check_Free(void);

#endif /* _CHECKLIB_H */
