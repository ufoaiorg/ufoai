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

/* greebo: This is where the interface for other plugins is defined.
 * Functions that should be accessible via GlobalRadiant() are defined here
 * as function pointers. The class RadiantCoreAPI in plugin.cpp makes sure
 * that these variables are pointing to the correct functions. */

#ifndef __QERPLUGIN_H__
#define __QERPLUGIN_H__

#include "generic/constant.h"
#include "iclipper.h"
#include <string>

// ========================================
// GTK+ helper functions

// NOTE: parent can be 0 in all functions but it's best to set them

// this API does not depend on gtk+ or glib
typedef struct _GtkWidget GtkWidget;

enum EMessageBoxType
{
	eMB_OK, eMB_OKCANCEL, eMB_YESNO, eMB_YESNOCANCEL, eMB_NOYES
};

enum EMessageBoxIcon
{
	eMB_ICONDEFAULT, eMB_ICONERROR, eMB_ICONWARNING, eMB_ICONQUESTION, eMB_ICONASTERISK
};

enum EMessageBoxReturn
{
	eIDOK, eIDCANCEL, eIDYES, eIDNO
};

// simple Message Box, see above for the 'type' flags

typedef EMessageBoxReturn (* PFN_QERAPP_MESSAGEBOX) (GtkWidget *parent, const std::string& text,
		const std::string& caption, EMessageBoxType type, EMessageBoxIcon icon);

// file and directory selection functions return null if the user hits cancel
// - 'title' is the dialog title (can be null)
// - 'path' is used to set the initial directory (can be null)
// - 'pattern': the first pattern is for the win32 mode, then comes the Gtk pattern list, see Radiant source for samples
typedef const char* (* PFN_QERAPP_FILEDIALOG) (GtkWidget *parent, bool open, const std::string& title,
		const std::string& path, const std::string& pattern);

// ========================================

// Forward declarations
namespace scene
{
	class Node;
}

class ModuleObserver;

#include "signal/signalfwd.h"
#include "windowobserver.h"
#include "math/Vector3.h"
#include "math/Plane3.h"

typedef struct _GdkEventButton GdkEventButton;
typedef SignalHandler3<int, int, GdkEventButton*> MouseEventHandler;
typedef SignalFwd<MouseEventHandler>::handler_id_type MouseEventHandlerId;

typedef struct _GtkWindow GtkWindow;

// the radiant core API
// This contains pointers to all the core functions that should be available via GlobalRadiant()
struct IRadiant
{
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "radiant");

		/** Return the main application GtkWindow.
		 */
		GtkWindow* (*getMainWindow) ();
		const std::string& (*getEnginePath) ();
		const std::string& (*getAppPath) ();
		const std::string& (*getSettingsPath) ();
		const std::string& (*getMapsPath) ();

		const std::string (*getGamePath) ();
		/**
		 * @return The full path to the current loaded map
		 */
		const std::string (*getMapName) ();
		scene::Node& (*getMapWorldEntity) ();

		const std::string& (*getGameDescriptionKeyValue) (const std::string& key);
		const std::string& (*getRequiredGameDescriptionKeyValue) (const std::string& key);

		void (*attachGameToolsPathObserver) (ModuleObserver& observer);
		void (*detachGameToolsPathObserver) (ModuleObserver& observer);
		void (*attachEnginePathObserver) (ModuleObserver& observer);
		void (*detachEnginePathObserver) (ModuleObserver& observer);
		void (*attachGameModeObserver) (ModuleObserver& observer);
		void (*detachGameModeObserver) (ModuleObserver& observer);

		// GTK+ functions
		PFN_QERAPP_MESSAGEBOX m_pfnMessageBox;
		PFN_QERAPP_FILEDIALOG m_pfnFileDialog;
};

// IRadiant Module Definitions
#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IRadiant> GlobalRadiantModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IRadiant> GlobalRadiantModuleRef;

inline IRadiant& GlobalRadiant ()
{
	return GlobalRadiantModule::getTable();
}

#endif
