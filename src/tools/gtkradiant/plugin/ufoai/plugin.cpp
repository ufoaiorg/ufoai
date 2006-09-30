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

#include "plugin.h"

#if 0
#include "ifilter.h"
#include "filters.h"
#endif

#include "debugging/debugging.h"
#include "iplugin.h"
#include "string/string.h"
#include "modulesystem/singletonmodule.h"
#include <gtk/gtk.h>
#include "gtkutil/messagebox.h"
#include "gtkutil/filechooser.h"
#include "iscenegraph.h" // declaration of datastructure of the map
#include "scenelib.h"    // declaration of datastructure of the map
#include "qerplugin.h"   // declaration to use other interfaces as a plugin
#include "iundo.h"       // declaration of undo system
#include "ientity.h"     // declaration of entity system
#include "string/string.h"

#define VERSION "0.1"

void about_plugin_window(void);
void WorldSpawnSettings(void);
void initFilters();

#ifdef __linux__
// linux itoa implementation
char* itoa( int value, char* result, int base )
{
	// check that the base if valid
	if (base < 2 || base > 16)
	{
	  *result = 0;
	  return result;
	}

	char* out = result;
	int quotient = value;

	do
	{
		*out = "0123456789abcdef"[abs(quotient % base)];
		++out;

		quotient /= base;
	} while (quotient);

	// Only apply negative sign for base 10
	if( value < 0 && base == 10)
	  *out++ = '-';

	std::reverse(result, out);

	*out = 0;
	return result;
}
#endif

//  **************************
// ** GTK callback functions **
//  **************************

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	/* If you return FALSE in the "delete_event" signal handler,
	* GTK will emit the "destroy" signal. Returning TRUE means
	* you don't want the window to be destroyed.
	* This is useful for popping up 'are you sure you want to quit?'
	* type dialogs. */

	return FALSE;
}

// destroy widget if destroy signal is passed to widget
static void destroy(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(widget);
}

// function for close button to destroy the toplevel widget
static void close_window(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(gtk_widget_get_toplevel(widget));
}

#if 0
class filter_face_level : public UfoAIFilter
{
	int m_contents;
	public:
	filter_face_level(int contents) : m_contents(contents) {
	}
	bool filter(const Face& face) const {
		return (face.getShader().m_flags.m_contentFlags & m_contents) == 0;
	}
};
#endif

namespace UfoAI
{
	GtkWindow* main_window = NULL;
	Entity *theWorldspawn = NULL;
	int maxlevel = 8;

	const char* init(void* hApp, void* pMainWidget) {
		main_window = (GtkWindow*)pMainWidget;
		ASSERT_NOTNULL(main_window);
		initFilters();
		return "UFO:AI Filters";
	}
	const char* getName() {
		return "UFO:AI Filters";
	}
	const char* getCommandList() {
		return "StepOn,-,Level8,Level7,Level6,Level5,Level4,Level3,Level2,Level1,-,Map properties,-,About...";
	}
	const char* getCommandTitleList() {
		return "";
	}
/*	void add_ufoai_filter(EntityFilter& filter, int mask, bool invert) {
		GlobalFilterSystem().addFilter(g_entityFilters.back(), mask);
	}*/
	void dispatch(const char* command, float* vMin, float* vMax, bool bSingleBrush) {
		globalOutputStream() << "UFO:Alien Invasion Plugin\n";
		if(string_equal(command, "Map properties")) {
			globalOutputStream() << "Worldspawn properties\n";
			WorldSpawnSettings();
		} else if(string_equal(command, "Level1")) {
			globalOutputStream() << "Show only level 1\n";
		} else if(string_equal(command, "Level2")) {
			globalOutputStream() << "Show only level 2\n";
		} else if(string_equal(command, "Level3")) {
			globalOutputStream() << "Show only level 3\n";
		} else if(string_equal(command, "Level4")) {
			globalOutputStream() << "Show only level 4\n";
		} else if(string_equal(command, "Level5")) {
			globalOutputStream() << "Show only level 5\n";
		} else if(string_equal(command, "Level6")) {
			globalOutputStream() << "Show only level 6\n";
		} else if(string_equal(command, "Level7")) {
			globalOutputStream() << "Show only level 7\n";
		} else if(string_equal(command, "Level8")) {
			globalOutputStream() << "Show only level 8\n";
		} else if(string_equal(command, "StepOn")) {
			globalOutputStream() << "Hide all stepons\n";
		} else if(string_equal(command, "About...")) {
			about_plugin_window();
		}
    }
} // namespace
/*
class UfoAIDependencies :
  public GlobalFilterModuleRef
{
};
*/
class UfoAIPluginModule
{
	_QERPluginTable m_plugin;
	public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT(Name, "UFO:AI Plugin");

	UfoAIPluginModule() {
		m_plugin.m_pfnQERPlug_Init = &UfoAI::init;
		m_plugin.m_pfnQERPlug_GetName = &UfoAI::getName;
		m_plugin.m_pfnQERPlug_GetCommandList = &UfoAI::getCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = &UfoAI::getCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch = &UfoAI::dispatch;
	}
	_QERPluginTable* getTable() {
		return &m_plugin;
	}
};

typedef SingletonModule<UfoAIPluginModule> SingletonUfoAIPluginModule;

SingletonUfoAIPluginModule g_UfoAIPluginModule;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules(ModuleServer& server)
{
	initialiseModule(server);

	g_UfoAIPluginModule.selfRegister();
}


//  **************************
// ** find entities by class **  from radiant/map.cpp
//  **************************
class EntityFindByClassname : public scene::Graph::Walker
{
	const char* m_name;
	Entity*& m_entity;
	public:
	EntityFindByClassname(const char* name, Entity*& entity) : m_name(name), m_entity(entity) {
		m_entity = 0;
	}
	bool pre(const scene::Path& path, scene::Instance& instance) const {
		if(m_entity == 0) {
			Entity* entity = Node_getEntity(path.top());
			if(entity != 0 && string_equal(m_name, entity->getKeyValue("classname"))) {
				m_entity = entity;
			}
		}
		return true;
	}
};

Entity* Scene_FindEntityByClass(const char* name)
{
	Entity* entity;
	GlobalSceneGraph().traverse(EntityFindByClassname(name, entity));
	return entity;
}


// About dialog
void about_plugin_window()
{
	GtkWidget *window, *vbox, *label, *button;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL); // create a window
	gtk_window_set_transient_for(GTK_WINDOW(window), UfoAI::main_window); // make the window to stay in front of the main window
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL); // connect the delete event
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL); // connect the destroy event for the window
	gtk_window_set_title(GTK_WINDOW(window), "About UFO:AI Plugin"); // set the title of the window for the window
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE); // don't let the user resize the window
	gtk_window_set_modal(GTK_WINDOW(window), TRUE); // force the user not to do something with the other windows
	gtk_container_set_border_width(GTK_CONTAINER(window), 10); // set the border of the window

	vbox = gtk_vbox_new(FALSE, 10); // create a box to arrange new objects vertically
	gtk_container_add(GTK_CONTAINER(window), vbox); // add the box to the window

	label = gtk_label_new("UFO:AI Plugin v"VERSION" for GtkRadiant 1.5\nby Martin Gerhardy"); // create a label
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT); // text align left
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2); // insert the label in the box

	button = gtk_button_new_with_label("OK"); // create a button with text
	g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK (gtk_widget_destroy), window); // connect the click event to close the window
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 2); // insert the button in the box

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER); // center the window on screen

	gtk_widget_show_all(window); // show the window and all subelements
}

// write the values of the Spinner-Boxes to the worldspawn
static void set_maxlevel(GtkWidget *widget, gpointer data)
{
	//Str str_min, str_max;
	char buffer[10];

	strcat(buffer, itoa(UfoAI::maxlevel, buffer, 10));
	UndoableCommand undo("UfoAI.entitySetMaxlevel");
	UfoAI::theWorldspawn->setKeyValue("maxlevel", buffer);

	close_window(widget, NULL);
}

// callback function to assign the optimal maxlevel value
static void set_optimal_maxlevel(GtkWidget *widget, gpointer data)
{
}

// MapCoordinator dialog window
void WorldSpawnSettings(void)
{
	GtkWidget *window, *vbox, *label, *button;
	const char *buffer;

	// in any case we need a window to show the user what to do
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL); // create the window
	gtk_window_set_transient_for(GTK_WINDOW(window), UfoAI::main_window); // make the window to stay in front of the main window
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL); // connect the delete event for the window
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL); // connect the destroy event for the window
	gtk_window_set_title(GTK_WINDOW(window), "UFO:AI Worlspawn settings"); // set the title of the window for the window
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE); // don't let the user resize the window
	gtk_window_set_modal(GTK_WINDOW(window), TRUE); // force the user not to do something with the other windows
	gtk_container_set_border_width(GTK_CONTAINER(window), 10); // set the border of the window

	vbox = gtk_vbox_new(FALSE, 10); // create a box to arrange new objects vertically
	gtk_container_add(GTK_CONTAINER(window), vbox); // add the box to the window

	UfoAI::theWorldspawn = Scene_FindEntityByClass("worldspawn"); // find the entity worldspawn
	if (UfoAI::theWorldspawn != NULL) { // need to have a worldspawn otherwise setting a value crashes the radiant
		globalOutputStream() << "UfoAI: worldspawn found!\n"; // output error to console

		buffer = UfoAI::theWorldspawn->getKeyValue("maxlevel"); // upper left corner
		if (strlen(buffer) == 1) {
			UfoAI::maxlevel = atoi(buffer);
		}

		button = gtk_button_new_with_label("Set optimal maxlevel"); // create button with text
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(set_optimal_maxlevel), NULL); // connect button with callback function
		gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 2); // insert button into vbox
		gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 2); // insert separator into vbox

		button = gtk_button_new_with_label("Cancel"); // create a button with text
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_window), NULL); // connect the click event to close the window
		gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 2); // insert the button in the box
	} else {
		globalOutputStream() << "UfoAI: no worldspawn found!\n"; // output error to console

		label = gtk_label_new("ERROR: No worldspawn was found in the map!\nIn order to use this tool the map must have at least one brush in the worldspawn. "); // create a label
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT); // text align left
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2); // insert the label in the box

		button = gtk_button_new_with_label("OK"); // create a button with text
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_window), NULL); // connect the click event to close the window
		gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 2); // insert the button in the box
	}

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER); // center the window
	gtk_widget_show_all(window); // show the window and all subelements
}

#include "filters.h"

class filter_ufoai : public UfoAIFilter
{
	const char* m_classname;
	public:
	filter_ufoai(const char* classname) : m_classname(classname)
	{
	}
	bool filter(const Entity& entity) const
	{
		return string_equal(entity.getKeyValue("classname"), m_classname);
	}
};

filter_ufoai g_filter_level1("level1");

void initFilters()
{
	add_ufoai_filter(g_filter_level1, CONTENTS_LEVEL8);
}

