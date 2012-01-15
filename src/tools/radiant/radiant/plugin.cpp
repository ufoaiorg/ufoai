/**
 * @file plugin.cpp
 */

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

#include <iostream>
#include "plugin.h"

#include "debugging/debugging.h"

#include "iradiant.h"
#include "ifilesystem.h"
#include "ishadersystem.h"
#include "ientity.h"
#include "ieclass.h"
#include "irender.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ifilter.h"
#include "iscriplib.h"
#include "igl.h"
#include "iundo.h"
#include "itextures.h"
#include "ireference.h"
#include "igamemanager.h"
#include "ifiletypes.h"
#include "preferencesystem.h"
#include "ibrush.h"
#include "iimage.h"
#include "iregistry.h"
#include "imaterial.h"
#include "iump.h"
#include "iufoscript.h"
#include "imap.h"
#include "iselectionset.h"
#include "iclipper.h"
#include "inamespace.h"
#include "isound.h"
#include "ifilter.h"
#include "ieventmanager.h"
#include "igrid.h"
#include "iufoscript.h"
#include "ioverlay.h"
#include "iuimanager.h"
#include "iparticles.h"
#include "imapcompiler.h"
#include "imaterial.h"
#include "ipathfinding.h"

#include "gtkutil/image.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/filechooser.h"

#include "map/map.h"
#include "map/CounterManager.h"
#include "map/AutoSaver.h"

#include "os/path.h"

#include "ui/mainframe/mainframe.h"
#include "ui/mru/MRU.h"
#include "entity/entity.h"
#include "select.h"
#include "referencecache/nullmodel.h"
#include "xyview/GlobalXYWnd.h"
#include "camera/GlobalCamera.h"
#include "model.h"

#include "sidebar/sidebar.h"
#include "sidebar/texturebrowser.h"

#include "modulesystem/modulesmap.h"
#include "modulesystem/singletonmodule.h"

#include "generic/callback.h"

#include <set>

class RadiantModule : public IRadiant
{
	private:

		typedef std::set<RadiantEventListener*> EventListenerList;
		EventListenerList _eventListeners;

		map::CounterManager _counters;

	public:
		typedef IRadiant Type;
		STRING_CONSTANT(Name, "*");

		IRadiant* getTable ()
		{
			return this;
		}

	public:

		// Registers/de-registers an event listener class
		void addEventListener(RadiantEventListener* listener) {
			_eventListeners.insert(listener);
		}

		void removeEventListener(RadiantEventListener* listener) {
			EventListenerList::iterator found = _eventListeners.find(listener);

			if (found != _eventListeners.end()) {
				_eventListeners.erase(found);
			}
		}

		// Broadcasts a "shutdown" event to all the listeners, this also clears all listeners!
		void broadcastShutdownEvent ()
		{
			for (EventListenerList::const_iterator i = _eventListeners.begin(); i != _eventListeners.end(); ++i) {
				(*i)->onRadiantShutdown();
			}

			// This was the final radiant event, no need for keeping any pointers anymore
			_eventListeners.clear();
		}

		// Broadcasts a "startup" event to all the listeners
		void broadcastStartupEvent ()
		{
			for (EventListenerList::const_iterator i = _eventListeners.begin(); i != _eventListeners.end(); ++i) {
				(*i)->onRadiantStartup();
			}
		}

		/** Return the main application GtkWindow.
		 */
		GtkWindow* getMainWindow() {
			return MainFrame_getWindow();
		}

		const std::string& getEnginePath ()
		{
			return GlobalGameManager().getEnginePath();
		}
		const std::string& getGamePath ()
		{
			return GlobalGameManager().getKeyValue("basegame");
		}
		const std::string getFullGamePath ()
		{
			const std::string enginePath = getEnginePath();
			const std::string gamePath = getGamePath();
			return DirectoryCleaned(enginePath + gamePath);
		}

		virtual Counter& getCounter(CounterType counter) {
			// Pass the call to the helper class
			return _counters.get(counter);
		}

		void setStatusText (const std::string& statusText) {
			Sys_Status(statusText);
		}

		void attachGameToolsPathObserver (ModuleObserver& observer) {
			Radiant_attachGameToolsPathObserver(observer);
		}
		void detachGameToolsPathObserver (ModuleObserver& observer) {
			Radiant_detachGameToolsPathObserver(observer);
		}

		void attachGameModeObserver (ModuleObserver& observer) {
			Radiant_attachGameModeObserver(observer);
		}
		void detachGameModeObserver (ModuleObserver& observer) {
			Radiant_detachGameModeObserver(observer);
		}
};

typedef SingletonModule<RadiantModule> RadiantCoreModule;
typedef Static<RadiantCoreModule> StaticRadiantCoreModule;
StaticRegisterModule staticRegisterRadiantCore(StaticRadiantCoreModule::instance());

class RadiantDependencies: public GlobalRadiantModuleRef,
		public GlobalEventManagerModuleRef,
		public GlobalUIManagerModuleRef,
		public GlobalFileSystemModuleRef,
		public GlobalSoundManagerModuleRef,
		public GlobalUMPSystemModuleRef,
		public GlobalUFOScriptSystemModuleRef,
		public GlobalMaterialSystemModuleRef,
		public GlobalEntityModuleRef,
		public GlobalShadersModuleRef,
		public GlobalBrushModuleRef,
		public GlobalRegistryModuleRef,
		public GlobalSceneGraphModuleRef,
		public GlobalShaderCacheModuleRef,
		public GlobalFiletypesModuleRef,
		public GlobalSelectionModuleRef,
		public GlobalReferenceModuleRef,
		public GlobalOpenGLModuleRef,
		public GlobalEntityClassManagerModuleRef,
		public GlobalUndoModuleRef,
		public GlobalScripLibModuleRef,
		public GlobalNamespaceModuleRef,
		public GlobalFilterModuleRef,
		public GlobalClipperModuleRef,
		public GlobalGridModuleRef,
		public GlobalOverlayModuleRef,
		public GlobalSelectionSetManagerModuleRef,
		public GlobalParticleModuleRef,
		public GlobalMapCompilerModuleRef,
		public GlobalGameManagerModuleRef,
		public GlobalPathfindingSystemModuleRef
{
		ImageModulesRef m_image_modules;
		MapModulesRef m_map_modules;

	public:
		RadiantDependencies () :
			GlobalSoundManagerModuleRef("*"), GlobalUMPSystemModuleRef("*"), GlobalUFOScriptSystemModuleRef("*"),
					GlobalMaterialSystemModuleRef("*"), GlobalEntityModuleRef("*"), GlobalShadersModuleRef("*"),
					GlobalBrushModuleRef("*"), GlobalEntityClassManagerModuleRef("*"), m_image_modules("*"),
					m_map_modules("*")
		{
		}

		ImageModules& getImageModules ()
		{
			return m_image_modules.get();
		}
		MapModules& getMapModules ()
		{
			return m_map_modules.get();
		}
};

class Radiant
{
	public:
		Radiant ()
		{
			/** @todo Add soundtypes support into game.xml */
			GlobalFiletypes().addType("sound", "wav", filetype_t("PCM sound files", "*.wav"));
			GlobalFiletypes().addType("sound", "ogg", filetype_t("OGG sound files", "*.ogg"));

			GlobalMap().Construct();
			MainFrame_Construct();
			GlobalCamera().construct();
			GlobalXYWnd().construct();
			GlobalMaterialSystem()->construct();
			TextureBrowser_Construct();
			Entity_Construct();
			map::AutoSaver().init();

			GlobalGameManager().init();

			GlobalPathfindingSystem().init();
			GlobalUFOScriptSystem()->init();

			// Load the shortcuts from the registry
			GlobalEventManager().loadAccelerators();
		}
		~Radiant ()
		{
			GlobalGameManager().destroy();

			GlobalCamera().destroy();
			GlobalXYWnd().destroy();
			MainFrame_Destroy();
			GlobalMap().Destroy();
		}
};

namespace
{
	bool g_RadiantInitialised = false;
	RadiantDependencies* g_RadiantDependencies;
	Radiant* g_Radiant;
}

bool Radiant_Construct (ModuleServer& server)
{
	GlobalModuleServer::instance().set(server);
	/** @todo find a way to do this with in a static way */
	ModelModules_Init();
	StaticModuleRegistryList().instance().registerModules();

	g_RadiantDependencies = new RadiantDependencies();

	g_RadiantInitialised = !server.getError();

	if (g_RadiantInitialised) {
		g_Radiant = new Radiant;
	}

	return g_RadiantInitialised;
}

void Radiant_Destroy ()
{
	if (g_RadiantInitialised) {
		delete g_Radiant;
	}

	delete g_RadiantDependencies;
}

ui::ColourSchemeManager& ColourSchemes() {
	static ui::ColourSchemeManager _manager;
	return _manager;
}

ImageModules& Radiant_getImageModules ()
{
	return g_RadiantDependencies->getImageModules();
}
MapModules& Radiant_getMapModules ()
{
	return g_RadiantDependencies->getMapModules();
}
