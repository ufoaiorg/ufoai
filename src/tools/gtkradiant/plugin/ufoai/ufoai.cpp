/*
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

#include "ufoai.h"
#include "ufoai_level.h"
#include "ufoai_gtk.h"
#include "ufoai_filters.h"

#include "debugging/debugging.h"

#include "iplugin.h"
#include "ifilter.h"
#include "iundo.h"       // declaration of undo system
#include "ientity.h"     // declaration of entity system
#include "iscenegraph.h" // declaration of datastructure of the map
#include "scenelib.h"    // declaration of datastructure of the map
#include "qerplugin.h"   // declaration to use other interfaces as a plugin

#include "string/string.h"
#include "modulesystem/singletonmodule.h"

#include <gtk/gtk.h>

class UFOAIPluginDependencies :
  public GlobalRadiantModuleRef,    // basic class for all other module refs
  public GlobalUndoModuleRef,       // used to say radiant that something has changed and to undo that
  public GlobalSceneGraphModuleRef, // necessary to handle data in the mapfile (change, retrieve data)
  public GlobalEntityModuleRef      // to access and modify the entities
{
public:
	UFOAIPluginDependencies() :
		GlobalEntityModuleRef(GlobalRadiant().getRequiredGameDescriptionKeyValue("entities"))
	{
	}
};

namespace UFOAI
{
	GtkWindow* g_mainwnd;
	char UFOMenuList[100] = "About...";

	const char* init(void* hApp, void* pMainWidget)
	{
		return "";
	}
	const char* getName()
	{
		return "UFO:AI";
	}
	const char* getCommandList()
	{
		if (strncmp(GlobalRadiant().getGameName(), "ufoai", 6) == 0)
			strncat(UFOMenuList, ";Level 1,Level 2,Level 3,Level 4,Level 5,Level 6,Level 7,Level 8,StepOn", sizeof(UFOMenuList));
		else
			strncat(UFOMenuList, GlobalRadiant().getGameName(), sizeof(UFOMenuList));
		return (const char*)UFOMenuList;
	}
	const char* getCommandTitleList()
	{
		return "";
	}
	void dispatch(const char* command, float* vMin, float* vMax, bool bSingleBrush)
	{
		if(string_equal(command, "About"))
		{
			GlobalRadiant().m_pfnMessageBox(GTK_WIDGET(g_mainwnd),
				"UFO:AI Plugin (http://www.ufoai.net)\n", "About",
				eMB_OK, eMB_ICONDEFAULT);
		}
		else if(string_equal(command, "Level 1"))
		{
			globalOutputStream() << "Level 1\n";
		}
	}
} // namespace

class UFOAIPluginModule
{
	_QERPluginTable m_plugin;
public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT(Name, "ufoai");

	UFOAIPluginModule()
	{
		m_plugin.m_pfnQERPlug_Init = &UFOAI::init;
		m_plugin.m_pfnQERPlug_GetName = &UFOAI::getName;
		m_plugin.m_pfnQERPlug_GetCommandList = &UFOAI::getCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = &UFOAI::getCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch = &UFOAI::dispatch;
	}
	_QERPluginTable* getTable()
	{
		return &m_plugin;
	}
};

typedef SingletonModule<UFOAIPluginModule> SingletonUFOAIPluginModule;

SingletonUFOAIPluginModule g_UFOAIPluginModule;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules(ModuleServer& server)
{
	initialiseModule(server);

	g_UFOAIPluginModule.selfRegister();
}
