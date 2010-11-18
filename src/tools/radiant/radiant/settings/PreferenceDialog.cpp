/**
 * @file preferences.cpp
 * @brief User preferences
 * @author Leonardo Zide (leo@lokigames.com)
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

#include "PreferenceDialog.h"
#include "radiant_i18n.h"

#include "iregistry.h"

#include "debugging/debugging.h"

#include "generic/callback.h"
#include "stream/stringstream.h"
#include "os/file.h"
#include "os/path.h"
#include "os/dir.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"

#include "../environment.h"
#include "../log/console.h"
#include "../mainframe.h"
#include "../map/map.h"
#include <string>

#include "stream/textfilestream.h"
#include "container/array.h"
#include "xml/ixml.h"
#include "xml/xmlparser.h"
#include "xml/xmlwriter.h"

#include "preferencedictionary.h"
#include "stringio.h"

static const char* const PREFERENCES_VERSION = "1.0";

static bool Preferences_Load (PreferenceDictionary& preferences, const std::string& filename)
{
	TextFileInputStream file(filename);
	if (!file.failed()) {
		XMLStreamParser parser(file);
		XMLPreferenceDictionaryImporter importer(preferences, PREFERENCES_VERSION);
		parser.exportXML(importer);
		return true;
	}
	return false;
}

static bool Preferences_Save (PreferenceDictionary& preferences, const std::string& filename)
{
	TextFileOutputStream file(filename);
	if (!file.failed()) {
		XMLStreamWriter writer(file);
		XMLPreferenceDictionaryExporter exporter(preferences, PREFERENCES_VERSION);
		exporter.exportXML(writer);
		return true;
	}
	return false;
}

static bool Preferences_Save_Safe (PreferenceDictionary& preferences, const std::string& filename)
{
	Array<char> tmpName(filename.c_str(), filename.c_str() + filename.length() + 1 + 3);
	*(tmpName.end() - 4) = 'T';
	*(tmpName.end() - 3) = 'M';
	*(tmpName.end() - 2) = 'P';
	*(tmpName.end() - 1) = '\0';

	return Preferences_Save(preferences, tmpName.data()) && (!file_exists(filename) || file_remove(filename))
			&& file_move(tmpName.data(), filename);
}

/**
 * Widget callback for PrefsDlg
 * @sa Preferences_Save
 */
static void OnButtonClean (GtkWidget *widget, gpointer data)
{
	// make sure this is what the user wants
	if (gtk_MessageBox(GTK_WIDGET(g_Preferences.GetWidget()),
			_("This will close UFORadiant and clean the corresponding registry entries.\n"
					"Next time you start UFORadiant it will be good as new. Do you wish to continue?"),
			_("Reset Registry"), eMB_YESNO, eMB_ICONASTERISK) == eIDYES) {
		PrefsDlg *dlg = (PrefsDlg*) data;
		dlg->EndModal(eIDCANCEL);

		g_preferences_globals.disable_ini = true;
		Preferences_Reset();
		gtk_main_quit();
	}
}

// =============================================================================
// PrefsDlg class

static void notebook_set_page (GtkWidget* notebook, GtkWidget* page)
{
	int pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), page);
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) != pagenum) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), pagenum);
	}
}

void PrefsDlg::showPrefPage (GtkWidget* prefpage)
{
	notebook_set_page(m_notebook, prefpage);
}

static void treeSelection (GtkTreeSelection* selection, gpointer data)
{
	PrefsDlg *dlg = (PrefsDlg*) data;

	GtkTreeModel* model;
	GtkTreeIter selected;
	if (gtk_tree_selection_get_selected(selection, &model, &selected)) {
		GtkWidget* prefpage;
		gtk_tree_model_get(model, &selected, 1, (gpointer*) &prefpage, -1);
		dlg->showPrefPage(prefpage);
	}
}

typedef std::list<PreferenceGroupCallback> PreferenceGroupCallbacks;

inline void PreferenceGroupCallbacks_constructGroup (const PreferenceGroupCallbacks& callbacks, PreferenceGroup& group)
{
	for (PreferenceGroupCallbacks::const_iterator i = callbacks.begin(); i != callbacks.end(); ++i) {
		(*i)(group);
	}
}

inline void PreferenceGroupCallbacks_pushBack (PreferenceGroupCallbacks& callbacks,
		const PreferenceGroupCallback& callback)
{
	callbacks.push_back(callback);
}

typedef std::list<PreferencesPageCallback> PreferencesPageCallbacks;

inline void PreferencesPageCallbacks_constructPage (const PreferencesPageCallbacks& callbacks, PrefPage* page)
{
	for (PreferencesPageCallbacks::const_iterator i = callbacks.begin(); i != callbacks.end(); ++i) {
		(*i)(page);
	}
}

inline void PreferencesPageCallbacks_pushBack (PreferencesPageCallbacks& callbacks,
		const PreferencesPageCallback& callback)
{
	callbacks.push_back(callback);
}

PreferencesPageCallbacks g_settingsPreferences;

PreferenceGroupCallbacks g_settingsCallbacks;
void PreferencesDialog_addSettingsPage (const PreferenceGroupCallback& callback)
{
	PreferenceGroupCallbacks_pushBack(g_settingsCallbacks, callback);
}

inline GtkWidget* getVBox (GtkWidget* page)
{
	return gtk_bin_get_child(GTK_BIN(page));
}

GtkTreeIter PreferenceTree_appendPage (GtkTreeStore* store, GtkTreeIter* parent, const std::string& name, GtkWidget* page)
{
	GtkTreeIter group;
	gtk_tree_store_append(store, &group, parent);
	gtk_tree_store_set(store, &group, 0, name.c_str(), 1, page, -1);
	return group;
}

GtkWidget* PreferencePages_addPage (GtkWidget* notebook, const std::string& name)
{
	GtkWidget* preflabel = gtk_label_new(name.c_str());
	gtk_widget_show(preflabel);

	GtkWidget* pageframe = gtk_frame_new(name.c_str());
	gtk_container_set_border_width(GTK_CONTAINER(pageframe), 4);
	gtk_widget_show(pageframe);

	GtkWidget* vbox = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_container_add(GTK_CONTAINER(pageframe), vbox);

	// Add the page to the notebook
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), pageframe, preflabel);

	return pageframe;
}

class PreferenceTreeGroup: public PreferenceGroup
{
		Dialog& m_dialog;
		GtkWidget* m_notebook;
		GtkTreeStore* m_store;
		GtkTreeIter m_group;
		typedef std::list<PrefPage> PrefPageList;
		PrefPageList _prefPages;
	public:
		PreferenceTreeGroup (Dialog& dialog, GtkWidget* notebook, GtkTreeStore* store, GtkTreeIter group) :
			m_dialog(dialog), m_notebook(notebook), m_store(store), m_group(group)
		{
		}
		PrefPage* createPage (const std::string& treeName, const std::string& frameName)
		{
			GtkWidget* page = PreferencePages_addPage(m_notebook, frameName);
			PreferenceTree_appendPage(m_store, &m_group, treeName, page);

			_prefPages.push_back(PrefPage(m_dialog, getVBox(page)));
			PrefPageList::iterator i = _prefPages.end();

			// Return the last item in the list, except the case there is none
			if (i != _prefPages.begin()) {
				i--;
				PrefPage* pagePtr = &(*i);
				return pagePtr;
			}
			return NULL;
		}
};

void PrefsDlg::addConstructor(PreferenceConstructor* constructor) {
	_constructors.push_back(constructor);
}

void PrefsDlg::callConstructors(PreferenceTreeGroup& preferenceGroup) {
	for (PreferenceConstructorList::iterator i = _constructors.begin(); i != _constructors.end(); i++) {
		PreferenceConstructor* constructor = *i;
		if (constructor != NULL) {
			constructor->constructPreferencePage(preferenceGroup);
		}
	}
}

GtkWindow* PrefsDlg::BuildDialog ()
{
	// Construct the main dialog window. Set a vertical default size as the
	// size_request is too small.
	GtkWindow* dialog = create_floating_window(_("UFORadiant Preferences"), m_parent);
	gtk_window_set_default_size(dialog, -1, 450);

	{
		GtkWidget* mainvbox = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(dialog), mainvbox);
		gtk_container_set_border_width(GTK_CONTAINER(mainvbox), 5);
		gtk_widget_show(mainvbox);

		{
			GtkWidget* hbox = gtk_hbox_new(FALSE, 5);
			gtk_widget_show(hbox);
			gtk_box_pack_end(GTK_BOX(mainvbox), hbox, FALSE, TRUE, 0);

			{
				GtkButton* button = create_dialog_button(_("OK"), G_CALLBACK(dialog_button_ok), &m_modal);
				gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(button), FALSE, FALSE, 0);
			}
			{
				GtkButton* button = create_dialog_button(_("Cancel"), G_CALLBACK(dialog_button_cancel), &m_modal);
				gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(button), FALSE, FALSE, 0);
			}
			{
				GtkButton* button = create_dialog_button(_("Clean"), G_CALLBACK(OnButtonClean), this);
				gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(button), FALSE, FALSE, 0);
			}
		}

		{
			GtkWidget* hbox = gtk_hbox_new(FALSE, 5);
			gtk_box_pack_start(GTK_BOX(mainvbox), hbox, TRUE, TRUE, 0);
			gtk_widget_show(hbox);

			{
				GtkWidget* sc_win = gtk_scrolled_window_new(0, 0);
				gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
				gtk_box_pack_start(GTK_BOX(hbox), sc_win, FALSE, FALSE, 0);
				gtk_widget_show(sc_win);
				gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sc_win), GTK_SHADOW_IN);

				// prefs pages notebook
				m_notebook = gtk_notebook_new();
				// hide the notebook tabs since its not supposed to look like a notebook
				gtk_notebook_set_show_tabs(GTK_NOTEBOOK(m_notebook), FALSE);
				gtk_box_pack_start(GTK_BOX(hbox), m_notebook, TRUE, TRUE, 0);
				gtk_widget_show(m_notebook);

				{
					GtkTreeStore* store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);

					GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
					gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

					{
						GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
						GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Preferences"),
								renderer, "text", 0, (char const*) 0);
						gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
					}

					{
						GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
						g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(treeSelection), this);
					}

					gtk_widget_show(view);

					gtk_container_add(GTK_CONTAINER (sc_win), view);

					{
						// Add preference tree options
						PreferencePages_addPage(m_notebook, _("Front Page"));
						{
							GtkWidget* settings = PreferencePages_addPage(m_notebook, _("General Settings"));
							{
								PrefPage preferencesPage(*this, getVBox(settings));
								PreferencesPageCallbacks_constructPage(g_settingsPreferences, &preferencesPage);
							}

							GtkTreeIter group = PreferenceTree_appendPage(store, 0, _("Settings"), settings);
							PreferenceTreeGroup preferenceGroup(*this, m_notebook, store, group);

							// greebo: Invoke the registered constructors to do their stuff
							callConstructors(preferenceGroup);

							PreferenceGroupCallbacks_constructGroup(g_settingsCallbacks, preferenceGroup);
						}
					}

					gtk_tree_view_expand_all(GTK_TREE_VIEW(view));

					g_object_unref(G_OBJECT(store));
				}
			}
		}
	}

	gtk_notebook_set_page(GTK_NOTEBOOK(m_notebook), 0);

	return dialog;
}

preferences_globals_t g_preferences_globals;

PrefsDlg g_Preferences; // global prefs instance


void PreferencesDialog_constructWindow (GtkWindow* main_window)
{
	g_Preferences.m_parent = main_window;
	g_Preferences.Create();
}
void PreferencesDialog_destroyWindow ()
{
	g_Preferences.Destroy();
}

PreferenceDictionary g_preferences;

class PreferenceSystemAPI
{
	public:
		typedef PreferenceSystem Type;
		STRING_CONSTANT(Name, "*");

		PreferenceSystemAPI ()
		{
		}
		PreferenceSystem* getTable ()
		{
			return &g_preferences;
		}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<PreferenceSystemAPI> PreferenceSystemModule;
typedef Static<PreferenceSystemModule> StaticPreferenceSystemModule;
StaticRegisterModule staticRegisterPreferenceSystem(StaticPreferenceSystemModule::instance());

#define PREFS_LOCAL_FILENAME "settings.xml"

void Preferences_Load ()
{
	std::string filename = GlobalRegistry().get(RKEY_SETTINGS_PATH) + PREFS_LOCAL_FILENAME;

	g_message("loading settings from %s\n", filename.c_str());

	if (!Preferences_Load(g_preferences, filename)) {
		g_warning("failed to load settings from %s\n", filename.c_str());
	}
}

/**
 * @sa OnButtonClean
 */
void Preferences_Save (void)
{
	// we might want to skip the ini settings due to an error
	if (g_preferences_globals.disable_ini)
		return;

	std::string filename = GlobalRegistry().get(RKEY_SETTINGS_PATH) + PREFS_LOCAL_FILENAME;
	g_message("saving settings to %s\n", filename.c_str());

	if (!Preferences_Save_Safe(g_preferences, filename)) {
		g_warning("failed to save settings to %s\n", filename.c_str());
	}
}

void Preferences_Reset (void)
{
	std::string filename = GlobalRegistry().get(RKEY_SETTINGS_PATH) + PREFS_LOCAL_FILENAME;
	file_remove(filename);
}

void PrefsDlg::PostModal (EMessageBoxReturn code)
{
	if (code == eIDOK) {
		Preferences_Save();
		// Save the values back into the registry
		_registryConnector.exportValues();
		UpdateAllWindows();
	}
}

std::vector<std::string> g_restart_required;

void PreferencesDialog_restartRequired (const std::string& staticName)
{
	g_restart_required.push_back(staticName);
}

void PreferencesDialog_showDialog ()
{
	if (GlobalMap().askForSave(_("Edit Preferences")) && g_Preferences.DoModal() == eIDOK) {
		if (!g_restart_required.empty()) {
			std::string message = _("Preference changes require a restart:\n");
			for (std::vector<std::string>::iterator i = g_restart_required.begin(); i != g_restart_required.end(); ++i) {
				message += "<b>" + (*i) + "</b>\n";
			}
			gtkutil::infoDialog(message);
			g_restart_required.clear();
		}
	}
}
