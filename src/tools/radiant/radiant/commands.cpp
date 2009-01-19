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
#include "radiant.h"

#include "debugging/debugging.h"

#include <map>
#include "string/string.h"
#include "versionlib.h"
#include "gtkutil/accelerator.h"
#include "preferences.h"
#include "stringio.h"

typedef std::pair<Accelerator, bool> ShortcutValue; // accelerator, isRegistered
typedef std::map<CopiedString, ShortcutValue> Shortcuts;

static Shortcuts g_shortcuts;

const Accelerator& GlobalShortcuts_insert (const char* name, const Accelerator& accelerator)
{
	return (*g_shortcuts.insert(Shortcuts::value_type(name, ShortcutValue(accelerator, false))).first).second.first;
}

void GlobalShortcuts_foreach (CommandVisitor& visitor)
{
	for (Shortcuts::iterator i = g_shortcuts.begin(); i != g_shortcuts.end(); ++i) {
		visitor.visit((*i).first.c_str(), (*i).second.first);
	}
}

void GlobalShortcuts_register (const char* name)
{
	Shortcuts::iterator i = g_shortcuts.find(name);
	if (i != g_shortcuts.end()) {
		(*i).second.second = true;
	}
}

void GlobalShortcuts_reportUnregistered (void)
{
	for (Shortcuts::iterator i = g_shortcuts.begin(); i != g_shortcuts.end(); ++i) {
		if ((*i).second.first.key != 0 && !(*i).second.second) {
			g_warning("Shortcut not registered: '%s'\n", (*i).first.c_str());
		}
	}
}

typedef std::map<CopiedString, Command> Commands;

static Commands g_commands;

void GlobalCommands_insert (const char* name, const Callback& callback, const Accelerator& accelerator)
{
	bool added = g_commands.insert(Commands::value_type(name, Command(callback, GlobalShortcuts_insert(name, accelerator)))).second;
	ASSERT_MESSAGE(added, "command already registered: " << makeQuoted(name));
}

const Command& GlobalCommands_find (const char* command)
{
	Commands::iterator i = g_commands.find(command);
	ASSERT_MESSAGE(i != g_commands.end(), "failed to lookup command " << makeQuoted(command));
	return (*i).second;
}

typedef std::map<CopiedString, Toggle> Toggles;


static Toggles g_toggles;

void GlobalToggles_insert (const char* name, const Callback& callback, const BoolExportCallback& exportCallback, const Accelerator& accelerator)
{
	bool added = g_toggles.insert(Toggles::value_type(name, Toggle(callback, GlobalShortcuts_insert(name, accelerator), exportCallback))).second;
	ASSERT_MESSAGE(added, "toggle already registered: " << makeQuoted(name));
}

const Toggle& GlobalToggles_find (const char* name)
{
	Toggles::iterator i = g_toggles.find(name);
	ASSERT_MESSAGE(i != g_toggles.end(), "failed to lookup toggle " << makeQuoted(name));
	return (*i).second;
}

typedef std::map<CopiedString, KeyEvent> KeyEvents;


static KeyEvents g_keyEvents;

void GlobalKeyEvents_insert (const char* name, const Accelerator& accelerator, const Callback& keyDown, const Callback& keyUp)
{
	bool added = g_keyEvents.insert(KeyEvents::value_type(name, KeyEvent(GlobalShortcuts_insert(name, accelerator), keyDown, keyUp))).second;
	ASSERT_MESSAGE(added, "command already registered: " << makeQuoted(name));
}

const KeyEvent& GlobalKeyEvents_find (const char* name)
{
	KeyEvents::iterator i = g_keyEvents.find(name);
	ASSERT_MESSAGE(i != g_keyEvents.end(), "failed to lookup keyEvent " << makeQuoted(name));
	return (*i).second;
}




#include <cctype>

#include <gtk/gtk.h>

#include "gtkutil/dialog.h"
#include "mainframe.h"

#include "stream/textfilestream.h"
#include "stream/stringstream.h"

static inline const char* stringrange_find (const char* first, const char* last, char c)
{
	const char* p = strchr(first, '+');
	if (p == 0) {
		return last;
	}
	return p;
}

static void keyShortcutEdited (GtkCellRendererText *renderer, gchar* path, gchar *newText, GtkTreeView *treeview)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
		char *name;
		gtk_tree_model_get(model, &iter, 0, &name, -1);
		globalOutputStream() << "name: "<< name<<"\n";
		Shortcuts::iterator i = g_shortcuts.find(name);
		if (i != g_shortcuts.end()) {
			Accelerator& accelerator = (*i).second.first;
			if (string_empty(newText)) {
				// remove binding
				accelerator.key = 0;
				accelerator.modifiers = (GdkModifierType)0;
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

				accelerator.modifiers = (GdkModifierType)modifiers;
			}
		}
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, newText, -1);
	}

	SaveCommandMap(SettingsPath_get());
	LoadCommandMap(SettingsPath_get());
}

struct command_list_dialog_t : public ModalDialog {
	command_list_dialog_t()
			: m_close_button(*this, eIDCANCEL) {
	}
	ModalDialogButton m_close_button;
};

void DoCommandListDlg (void)
{
	command_list_dialog_t dialog;

	GtkWindow* window = create_modal_dialog_window(MainFrame_getWindow(), "Mapped Commands", dialog, -1, 400);

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group(window, accel);

	GtkHBox* hbox = create_dialog_hbox(4, 4);
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(hbox));

	{
		GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(scr), TRUE, TRUE, 0);

		{
			GtkListStore* store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

			GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Command"), renderer, "text", 0, (char const*)0);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
			}

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Key"), renderer, "text", 1, (char const*)0);
				g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, (char const*)0);
				g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(keyShortcutEdited), (gpointer)view);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
			}

			gtk_widget_show(view);
			gtk_container_add(GTK_CONTAINER (scr), view);

			for (Shortcuts::iterator i = g_shortcuts.begin(); i != g_shortcuts.end(); ++i) {
				GtkTreeIter iter;
				StringOutputStream modifiers;
				modifiers << (*i).second.first;
				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter, 0, (*i).first.c_str(), 1, modifiers.c_str(), -1);
			}

			g_object_unref(G_OBJECT(store));
		}
	}

	GtkVBox* vbox = create_dialog_vbox(4);
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), FALSE, FALSE, 0);
	{
		GtkButton* button = create_modal_dialog_button(_("Close"), dialog.m_close_button);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
	}

	modal_dialog_show(window, dialog);
	gtk_widget_destroy(GTK_WIDGET(window));
}

#include "profile/profile.h"

static const char* const COMMANDS_VERSION = "1.0";

/**
 * @sa LoadCommandMap
 */
void SaveCommandMap(const char* path) {
	StringOutputStream strINI(256);
	strINI << path << "shortcuts.ini";

	TextFileOutputStream file(strINI.c_str());
	if (!file.failed()) {
		file << "[Version]\n";
		file << "number=" << COMMANDS_VERSION << "\n";
		file << "\n";
		file << "[Commands]\n";
		class WriteCommandMap : public CommandVisitor {
			TextFileOutputStream& m_file;
		public:
			WriteCommandMap(TextFileOutputStream& file) : m_file(file) {
			}
			void visit(const char* name, Accelerator& accelerator) {
				m_file << name << "=";

				const char* key = global_keys_find(accelerator.key);
				if (!string_empty(key)) {
					m_file << key;
				}

				if (accelerator.modifiers & GDK_MOD1_MASK) {
					m_file << "+Alt";
				}
				if (accelerator.modifiers & GDK_CONTROL_MASK) {
					m_file << "+Ctrl";
				}
				if (accelerator.modifiers & GDK_SHIFT_MASK) {
					m_file << "+Shift";
				}

				m_file << "\n";
			}
		} visitor(file);
		GlobalShortcuts_foreach(visitor);
	}
}

class ReadCommandMap : public CommandVisitor {
	const char* m_filename;
	std::size_t m_count;
public:
	ReadCommandMap(const char* filename) : m_filename(filename), m_count(0) {
	}
	void visit(const char* name, Accelerator& accelerator) {
		char value[1024];
		if (read_var(m_filename, "Commands", name, value )) {
			if (string_empty(value)) {
				accelerator.key = 0;
				accelerator.modifiers = (GdkModifierType)0;
				return;
			}
			int modifiers = 0;
			const char* last = value + string_length(value);
			const char* keyEnd = stringrange_find(value, last, '+');
			for (const char* modifier = keyEnd; modifier != last;) {
				const char* next = stringrange_find(modifier + 1, last, '+');
				if (next - modifier == 4
				        && string_equal_nocase_n(modifier, "+alt", 4)) {
					modifiers |= GDK_MOD1_MASK;
				} else if (next - modifier == 5
				           && string_equal_nocase_n(modifier, "+ctrl", 5) != 0) {
					modifiers |= GDK_CONTROL_MASK;
				} else if (next - modifier == 6
				           && string_equal_nocase_n(modifier, "+shift", 6) != 0) {
					modifiers |= GDK_SHIFT_MASK;
				} else {
					g_warning("failed to parse user command '%s': unknown modifier '%s'\n", value, modifier);
				}
				modifier = next;
			}
			accelerator.modifiers = (GdkModifierType)modifiers;

			CopiedString keyName(StringRange(value, keyEnd));
			accelerator.key = global_keys_find(keyName.c_str());
			if (accelerator.key != 0) {
				++m_count;
			} else {
				g_warning("Failed to parse user command '%s': unknown key '%s'\n", value, keyName.c_str());
				// modifier only bindings are not allowed
				accelerator.modifiers = (GdkModifierType)0;
			}
			accelerator.key = gdk_keyval_from_name(CopiedString(StringRange(value, keyEnd)).c_str());
			if (accelerator.key == GDK_VoidSymbol)
				accelerator.key = 0;
		}
	}
	std::size_t count() const {
		return m_count;
	}
};

/**
 * @sa SaveCommandMap
 */
void LoadCommandMap (const char* path)
{
	StringOutputStream strINI(256);
	strINI << path << "shortcuts.ini";

	FILE* f = fopen (strINI.c_str(), "r");
	if (f != 0) {
		fclose(f);
		globalOutputStream() << "loading custom shortcuts list from " << makeQuoted(strINI.c_str()) << "\n";

		Version version = version_parse(COMMANDS_VERSION);
		Version dataVersion = { 0, 0 };

		{
			char value[1024];
			if (read_var(strINI.c_str(), "Version", "number", value)) {
				dataVersion = version_parse(value);
			}
		}

		if (version_compatible(version, dataVersion)) {
			globalOutputStream() << "commands import: data version " << dataVersion << " is compatible with code version " << version << "\n";
			ReadCommandMap visitor(strINI.c_str());
			GlobalShortcuts_foreach(visitor);
			g_message("parsed %u custom shortcuts\n", Unsigned(visitor.count()));
		} else {
			globalWarningStream() << "commands import: data version " << dataVersion << " is not compatible with code version " << version << "\n";
		}
	} else {
		g_warning("failed to load custom shortcuts from '%s'\n", strINI.c_str());
	}
}
