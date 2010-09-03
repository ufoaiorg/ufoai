/**
 * @file images.c
 * @brief Image loading and saving functions
 */

/*
 Copyright (C) 2002-2009 Quake2World.
 Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "images.h"

/* Workaround for a warning about redeclaring the macro. jpeglib sets this macro
 * and SDL, too. */
#undef HAVE_STDLIB_H

#include <jpeglib.h>
#include <png.h>

/** image formats, tried in this order */
static char *IMAGE_TYPES[] = { "tga", "png", "jpg", NULL };

/** default pixel format to which all images are converted */
static SDL_PixelFormat format = {
	NULL,  /**< palette */
	32,  /**< bits */
	4,  /**< bytes */
	0,  /**< rloss */
	0,  /**< gloss */
	0,  /**< bloss */
	0,  /**< aloss */
	24,  /**< rshift */
	16,  /**< gshift */
	8,  /**< bshift */
	0,  /**< ashift */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	0xff000000,
	0x00ff0000,
	0x0000ff00,
	0x000000ff,
#else
	0x000000ff,
	0x0000ff00,
	0x00ff0000,
	0xff000000,
#endif
	0,  /**< colorkey */
	1   /**< alpha */
};

#define TGA_UNMAP_UNCOMP		2
#define TGA_UNMAP_COMP			10

const char** Img_GetImageTypes (void)
{
	return (const char **)IMAGE_TYPES;
}

/**
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_FindImage
 */
void R_WritePNG (qFILE *f, byte *buffer, int width, int height)
{
	int			i;
	png_structp	png_ptr;
	png_infop	info_ptr;
	png_bytep	*row_pointers;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		return;
	}

	png_init_io(png_ptr, f->f);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 9);

	png_write_info(png_ptr, info_ptr);

	row_pointers = (png_bytep *)malloc(height * sizeof(png_bytep));
	for (i = 0; i < height; i++)
		row_pointers[i] = buffer + (height - 1 - i) * 3 * width;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(row_pointers);
}

#define TGA_CHANNELS 3

/**
 * @sa R_LoadTGA
 * @sa R_WriteTGA
 */
void R_WriteCompressedTGA (qFILE *f, const byte *buffer, int width, int height)
{
	byte pixel_data[TGA_CHANNELS];
	byte block_data[TGA_CHANNELS * 128];
	byte rle_packet;
	int compress = 0;
	size_t block_length = 0;
	byte header[18];
	char footer[26];

	int y;
	size_t x;

	memset(header, 0, sizeof(header));
	memset(footer, 0, sizeof(footer));

	/* Fill in header */
	/* byte 0: image ID field length */
	/* byte 1: color map type */
	header[2] = TGA_UNMAP_COMP;		/* image type: Truecolor RLE encoded */
	/* byte 3 - 11: palette data */
	/* image width */
	header[12] = width & 255;
	header[13] = (width >> 8) & 255;
	/* image height */
	header[14] = height & 255;
	header[15] = (height >> 8) & 255;
	header[16] = 8 * TGA_CHANNELS;	/* Pixel size 24 (RGB) or 32 (RGBA) */
	header[17] = 0x20;	/* Origin at bottom left */

	/* write header */
	FS_Write(header, sizeof(header), f);

	for (y = height - 1; y >= 0; y--) {
		for (x = 0; x < width; x++) {
			const size_t index = y * width * TGA_CHANNELS + x * TGA_CHANNELS;
			pixel_data[0] = buffer[index + 2];
			pixel_data[1] = buffer[index + 1];
			pixel_data[2] = buffer[index];

			if (block_length == 0) {
				memcpy(block_data, pixel_data, TGA_CHANNELS);
				block_length++;
				compress = 0;
			} else {
				if (!compress) {
					/* uncompressed block and pixel_data differs from the last pixel */
					if (memcmp(&block_data[(block_length - 1) * TGA_CHANNELS], pixel_data, TGA_CHANNELS) != 0) {
						/* append pixel */
						memcpy(&block_data[block_length * TGA_CHANNELS], pixel_data, TGA_CHANNELS);

						block_length++;
					} else {
						/* uncompressed block and pixel data is identical */
						if (block_length > 1) {
							/* write the uncompressed block */
							rle_packet = block_length - 2;
							FS_Write(&rle_packet,1, f);
							FS_Write(block_data, (block_length - 1) * TGA_CHANNELS, f);
							block_length = 1;
						}
						memcpy(block_data, pixel_data, TGA_CHANNELS);
						block_length++;
						compress = 1;
					}
				} else {
					/* compressed block and pixel data is identical */
					if (memcmp(block_data, pixel_data, TGA_CHANNELS) == 0) {
						block_length++;
					} else {
						/* compressed block and pixel data differs */
						if (block_length > 1) {
							/* write the compressed block */
							rle_packet = block_length + 127;
							FS_Write(&rle_packet, 1, f);
							FS_Write(block_data, TGA_CHANNELS, f);
							block_length = 0;
						}
						memcpy(&block_data[block_length * TGA_CHANNELS], pixel_data, TGA_CHANNELS);
						block_length++;
						compress = 0;
					}
				}
			}

			if (block_length == 128) {
				rle_packet = block_length - 1;
				if (!compress) {
					FS_Write(&rle_packet, 1, f);
					FS_Write(block_data, 128 * TGA_CHANNELS, f);
				} else {
					rle_packet += 128;
					FS_Write(&rle_packet, 1, f);
					FS_Write(block_data, TGA_CHANNELS, f);
				}

				block_length = 0;
				compress = 0;
			}
		}
	}

	/* write remaining bytes */
	if (block_length) {
		rle_packet = block_length - 1;
		if (!compress) {
			FS_Write(&rle_packet, 1, f);
			FS_Write(block_data, block_length * TGA_CHANNELS, f);
		} else {
			rle_packet += 128;
			FS_Write(&rle_packet, 1, f);
			FS_Write(block_data, TGA_CHANNELS, f);
		}
	}

	/* write footer (optional, but the specification recommends it) */
	strncpy(&footer[8] , "TRUEVISION-XFILE", 16);
	footer[24] = '.';
	footer[25] = 0;
	FS_Write(footer, sizeof(footer), f);
}

/**
 * @sa R_ScreenShot_f
 * @sa R_LoadJPG
 */
void R_WriteJPG (qFILE *f, byte *buffer, int width, int height, int quality)
{
	int offset, w3;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *s;

	/* Initialise the jpeg compression object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, f->f);

	/* Setup jpeg parameters */
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	cinfo.progressive_mode = TRUE;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, qtrue);	/* start compression */
	jpeg_write_marker(&cinfo, JPEG_COM, (const byte *) "UFOAI", (uint32_t) 5);

	/* Feed scanline data */
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while (cinfo.next_scanline < cinfo.image_height) {
		s = &buffer[offset - (cinfo.next_scanline * w3)];
		jpeg_write_scanlines(&cinfo, &s, 1);
	}

	/* Finish compression */
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
}

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface.
 */
static qboolean Img_LoadTypedImage (const char *name, char *type, SDL_Surface **surf)
{
	char path[MAX_QPATH];
	byte *buf;
	int len;
	SDL_RWops *rw;
	SDL_Surface *s;

	snprintf(path, sizeof(path), "%s.%s", name, type);

	if ((len = FS_LoadFile(path, &buf)) == -1)
		return qfalse;

	if (!(rw = SDL_RWFromMem(buf, len))) {
		FS_FreeFile(buf);
		return qfalse;
	}

	if (!(*surf = IMG_LoadTyped_RW(rw, 0, type))) {
		SDL_FreeRW(rw);
		FS_FreeFile(buf);
		return qfalse;
	}

	SDL_FreeRW(rw);
	FS_FreeFile(buf);

	if (!(s = SDL_ConvertSurface(*surf, &format, 0))) {
		SDL_FreeSurface(*surf);
		return qfalse;
	}

	SDL_FreeSurface(*surf);
	*surf = s;

	return qtrue;
}

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface.
 *
 * Image formats are tried in the order they appear in TYPES.
 * @note Make sure to free the given @c SDL_Surface after you are done with it.
 */
qboolean Img_LoadImage (const char *name, SDL_Surface **surf)
{
	int i;

	i = 0;
	while (IMAGE_TYPES[i]) {
		if (Img_LoadTypedImage(name, IMAGE_TYPES[i++], surf))
			return qtrue;
	}

	return qfalse;
}
