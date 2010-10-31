/**
 * @file lastused.cpp
 * @brief Handles last used files
 * @todo I think there are gtk built-in functions for this
 */

/*
 Copyright(C) 1999-2006 Id Software, Inc. and contributors.
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

#include "lastused.h"
#include "radiant_i18n.h"

#include <string.h>
#include <stdio.h>
#include <gtk/gtklabel.h>

#include "os/file.h"
#include "generic/callback.h"
#include "stream/stringstream.h"
#include "convert.h"
#include "ieventmanager.h"

#include "gtkmisc.h"
#include "gtkutil/menu.h"
#include "gtkutil/IConv.h"
#include "map/map.h"
#include "qe3.h"

#define MRU_MAX 4
namespace
{
	GtkMenuItem *MRU_items[MRU_MAX];
	std::size_t MRU_used;
	typedef std::string MRU_filename_t;
	MRU_filename_t MRU_filenames[MRU_MAX];
	typedef std::string MRU_key_t;
	MRU_key_t MRU_keys[MRU_MAX] = { "File0", "File1", "File2", "File3" };
}

static inline const char* MRU_GetText (std::size_t index)
{
	return MRU_filenames[index].c_str();
}

static void MRU_updateWidget (std::size_t index, const std::string& filename)
{
	const std::string mnemonic = "_" + string::toString(index + 1) + " - " + string::replaceAll(gtkutil::IConv::localeToUTF8(filename), "_", "__");
	gtk_label_set_text_with_mnemonic(GTK_LABEL(gtk_bin_get_child(GTK_BIN(MRU_items[index]))), mnemonic.c_str());
}

static void MRU_SetText (std::size_t index, const std::string&filename)
{
	MRU_filenames[index] = filename;
	MRU_updateWidget(index, filename);
}

void MRU_AddFile (const std::string& str)
{
	std::size_t i;

	// check if file is already in our list
	for (i = 0; i < MRU_used; i++) {
		const std::string text = MRU_GetText(i);

		if (text == str) {
			// reorder menu
			for (; i > 0; i--)
				MRU_SetText(i, MRU_GetText(i - 1));

			MRU_SetText(0, str);

			return;
		}
	}

	if (MRU_used < MRU_MAX)
		MRU_used++;

	// move items down
	for (i = MRU_used - 1; i > 0; i--)
		MRU_SetText(i, MRU_GetText(i - 1));

	MRU_SetText(0, str);
	gtk_widget_set_sensitive(GTK_WIDGET(MRU_items[0]), TRUE);
	gtk_widget_show(GTK_WIDGET(MRU_items[MRU_used - 1]));
}

static void MRU_Init (void)
{
	if (MRU_used > MRU_MAX)
		MRU_used = MRU_MAX;
}

static void MRU_AddWidget (GtkMenuItem *widget, std::size_t pos)
{
	if (pos < MRU_MAX) {
		MRU_items[pos] = widget;
		if (pos < MRU_used) {
			MRU_updateWidget(pos, MRU_GetText(pos));
			gtk_widget_set_sensitive(GTK_WIDGET(MRU_items[0]), TRUE);
			gtk_widget_show(GTK_WIDGET(MRU_items[pos]));
		}
	}
}

static void MRU_Activate (std::size_t index)
{
	std::string text = MRU_GetText(index);
	bool success = false;

	if (file_readable(text)) {
		success = Map_ChangeMap("",text);
	}
	if (!success) {
		MRU_used--;

		for (std::size_t i = index; i < MRU_used; i++)
			MRU_SetText(i, MRU_GetText(i + 1));

		if (MRU_used == 0) {
			gtk_label_set_text(GTK_LABEL(GTK_BIN(MRU_items[0])->child), _("Recent Files"));
			gtk_widget_set_sensitive(GTK_WIDGET(MRU_items[0]), FALSE);
		} else {
			gtk_widget_hide(GTK_WIDGET(MRU_items[MRU_used]));
		}
	}
}

class LoadMRU
{
		std::size_t m_number;
	public:
		LoadMRU (std::size_t number) :
			m_number(number)
		{
		}
		void load ()
		{
			if (ConfirmModified(_("Open Map"))) {
				MRU_Activate(m_number - 1);
			}
		}
};

typedef MemberCaller<LoadMRU, &LoadMRU::load> LoadMRUCaller;

static LoadMRU g_load_mru1(1);
static LoadMRU g_load_mru2(2);
static LoadMRU g_load_mru3(3);
static LoadMRU g_load_mru4(4);

void MRU_constructMenu (GtkMenu* menu)
{
	GlobalEventManager().addCommand("LoadMRU1", LoadMRUCaller(g_load_mru1));
	GlobalEventManager().addCommand("LoadMRU2", LoadMRUCaller(g_load_mru2));
	GlobalEventManager().addCommand("LoadMRU3", LoadMRUCaller(g_load_mru3));
	GlobalEventManager().addCommand("LoadMRU4", LoadMRUCaller(g_load_mru4));
	{
		GtkMenuItem* item = createMenuItemWithMnemonic(menu, "1", "LoadMRU1");
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
		MRU_AddWidget(item, 0);
	}
	{
		GtkMenuItem* item = createMenuItemWithMnemonic(menu, "2", "LoadMRU2");
		gtk_widget_hide(GTK_WIDGET(item));
		MRU_AddWidget(item, 1);
	}
	{
		GtkMenuItem* item = createMenuItemWithMnemonic(menu, "3", "LoadMRU3");
		gtk_widget_hide(GTK_WIDGET(item));
		MRU_AddWidget(item, 2);
	}
	{
		GtkMenuItem* item = createMenuItemWithMnemonic(menu, "4", "LoadMRU4");
		gtk_widget_hide(GTK_WIDGET(item));
		MRU_AddWidget(item, 3);
	}
}

#include "preferencesystem.h"
#include "stringio.h"

void MRU_Construct (void)
{
	GlobalPreferenceSystem().registerPreference(C_("Last used files count", "Count"), SizeImportStringCaller(MRU_used),
			SizeExportStringCaller(MRU_used));

	for (std::size_t i = 0; i != MRU_MAX; ++i) {
		GlobalPreferenceSystem().registerPreference(MRU_keys[i], StringImportStringCaller(MRU_filenames[i]),
				StringExportStringCaller(MRU_filenames[i]));
	}

	MRU_Init();
}

void MRU_Destroy (void)
{
}
