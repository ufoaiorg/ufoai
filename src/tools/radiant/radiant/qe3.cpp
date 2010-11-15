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
#include "scenelib.h"
#include "gtkutil/messagebox.h"
#include "map/map.h"
#include "iradiant.h"
#include "mainframe.h"
#include "radiant_i18n.h"

bool ConfirmModified (const std::string& title)
{
	if (!GlobalMap().isModified())
		return true;

	EMessageBoxReturn result = gtk_MessageBox(
		GTK_WIDGET(GlobalRadiant().getMainWindow()),
		_("The current map has changed since it was last saved.\nDo you want to save the current map before continuing?"),
		title, eMB_YESNOCANCEL, eMB_ICONQUESTION);
	if (result == eIDCANCEL)
		return false;

	if (result == eIDYES) {
		if (GlobalMap().isUnnamed())
			return GlobalMap().saveAsDialog();
		else
			return GlobalMap().save();
	}
	return true;
}
