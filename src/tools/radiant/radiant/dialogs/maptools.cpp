/**
 * @file maptools.cpp
 * @brief Creates the maptools dialogs - like checking for errors and compiling
 * a map
 */

/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "maptools.h"
#include "cmdlib.h"
#include "../map.h"
#include "../preferences.h"

/**
 * @todo Implement me
 */
void ToolsCheckErrors (void)
{
#if 0
	const char* path = getMapsPath();
	const char* fullname = Map_Name(g_map);
	const char* mapcompiler = g_pGameDescription->getRequiredKeyValue("mapcompiler");
	const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param_check");

	Q_Exec(fullname, NULL, NULL, true);
#endif
}

/**
 * @todo Implement me
 */
void ToolsCompile (void)
{
#if 0
	const char* path = getMapsPath();
	const char* fullname = Map_Name(g_map);
	const char* mapcompiler = g_pGameDescription->getRequiredKeyValue("mapcompiler");
	const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param");

	Q_Exec(fullname, NULL, NULL, true);
#endif
}
