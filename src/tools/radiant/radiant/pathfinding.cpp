/**
 * @file pathfinding.cpp
 * @brief Show pathfinding related info in the radiant windows
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

#include "pathfinding.h"
#include "radiant_i18n.h"

#include "mainframe.h"
#include "os/path.h"
#include "os/file.h"
#include "gtkutil/filechooser.h"
#include "autoptr.h"
#include "ifilesystem.h"
#include "archivelib.h"
#include "script/scripttokeniser.h"

/**
 * @brief Parses the ufo2map pathfinding output to be able to display it afterwards
 * @returns 0 on success
 * @note >>mapname<<.elevation.csv and >>mapname<<.walls.csv are the files we have to parse
 */
static int ParsePathfindingLogFile (const char *filename)
{
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (file) {
		AutoPtr<Tokeniser> tokeniser(NewScriptTokeniser(file->getInputStream()));

		for (;;) {
			const char* token = tokeniser->getToken();

			if (token == 0) {
				break;
			}

			/** @todo parse the data */
		}
	}

	return 0;
}

/**
 * @todo Maybe also use the ufo2map output directly
 * @sa ToolsCompile
 */
void ShowPathfinding (void)
{
	const char *filename = file_dialog(GTK_WIDGET(MainFrame_getWindow()), TRUE, _("Pathfinding log file"), NULL, NULL);
	if (!filename)
		return;
	const int retVal = ParsePathfindingLogFile(filename);
	if (retVal)
		return;

	/** @todo render the parsed data */
}
