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

#include "commands.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include <map>
#include "string/string.h"
#include "versionlib.h"
#include "gtkutil/accelerator.h"
#include "gtkutil/messagebox.h"
#include <gtk/gtktreeselection.h>
#include <gtk/gtkbutton.h>
#include "gtkmisc.h"
#include "iradiant.h"
#include "settings/preferences.h"
#include "stringio.h"
#include "os/file.h"

typedef std::pair<Accelerator, int> ShortcutValue; // accelerator, isRegistered
typedef std::map<std::string, ShortcutValue> Shortcuts;

static Shortcuts g_shortcuts;

const Accelerator& GlobalShortcuts_insert (const std::string& name, const Accelerator& accelerator)
{
	return (*g_shortcuts.insert(Shortcuts::value_type(name, ShortcutValue(accelerator, false))).first).second.first;
}

void GlobalShortcuts_foreach (CommandVisitor& visitor)
{
	for (Shortcuts::iterator i = g_shortcuts.begin(); i != g_shortcuts.end(); ++i) {
		visitor.visit((*i).first, (*i).second.first);
	}
}

void GlobalShortcuts_register (const std::string& name, int type)
{
	Shortcuts::iterator i = g_shortcuts.find(name);
	if (i != g_shortcuts.end()) {
		(*i).second.second = type;
	}
}

void GlobalShortcuts_reportUnregistered (void)
{
	for (Shortcuts::iterator i = g_shortcuts.begin(); i != g_shortcuts.end(); ++i) {
		if ((*i).second.first.key != 0 && !(*i).second.second) {
			g_message("Shortcut not registered: '%s'\n", (*i).first.c_str());
		}
	}
}

typedef std::map<std::string, Command> Commands;

static Commands g_commands;

void GlobalCommands_insert (const std::string& name, const Callback& callback, const Accelerator& accelerator)
{
	bool added = g_commands.insert(Commands::value_type(name, Command(callback, GlobalShortcuts_insert(name,
			accelerator)))).second;
	ASSERT_MESSAGE(added, "command already registered: " << makeQuoted(name));
}

const Command& GlobalCommands_find (const std::string& command)
{
	Commands::iterator i = g_commands.find(command);
	ASSERT_MESSAGE(i != g_commands.end(), "failed to lookup command " << makeQuoted(command));
	return (*i).second;
}

typedef std::map<std::string, Toggle> Toggles;

static Toggles g_toggles;

void GlobalToggles_insert (const std::string& name, const Callback& callback, const BoolExportCallback& exportCallback,
		const Accelerator& accelerator)
{
	bool added = g_toggles.insert(Toggles::value_type(name, Toggle(callback, GlobalShortcuts_insert(name, accelerator),
			exportCallback))).second;
	ASSERT_MESSAGE(added, "toggle already registered: " << makeQuoted(name));
}

const Toggle& GlobalToggles_find (const std::string& name)
{
	Toggles::iterator i = g_toggles.find(name);
	ASSERT_MESSAGE(i != g_toggles.end(), "failed to lookup toggle " << makeQuoted(name));
	return (*i).second;
}

typedef std::map<std::string, KeyEvent> KeyEvents;

static KeyEvents g_keyEvents;

void GlobalKeyEvents_insert (const std::string& name, const Accelerator& accelerator, const Callback& keyDown,
		const Callback& keyUp)
{
	bool added = g_keyEvents.insert(KeyEvents::value_type(name, KeyEvent(GlobalShortcuts_insert(name, accelerator),
			keyDown, keyUp))).second;
	ASSERT_MESSAGE(added, "command already registered: " << makeQuoted(name));
}

const KeyEvent& GlobalKeyEvents_find (const std::string& name)
{
	KeyEvents::iterator i = g_keyEvents.find(name);
	ASSERT_MESSAGE(i != g_keyEvents.end(), "failed to lookup keyEvent " << makeQuoted(name));
	return (*i).second;
}

void disconnect_accelerator (const std::string& name)
{
	Shortcuts::iterator i = g_shortcuts.find(name);
	if (i != g_shortcuts.end()) {
		switch ((*i).second.second) {
		case 1:
			// command
			command_disconnect_accelerator(name);
			break;
		case 2:
			// toggle
			toggle_remove_accelerator(name);
			break;
		}
	}
}

void connect_accelerator (const std::string& name)
{
	Shortcuts::iterator i = g_shortcuts.find(name);
	if (i != g_shortcuts.end()) {
		switch ((*i).second.second) {
		case 1:
			// command
			command_connect_accelerator(name);
			break;
		case 2:
			// toggle
			toggle_add_accelerator(name);
			break;
		}
	}
}

#include <cctype>

#include <gtk/gtk.h>

#include "gtkutil/dialog.h"
#include "mainframe.h"

#include "stream/textfilestream.h"
#include "stream/stringstream.h"

namespace
{
	enum
	{
		CMDLIST_COMMAND, CMDLIST_SHORTCUT, CMDLIST_WEIGHTSET, CMDLIST_WEIGHT, CMDLIST_STORE_SIZE
	};
}
static void keyShortcutEdited (GtkCellRendererText *renderer, gchar* path, gchar *newText, GtkTreeView *treeview)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
		char *name;
		gtk_tree_model_get(model, &iter, 0, &name, -1);
		globalOutputStream() << "name: " << name << "\n";
		Shortcuts::iterator i = g_shortcuts.find(name);
		if (i != g_shortcuts.end()) {
			Accelerator& accelerator = (*i).second.first;
			if (string_empty(newText)) {
				// remove binding
				accelerator.key = 0;
				accelerator.modifiers = (GdkModifierType) 0;
			} else {
				int modifiers = 0;
				StringTokeniser outputTokeniser(newText, " +");
				do {
					const char *key = outputTokeniser.getToken();
					if (string_empty(key))
						break;
					if (!strcmp(key, "Alt"))
						modifiers |= GDK_MOD1_MASK;
					else if (!strcmp(key, "Shift"))
						modifiers |= GDK_SHIFT_MASK;
					else if (!strcmp(key, "Control"))
						modifiers |= GDK_CONTROL_MASK;
					else {
						// only one key
						if (strlen(key) == 1)
							accelerator.key = std::toupper(key[0]);
						else
							accelerator.key = global_keys_find(key);
						break;
					}
				} while (1);

				// modifiers only is not allowed
				if (!accelerator.key)
					modifiers = 0;

				accelerator.modifiers = (GdkModifierType) modifiers;
			}
		}
		GtkListStore *store = GTK_LIST_STORE(gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model)));
		GtkTreeIter child;
		gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &child, &iter);
		gtk_list_store_set(GTK_LIST_STORE(store), &child, CMDLIST_SHORTCUT, newText, -1);
	}

	SaveCommandMap(SettingsPath_get());
	LoadCommandMap(SettingsPath_get());
}

struct command_list_dialog_t: public ModalDialog
{
		command_list_dialog_t () :
			m_close_button(*this, eIDCANCEL), m_list(NULL), m_command_iter(), m_model(NULL), m_waiting_for_key(false)
		{
		}
		ModalDialogButton m_close_button;
		GtkTreeView *m_list;
		GtkTreeIter m_command_iter;
		GtkTreeModel *m_model;
		bool m_waiting_for_key;
};

void accelerator_clear_button_clicked (GtkButton *btn, gpointer dialogptr)
{
	command_list_dialog_t &dialog = *(command_list_dialog_t *) dialogptr;

	if (dialog.m_waiting_for_key) {
		// just unhighlight, user wanted to cancel
		dialog.m_waiting_for_key = false;
		gtk_list_store_set(GTK_LIST_STORE(dialog.m_model), &dialog.m_command_iter, CMDLIST_WEIGHTSET, false, -1);
		gtk_widget_set_sensitive(GTK_WIDGET(dialog.m_list), true);
		dialog.m_model = NULL;
		return;
	}

	GtkTreeSelection *sel = gtk_tree_view_get_selection(dialog.m_list);
	GtkTreeModel *model;
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	GValue val;
	memset(&val, 0, sizeof(val));
	gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CMDLIST_COMMAND, &val);
	const char *commandName = g_value_get_string(&val);

	// clear the ACTUAL accelerator too!
	disconnect_accelerator(commandName);

	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find(commandName);
	if (thisShortcutIterator == g_shortcuts.end())
		return;
	thisShortcutIterator->second.first = accelerator_null();

	GtkListStore *store = GTK_LIST_STORE(gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model)));
	GtkTreeIter child;
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &child, &iter);
	gtk_list_store_set(GTK_LIST_STORE(store), &child, CMDLIST_SHORTCUT, "", -1);

	g_value_unset(&val);
}

void accelerator_edit_button_clicked (GtkButton *btn, gpointer dialogptr)
{
	command_list_dialog_t &dialog = *(command_list_dialog_t *) dialogptr;

	// 1. find selected row
	GtkTreeSelection *sel = gtk_tree_view_get_selection(dialog.m_list);
	GtkTreeModel *model;
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;
	GtkTreeIter child;
	gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &child, &iter);

	dialog.m_command_iter = child;
	dialog.m_model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));

	// 2. disallow changing the row
	//gtk_widget_set_sensitive(GTK_WIDGET(dialog.m_list), false);

	// 3. highlight the row
	gtk_list_store_set(GTK_LIST_STORE(dialog.m_model), &child, CMDLIST_WEIGHTSET, true, -1);

	// 4. grab keyboard focus
	dialog.m_waiting_for_key = true;
}

gboolean accelerator_window_key_press (GtkWidget *widget, GdkEventKey *event, gpointer dialogptr)
{
	command_list_dialog_t &dialog = *(command_list_dialog_t *) dialogptr;

	if (!dialog.m_waiting_for_key)
		return false;

#if 0
	if (event->is_modifier)
	return false;
#else
	switch (event->keyval) {
	case GDK_Shift_L:
	case GDK_Shift_R:
	case GDK_Control_L:
	case GDK_Control_R:
	case GDK_Caps_Lock:
	case GDK_Shift_Lock:
	case GDK_Meta_L:
	case GDK_Meta_R:
	case GDK_Alt_L:
	case GDK_Alt_R:
	case GDK_Super_L:
	case GDK_Super_R:
	case GDK_Hyper_L:
	case GDK_Hyper_R:
		return false;
	}
#endif

	dialog.m_waiting_for_key = false;

	// 7. find the name of the accelerator
	GValue val;
	memset(&val, 0, sizeof(val));
	gtk_tree_model_get_value(GTK_TREE_MODEL(dialog.m_model), &dialog.m_command_iter, CMDLIST_COMMAND, &val);
	const char *commandName = g_value_get_string(&val);
	;
	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find(commandName);
	if (thisShortcutIterator == g_shortcuts.end()) {
		gtk_list_store_set(GTK_LIST_STORE(dialog.m_model), &dialog.m_command_iter, CMDLIST_WEIGHTSET, false, -1);
		gtk_widget_set_sensitive(GTK_WIDGET(dialog.m_list), true);
		return true;
	}

	// 8. build an Accelerator
	Accelerator newAccel(event->keyval, (GdkModifierType) event->state);

	// 8. verify the key is still free, show a dialog to ask what to do if not
	class VerifyAcceleratorNotTaken: public CommandVisitor
	{
			const std::string& commandName;
			const Accelerator &newAccel;
			GtkWidget *widget;
			GtkTreeModel *model;
		public:
			bool allow;
			VerifyAcceleratorNotTaken (const std::string& name, const Accelerator &accelerator, GtkWidget *w,
					GtkTreeModel *m) :
				commandName(name), newAccel(accelerator), widget(w), model(m), allow(true)
			{
			}
			void visit (const std::string& name, Accelerator& accelerator)
			{
				if (name == commandName)
					return;
				if (!allow)
					return;
				if (accelerator.key == 0)
					return;
				if (accelerator == newAccel) {
					StringOutputStream msg;
					msg << "The command " << name << " is already assigned to the key " << accelerator << ".\n\n"
							<< "Do you want to unassign " << name << " first?";
					EMessageBoxReturn r = gtk_MessageBox(widget, msg.toString(), _("Key already used"), eMB_YESNOCANCEL);
					if (r == eIDYES) {
						// clear the ACTUAL accelerator too!
						disconnect_accelerator(name);
						// delete the modifier
						accelerator = accelerator_null();
						// empty the cell of the key binds dialog
						GtkTreeIter i;
						if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &i)) {
							for (;;) {
								GValue val;
								memset(&val, 0, sizeof(val));
								gtk_tree_model_get_value(GTK_TREE_MODEL(model), &i, CMDLIST_COMMAND, &val);
								const char *thisName = g_value_get_string(&val);
								if (!name.compare(thisName))
									gtk_list_store_set(GTK_LIST_STORE(model), &i, CMDLIST_SHORTCUT, "", -1);
								g_value_unset(&val);
								if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &i))
									break;
							}
						}
					} else if (r == eIDCANCEL) {
						// aborted
						allow = false;
					}
				}
			}
	} verify_visitor(commandName, newAccel, widget, dialog.m_model);
	GlobalShortcuts_foreach(verify_visitor);
	gtk_list_store_set(GTK_LIST_STORE(dialog.m_model), &dialog.m_command_iter, CMDLIST_WEIGHTSET, false, -1);
	gtk_widget_set_sensitive(GTK_WIDGET(dialog.m_list), true);

	if (verify_visitor.allow) {
		// clear the ACTUAL accelerator first
		disconnect_accelerator(commandName);

		thisShortcutIterator->second.first = newAccel;

		// write into the cell
		StringOutputStream modifiers;
		modifiers << newAccel;
		gtk_list_store_set(GTK_LIST_STORE(dialog.m_model), &dialog.m_command_iter, CMDLIST_SHORTCUT, modifiers.c_str(),
				-1);

		// set the ACTUAL accelerator too!
		connect_accelerator(commandName);
	}

	g_value_unset(&val);

	dialog.m_model = NULL;

	return true;
}

namespace
{
	static GtkTreeModelFilter *g_filterModel = 0;
	static GtkEntry *g_searchEntry = 0;
}

/**
 * @brief filter implementation that uses actual search entry content to decide whether an entry should be included.
 * Comparing is case-insensitive for both command name and command shortcut
 * @param model data model for dialog
 * @param iter current entry to decide whether it should be filtered
 * @return @q true if entry should be shown
 */
static gboolean CommandListDlg_FilterCommands (GtkTreeModel *model, GtkTreeIter *iter)
{
	if (!g_searchEntry)
		return true;
#if GTK_CHECK_VERSION(2,14,0)
	if (gtk_entry_get_text_length(g_searchEntry) == 0)
		return true;
#else
	if (strlen(gtk_entry_get_text(g_searchEntry)) == 0)
	return true;
#endif
	char* searchText = const_cast<char*> (gtk_entry_get_text(g_searchEntry));
	char *currentEntry, *currentShortcut;
	gtk_tree_model_get(model, iter, CMDLIST_COMMAND, &currentEntry, CMDLIST_SHORTCUT, &currentShortcut, -1);
	return (string_contains_nocase(currentEntry, searchText) != 0
			|| string_contains_nocase(currentShortcut, searchText) != 0);
}

/**
 * @brief Callback for 'changed' event of search entry used to reinit file filter
 * @return false
 */
static gboolean CommandListDlg_Refilter (void)
{
	gtk_tree_model_filter_refilter(g_filterModel);
	return false;
}

void DoCommandListDlg (void)
{
	command_list_dialog_t dialog;

	GtkWindow* window = create_modal_dialog_window(GlobalRadiant().getMainWindow(), _("Mapped Commands"), dialog, -1,
			400);
	g_signal_connect(G_OBJECT(window), "key-press-event", (GCallback) accelerator_window_key_press, &dialog);

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group(window, accel);

	GtkHBox* hbox = create_dialog_hbox(4, 4);
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(hbox));

	{
		GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(scr), TRUE, TRUE, 0);

		{
			GtkListStore* store = gtk_list_store_new(CMDLIST_STORE_SIZE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN,
					G_TYPE_INT);

			GtkTreeModel *filteredStore = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
			GtkWidget* view = gtk_tree_view_new_with_model(filteredStore);
			dialog.m_list = GTK_TREE_VIEW(view);
			g_filterModel = GTK_TREE_MODEL_FILTER(filteredStore);
			gtk_tree_model_filter_set_visible_func(g_filterModel,
					(GtkTreeModelFilterVisibleFunc) CommandListDlg_FilterCommands, NULL, NULL);

			gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), true); // annoying

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Command"), renderer, "text",
						CMDLIST_COMMAND, "weight-set", CMDLIST_WEIGHTSET, "weight", CMDLIST_WEIGHT, NULL);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
			}

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Key"), renderer, "text",
						CMDLIST_SHORTCUT, "weight-set", CMDLIST_WEIGHTSET, "weight", CMDLIST_WEIGHT, NULL);
				g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, (char const*) 0);
				g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(keyShortcutEdited), (gpointer)view);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
			}

			gtk_widget_show(view);
			gtk_container_add(GTK_CONTAINER (scr), view);

			class BuildCommandlist: public CommandVisitor
			{
					GtkListStore* m_store;
				public:
					BuildCommandlist (GtkListStore * store) :
						m_store(store)
					{
					}
					void visit (const std::string& name, Accelerator& accelerator)
					{
						StringOutputStream modifiers;
						modifiers << accelerator;

						{
							GtkTreeIter iter;
							gtk_list_store_append(m_store, &iter);
							gtk_list_store_set(m_store, &iter, CMDLIST_COMMAND, name.c_str(), CMDLIST_SHORTCUT,
									modifiers.c_str(), CMDLIST_WEIGHTSET, false, CMDLIST_WEIGHT, 800, -1);
						}
					}

			} visitor(store);
			GlobalShortcuts_foreach(visitor);

			g_object_unref(G_OBJECT(store));
		}
	}

	GtkVBox* vbox = create_dialog_vbox(4);
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);
	{
		GtkButton* editbutton = create_dialog_button(_("Edit"), (GCallback) accelerator_edit_button_clicked, &dialog);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(editbutton), FALSE, FALSE, 0);

		GtkButton* clearbutton =
				create_dialog_button(_("Clear"), (GCallback) accelerator_clear_button_clicked, &dialog);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(clearbutton), FALSE, FALSE, 0);

		GtkWidget *spacer = gtk_image_new();
		gtk_widget_show(spacer);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(spacer), TRUE, TRUE, 0);

		GtkWidget *searchEntry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(vbox), searchEntry, FALSE, FALSE, 0);
		gtk_widget_show(searchEntry);

#if GTK_CHECK_VERSION(2,10,0)
		gtk_tree_view_set_search_entry(dialog.m_list, GTK_ENTRY(searchEntry));
#endif
		g_signal_connect(G_OBJECT(searchEntry), "changed", G_CALLBACK(CommandListDlg_Refilter), NULL);
		g_searchEntry = GTK_ENTRY(searchEntry);

		GtkButton* button = create_modal_dialog_button(_("Close"), dialog.m_close_button);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
		widget_make_default(GTK_WIDGET(button));
		gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Return, (GdkModifierType) 0,
				(GtkAccelFlags) 0);
		gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Escape, (GdkModifierType) 0,
				(GtkAccelFlags) 0);
	}
	modal_dialog_show(window, dialog);
	gtk_widget_destroy(GTK_WIDGET(window));
	g_filterModel = 0;
	g_searchEntry = 0;
}

#include "profile/profile.h"

static const char* const COMMANDS_VERSION = "1.0.gtk-accelnames";//"1.0-gtk-accelnames"

/**
 * @sa LoadCommandMap
 */
void SaveCommandMap (const std::string& path)
{
	std::string strINI = path + "shortcuts.ini";

	TextFileOutputStream file(strINI);
	if (!file.failed()) {
		file << "[Version]\n";
		file << "number=" << COMMANDS_VERSION << "\n";
		file << "\n";
		file << "[Commands]\n";
		class WriteCommandMap: public CommandVisitor
		{
				TextFileOutputStream& m_file;
			public:
				WriteCommandMap (TextFileOutputStream& file) :
					m_file(file)
				{
				}
				void visit (const std::string& name, Accelerator& accelerator)
				{
					m_file << name << "=";

					const char* key = gtk_accelerator_name(accelerator.key, accelerator.modifiers);
					m_file << key;

					m_file << "\n";
				}
		} visitor(file);
		GlobalShortcuts_foreach(visitor);
	}
}

class ReadCommandMap: public CommandVisitor
{
		const std::string& m_filename;
		std::size_t m_count;
	public:
		ReadCommandMap (const std::string& filename) :
			m_filename(filename), m_count(0)
		{
		}
		void visit (const std::string& name, Accelerator& accelerator)
		{
			char value[1024];
			if (read_var(m_filename, "Commands", name.c_str(), value)) {
				if (string_empty(value)) {
					accelerator.key = 0;
					accelerator.modifiers = (GdkModifierType) 0;
					return;
				}
				gtk_accelerator_parse(value, &accelerator.key, &accelerator.modifiers);
				accelerator = accelerator; // fix modifiers

				if (accelerator.key != 0) {
					++m_count;
				} else {
					globalOutputStream() << "WARNING: failed to parse user command " << makeQuoted(name)
							<< ": unknown key " << makeQuoted(value) << "\n";
				}
			}
		}
		std::size_t count () const
		{
			return m_count;
		}
};

/**
 * @sa SaveCommandMap
 */
void LoadCommandMap (const std::string& path)
{
	std::string strINI = path + "shortcuts.ini";

	if (file_exists(strINI)) {
		g_message("loading custom shortcuts list from %s\n", strINI.c_str());

		Version version = version_parse(COMMANDS_VERSION);
		Version dataVersion = { 0, 0 };

		{
			char value[1024];
			if (read_var(strINI, "Version", "number", value)) {
				dataVersion = version_parse(value);
			}
		}

		if (version_compatible(version, dataVersion)) {
			g_message("commands import: data version %i:%i is compatible with code version %i:%i\n", version.major,
					version.minor, dataVersion.major, dataVersion.minor);
			ReadCommandMap visitor(strINI);
			GlobalShortcuts_foreach(visitor);
			g_message("parsed %u custom shortcuts\n", Unsigned(visitor.count()));
		} else {
			g_message("commands import: data version %i:%i is not compatible with code version %i:%i\n", version.major,
					version.minor, dataVersion.major, dataVersion.minor);
		}
	} else {
		g_warning("failed to load custom shortcuts from '%s'\n", strINI.c_str());
	}
}
