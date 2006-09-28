/**
 * @file support.c
 * @brief
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "support.h"

/**
 * @brief
 * @param[in] widget
 * @param[in] widget_name
 */
GtkWidget* lookup_widget (GtkWidget *widget, const gchar *widget_name)
{
	GtkWidget *parent, *found_widget;

	for (;;) {
		if (GTK_IS_MENU (widget))
			parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
		else
			parent = widget->parent;
		if (!parent)
			parent = (GtkWidget*) g_object_get_data (G_OBJECT (widget), "GladeParentKey");
		if (parent == NULL)
			break;
		widget = parent;
	}

	found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget), widget_name);
	if (!found_widget)
		g_warning ("Widget not found: %s", widget_name);
	return found_widget;
}

static GList *pixmaps_directories = NULL;

/**
 * @brief
 * @param[in] directory
 * @note Use this function to set the directory containing installed pixmaps.
 */
void add_pixmap_directory (const gchar *directory)
{
	pixmaps_directories = g_list_prepend (pixmaps_directories, g_strdup (directory));
}

/**
 * @brief
 * @param[in] filename
 * @note This is an internally used function to find pixmap files.
 */
static gchar* find_pixmap_file (const gchar *filename)
{
	GList *elem;

	/* We step through each of the pixmaps directory to find it. */
	elem = pixmaps_directories;
	while (elem) {
		gchar *pathname = g_strdup_printf ("%s%s%s", (gchar*)elem->data,
											G_DIR_SEPARATOR_S, filename);
		if (g_file_test (pathname, G_FILE_TEST_EXISTS))
			return pathname;
		g_free (pathname);
		elem = elem->next;
	}
	return NULL;
}

/**
 * @brief
 * @param[in] widget
 * @param[in] filename
 * @note This is an internally used function to create pixmaps.
 */
GtkWidget* create_pixmap (GtkWidget *widget, const gchar *filename)
{
	gchar *pathname = NULL;
	GtkWidget *pixmap;

	if (!filename || !filename[0])
		return gtk_image_new ();

	pathname = find_pixmap_file (filename);

	if (!pathname) {
		g_warning (_("Couldn't find pixmap file: %s"), filename);
		return gtk_image_new ();
	}

	pixmap = gtk_image_new_from_file (pathname);
	g_free (pathname);
	return pixmap;
}

/**
 * @brief
 * @param[in] filename
 * @note This is an internally used function to create pixmaps.
 */
GdkPixbuf* create_pixbuf (const gchar *filename)
{
	gchar *pathname = NULL;
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	if (!filename || !filename[0])
		return NULL;

	pathname = find_pixmap_file (filename);

	if (!pathname) {
		g_warning (_("Couldn't find pixmap file: %s"), filename);
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
	if (!pixbuf) {
		fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
				pathname, error->message);
		g_error_free (error);
	}
	g_free (pathname);
	return pixbuf;
}

/**
 * @brief
 * @param[in] action
 * @param[in] action_name
 * @param[in] description
 * @note This is used to set ATK action descriptions.
 */
void glade_set_atk_action_description (AtkAction *action, const gchar *action_name, const gchar *description)
{
	gint n_actions, i;

	n_actions = atk_action_get_n_actions (action);
	for (i = 0; i < n_actions; i++) {
		if (!strcmp (atk_action_get_name (action, i), action_name))
			atk_action_set_description (action, i, description);
	}
}

/**
 * @brief
 * @param[in] filename
 */
int file_exists (char *filename)
{
#ifdef _WIN32
	return (_access(filename, 00) == 0);
#else
	return (access(filename, R_OK) == 0);
#endif
}

/**
 * @brief
 * @param[in] filename
 * @param[in] fileext
 */
int file_ext (char *filename, char *fileext)
{
	if ( strstr(filename, fileext))
		return 1;
	/* no match */
	return 0;
}

/**
 * @brief
 * @param[in] in
 * @param[in] out
 */
void file_strip_ext (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/**
 * @brief
 * @param[in] format
 */
char *va(char *format, ...)
{
	va_list argptr;
	static char string[2048];
	va_start(argptr, format);
	vsprintf(string, format, argptr);
	va_end(argptr);
	return string;
}

/**
 * @brief
 * @param[in] value
 */
char *bool_translate( int value )
{
	if ( value )
		return "true";
	return "false";
}

/**
 * @brief
 */
int file_length(FILE * f)
{
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

/**
 * @brief
 */
void fatal_error (char* errormessage)
{
	fprintf(stderr, "%s\n", errormessage);
	gtk_exit(0);
}

static byte* globalFileBuffer = NULL;

/**
 * @brief
 * @sa file_load
 */
void file_close(void)
{
	free(globalFileBuffer);
	globalFileBuffer = NULL;
}

#define	MAX_READ	0x10000		/* read in blocks of 64k */
/**
 * @brief
 * @sa file_close
 */
int file_load(char* filename, void** buffer)
{
	FILE *f;
	byte *buf = NULL;
	int len, block, remaining, read, tries;

	if (globalFileBuffer)
		fatal_error("still another buffer active\n");

	f = fopen(filename, "rb");
	if (!f) {
		if (buffer)
			*buffer = NULL;
		fprintf(stderr, "could not load %s\n", filename);
		return -1;
	}

	if (!buffer) {
		fprintf(stderr, "no buffer for %s\n", filename);
		fclose(f);
		return -1;
	}

	buf = (byte *) buffer;

	len = file_length(f);
	if (!len) {
		fclose(f);
		fprintf(stderr, "file %s is empty\n", filename);
		return -1;
	}

	buf = (byte*)malloc(sizeof(byte)*(len + 1));
	if (!buf)
		fatal_error("could not allocate memory\n");
	globalFileBuffer = buf;
	*buffer = buf;

	/* read in chunks for progress bar */
	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread(buf, 1, block, f);
		if (read == 0) {
			/* we might have been trying to read from a CD */
			if (!tries) {
				tries = 1;
			} else {
				fatal_error("file_load: 0 bytes read");
			}
		}

		if (read == -1)
			fatal_error("file_load: -1 bytes read");

		remaining -= read;
		buf += read;
	}

	if ((int)buf > (int)&buf[len])
		fatal_error("overflow\n");

	buf[len] = 0;

	fclose(f);

	return len;
}
