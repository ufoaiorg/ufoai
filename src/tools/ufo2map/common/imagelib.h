/**
 * @file imagelib.h
 */

#ifndef _IMAGELIB_HEADER_H
#define _IMAGELIB_HEADER_H

/* loads a pcx */
void LoadPCX(const char *filename, byte **picture, byte **palette, int *width, int *height);

void Load256Image(const char *name, byte **pixels, byte **palette, int *width, int *height);

/*==============================================================================
JPEG
==============================================================================*/
#include <jpeglib.h>

void LoadJPG(const char *filename, byte ** pic, int *width, int *height);
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
void LoadTGA(const char *filename, byte **pixels, int *width, int *height);

#endif /* _IMAGELIB_HEADER_H */
