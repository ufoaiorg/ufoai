/**
 * @file src/tools/ufo2map/common/imagelib.h
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef _IMAGELIB_HEADER_H
#define _IMAGELIB_HEADER_H

/*==============================================================================
JPEG
==============================================================================*/
#include <jpeglib.h>

int TryLoadJPG(const char *path, miptex_t **mt);

/*==============================================================================
Targa header structure, encapsulates only the header.
==============================================================================*/
typedef struct targa_header_s {
	unsigned char id_length;
	unsigned char colormap_type;
	unsigned char image_type;
	unsigned short colormap_index;
	unsigned short colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin;
	unsigned short y_origin;
	unsigned short width;
	unsigned short height;
	unsigned char pixel_size;
	unsigned char attributes;
} targa_header_t;

int TryLoadTGA(const char *path, miptex_t **mt);

int TryLoadPNG(const char *path, miptex_t **mt);

#endif /* _IMAGELIB_HEADER_H */
