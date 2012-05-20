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

#include "image.h"

#include <gtk/gtk.h>

#include "string/string.h"
#include "stream/stringstream.h"
#include "stream/textstream.h"

namespace
{
	std::string g_bitmapsPath;
}

void BitmapsPath_set (const std::string& path)
{
	g_bitmapsPath = path;
}

GdkPixbuf* pixbuf_new_from_file_with_mask (const std::string& path)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path.c_str(), &error);
	if (!pixbuf) {
		g_warning("image could not get loaded: '%s' %s\n",
				path.c_str(), (error != (GError *) 0) ? error->message : "");
		if (error)
			g_error_free(error);
		return (GdkPixbuf *)0;
	} else {
		GdkPixbuf* rgba = gdk_pixbuf_add_alpha(pixbuf, TRUE, 255, 0, 255);
		g_object_unref(pixbuf);
		return rgba;
	}
}

namespace gtkutil
{
	GtkWidget* getImage (const std::string& fileName)
	{
		GtkWidget* image = gtk_image_new_from_pixbuf(gtkutil::getLocalPixbuf(fileName));
		if (!image) {
			image = gtk_image_new_from_stock(GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_SMALL_TOOLBAR);
			if (!image) {
				g_warning("stock image could not get loaded\n");
			}
		}
		return image;
	}

	// Return a GdkPixbuf from a local image
	GdkPixbuf* getLocalPixbuf (const std::string& fileName)
	{
		if (fileName.empty())
			return NULL;

		const std::string path = g_bitmapsPath + fileName;
		GError *error = NULL;
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path.c_str(), &error);
		if (pixbuf == NULL) {
			g_warning("image could not get loaded: '%s' %s\n",
					path.c_str(), (error != (GError *) 0) ? error->message : "");
			if (error)
				g_error_free(error);
		}
		return pixbuf;
	}

	GdkPixbuf* getLocalPixbufWithMask (const std::string& fileName)
	{
		if (fileName.empty())
			return NULL;

		return pixbuf_new_from_file_with_mask(g_bitmapsPath + fileName);
	}
} // namespace gtkutil
