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
#include "os/file.h"
#include "../map.h"
#include "../preferences.h"

void ToolsCheckErrors (void)
{
/*	const char* path = getMapsPath(); */
	const char* mapcompiler = g_pGameDescription->getRequiredKeyValue("mapcompiler");

	if (file_exists(mapcompiler)) {
		char buf[1024];
		const char* fullname = Map_Name(g_map);
		const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param_check");

		snprintf(buf, sizeof(buf) - 1, "%s %s", compiler_parameter, fullname);
		buf[sizeof(buf) - 1] = '\0';

		char* output = Q_Exec(mapcompiler, buf, NULL, false);
		if (output) {
			/** @todo parse and display this in a gtk window */
			globalOutputStream() << output;
			free(output);
		} else
			globalOutputStream() << "No output for checking " << fullname << "\n";
	} else {
		globalOutputStream() << "Could not find " << mapcompiler << "\n";
	}
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

	char* output = Q_Exec(mapcompiler, compiler_parameter, NULL, false);
	if (output)
		free(output);
#endif
}
