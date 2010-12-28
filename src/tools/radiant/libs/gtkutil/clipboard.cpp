/**
 * @file clipboard.cpp
 * @brief Platform-independent GTK clipboard support.
 * @todo Using GDK_SELECTION_CLIPBOARD fails on win32, so we use the win32 API directly for now.
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

#include "clipboard.h"

#include "stream/memstream.h"
#include "stream/textstream.h"
#include "string/string.h"

#include <gtk/gtk.h>

#define RADIANT_CLIPPINGS 23

static gchar clipboard_target[] = "RADIANT_CLIPPINGS";
static const GtkTargetEntry clipboard_targets = { clipboard_target, 0, RADIANT_CLIPPINGS };
/**
 * @sa Map_ExportSelected
 * @sa Map_ImportSelected
 */
static ClipboardPasteFunc g_clipboardPasteFunc = 0;

static void clipboard_get (GtkClipboard *clipboard, GtkSelectionData *selection_data, guint info, gpointer data)
{
	std::size_t len = *reinterpret_cast<std::size_t*> (data);
	const char* buffer = (len != 0) ? reinterpret_cast<const char*> (data) + sizeof(std::size_t) : 0;

	GdkAtom type = GDK_NONE;
	if (info == clipboard_targets.info) {
		type = gdk_atom_intern(clipboard_targets.target, FALSE);
	}

	gtk_selection_data_set(selection_data, type, 8, reinterpret_cast<const guchar*> (buffer), static_cast<gint> (len));
}

static void clipboard_clear (GtkClipboard *clipboard, gpointer data)
{
	delete[] reinterpret_cast<const char*> (data);
}

static void clipboard_received (GtkClipboard *clipboard, GtkSelectionData *data, gpointer user_data)
{
	if (data->length < 0) {
		globalErrorStream() << "Error retrieving selection\n";
	} else if (string_equal(gdk_atom_name(data->type), clipboard_targets.target)) {
		BufferInputStream istream(reinterpret_cast<const char*> (data->data), data->length);
		(*reinterpret_cast<ClipboardPasteFunc*> (user_data))(istream);
	}
}

/**
 * @sa clipboard_paste
 */
void clipboard_copy (ClipboardCopyFunc copy)
{
	GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	BufferOutputStream ostream;
	copy(ostream);
	std::size_t length = ostream.size();
	char* data = new char[length + sizeof(std::size_t)];
	*reinterpret_cast<std::size_t*> (data) = length;
	memcpy(data + sizeof(std::size_t), ostream.data(), length);

	gtk_clipboard_set_with_data(clipboard, &clipboard_targets, 1, clipboard_get, clipboard_clear, data);
}

/**
 * @sa clipboard_copy
 */
void clipboard_paste (ClipboardPasteFunc paste)
{
	GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	g_clipboardPasteFunc = paste;
	gtk_clipboard_request_contents(clipboard, gdk_atom_intern(clipboard_targets.target, FALSE), clipboard_received,
			&g_clipboardPasteFunc);
}
