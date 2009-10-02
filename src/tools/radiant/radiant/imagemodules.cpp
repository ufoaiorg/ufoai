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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "ifilesystem.h"
#include "imagelib.h"
#include "iimage.h"
#include "modulesystem/singletonmodule.h"

class ImageDependencies: public GlobalFileSystemModuleRef
{
};

typedef unsigned char byte;

static Image* LoadImage (ArchiveFile& file, const char *extension)
{
	RGBAImage* image = (RGBAImage *) 0;

	/* load the buffer from pk3 or filesystem */
	ScopedArchiveBuffer buffer(file);

	GdkPixbufLoader *loader = gdk_pixbuf_loader_new_with_type(extension, (GError**) 0);

	if (gdk_pixbuf_loader_write(loader, (const guchar *) buffer.buffer, static_cast<gsize> (buffer.length),
			(GError**) 0)) {
		int pos = 0;
		GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
		const int width = gdk_pixbuf_get_width(pixbuf);
		const int height = gdk_pixbuf_get_height(pixbuf);
		const gboolean hasAlpha = gdk_pixbuf_get_has_alpha(pixbuf);
		const int stepWidth = gdk_pixbuf_get_n_channels(pixbuf);
		const guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

		image = new RGBAImage(width, height);
		byte *rgba = image->getRGBAPixels();
		const int rowextra = gdk_pixbuf_get_rowstride(pixbuf) - width * stepWidth;

		for (int y = 0; y < height; ++y, pixels += rowextra) {
			for (int x = 0; x < width; ++x) {
				rgba[pos++] = *(pixels++);
				rgba[pos++] = *(pixels++);
				rgba[pos++] = *(pixels++);
				rgba[pos++] = hasAlpha ? *(pixels++) : 255;
			}
		}

		g_object_unref(pixbuf);
	}

	gdk_pixbuf_loader_close(loader, (GError**) 0);

	return image;
}

static Image* LoadJPG (ArchiveFile& file)
{
	return LoadImage(file, "jpeg");
}

static Image* LoadTGA (ArchiveFile& file)
{
	return LoadImage(file, "tga");
}

static Image* LoadPNG (ArchiveFile& file)
{
	return LoadImage(file, "png");
}

class ImageTGAAPI
{
		_QERPlugImageTable m_imagetga;
	public:
		typedef _QERPlugImageTable Type;
		STRING_CONSTANT(Name, "tga");

		ImageTGAAPI ()
		{
			m_imagetga.loadImage = LoadTGA;
		}
		_QERPlugImageTable* getTable ()
		{
			return &m_imagetga;
		}
};

typedef SingletonModule<ImageTGAAPI> ImageTGAModule;

typedef Static<ImageTGAModule> StaticImageTGAModule;

class ImageJPGAPI
{
		_QERPlugImageTable m_imagejpg;
	public:
		typedef _QERPlugImageTable Type;
		STRING_CONSTANT(Name, "jpg");

		ImageJPGAPI ()
		{
			m_imagejpg.loadImage = LoadJPG;
		}
		_QERPlugImageTable* getTable ()
		{
			return &m_imagejpg;
		}
};

typedef SingletonModule<ImageJPGAPI, ImageDependencies> ImageJPGModule;

typedef Static<ImageJPGModule> StaticImageJPGModule;

class ImagePNGAPI
{
		_QERPlugImageTable m_imagepng;
	public:
		typedef _QERPlugImageTable Type;
		STRING_CONSTANT(Name, "png");

		ImagePNGAPI ()
		{
			m_imagepng.loadImage = LoadPNG;
		}
		_QERPlugImageTable* getTable ()
		{
			return &m_imagepng;
		}
};

typedef SingletonModule<ImagePNGAPI> ImagePNGModule;

typedef Static<ImagePNGModule> StaticImagePNGModule;

StaticRegisterModule staticRegisterImageJPG(StaticImageJPGModule::instance());
StaticRegisterModule staticRegisterImageTGA(StaticImageTGAModule::instance());
StaticRegisterModule staticRegisterImagePNG(StaticImagePNGModule::instance());
