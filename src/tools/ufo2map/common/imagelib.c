/**
 * @file imagelib.c
 * @brief Image related code
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

#include "shared.h"
#include "imagelib.h"

#include <jpeglib.h>
#include <png.h>

/*
============================================================================
TARGA IMAGE
============================================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

static void LoadTGA (const char *name, byte ** pic, int *width, int *height)
{
	int columns, rows, numPixels;
	byte *pixbuf;
	int row, column;
	byte *buf_p;
	byte *buffer;
	int length;
	TargaHeader targa_header;
	byte *targa_rgba;
	byte tmp[2];

	if (*pic != NULL)
		Sys_Error("possible mem leak in LoadTGA");

	/* load the file */
	length = FS_LoadFile(name, (byte **) &buffer);
	if (length == -1)
		return;

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = LittleShort(*((short *) tmp));
	buf_p += 2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = LittleShort(*((short *) tmp));
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort(*((short *) buf_p));
	buf_p += 2;
	targa_header.y_origin = LittleShort(*((short *) buf_p));
	buf_p += 2;
	targa_header.width = LittleShort(*((short *) buf_p));
	buf_p += 2;
	targa_header.height = LittleShort(*((short *) buf_p));
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
		Sys_Error("LoadTGA: Only type 2 and 10 targa RGB images supported (%s) (type: %i)", name, targa_header.image_type);

	if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		Sys_Error("LoadTGA: Only 32 or 24 bit images supported (no colormaps) (%s) (pixel_size: %i)", name, targa_header.pixel_size);

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = Mem_Alloc(numPixels * 4);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;	/* skip TARGA image comment */

	if (targa_header.image_type == 2) {	/* Uncompressed, RGB images */
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row * columns * 4;
			for (column = 0; column < columns; column++) {
				unsigned char red, green, blue, alphabyte;

				red = green = blue = alphabyte = 0;
				switch (targa_header.pixel_size) {
				case 24:

					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default:
					break;
				}
			}
		}
	} else if (targa_header.image_type == 10) {	/* Runlength encoded RGB images */
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;

		red = green = blue = alphabyte = 0;
		for (row = rows - 1; row >= 0; row--) {
			pixbuf = targa_rgba + row * columns * 4;
			for (column = 0; column < columns;) {
				packetHeader = *buf_p++;
				/* SCHAR_MAX although it's unsigned */
				packetSize = 1 + (packetHeader & SCHAR_MAX);
				if (packetHeader & 0x80) {	/* run-length packet */
					switch (targa_header.pixel_size) {
					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = 255;
						break;
					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						break;
					default:
						break;
					}

					for (j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) {	/* run spans across rows */
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				} else {		/* non run-length packet */
					for (j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						default:
							break;
						}
						column++;
						if (column == columns) {	/* pixel packet run spans across rows */
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
		  breakOut:;
		}
	}

	FS_FreeFile(buffer);
}

/**
 * @brief This is robbed from the engine source code, and refactored for
 * use during map compilation.
 */
int TryLoadTGA (const char *path, miptex_t **mt)
{
	byte *pic = NULL;
	int width, height;
	size_t size;
	byte *dest;

	LoadTGA(path, &pic, &width, &height);
	if (pic == NULL)
		return -1;

	size = sizeof(*mt) + width * height * 4;
	*mt = (miptex_t *)Mem_Alloc(size);

	dest = (byte*)(*mt) + sizeof(*mt);
	memcpy(dest, pic, width * height * 4);  /* stuff RGBA into this opaque space */
	Mem_Free(pic);

	/* copy relevant header fields to miptex_t */
	(*mt)->width = width;
	(*mt)->height = height;
	(*mt)->offsets[0] = sizeof(*mt);

	return 0;
}

/*
==============================================================================
PNG LOADING
==============================================================================
*/

#include <png.h>

typedef struct pngBuf_s {
	byte	*buffer;
	size_t	pos;
} pngBuf_t;

static void PngReadFunc (png_struct *Png, png_bytep buf, png_size_t size)
{
	pngBuf_t *PngFileBuffer = (pngBuf_t*)png_get_io_ptr(Png);
	memcpy(buf,PngFileBuffer->buffer + PngFileBuffer->pos, size);
	PngFileBuffer->pos += size;
}

/**
 * @sa LoadTGA
 * @sa LoadJPG
 */
static void LoadPNG (const char *name, byte **pic, int *width, int *height)
{
	int rowptr;
	int color_type, bit_depth;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	byte **row_pointers;
	byte *img;
	uint32_t i;
	pngBuf_t PngFileBuffer = {NULL, 0};
	png_uint_32 png_height, png_width, rowbytes;
	png_byte channels;

	if (*pic != NULL)
		Sys_Error("possible mem leak in LoadPNG");

	/* Load the file */
	FS_LoadFile(name, (byte **)&PngFileBuffer.buffer);
	if (!PngFileBuffer.buffer)
		return;

	/* Parse the PNG file */
	if ((png_sig_cmp(PngFileBuffer.buffer, 0, 8)) != 0) {
		Com_Printf("LoadPNG: Not a PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return;
	}

	PngFileBuffer.pos = 0;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return;
	}

	end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return;
	}

	/* get some usefull information from header */
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);

	/**
	 * we want to treat all images the same way
	 * The following code transforms grayscale images of less than 8 to 8 bits,
	 * changes paletted images to RGB, and adds a full alpha channel if there is
	 * transparency information in a tRNS chunk.
	 */

	/* convert index color images to RGB images */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	/* convert 1-2-4 bits grayscale images to 8 bits grayscale */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);
	else if (bit_depth < 8)
		png_set_packing(png_ptr);

	png_set_read_fn(png_ptr, (png_voidp)&PngFileBuffer, (png_rw_ptr)PngReadFunc);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	row_pointers = png_get_rows(png_ptr, info_ptr);
	rowptr = 0;

	png_height = png_get_image_height(png_ptr, info_ptr);
	png_width = png_get_image_width(png_ptr, info_ptr);
	img = Mem_Alloc(png_width * png_height * 4);
	if (pic)
		*pic = img;

	channels = png_get_channels(png_ptr, info_ptr);
	rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	if (channels == 4) {
		for (i = 0; i < png_height; i++) {
			memcpy(img + rowptr, row_pointers[i], rowbytes);
			rowptr += rowbytes;
		}
	} else {
		uint32_t j;

		memset(img, 255, png_width * png_height * 4);
		for (rowptr = 0, i = 0; i < png_height; i++) {
			for (j = 0; j < rowbytes; j += channels) {
				memcpy(img + rowptr, row_pointers[i] + j, channels);
				rowptr += 4;
			}
		}
	}

	if (width)
		*width = (int)png_width;
	if (height)
		*height = (int)png_height;

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	FS_FreeFile(PngFileBuffer.buffer);
}

/**
 * @brief This is robbed from the engine source code, and refactored for
 * use during map compilation.
 */
int TryLoadPNG (const char *path, miptex_t **mt)
{
	byte *pic = NULL;
	int width, height;
	size_t size;
	byte *dest;

	LoadPNG(path, &pic, &width, &height);
	if (pic == NULL)
		return -1;

	size = sizeof(*mt) + width * height * 4;
	*mt = (miptex_t *)Mem_Alloc(size);

	dest = (byte*)(*mt) + sizeof(*mt);
	memcpy(dest, pic, width * height * 4);  /* stuff RGBA into this opaque space */
	Mem_Free(pic);

	/* copy relevant header fields to miptex_t */
	(*mt)->width = width;
	(*mt)->height = height;
	(*mt)->offsets[0] = sizeof(*mt);

	return 0;
}


/*
=================================================================
JPEG LOADING
By Robert 'Heffo' Heffernan
=================================================================
*/

static void ufo_jpg_null (j_decompress_ptr cinfo)
{
}

static boolean ufo_jpg_fill_input_buffer (j_decompress_ptr cinfo)
{
	Verb_Printf(VERB_EXTRA, "Premature end of JPEG data\n");
	return 1;
}

static void ufo_jpg_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	if (cinfo->src->bytes_in_buffer < (size_t) num_bytes)
		Verb_Printf(VERB_EXTRA, "Premature end of JPEG data\n");

	cinfo->src->next_input_byte += (size_t) num_bytes;
	cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void ufo_jpeg_mem_src (j_decompress_ptr cinfo, byte * mem, int len)
{
	cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
	cinfo->src->init_source = ufo_jpg_null;
	cinfo->src->fill_input_buffer = ufo_jpg_fill_input_buffer;
	cinfo->src->skip_input_data = ufo_jpg_skip_input_data;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart;
	cinfo->src->term_source = ufo_jpg_null;
	cinfo->src->bytes_in_buffer = len;
	cinfo->src->next_input_byte = mem;
}

/**
 * @sa LoadTGA
 * @sa LoadPNG
 * @sa R_FindImage
 */
static void LoadJPG (const char *filename, byte ** pic, int *width, int *height)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *rawdata, *rgbadata, *scanline, *p, *q;
	int rawsize, i;

	if (*pic != NULL)
		Sys_Error("possible mem leak in LoadJPG");

	/* Load JPEG file into memory */
	rawsize = FS_LoadFile(filename, (byte **) &rawdata);
	if (rawsize == -1)
		return;

	/* Knightmare- check for bad data */
	if (rawdata[6] != 'J' || rawdata[7] != 'F' || rawdata[8] != 'I' || rawdata[9] != 'F') {
		Com_Printf("Bad jpg file %s\n", filename);
		Mem_Free(rawdata);
		return;
	}

	/* Initialise libJpeg Object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	/* Feed JPEG memory into the libJpeg Object */
	ufo_jpeg_mem_src(&cinfo, rawdata, rawsize);

	/* Process JPEG header */
	jpeg_read_header(&cinfo, qtrue);

	/* Start Decompression */
	jpeg_start_decompress(&cinfo);

	/* Check Colour Components */
	if (cinfo.output_components != 3 && cinfo.output_components != 4) {
		Com_Printf("Invalid JPEG colour components\n");
		jpeg_destroy_decompress(&cinfo);
		Mem_Free(rawdata);
		return;
	}

	/* Allocate Memory for decompressed image */
	rgbadata = Mem_Alloc(cinfo.output_width * cinfo.output_height * 4);
	if (!rgbadata) {
		Com_Printf("Insufficient RAM for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		Mem_Free(rawdata);
		return;
	}

	/* Pass sizes to output */
	*width = cinfo.output_width;
	*height = cinfo.output_height;

	/* Allocate Scanline buffer */
	scanline = Mem_Alloc(cinfo.output_width * 4);
	if (!scanline) {
		Com_Printf("Insufficient RAM for JPEG scanline buffer\n");
		Mem_Free(rgbadata);
		jpeg_destroy_decompress(&cinfo);
		Mem_Free(rawdata);
		return;
	}

	/* Read Scanlines, and expand from RGB to RGBA */
	q = rgbadata;
	while (cinfo.output_scanline < cinfo.output_height) {
		p = scanline;
		jpeg_read_scanlines(&cinfo, &scanline, 1);

		for (i = 0; i < cinfo.output_width; i++) {
			q[0] = p[0];
			q[1] = p[1];
			q[2] = p[2];
			q[3] = 255;

			p += 3;
			q += 4;
		}
	}

	/* Free the scanline buffer */
	Mem_Free(scanline);

	/* Finish Decompression */
	jpeg_finish_decompress(&cinfo);

	/* Destroy JPEG object */
	jpeg_destroy_decompress(&cinfo);

	/* Return the 'rgbadata' */
	*pic = rgbadata;
	Mem_Free(rawdata);
}

/**
 * @brief This is robbed from the engine source code, and refactored for
 * use during map compilation.
 */
int TryLoadJPG (const char *path, miptex_t **mt)
{
	byte *pic = NULL;
	int width, height;
	size_t size;
	byte *dest;

	LoadJPG(path, &pic, &width, &height);
	if (pic == NULL)
		return -1;

	size = sizeof(*mt) + width * height * 4;
	*mt = (miptex_t *)Mem_Alloc(size);

	dest = (byte*)(*mt) + sizeof(*mt);
	memcpy(dest, pic, width * height * 4);  /* stuff RGBA into this opaque space */
	Mem_Free(pic);

	/* copy relevant header fields to miptex_t */
	(*mt)->width = width;
	(*mt)->height = height;
	(*mt)->offsets[0] = sizeof(*mt);

	return 0;
}
