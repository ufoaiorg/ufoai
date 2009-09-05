/**
 * @file autosave.cpp
 * @brief Handles the autosave mechanism and the affected preference handling
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

#include "autosave.h"
#include "radiant_i18n.h"

#include "os/file.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "gtkutil/messagebox.h"
#include "scenelib.h"
#include "mapfile.h"

#include "map.h"
#include "mainframe.h"
#include "qe3.h"
#include "preferences.h"

static inline bool DoesFileExist (const std::string& name, std::size_t& size)
{
	if (file_exists(name)) {
		size += file_size(name);
		return true;
	}
	return false;
}

static void Map_Snapshot ()
{
	// we need to do the following
	// 1. make sure the snapshot directory exists (create it if it doesn't)
	// 2. find out what the lastest save is based on number
	// 3. inc that and save the map
	const std::string& path = Map_Name(g_map);
	const char* name = path_get_filename_start(path.c_str());

	StringOutputStream snapshotsDir(256);
	snapshotsDir << StringRange(path.c_str(), name) << "snapshots";

	if (file_exists(snapshotsDir.c_str()) || g_mkdir(snapshotsDir.c_str(), 0775)) {
		std::size_t lSize = 0;
		StringOutputStream strNewPath(256);
		strNewPath << snapshotsDir.c_str() << '/' << name;

		StringOutputStream snapshotFilename(256);

		for (int nCount = 0;; ++nCount) {
			// The original map's filename is "<path>/<name>.<ext>"
			// The snapshot's filename will be "<path>/snapshots/<name>.<count>.<ext>"
			const char* end = path_get_filename_base_end(strNewPath.c_str());
			snapshotFilename << StringRange(strNewPath.c_str(), end) << '.' << nCount << end;

			if (!DoesFileExist(snapshotFilename.c_str(), lSize)) {
				break;
			}

			snapshotFilename.clear();
		}

		// save in the next available slot
		Map_SaveFile(snapshotFilename.c_str());

		if (lSize > 50 * 1024 * 1024) { // total size of saves > 50 mb
			g_warning("The snapshot files in %s total more than 50 megabytes. You might consider cleaning up.\n",
				snapshotsDir.c_str());
		}
	} else {
		StringOutputStream strMsg(256);
		strMsg << "Snapshot save failed.. unabled to create directory\n" << snapshotsDir.c_str();
		gtk_MessageBox(GTK_WIDGET(MainFrame_getWindow()), strMsg.c_str());
	}
}

static bool g_AutoSave_Enabled = true;
static int m_AutoSave_Frequency = 5;
static bool g_SnapShots_Enabled = false;

namespace
{
	time_t s_start = 0;
	std::size_t s_changes = 0;
}

void AutoSave_clear ()
{
	s_changes = 0;
}

static inline scene::Node& Map_Node ()
{
	return GlobalSceneGraph().root();
}

/**
 * @brief If five minutes have passed since making a change
 * and the map hasn't been saved, save it out.
 */
void QE_CheckAutoSave (void)
{
	if (!Map_Valid(g_map) || !ScreenUpdates_Enabled()) {
		return;
	}

	time_t now;
	time(&now);

	if (s_start == 0 || s_changes == Node_getMapFile(Map_Node())->changes()) {
		s_start = now;
	}

	if ((now - s_start) > (60 * m_AutoSave_Frequency)) {
		s_start = now;
		s_changes = Node_getMapFile(Map_Node())->changes();

		if (g_AutoSave_Enabled) {
			const char* strMsg = g_SnapShots_Enabled ? _("Autosaving snapshot...") : _("Autosaving...");
			g_message("%s\n", strMsg);
			//Sys_Status(strMsg);

			// only snapshot if not working on a default map
			if (g_SnapShots_Enabled && !Map_Unnamed(g_map)) {
				Map_Snapshot();
			} else {
				if (Map_Unnamed(g_map)) {
					std::string autosave = g_qeglobals.m_userGamePath + "maps/";
					g_mkdir(autosave.c_str(), 0775);
					autosave += "autosave.map";
					Map_SaveFile(autosave);
				} else {
					const std::string& name = Map_Name(g_map);
					const char* extension = path_get_filename_base_end(name.c_str());
					StringOutputStream autosave(256);
					autosave << StringRange(name.c_str(), extension) << ".autosave" << extension;
					Map_SaveFile(autosave.c_str());
				}
			}
		} else {
			g_message("Autosave skipped...\n");
		}
	}
}

static void Autosave_constructPreferences (PreferencesPage& page)
{
	GtkWidget* autosave_enabled = page.appendCheckBox(_("Autosave"), _("Enable Autosave"), g_AutoSave_Enabled);
	GtkWidget* autosave_frequency = page.appendSpinner(_("Autosave Frequency (minutes)"), m_AutoSave_Frequency, 1, 1, 60);
	Widget_connectToggleDependency(autosave_frequency, autosave_enabled);
	page.appendCheckBox("", _("Save Snapshots"), g_SnapShots_Enabled);
}
void Autosave_constructPage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage(_("Autosave"), _("Autosave Preferences")));
	Autosave_constructPreferences(page);
}
void Autosave_registerPreferencesPage ()
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Autosave_constructPage> ());
}

#include "preferencesystem.h"
#include "stringio.h"

void Autosave_Construct ()
{
	GlobalPreferenceSystem().registerPreference("Autosave", BoolImportStringCaller(g_AutoSave_Enabled),
			BoolExportStringCaller(g_AutoSave_Enabled));
	GlobalPreferenceSystem().registerPreference("AutosaveMinutes", IntImportStringCaller(m_AutoSave_Frequency),
			IntExportStringCaller(m_AutoSave_Frequency));
	GlobalPreferenceSystem().registerPreference("Snapshots", BoolImportStringCaller(g_SnapShots_Enabled),
			BoolExportStringCaller(g_SnapShots_Enabled));

	Autosave_registerPreferencesPage();
}

void Autosave_Destroy ()
{
}
