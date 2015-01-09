/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include <gtest/gtest.h>
#include "../common/common.h"
#include "../shared/shared.h"

void TEST_vPrintf(const char* fmt, va_list argptr);
void TEST_vPrintfSilent(const char* fmt, va_list argptr);
void TEST_Init(void);
void TEST_Shutdown(void);

/** interface to allow to custom tests with command line */
void TEST_RegisterProperty(const char* name, const char* value);
bool TEST_ExistsProperty(const char* name);
int TEST_GetIntProperty(const char* name);
long TEST_GetLongProperty(const char* name);
const char* TEST_GetStringProperty(const char* name);
