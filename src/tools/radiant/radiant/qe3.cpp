/**
 * @file qe3.cpp
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

/*
 The following source code is licensed by Id Software and subject to the terms of
 its LIMITED USE SOFTWARE LICENSE AGREEMENT, a copy of which is included with
 GtkRadiant. If you did not receive a LIMITED USE SOFTWARE LICENSE AGREEMENT,
 please contact Id Software immediately at info@idsoftware.com.
 */

#include "qe3.h"

#include "ifilesystem.h"
#include "stream/stringstream.h"
#include "scenelib.h"
#include "gtkutil/messagebox.h"
#include "map/map.h"
#include "iradiant.h"
#include "mainframe.h"
#include "convert.h"
#include "radiant_i18n.h"

QEGlobals_t g_qeglobals;

/**
 * VFS initialization
 * we will call GlobalFileSystem().initDirectory, giving the directories to look in (for files in pk3's and for standalone files)
 * we need to call in order, the mod ones first, then the base ones .. they will be searched in this order
 * *nix systems have a dual filesystem in ~/.ufoai, which is searched first .. so we need to add that too
 */
void QE_InitVFS (void)
{
	const std::string& basegame = basegame_get();

#if defined(__linux__) || defined (__FreeBSD__) || defined(__APPLE__)
	// ~/.<gameprefix>/<fs_main>
	const std::string& userRoot = g_qeglobals.m_userEnginePath;
	GlobalFileSystem().initDirectory(userRoot + basegame + "/");
#endif

	// <fs_basepath>/<fs_main>
	const std::string& globalRoot = EnginePath_get();
	GlobalFileSystem().initDirectory(globalRoot + basegame + "/");
}

/**
 * @brief Updates statusbar with brush and entity count
 * @sa MainFrame::RedrawStatusText
 * @sa MainFrame::SetStatusText
 * @todo Let his also count the filtered brushes and entities - to be able to
 * use this counter for level optimizations
 */
inline void QE_UpdateStatusBar (void)
{
	char buffer[128];
	sprintf(buffer, _("Brushes: %d Entities: %d"), int(g_brushCount.get()), int(g_entityCount.get()));
	g_pParentWnd->SetStatusText(g_pParentWnd->m_brushcount_status, buffer);
}

SimpleCounter g_brushCount;

void QE_brushCountChanged (void)
{
	QE_UpdateStatusBar();
}

SimpleCounter g_entityCount;

void QE_entityCountChanged (void)
{
	QE_UpdateStatusBar();
}

bool ConfirmModified (const std::string& title)
{
	if (!Map_Modified(g_map))
		return true;

	EMessageBoxReturn result = gtk_MessageBox(
		GTK_WIDGET(GlobalRadiant().getMainWindow()),
		_("The current map has changed since it was last saved.\nDo you want to save the current map before continuing?"),
		title, eMB_YESNOCANCEL, eMB_ICONQUESTION);
	if (result == eIDCANCEL)
		return false;

	if (result == eIDYES) {
		if (Map_Unnamed(g_map))
			return Map_SaveAs();
		else
			return Map_Save();
	}
	return true;
}

/**
 * @brief Sets the window title for UFORadiant
 * @sa Map_UpdateTitle
 */
void Sys_SetTitle (const std::string& text, bool modified)
{
	std::string title = "UFORadiant " + text;
	if (modified)
		title += " *";

	gtk_window_set_title(GlobalRadiant().getMainWindow(), title.c_str());
}
