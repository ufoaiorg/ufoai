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

#ifdef HAVE_CONFIG_H
#include "../../../../config.h"
#endif

#include "environment.h"

#include "iregistry.h"

#include "gtkutil/image.h"
#include "string/string.h"
#include "os/path.h"

#include <glib.h>

namespace {
const std::string RADIANT_DIRECTORY = "radiant/";
}

Environment::Environment() {
}

std::string Environment::getHomeDir ()
{
	const gchar *home;

#ifdef _WIN32
	home = g_get_user_config_dir();
	const std::string RADIANT_HOME = "UFOAI/";
#else
	home = g_get_home_dir();
	const std::string RADIANT_HOME = ".ufoai/";
#endif

	const std::string cleaned = DirectoryCleaned(home);
	return cleaned + RADIANT_HOME + RADIANT_DIRECTORY;
}

void Environment::init(int argc, char* argv[])
{
	_homePath = getHomeDir();

#ifdef PKGDATADIR
	_appPath = std::string(PKGDATADIR) + "/" + RADIANT_DIRECTORY;
	if (!(g_file_test(_appPath.c_str(), (GFileTest)G_FILE_TEST_IS_DIR) && g_path_is_absolute(_appPath.c_str())))
#endif
	{
		gchar *currentDir = g_get_current_dir();
		_appPath = DirectoryCleaned(currentDir);
		g_free(currentDir);
	}
	initPaths();
}

void Environment::initArgs(int argc, char* argv[]) {
	int i, j, k;

	for (i = 1; i < argc; i++) {
		for (k = i; k < argc; k++)
			if (argv[k] != 0)
				break;

		if (k > i) {
			k -= i;
			for (j = i + k; j < argc; j++)
				argv[j-k] = argv[j];
			argc -= k;
		}
	}

	_argc = argc;
	_argv = argv;
}

int Environment::getArgc() const {
	return _argc;
}

std::string Environment::getArgv(unsigned int index) const {
	if (index < _argc)
		return std::string(_argv[index]);
	return "";
}

std::string Environment::getHomePath() {
	return _homePath;
}

std::string Environment::getAppPath() {
	return _appPath;
}

std::string Environment::getSettingsPath() {
	return _settingsPath;
}

std::string Environment::getBitmapsPath() {
	return _bitmapsPath;
}

Environment& Environment::Instance() {
	static Environment _instance;
	return _instance;
}

void Environment::savePathsToRegistry() {
	GlobalRegistry().set(RKEY_APP_PATH, _appPath);
	GlobalRegistry().set(RKEY_HOME_PATH, _homePath);
	GlobalRegistry().set(RKEY_SETTINGS_PATH, _settingsPath);
	GlobalRegistry().set(RKEY_BITMAPS_PATH, _bitmapsPath);
}

void Environment::deletePathsFromRegistry() {
	GlobalRegistry().deleteXPath(RKEY_APP_PATH);
	GlobalRegistry().deleteXPath(RKEY_HOME_PATH);
	GlobalRegistry().deleteXPath(RKEY_SETTINGS_PATH);
	GlobalRegistry().deleteXPath(RKEY_BITMAPS_PATH);
}

const std::string& Environment::getMapsPath() const {
	return _mapsPath;
}

void Environment::setMapsPath(const std::string& path) {
	_mapsPath = path;
}

void Environment::initPaths() {
	// Make sure the home folder exists (attempt to create it)
	g_mkdir_with_parents(_homePath.c_str(), 0755);

	_settingsPath = _homePath;
	g_mkdir_with_parents(_settingsPath.c_str(), 0755);

	_bitmapsPath = _appPath + "bitmaps/";
	BitmapsPath_set(_bitmapsPath);
}
