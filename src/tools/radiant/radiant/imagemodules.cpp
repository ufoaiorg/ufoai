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

/* greebo: This loads a file from the disk using GDKPixbuf routines.
 *
 * The image is loaded and its alpha channel is set uniformly to 1, the
 * according Image class is instantiated and the pointer is returned.
 *
 * Note: returns NULL if the file could not be loaded.
 */

static Image* LoadImageGDK (ArchiveFile& file)
{
	// Allocate a new GdkPixBuf and create an alpha-channel with alpha=1.0
	GdkPixbuf* rawPixbuf = gdk_pixbuf_new_from_file(file.getName().c_str(), NULL);

	// Only create an alpha channel if the other rawPixbuf could be loaded
	GdkPixbuf* img = (rawPixbuf != NULL) ? gdk_pixbuf_add_alpha(rawPixbuf, TRUE, 255, 0, 255) : NULL;

	if (img != NULL) {
		// Allocate a new image
		RGBAImage* image = new RGBAImage(gdk_pixbuf_get_width(img), gdk_pixbuf_get_height(img), false);

		// Initialise the source buffer pointers
		guchar* gdkStart = gdk_pixbuf_get_pixels(img);
		int rowstride = gdk_pixbuf_get_rowstride(img);
		int numChannels = gdk_pixbuf_get_n_channels(img);

		// Set the target buffer pointer to the first RGBAPixel
		RGBAPixel* targetPixel = image->pixels;

		// Now do an unelegant cycle over all the pixels and move them into the target
		for (unsigned int y = 0; y < image->height; y++) {
			for (unsigned int x = 0; x < image->width; x++) {
				guchar* gdkPixel = gdkStart + y * rowstride + x * numChannels;

				// Copy the values from the GdkPixel
				targetPixel->red = gdkPixel[0];
				targetPixel->green = gdkPixel[1];
				targetPixel->blue = gdkPixel[2];
				targetPixel->alpha = gdkPixel[3];
				if (targetPixel->alpha != 255)
					image->setHasAlpha(true);

				// Increase the pointer
				targetPixel++;
			}
		}

		// Free the GdkPixbufs from the memory
		g_object_unref(G_OBJECT(img));
		g_object_unref(G_OBJECT(rawPixbuf));

		return image;
	}

	// No image could be loaded, return NULL
	return NULL;
}

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

		image = new RGBAImage(width, height, false);
		byte *rgba = image->getRGBAPixels();
		const int rowextra = gdk_pixbuf_get_rowstride(pixbuf) - width * stepWidth;

		for (int y = 0; y < height; ++y, pixels += rowextra) {
			for (int x = 0; x < width; ++x) {
				rgba[pos++] = *(pixels++);
				rgba[pos++] = *(pixels++);
				rgba[pos++] = *(pixels++);
				if (hasAlpha && *pixels != 255)
					image->setHasAlpha(true);
				rgba[pos++] = hasAlpha ? *(pixels++) : 255;
			}
		}

		g_object_unref(pixbuf);
	}

	gdk_pixbuf_loader_close(loader, (GError**) 0);

	return image;
}

class ImageTGAAPI: public IImageModule
{
	public:

		typedef IImageModule Type;
		STRING_CONSTANT(Name, "tga");

		IImageModule* getTable ()
		{
			return this;
		}

	public:

		Image* loadImage (ArchiveFile& file) const
		{
			return LoadImage(file, "tga");
		}
};

typedef SingletonModule<ImageTGAAPI> ImageTGAModule;
typedef Static<ImageTGAModule> StaticImageTGAModule;

class ImageJPGAPI: public IImageModule
{
	public:

		typedef IImageModule Type;
		STRING_CONSTANT(Name, "jpg");

		IImageModule* getTable ()
		{
			return this;
		}

	public:

		Image* loadImage (ArchiveFile& file) const
		{
			return LoadImage(file, "jpeg");
		}
};
typedef SingletonModule<ImageJPGAPI, ImageDependencies> ImageJPGModule;
typedef Static<ImageJPGModule> StaticImageJPGModule;

class ImagePNGAPI: public IImageModule
{
	public:

		typedef IImageModule Type;
		STRING_CONSTANT(Name, "png");

		IImageModule* getTable ()
		{
			return this;
		}

	public:

		Image* loadImage (ArchiveFile& file) const
		{
			return LoadImage(file, "png");
		}
};
typedef SingletonModule<ImagePNGAPI> ImagePNGModule;
typedef Static<ImagePNGModule> StaticImagePNGModule;

class ImageGDKAPI: public IImageModule
{
	public:

		typedef IImageModule Type;
		STRING_CONSTANT(Name, "GDK");

		IImageModule* getTable ()
		{
			return this;
		}

	public:

		Image* loadImage (ArchiveFile& file) const
		{
			return LoadImageGDK(file);
		}
};
typedef SingletonModule<ImageGDKAPI> ImageGDKModule;
typedef Static<ImageGDKModule> StaticImageGDKModule;

StaticRegisterModule staticRegisterImagePNG(StaticImagePNGModule::instance());
StaticRegisterModule staticRegisterImageJPG(StaticImageJPGModule::instance());
StaticRegisterModule staticRegisterImageTGA(StaticImageTGAModule::instance());
StaticRegisterModule staticRegisterImageGDK(StaticImageGDKModule::instance());
