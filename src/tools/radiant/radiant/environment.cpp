/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

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

#include "environment.h"

#include "stream/textstream.h"
#include "stream/stringstream.h"

#include <glib.h>

static CopiedString home_path;
static CopiedString app_path;

const char* environment_get_home_path (void)
{
	return home_path.c_str();
}

const char* environment_get_app_path (void)
{
	return app_path.c_str();
}

#ifdef _WIN32
#define RADIANT_DIRECTORY "/UFOAI/RadiantSettings/"
#else
#define RADIANT_DIRECTORY ".ufoai/radiant/"
#endif

void environment_init (void)
{
	StringOutputStream path(256);
	path << g_get_home_dir() << "/" << RADIANT_DIRECTORY;
	g_mkdir_with_parents(path.c_str(), 775);
	home_path = path.c_str();

#ifdef PKGDATADIR
	path.clear();
	path << PKGDATADIR << "/radiant/";
	if (g_file_test(PKGDATADIR, (GFileTest)G_FILE_TEST_IS_DIR))
		app_path = path.c_str();
	else
#endif
	{
		path.clear();
		path << g_path_get_dirname(g_get_prgname()) << "/radiant/";
		app_path = path.c_str();
	}
}
