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
#include "gtkutil/accelerator.h"
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

// return true if the user closed the dialog with 'Ok'
// 'color' is used to set the initial value and store the selected value
template<typename Element> class BasicVector3;
typedef BasicVector3<float> Vector3;
typedef bool (* PFN_QERAPP_COLORDIALOG) (GtkWidget *parent, Vector3& color, const std::string& title);

// load an image file and create a GtkWidget from it
// NOTE: 'filename' is relative to <radiant_path>/bitmaps/
typedef struct _GtkWidget GtkWidget;
typedef GtkWidget* (* PFN_QERAPP_NEWIMAGE) (const std::string& filename);

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

typedef SignalHandler3<const WindowVector&, ButtonIdentifier, ModifierFlags> MouseEventHandler;
typedef SignalFwd<MouseEventHandler>::handler_id_type MouseEventHandlerId;

// Possible types of the orthogonal view window
enum EViewType
{
	YZ = 0, XZ = 1, XY = 2
};

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
		void (*commandInsert) (const std::string& name, const Callback& callback, const Accelerator& accelerator);

		const std::string (*getGamePath) ();
		const std::string& (*getGameName) ();
		/**
		 * @return The full path to the current loaded map
		 */
		const std::string (*getMapName) ();
		scene::Node& (*getMapWorldEntity) ();
		float (*getGridSize) ();

		const std::string& (*getGameDescriptionKeyValue) (const std::string& key);
		const std::string& (*getRequiredGameDescriptionKeyValue) (const std::string& key);

		void (*attachGameToolsPathObserver) (ModuleObserver& observer);
		void (*detachGameToolsPathObserver) (ModuleObserver& observer);
		void (*attachEnginePathObserver) (ModuleObserver& observer);
		void (*detachEnginePathObserver) (ModuleObserver& observer);
		void (*attachGameNameObserver) (ModuleObserver& observer);
		void (*detachGameNameObserver) (ModuleObserver& observer);
		void (*attachGameModeObserver) (ModuleObserver& observer);
		void (*detachGameModeObserver) (ModuleObserver& observer);

		SignalHandlerId (*XYWindowDestroyed_connect) (const SignalHandler& handler);
		void (*XYWindowDestroyed_disconnect) (SignalHandlerId id);
		MouseEventHandlerId (*XYWindowMouseDown_connect) (const MouseEventHandler& handler);
		void (*XYWindowMouseDown_disconnect) (MouseEventHandlerId id);
		EViewType (*XYWindow_getViewType) ();
		Vector3 (*XYWindow_windowToWorld) (const WindowVector& position);
		const std::string& (*TextureBrowser_getSelectedShader) ();

		// GTK+ functions
		PFN_QERAPP_MESSAGEBOX m_pfnMessageBox;
		PFN_QERAPP_FILEDIALOG m_pfnFileDialog;
		PFN_QERAPP_COLORDIALOG m_pfnColorDialog;
		PFN_QERAPP_NEWIMAGE m_pfnNewImage;
};

// _QERFuncTable_1 Module Definitions
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
