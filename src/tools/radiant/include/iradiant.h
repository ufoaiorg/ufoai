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

#ifndef IRADIANT_H__
#define IRADIANT_H__

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

/** greebo: An EventListener gets notified by the Radiant module
 *          on global events like shutdown, startup and such.
 *
 *          EventListener classes must register themselves using
 *          the GlobalRadiant().addEventListener() method in order
 *          to get notified about the events.
 *
 * Note: Default implementations are empty, deriving classes are
 *       supposed to pick the events they want to listen to.
 */
class RadiantEventListener {
public:
	/** Destructor
	 */
	virtual ~RadiantEventListener() {}

	/** This gets called AFTER the MainFrame window has been constructed.
	 */
	virtual void onRadiantStartup() {}

	/** Gets called when BEFORE the MainFrame window is destroyed.
	 *  Note: After this call, the EventListeners are deregistered from the
	 *        Radiant module, all the internally held shared_ptrs are cleared.
	 */
	virtual void onRadiantShutdown() {}
};

enum CounterType {
	counterBrushes,
	counterEntities
};

class Counter;

/** greebo: This abstract class defines the interface to the core application.
 * 			Use this to access methods from the main codebase in radiant/
 */
struct IRadiant
{
	INTEGER_CONSTANT(Version, 1);
	STRING_CONSTANT(Name, "radiant");

	virtual ~IRadiant() {}

	// Registers/de-registers an event listener class
	virtual void addEventListener(RadiantEventListener* listener) = 0;
	virtual void removeEventListener(RadiantEventListener* listener) = 0;

	// Broadcasts a "shutdown" event to all the listeners, this also clears all listeners!
	virtual void broadcastShutdownEvent() = 0;

	// Broadcasts a "startup" event to all the listeners
	virtual void broadcastStartupEvent() = 0;

	/** Return the main application GtkWindow.
	 */
	virtual GtkWindow* getMainWindow() = 0;
	virtual const std::string& getEnginePath() = 0;
	virtual const std::string& getGamePath() = 0;
	virtual const std::string getFullGamePath() = 0;

	// Returns the Counter object of the given type
	virtual Counter& getCounter(CounterType counter) = 0;

	/** greebo: Set the status text of the main window
	 */
	virtual void setStatusText (const std::string& statusText) = 0;

	virtual void attachGameToolsPathObserver (ModuleObserver& observer) = 0;
	virtual void detachGameToolsPathObserver (ModuleObserver& observer) = 0;
	virtual void attachGameModeObserver (ModuleObserver& observer) = 0;
	virtual void detachGameModeObserver (ModuleObserver& observer) = 0;
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
