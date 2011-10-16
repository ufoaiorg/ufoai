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

#include "console.h"
#include "radiant_i18n.h"

#include "iregistry.h"

#include <iostream>
#include <time.h>

#include "gtkutil/messagebox.h"
#include "gtkutil/container.h"
#include "gtkutil/IConv.h"
#include "gtkutil/dialog.h"
#include "stream/stringstream.h"
#include "os/file.h"

#include "../environment.h"
#include "version.h"

// handle to the console log file
namespace
{
	FILE* g_hLogFile;
}

/**
 * @note called whenever we need to open/close/check the console log file
 */
void Sys_LogFile (bool enable)
{
	if (enable && !g_hLogFile) {
		// settings say we should be logging and we don't have a log file .. so create it
		// open a file to log the console (if user prefs say so)
		// the file handle is g_hLogFile
		// the log file is erased
		std::string logFileName = GlobalRegistry().get(RKEY_SETTINGS_PATH) + "radiant.log";
		g_hLogFile = fopen(logFileName.c_str(), "w");
		if (g_hLogFile != 0) {
			globalOutputStream() << "Started logging to " << logFileName << "\n";
			time_t localtime;
			time(&localtime);
			globalOutputStream() << "Today is: " << ctime(&localtime) << "\n";
			globalOutputStream()
					<< "This is UFORadiant '" RADIANT_VERSION "' compiled " __DATE__ "\n";
		} else {
			gtkutil::errorDialog(_("Failed to create log file, check write permissions in UFORadiant directory.\n"));
		}
	} else if (!enable && g_hLogFile != 0) {
		// settings say we should not be logging but still we have an active logfile .. close it
		time_t localtime;
		time(&localtime);
		g_message("Closing log file at %s\n", ctime(&localtime));
		fclose(g_hLogFile);
		g_hLogFile = NULL;
	}
}

static GtkWidget* g_console = NULL;

static void console_clear ()
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_console));
	gtk_text_buffer_set_text(buffer, "", -1);
}

static void console_populate_popup (GtkTextView* textview, GtkMenu* menu, gpointer user_data)
{
	GtkWidget* item = gtk_menu_item_new_with_label("Clear");
	g_signal_connect(G_OBJECT (item), "activate", G_CALLBACK(console_clear), 0);
	gtk_widget_show(item);
	container_add_widget(GTK_CONTAINER(menu), item);
}

static gboolean destroy_set_null (GtkWindow* widget, GtkWidget** p)
{
	*p = NULL;
	return FALSE;
}

GtkWidget* Console_constructWindow ()
{
	GtkWidget* scr = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);
	gtk_widget_show(scr);

	{
		GtkWidget* text = gtk_text_view_new();
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
		gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
		gtk_container_add(GTK_CONTAINER (scr), text);
		gtk_widget_show(text);
		g_console = text;

		g_signal_connect(G_OBJECT(g_console), "populate-popup", G_CALLBACK(console_populate_popup), 0);
		g_signal_connect(G_OBJECT(g_console), "destroy", G_CALLBACK(destroy_set_null), &g_console);
	}

	gtk_container_set_focus_chain(GTK_CONTAINER(scr), NULL);

	return scr;
}

class GtkTextBufferOutputStream: public TextOutputStream
{
		GtkTextBuffer* textBuffer;
		GtkTextIter* iter;
		GtkTextTag* tag;
	public:
		GtkTextBufferOutputStream (GtkTextBuffer* textBuffer, GtkTextIter* iter, GtkTextTag* tag) :
			textBuffer(textBuffer), iter(iter), tag(tag)
		{
		}
		std::size_t write (const char* buffer, std::size_t length)
		{
			gtk_text_buffer_insert_with_tags(textBuffer, iter, buffer, gint(length), tag, (char const*) 0);
			return length;
		}
};

std::size_t Sys_Print (int level, const char* buf, std::size_t length)
{
	const std::string str = std::string(buf, length);
	bool contains_newline = std::find(buf, buf + length, '\n') != buf + length;

	if (level == SYS_ERR) {
		Sys_LogFile(true);
		std::cerr << str;
	} else if (level == SYS_WRN) {
		std::cerr << str;
	} else {
		std::cout << str;
	}

	if (g_hLogFile != 0) {
		file_write(buf, length, g_hLogFile);
		if (contains_newline) {
			fflush(g_hLogFile);
		}
	}

	if (level != SYS_NOCON) {
		if (g_console != 0) {
			GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_console));

			GtkTextIter iter;
			gtk_text_buffer_get_end_iter(buffer, &iter);

			const GdkColor yellow = { 0, 0xb0ff, 0xb0ff, 0x0000 };
			const GdkColor red = { 0, 0xffff, 0x0000, 0x0000 };
			const GdkColor black = { 0, 0x0000, 0x0000, 0x0000 };

			static GtkTextTag* error_tag = gtk_text_buffer_create_tag(buffer, "red_foreground", "foreground-gdk", &red,
					0);
			static GtkTextTag* warning_tag = gtk_text_buffer_create_tag(buffer, "yellow_foreground", "foreground-gdk",
					&yellow, 0);
			static GtkTextTag* standard_tag = gtk_text_buffer_create_tag(buffer, "black_foreground", "foreground-gdk",
					&black, 0);
			GtkTextTag* tag;
			switch (level) {
			case SYS_WRN:
				tag = warning_tag;
				break;
			case SYS_ERR:
				tag = error_tag;
				break;
			case SYS_STD:
			case SYS_VRB:
			default:
				tag = standard_tag;
				break;
			}

			{
				GtkTextBufferOutputStream textBuffer(buffer, &iter, tag);
				if (!gtkutil::IConv::isUTF8()) {
					BufferedTextOutputStream<GtkTextBufferOutputStream> buffered(textBuffer);
					buffered << gtkutil::IConv::localeToUTF8(str);
				} else {
					textBuffer << str;
				}
			}
		}
	}
	return length;
}

class SysPrintOutputStream: public TextOutputStream
{
	public:
		std::size_t write (const char* buffer, std::size_t length)
		{
			return Sys_Print(SYS_STD, buffer, length);
		}
};

class SysPrintErrorStream: public TextOutputStream
{
	public:
		std::size_t write (const char* buffer, std::size_t length)
		{
			return Sys_Print(SYS_ERR, buffer, length);
		}
};

class SysPrintWarningStream: public TextOutputStream
{
	public:
		std::size_t write (const char* buffer, std::size_t length)
		{
			return Sys_Print(SYS_WRN, buffer, length);
		}
};

static SysPrintOutputStream g_outputStream;

TextOutputStream& getSysPrintOutputStream ()
{
	return g_outputStream;
}

static SysPrintErrorStream g_errorStream;

TextOutputStream& getSysPrintErrorStream ()
{
	return g_errorStream;
}

static SysPrintWarningStream g_warningStream;

TextOutputStream& getSysPrintWarningStream ()
{
	return g_warningStream;
}
