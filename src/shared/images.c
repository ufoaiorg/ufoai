/**
 * @file images.c
 * @brief Image loading and saving functions
 */

/*
 Copyright (C) 2002-2009 Quake2World.
 Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "shared.h"

/* Workaround for a warning about redeclaring the macro. jpeglib sets this macro
 * and SDL, too. */
#undef HAVE_STDLIB_H

#include <jpeglib.h>
#include <png.h>
#include <zlib.h>

#if JPEG_LIB_VERSION < 80
#	include <jerror.h>
#endif

/** image formats, tried in this order */
static char const* const IMAGE_TYPES[] = { "png", "jpg", NULL };

#define TGA_UNMAP_COMP			10

char const* const* Img_GetImageTypes (void)
{
	return IMAGE_TYPES;
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

	OBJZERO(header);
	OBJZERO(footer);

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

static byte* readFile(char const* const name, char const* const suffix, size_t *len)
{
	char path[MAX_QPATH];
	snprintf(path, sizeof(path), "%s.%s", name, suffix);
	byte* buf = NULL;
	*len = FS_LoadFile(path, &buf);
	return buf;
}

static SDL_Surface* createSurface(int const height, int const width)
{
	return SDL_CreateRGBSurface(0, width, height, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		0xFF000000U, 0x00FF0000U, 0x0000FF00U, 0x000000FFU
#else
		0x000000FFU, 0x0000FF00U, 0x00FF0000U, 0xFF000000U
#endif
	);
}

typedef struct
{
	byte*       ptr;
	byte const* end;
} bufState_t;

static void readMem(png_struct* const png, png_byte* const dst, png_size_t const size)
{
	bufState_t *state = (bufState_t*)(png_get_io_ptr(png));
	if (state->end - state->ptr < size) {
		png_error(png, "premature end of input");
	} else {
		memcpy(dst, state->ptr, size);
		state->ptr += size;
	}
}

static SDL_Surface* Img_LoadPNG(char const* const name)
{
	SDL_Surface* res = 0;
	size_t       len;
	byte         *buf;
	if ((buf = readFile(name, "png", &len))) {
		png_struct* png;
		if ((png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0))) {
			png_info* info = png_create_info_struct(png);
			if (info) {
				bufState_t state = { buf, buf + len };
				png_set_read_fn(png, &state, &readMem);

				png_read_info(png, info);

				png_uint_32 height;
				png_uint_32 width;
				int         bit_depth;
				int         color_type;
				png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, 0, 0, 0);

				/* Ensure that we always get a RGBA image with 8 bits per channel. */
				png_set_gray_to_rgb(png);
				png_set_strip_16(png);
				png_set_packing(png);
				png_set_expand(png);
				png_set_add_alpha(png, 255, PNG_FILLER_AFTER);

				SDL_Surface* s;
				if ((s = createSurface(height, width))) {
					png_start_read_image(png);

					png_byte*    dst   = (png_byte*)(s->pixels);
					size_t const pitch = s->pitch;
					for (size_t n = height; n != 0; dst += pitch, --n) {
						png_read_row(png, dst, 0);
					}

					png_read_end(png, info);
					res = s;
				}
			}

			png_destroy_read_struct(&png, &info, 0);
		}

		FS_FreeFile(buf);
	}

	return res;
}

typedef struct jpeg_error_mgr jpeg_error_mgr;
typedef struct jpeg_decompress_struct jpeg_decompress_struct;

#if JPEG_LIB_VERSION < 80
typedef struct jpeg_source_mgr jpeg_source_mgr;

static void djepg_nop(jpeg_decompress_struct* unused) {}

static boolean djepg_fill_input_buffer(jpeg_decompress_struct* const cinfo)
{
	ERREXIT(cinfo, JERR_INPUT_EOF);
	return qfalse;
}

static void djepg_skip_input_data(jpeg_decompress_struct* const cinfo, long const num_bytes)
{
	if (num_bytes < 0)
		return;

	jpeg_source_mgr* src = cinfo->src;
	if (src->bytes_in_buffer < (size_t)num_bytes)
		ERREXIT(cinfo, JERR_INPUT_EOF);

	src->next_input_byte += num_bytes;
	src->bytes_in_buffer -= num_bytes;
}
#endif

static SDL_Surface* Img_LoadJPG(char const* const name)
{
	SDL_Surface* res = 0;
	size_t       len;
	byte         *buf;
	if ((buf = readFile(name, "jpg", &len)) != NULL) {
		jpeg_decompress_struct cinfo;
		jpeg_error_mgr         jerr;

		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_decompress(&cinfo);

#if JPEG_LIB_VERSION < 80
		jpeg_source_mgr src;
		src.next_input_byte   = buf;
		src.bytes_in_buffer   = len;
		src.init_source       = &djepg_nop;
		src.fill_input_buffer = &djepg_fill_input_buffer;
		src.skip_input_data   = &djepg_skip_input_data;
		src.resync_to_restart = &jpeg_resync_to_restart;
		src.term_source       = &djepg_nop;
		cinfo.src             = &src;
#else
		jpeg_mem_src(&cinfo, buf, len);
#endif

		jpeg_read_header(&cinfo, qtrue);

		cinfo.out_color_space = JCS_RGB;

		SDL_Surface* s;
		if ((s = createSurface(cinfo.image_height, cinfo.image_width))) {
			jpeg_start_decompress(&cinfo);

			byte*        dst   = (byte*)(s->pixels);
			size_t const pitch = s->pitch;
			for (size_t n = cinfo.image_height; n != 0; dst += pitch, --n) {
				JSAMPLE* lines[1] = { dst };
				jpeg_read_scanlines(&cinfo, lines, 1);

				/* Convert RGB to RGBA. */
				for (size_t x = cinfo.image_width; x-- != 0;) {
					dst[4 * x + 0] = dst[3 * x + 0];
					dst[4 * x + 1] = dst[3 * x + 1];
					dst[4 * x + 2] = dst[3 * x + 2];
					dst[4 * x + 3] = 255;
				}
			}

			jpeg_finish_decompress(&cinfo);
			res = s;
		}

		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(buf);
	}

	return res;
}

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface.
 *
 * Image formats are tried in the order they appear in TYPES.
 * @note Make sure to free the given @c SDL_Surface after you are done with it.
 */
SDL_Surface* Img_LoadImage (char const* name)
{
	SDL_Surface* s;
	if ((s = Img_LoadPNG(name)))
		return s;
	if ((s = Img_LoadJPG(name)))
		return s;
	return 0;
}
