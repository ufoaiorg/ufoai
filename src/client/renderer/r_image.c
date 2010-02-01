/**
 * @file r_image.c
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

#include "r_local.h"
#include "r_error.h"
#include "r_overlay.h"

#define MAX_IMAGEHASH 256
static image_t *imageHash[MAX_IMAGEHASH];

/* Workaround for a warning about redeclaring the macro. jpeglib sets this macro
 * and SDL, too. */
#undef HAVE_STDLIB_H

#include <jpeglib.h>
#include <png.h>

image_t r_images[MAX_GL_TEXTURES];
int r_numImages;

/* generic environment map */
image_t *r_envmaptextures[MAX_ENVMAPTEXTURES];

#define MAX_TEXTURE_SIZE 8192

/**
 * @brief Free previously loaded materials and their stages
 * @sa R_LoadMaterials
 */
void R_ImageClearMaterials (void)
{
	image_t *image;
	int i;

	/* clear previously loaded materials */
	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		material_t *m = &image->material;
		materialStage_t *s = m->stages;

		while (s) {  /* free the stages chain */
			materialStage_t *ss = s->next;
			Mem_Free(s);
			s = ss;
		}

		memset(m, 0, sizeof(*m));
		m->bump = DEFAULT_BUMP;
		m->parallax = DEFAULT_PARALLAX;
		m->specular = DEFAULT_SPECULAR;
		m->hardness = DEFAULT_HARDNESS;
	}
}

/**
 * @brief Shows all loaded images
 */
void R_ImageList_f (void)
{
	int i;
	image_t *image;
	int texels;

	Com_Printf("------------------\n");
	texels = 0;

	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (!image->texnum)
			continue;
		texels += image->upload_width * image->upload_height;
		switch (image->type) {
		case it_effect:
			Com_Printf("EF");
			break;
		case it_skin:
			Com_Printf("SK");
			break;
		case it_wrappic:
			Com_Printf("WR");
			break;
		case it_chars:
			Com_Printf("CH");
			break;
		case it_static:
			Com_Printf("ST");
			break;
		case it_normalmap:
			Com_Printf("NM");
			break;
		case it_material:
			Com_Printf("MA");
			break;
		case it_lightmap:
			Com_Printf("LM");
			break;
		case it_world:
			Com_Printf("WO");
			break;
		case it_pic:
			Com_Printf("PI");
			break;
		default:
			Com_Printf("  ");
			break;
		}

		Com_Printf(" %4i %4i RGB: %5i idx: %s\n", image->upload_width, image->upload_height, image->texnum, image->name);
	}
	Com_Printf("Total textures: %i (max textures: %i)\n", r_numImages, MAX_GL_TEXTURES);
	Com_Printf("Total texel count (not counting mipmaps): %i\n", texels);
}

/*
==============================================================================
PNG LOADING
==============================================================================
*/

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
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_FindImage
 */
static void R_LoadPNG (const char *name, byte **pic, int *width, int *height)
{
	int rowptr;
	int samples, color_type, bit_depth;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	byte **row_pointers;
	byte *img;
	uint32_t i;
	pngBuf_t PngFileBuffer = {NULL, 0};

	if (*pic != NULL)
		Com_Error(ERR_FATAL, "possible mem leak in LoadPNG");

	/* Load the file */
	FS_LoadFile(name, (byte **)&PngFileBuffer.buffer);
	if (!PngFileBuffer.buffer)
		return;

	/* Parse the PNG file */
	if ((png_check_sig(PngFileBuffer.buffer, 8)) == 0) {
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
		png_set_gray_1_2_4_to_8(png_ptr);
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

	img = Mem_PoolAllocExt(info_ptr->width * info_ptr->height * 4, qfalse, vid_imagePool, 0);
	if (pic)
		*pic = img;

	if (info_ptr->channels == 4) {
		for (i = 0; i < info_ptr->height; i++) {
			memcpy(img + rowptr, row_pointers[i], info_ptr->rowbytes);
			rowptr += info_ptr->rowbytes;
		}
	} else {
		uint32_t j;

		memset(img, 255, info_ptr->width * info_ptr->height * 4);
		for (rowptr = 0, i = 0; i < info_ptr->height; i++) {
			for (j = 0; j < info_ptr->rowbytes; j += info_ptr->channels) {
				memcpy(img + rowptr, row_pointers[i] + j, info_ptr->channels);
				rowptr += 4;
			}
		}
	}

	if (width)
		*width = info_ptr->width;
	if (height)
		*height = info_ptr->height;
	samples = info_ptr->channels;

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	FS_FreeFile(PngFileBuffer.buffer);
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
		Com_Printf("R_WritePNG: LibPNG Error!\n");
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		Com_Printf("R_WritePNG: LibPNG Error!\n");
		return;
	}

	png_init_io(png_ptr, f->f);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 9);

	png_write_info(png_ptr, info_ptr);

	row_pointers = Mem_PoolAllocExt(height * sizeof(png_bytep), qfalse, vid_imagePool, 0);
	for (i = 0; i < height; i++)
		row_pointers[i] = buffer + (height - 1 - i) * 3 * width;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	Mem_Free(row_pointers);
}

/*
=========================================================
TARGA LOADING
=========================================================
*/

typedef struct targaHeader_s {
	byte idLength;
	byte colorMapType;
	byte imageType;

	uint16_t colorMapIndex;
	uint16_t colorMapLength;
	byte colorMapSize;

	uint16_t xOrigin;
	uint16_t yOrigin;
	uint16_t width;
	uint16_t height;

	byte pixelSize;

	byte attributes;
} targaHeader_t;

#define TGA_COLMAP_UNCOMP		1
#define TGA_COLMAP_COMP			9
#define TGA_UNMAP_UNCOMP		2
#define TGA_UNMAP_COMP			10
#define TGA_GREY_UNCOMP			3
#define TGA_GREY_COMP			11

/**
 * @sa R_LoadJPG
 * @sa R_LoadPNG
 * @sa R_FindImage
 */
static void R_LoadTGA (const char *name, byte ** pic, int *width, int *height)
{
	int i, columns, rows, row_inc, row, col;
	byte *buf_p, *buffer, *pixbuf, *targaRGBA;
	int length, samples, readPixelCount, pixelCount;
	byte palette[256][4], red, green, blue, alpha;
	qboolean compressed;
	targaHeader_t targaHeader;

	if (*pic != NULL)
		Com_Error(ERR_FATAL, "R_LoadTGA: possible mem leak");

	/* Load the file */
	length = FS_LoadFile(name, &buffer);
	if (!buffer || length <= 0) {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadTGA: Bad tga file %s\n", name);
		return;
	}

	/* Parse the header */
	buf_p = buffer;
	targaHeader.idLength = *buf_p++;
	targaHeader.colorMapType = *buf_p++;
	targaHeader.imageType = *buf_p++;

	targaHeader.colorMapIndex = buf_p[0] + buf_p[1] * 256; buf_p += 2;
	targaHeader.colorMapLength = buf_p[0] + buf_p[1] * 256; buf_p += 2;
	targaHeader.colorMapSize = *buf_p++;
	targaHeader.xOrigin = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.yOrigin = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.width = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.height = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.pixelSize = *buf_p++;
	targaHeader.attributes = *buf_p++;

	/* Skip TARGA image comment */
	if (targaHeader.idLength != 0)
		buf_p += targaHeader.idLength;

	compressed = qfalse;
	switch (targaHeader.imageType) {
	case TGA_COLMAP_COMP:
		compressed = qtrue;
	case TGA_COLMAP_UNCOMP:
		/* Uncompressed colormapped image */
		if (targaHeader.pixelSize != 8) {
			Com_Printf("R_LoadTGA: Only 8 bit images supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		if (targaHeader.colorMapLength != 256) {
			Com_Printf("R_LoadTGA: Only 8 bit colormaps are supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		if (targaHeader.colorMapIndex) {
			Com_Printf("R_LoadTGA: colorMapIndex is not supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}

		switch (targaHeader.colorMapSize) {
		case 32:
			for (i = 0; i < targaHeader.colorMapLength ; i++) {
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
			break;

		case 24:
			for (i = 0; i < targaHeader.colorMapLength ; i++) {
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = 255;
			}
			break;

		default:
			Com_Printf("R_LoadTGA: only 24 and 32 bit colormaps are supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;

	case TGA_UNMAP_COMP:
		compressed = qtrue;
	case TGA_UNMAP_UNCOMP:
		/* Uncompressed or RLE compressed RGB */
		if (targaHeader.pixelSize != 32 && targaHeader.pixelSize != 24) {
			Com_Printf("R_LoadTGA: Only 32 or 24 bit images supported for type 2 and 10 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;

	case TGA_GREY_COMP:
		compressed = qtrue;
	case TGA_GREY_UNCOMP:
		/* Uncompressed greyscale */
		if (targaHeader.pixelSize != 8) {
			Com_Printf("R_LoadTGA: Only 8 bit images supported for type 3 and 11 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;
	default:
		Com_Printf("R_LoadTGA: Unknown tga image type: %i for image %s\n", targaHeader.imageType, name);
		FS_FreeFile(buffer);
		return;
	}

	columns = targaHeader.width;
	if (width)
		*width = columns;

	rows = targaHeader.height;
	if (height)
		*height = rows;

	targaRGBA = Mem_PoolAllocExt(columns * rows * 4, qfalse, vid_imagePool, 0);
	*pic = targaRGBA;

	/* If bit 5 of attributes isn't set, the image has been stored from bottom to top */
	if (targaHeader.attributes & 0x20) {
		pixbuf = targaRGBA;
		row_inc = 0;
	} else {
		pixbuf = targaRGBA + (rows - 1) * columns * 4;
		row_inc = -columns * 4 * 2;
	}

	red = blue = green = alpha = 0;

	for (row = col = 0, samples = 3; row < rows;) {
		pixelCount = 0x10000;
		readPixelCount = 0x10000;

		if (compressed) {
			pixelCount = *buf_p++;
			if (pixelCount & 0x80)	/* Run-length packet */
				readPixelCount = 1;
			pixelCount = 1 + (pixelCount & 0x7f);
		}

		while (pixelCount-- && row < rows) {
			if (readPixelCount-- > 0) {
				switch (targaHeader.imageType) {
				case TGA_COLMAP_UNCOMP:
				case TGA_COLMAP_COMP:
					/* Colormapped image */
					blue = *buf_p++;
					red = palette[blue][0];
					green = palette[blue][1];
					alpha = palette[blue][3];
					blue = palette[blue][2];
					if (alpha != 255)
						samples = 4;
					break;
				case TGA_UNMAP_UNCOMP:
				case TGA_UNMAP_COMP:
					/* 24 or 32 bit image */
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					if (targaHeader.pixelSize == 32) {
						alpha = *buf_p++;
						if (alpha != 255)
							samples = 4;
					} else
						alpha = 255;
					break;
				case TGA_GREY_UNCOMP:
				case TGA_GREY_COMP:
					/* Greyscale image */
					blue = green = red = *buf_p++;
					alpha = 255;
					break;
				default:
					Com_Error(ERR_FATAL, "R_LoadTGA: Unknown tga image type: %i", targaHeader.imageType);
				}
			}

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			if (++col == columns) {
				/* Run spans across rows */
				row++;
				col = 0;
				pixbuf += row_inc;
			}
		}
	}

	FS_FreeFile(buffer);
}


/**
 * @sa R_LoadTGA
 * @sa R_WriteCompressedTGA
 */
void R_WriteTGA (qFILE *f, const byte *buffer, int width, int height, int channels)
{
	int i, temp;
	byte *out;
	size_t size;

	/* Allocate an output buffer */
	size = (width * height * channels) + 18;
	out = Mem_PoolAllocExt(size, qfalse, vid_imagePool, 0);

	/* Fill in header */
	/* byte 0: image ID field length */
	/* byte 1: color map type */
	out[2] = TGA_UNMAP_UNCOMP;		/* image type: Uncompressed type */
	/* byte 3 - 11: palette data */
	/* image width */
	out[12] = width & 255;
	out[13] = (width >> 8) & 255;
	/* image height */
	out[14] = height & 255;
	out[15] = (height >> 8) & 255;
	out[16] = channels * 8;	/* Pixel size 24 (RGB) or 32 (RGBA) */
	/* byte 17: image descriptor byte */

	/* Copy to temporary buffer */
	memcpy(out + 18, buffer, width * height * channels);

	/* Swap rgb to bgr */
	for (i = 18; i < size; i += channels) {
		temp = out[i];
		out[i] = out[i+2];
		out[i + 2] = temp;
	}

	if (FS_Write(out, size, f) != size)
		Com_Printf("R_WriteTGA: Failed to write the tga file\n");

	Mem_Free(out);
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

/*
=================================================================
JPEG LOADING
By Robert 'Heffo' Heffernan
=================================================================
*/

static void jpg_null (j_decompress_ptr cinfo)
{
}

static boolean jpg_fill_input_buffer (j_decompress_ptr cinfo)
{
	Com_Printf("Premature end of JPEG data\n");
	return 1;
}

static void jpg_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	if (cinfo->src->bytes_in_buffer < (size_t) num_bytes)
		Com_Printf("Premature end of JPEG data\n");

	cinfo->src->next_input_byte += (size_t) num_bytes;
	cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void jpeg_mem_src (j_decompress_ptr cinfo, byte * mem, int len)
{
	cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
	cinfo->src->init_source = jpg_null;
	cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
	cinfo->src->skip_input_data = jpg_skip_input_data;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart;
	cinfo->src->term_source = jpg_null;
	cinfo->src->bytes_in_buffer = len;
	cinfo->src->next_input_byte = mem;
}

/**
 * @sa R_LoadTGA
 * @sa R_LoadPNG
 * @sa R_FindImage
 */
static void R_LoadJPG (const char *filename, byte ** pic, int *width, int *height)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *rawdata, *rgbadata, *scanline, *p, *q;
	int rawsize, i, components;

	if (*pic != NULL)
		Com_Error(ERR_FATAL, "possible mem leak in LoadJPG");

	/* Load JPEG file into memory */
	rawsize = FS_LoadFile(filename, &rawdata);

	if (!rawdata)
		return;

	/* Knightmare- check for bad data */
	if (rawdata[6] != 'J' || rawdata[7] != 'F' || rawdata[8] != 'I' || rawdata[9] != 'F') {
		Com_Printf("Bad jpg file %s\n", filename);
		FS_FreeFile(rawdata);
		return;
	}

	/* Initialise libJpeg Object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	/* Feed JPEG memory into the libJpeg Object */
	jpeg_mem_src(&cinfo, rawdata, rawsize);

	/* Process JPEG header */
	jpeg_read_header(&cinfo, qtrue);

	/* Start Decompression */
	jpeg_start_decompress(&cinfo);

	components = cinfo.output_components;
	if (components != 3 && components != 1) {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadJPG: Bad jpeg components '%s' (%d)\n", filename, components);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	/* Check Colour Components */
	if (cinfo.output_components != 3 && cinfo.output_components != 4) {
		Com_Printf("Invalid JPEG colour components\n");
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	/* Allocate Memory for decompressed image */
	rgbadata = Mem_PoolAllocExt(cinfo.output_width * cinfo.output_height * 4, qfalse, vid_imagePool, 0);
	if (!rgbadata) {
		Com_Printf("Insufficient RAM for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}
	/* Allocate Scanline buffer */
	scanline = Mem_PoolAllocExt(cinfo.output_width * components, qfalse, vid_imagePool, 0);
	if (!scanline) {
		Com_Printf("Insufficient RAM for JPEG scanline buffer\n");
		Mem_Free(rgbadata);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	/* Pass sizes to output */
	*width = cinfo.output_width;
	*height = cinfo.output_height;

	/* Read Scanlines, and expand from RGB to RGBA */
	q = rgbadata;
	while (cinfo.output_scanline < cinfo.output_height) {
		p = scanline;
		jpeg_read_scanlines(&cinfo, &scanline, 1);

		if (components == 1) {
			for (i = 0; i < cinfo.output_width; i++, q += 4) {
				q[0] = q[1] = q[2] = *p++;
				q[3] = 255;
			}
		} else {
			for (i = 0; i < cinfo.output_width; i++, q += 4, p += 3) {
				q[0] = p[0], q[1] = p[1], q[2] = p[2];
				q[3] = 255;
			}
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
	FS_FreeFile(rawdata);
}

/**
 * @brief Generic image-data loading fucntion.
 * @param[in] name (Full) pathname to the image to load. Extension (if given) will be ignored.
 * @param[out] pic Image data.
 * @param[out] width Width of the loaded image.
 * @param[out] height Height of the loaded image.
 * @sa R_FindImage
 */
void R_LoadImage (const char *name, byte **pic, int *width, int *height)
{
	char filename_temp[MAX_QPATH];
	char *ename;
	int len;

	if (!name)
		Com_Error(ERR_FATAL, "R_LoadImage: NULL name");
	len = strlen(name);

	if (len >= 5) {
		/* Drop extension */
		Q_strncpyz(filename_temp, name, MAX_QPATH);
		if (filename_temp[len - 4] == '.')
			len -= 4;
	}

	ename = &(filename_temp[len]);
	*ename = 0;

	/* Check if there is any image at this path. */

	strcpy(ename, ".tga");
	if (FS_CheckFile("%s", filename_temp) != -1) {
		R_LoadTGA(filename_temp, pic, width, height);
		if (pic)
			return;
	}

	strcpy(ename, ".jpg");
	if (FS_CheckFile("%s", filename_temp) != -1) {
		R_LoadJPG(filename_temp, pic, width, height);
		if (pic)
			return;
	}

	strcpy(ename, ".png");
	if (FS_CheckFile("%s", filename_temp) != -1) {
		R_LoadPNG(filename_temp, pic, width, height);
	}
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
	jpeg_write_marker(&cinfo, JPEG_COM, (const byte *) GAME_TITLE, (uint32_t) strlen(GAME_TITLE));

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

void R_ScaleTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	int i, j;
	unsigned frac;
	unsigned p1[MAX_TEXTURE_SIZE], p2[MAX_TEXTURE_SIZE];
	const unsigned fracstep = inwidth * 0x10000 / outwidth;

	assert(outwidth <= MAX_TEXTURE_SIZE);

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++) {
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++) {
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth) {
		const int index = inwidth * (int) ((i + 0.25) * inheight / outheight);
		const unsigned *inrow = in + index;
		const int index2 = inwidth * (int) ((i + 0.75) * inheight / outheight);
		const unsigned *inrow2 = in + index2;

		assert(index < inwidth * inheight);
		assert(index2 < inwidth * inheight);

		for (j = 0; j < outwidth; j++) {
			const byte *pix1 = (const byte *) inrow + p1[j];
			const byte *pix2 = (const byte *) inrow + p2[j];
			const byte *pix3 = (const byte *) inrow2 + p1[j];
			const byte *pix4 = (const byte *) inrow2 + p2[j];
			((byte *) (out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *) (out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *) (out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *) (out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/**
 * @brief Applies brightness and contrast to the specified image while optionally computing
 * the image's average color. Also handles image inversion and monochrome. This is
 * all munged into one function to reduce loops on level load.
 */
void R_FilterTexture (byte *in, int width, int height, imagetype_t type, int bpp)
{
	const float scale = 1.0 / 255.0;

	byte *p = in;
	const byte *end = p + width * height * bpp;
	const float brightness = type == it_lightmap ? r_modulate->value : r_brightness->value;
	const float contrast = r_contrast->value;
	const float saturation = r_saturation->value;
	vec3_t intensity, luminosity, temp;
	int j, mask;
	float max, d;

	enum filter_flags_t {
		FILTER_NONE       = 0,
		FILTER_MONOCHROME = 1U << 0,
		FILTER_INVERT     = 1U << 1
	} filter;

	/* monochrome/invert */
	switch (type) {
	case it_world:
	case it_effect:
	case it_material:
		mask = 1;
		break;
	case it_lightmap:
		mask = 2;
		break;
	default:
		mask = 0;
		break;
	}

	filter = FILTER_NONE;
	if (r_monochrome->integer & mask)
		filter |= FILTER_MONOCHROME;
	if (r_invert->integer & mask)
		filter |= FILTER_INVERT;

	VectorSet(luminosity, 0.2125, 0.7154, 0.0721);

	for (; p != end; p += bpp) {
		VectorCopy(p, temp);
		VectorScale(temp, scale, temp);  /* convert to float */

		VectorScale(temp, brightness, temp);  /* apply brightness */

		max = 0.0;  /* determine brightest component */

		for (j = 0; j < 3; j++) {
			if (temp[j] > max)
				max = temp[j];

			if (temp[j] < 0.0)  /* enforcing positive values */
				temp[j] = 0.0;
		}

		if (max > 1.0)  /* clamp without changing hue */
			VectorScale(temp, 1.0 / max, temp);

		for (j = 0; j < 3; j++) {  /* apply contrast */
			temp[j] -= 0.5;  /* normalize to -0.5 through 0.5 */
			temp[j] *= contrast;  /* scale */
			temp[j] += 0.5;

			if (temp[j] > 1.0)  /* clamp */
				temp[j] = 1.0;
			else if (temp[j] < 0)
				temp[j] = 0;
		}

		/* finally saturation, which requires rgb */
		d = DotProduct(temp, luminosity);

		VectorSet(intensity, d, d, d);
		VectorMix(intensity, temp, saturation, temp);

		for (j = 0; j < 3; j++) {
			temp[j] *= 255;  /* back to byte */

			if (temp[j] > 255)  /* clamp */
				temp[j] = 255;
			else if (temp[j] < 0)
				temp[j] = 0;

			p[j] = (byte)temp[j];
		}

		if (filter & FILTER_MONOCHROME)
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;

		if (filter & FILTER_INVERT) {
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}
}

/**
 * @brief Calculates the texture size that should be used to upload the texture data
 * @param[in] width The width of the source texture data
 * @param[in] height The heigt of the source texture data
 * @param[out] scaledWidth The resulting width - can be the same as the given @c width
 * @param[out] scaledHeight The resulting height - can be the same as the given @c height
 */
void R_GetScaledTextureSize (int width, int height, int *scaledWidth, int *scaledHeight)
{
	for (*scaledWidth = 1; *scaledWidth < width; *scaledWidth <<= 1) {}
	for (*scaledHeight = 1; *scaledHeight < height; *scaledHeight <<= 1) {}

	while (*scaledWidth > r_config.maxTextureSize || *scaledHeight > r_config.maxTextureSize) {
		*scaledWidth >>= 1;
		*scaledHeight >>= 1;
	}

	if (*scaledWidth > MAX_TEXTURE_SIZE)
		*scaledWidth = MAX_TEXTURE_SIZE;
	else if (*scaledWidth < 1)
		*scaledWidth = 1;

	if (*scaledHeight > MAX_TEXTURE_SIZE)
		*scaledHeight = MAX_TEXTURE_SIZE;
	else if (*scaledHeight < 1)
		*scaledHeight = 1;
}

/**
 * @brief Uploads the opengl texture to the server
 * @param[in] data Must be in RGBA format
 */
void R_UploadTexture (unsigned *data, int width, int height, image_t* image)
{
	unsigned *scaled;
	int scaledWidth, scaledHeight;
	int samples;
	int i, c;
	byte *scan;
	qboolean mipmap = (image->type != it_pic && image->type != it_chars);
	qboolean clamp = (image->type == it_pic);

	R_GetScaledTextureSize(width, height, &scaledWidth, &scaledHeight);

	/* some images need very little attention (pics, fonts, etc..) */
	if (!mipmap && scaledWidth == width && scaledHeight == height) {
		/* no mipmapping for these images - just use GL_NEAREST here to not waste memory */
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		return;
	}

	if (scaledWidth != width || scaledHeight != height) {  /* whereas others need to be scaled */
		scaled = (unsigned *)Mem_PoolAllocExt(scaledWidth * scaledHeight * sizeof(unsigned), qfalse, vid_imagePool, 0);
		R_ScaleTexture(data, width, height, scaled, scaledWidth, scaledHeight);
	} else {
		scaled = data;
	}

	/* and filter */
	if (image->type == it_effect || image->type == it_world || image->type == it_material || image->type == it_skin)
		R_FilterTexture((byte*)scaled, scaledWidth, scaledHeight, image->type, 4);

	/* scan the texture for any non-255 alpha */
	c = scaledWidth * scaledHeight;
	samples = r_config.gl_compressed_solid_format ? r_config.gl_compressed_solid_format : r_config.gl_solid_format;
	/* set scan to the first alpha byte */
	for (i = 0, scan = ((byte *) scaled) + 3; i < c; i++, scan += 4) {
		if (*scan != 255) {
			samples = r_config.gl_compressed_alpha_format ? r_config.gl_compressed_alpha_format : r_config.gl_alpha_format;
			break;
		}
	}

	image->has_alpha = (samples == r_config.gl_alpha_format || samples == r_config.gl_compressed_alpha_format);
	image->upload_width = scaledWidth;	/* after power of 2 and scales */
	image->upload_height = scaledHeight;

	/* and mipmapped */
	if (mipmap) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		if (r_config.anisotropic) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_config.maxAnisotropic);
			R_CheckError();
		}
		if (r_texture_lod->integer && r_config.lod_bias) {
			glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, r_texture_lod->value);
			R_CheckError();
		}
	} else {
		if (r_config.anisotropic) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
			R_CheckError();
		}
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
	}
	R_CheckError();

	if (clamp) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		R_CheckError();
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		R_CheckError();
	}

	glTexImage2D(GL_TEXTURE_2D, 0, samples, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	R_CheckError();

	if (image->flags & if_framebuffer) {
		image->fbo = R_RegisterFBObject();
		if (image->fbo && !R_AttachTextureToFBOject(image->fbo, image)) {
			Com_Printf("Warning: Error attaching texture to a FBO: %s\n", image->name);
			image->fbo = 0;
		}
	}

	if (scaled != data)
		Mem_Free(scaled);
}

/**
 * @brief Applies blurring to a texture
 * @sa R_BuildLightMap
 */
void R_SoftenTexture (byte *in, int width, int height, int bpp)
{
	byte *out;
	int i, j, k;
	const int size = width * height * bpp;

	/* soften into a copy of the original image, as in-place would be incorrect */
	out = (byte *)Mem_PoolAllocExt(size, qfalse, vid_imagePool, 0);
	if (!out)
		Com_Error(ERR_FATAL, "Mem_PoolAllocExt: failed on allocation of %i bytes for R_SoftenTexture", width * height * bpp);

	memcpy(out, in, size);

	for (i = 1; i < height - 1; i++) {
		for (j = 1; j < width - 1; j++) {
			const byte *src = in + ((i * width) + j) * bpp;  /* current input pixel */

			const byte *u = (src - (width * bpp));  /* and it's neighbors */
			const byte *d = (src + (width * bpp));
			const byte *l = (src - (1 * bpp));
			const byte *r = (src + (1 * bpp));

			byte *dest = out + ((i * width) + j) * bpp;  /* current output pixel */

			for (k = 0; k < bpp; k++)
				dest[k] = (u[k] + d[k] + l[k] + r[k]) / 4;
		}
	}

	/* copy the softened image over the input image, and free it */
	memcpy(in, out, size);
	Mem_Free(out);
}

#define DAN_WIDTH	512
#define DAN_HEIGHT	256

#define DAWN		0.03

/** this is the data that is used with r_dayandnightTexture */
static byte r_dayandnightAlpha[DAN_WIDTH * DAN_HEIGHT];
/** this is the mask that is used to display day/night on (2d-)geoscape */
image_t *r_dayandnightTexture;

/**
 * @brief Applies alpha values to the night overlay image for 2d geoscape
 * @param[in] q The angle the sun is standing against the equator on earth
 */
void R_CalcAndUploadDayAndNightTexture (float q)
{
	int x, y;
	const float dphi = (float) 2 * M_PI / DAN_WIDTH;
	const float da = M_PI / 2 * (HIGH_LAT - LOW_LAT) / DAN_HEIGHT;
	const float sin_q = sin(q);
	const float cos_q = cos(q);
	float sin_phi[DAN_WIDTH], cos_phi[DAN_WIDTH];
	byte *px;

	for (x = 0; x < DAN_WIDTH; x++) {
		const float phi = x * dphi - q;
		sin_phi[x] = sin(phi);
		cos_phi[x] = cos(phi);
	}

	/* calculate */
	px = r_dayandnightAlpha;
	for (y = 0; y < DAN_HEIGHT; y++) {
		const float a = sin(M_PI / 2 * HIGH_LAT - y * da);
		const float root = sqrt(1 - a * a);
		for (x = 0; x < DAN_WIDTH; x++) {
			const float pos = sin_phi[x] * root * sin_q - (a * SIN_ALPHA + cos_phi[x] * root * COS_ALPHA) * cos_q;

			if (pos >= DAWN)
				*px++ = 255;
			else if (pos <= -DAWN)
				*px++ = 0;
			else
				*px++ = (byte) (128.0 * (pos / DAWN + 1));
		}
	}

	/* upload alpha map into the r_dayandnighttexture */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, DAN_WIDTH, DAN_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, r_dayandnightAlpha);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	R_CheckError();
}

static inline void R_DeleteImage (image_t *image)
{
	image_t *next = image->hashNext;
	image_t *prev = image->hashPrev;

	/* free it */
	glDeleteTextures(1, (GLuint *) &image->texnum);
	R_CheckError();

	if (next) {
		assert(next->hashPrev == image);
		next->hashPrev = prev;
	}
	if (prev) {
		assert(prev->hashNext == image);
		prev->hashNext = next;
	} else {
		const unsigned int hash = Com_HashKey(image->name, MAX_IMAGEHASH);
		assert(imageHash[hash] == image);
		imageHash[hash] = next;
	}

	memset(image, 0, sizeof(*image));
}

static inline image_t *R_GetImage (const char *name)
{
	image_t *image;
	const unsigned hash = Com_HashKey(name, MAX_IMAGEHASH);

	/* look for it */
	for (image = imageHash[hash]; image; image = image->hashNext)
		if (!strcmp(name, image->name))
			return image;

	return NULL;
}

/**
 * @brief Creates a new image from RGBA data. Stores it in the gltextures array
 * and also uploads it.
 * @note This is also used as an entry point for the generated r_noTexture
 * @param[in] name The name of the newly created image
 * @param[in] pic The RGBA data of the image
 * @param[in] width The width of the image (power of two, please)
 * @param[in] height The height of the image (power of two, please)
 * @param[in] type The image type @sa imagetype_t
 */
image_t *R_LoadImageData (const char *name, byte * pic, int width, int height, imagetype_t type)
{
	image_t *image;
	int i;
	size_t len;
	unsigned hash;

	len = strlen(name);
	if (len >= sizeof(image->name))
		Com_Error(ERR_DROP, "R_LoadImageData: \"%s\" is too long", name);

	/* look for it */
	image = R_GetImage(name);
	if (image) {
		Com_Printf("R_LoadImageData: image '%s' is already uploaded\n", name);
		return image;
	}

	/* find a free image_t */
	for (i = 0, image = r_images; i < r_numImages; i++, image++)
		if (!image->texnum)
			break;

	if (i == r_numImages) {
		if (r_numImages >= MAX_GL_TEXTURES)
			Com_Error(ERR_DROP, "R_LoadImageData: MAX_GL_TEXTURES hit");
		r_numImages++;
	}
	image = &r_images[i];
	image->has_alpha = qfalse;
	image->type = type;
	image->width = width;
	image->height = height;
	image->texnum = i + 1;

	Q_strncpyz(image->name, name, sizeof(image->name));
	/* drop extension */
	if (len >= 4 && image->name[len - 4] == '.') {
		image->name[len - 4] = '\0';
		Com_Printf("Image with extension: '%s'\n", name);
	}

	hash = Com_HashKey(image->name, MAX_IMAGEHASH);
	image->hashNext = imageHash[hash];
	if (imageHash[hash])
		imageHash[hash]->hashPrev = image;
	imageHash[hash] = image;

	if (pic) {
		R_BindTexture(image->texnum);
		R_UploadTexture((unsigned *) pic, width, height, image);
	}
	return image;
}

typedef struct imageLoader_s {
	const char *extension;
	void (*load) (const char *name, byte **pic, int *width, int *height);
} imageLoader_t;

static const imageLoader_t imageLoader[] = {
	{".tga", R_LoadTGA},
	{".jpg", R_LoadJPG},
	{".png", R_LoadPNG},

	{NULL, NULL}
};

/**
 * @brief Finds or loads the given image
 * @sa R_RegisterImage
 * @param pname Image name
 * @note the image name has to be at least 5 chars long
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_LoadPNG
 */
#ifdef DEBUG
image_t *R_FindImageDebug (const char *pname, imagetype_t type, const char *file, int line)
#else
image_t *R_FindImage (const char *pname, imagetype_t type)
#endif
{
	char lname[MAX_QPATH];
	char *ename;
	image_t *image;
	size_t len;
	const imageLoader_t *loader;

	if (!pname)
		Com_Error(ERR_FATAL, "R_FindImage: NULL name");
	len = strlen(pname);
	if (len < 5 || len > sizeof(lname) - 5)
		return r_noTexture;

	/* drop extension */
	Q_strncpyz(lname, pname, sizeof(lname));
	if (lname[len - 4] == '.')
		len -= 4;
	ename = &(lname[len]);
	*ename = 0;

	image = R_GetImage(lname);
	if (image)
		return image;

	loader = imageLoader;
	while (loader->load) {
		strcpy(ename, loader->extension);
		if (FS_CheckFile("%s", lname) != -1) {
			byte *pic = NULL;
			int width, height;
			loader->load(lname, &pic, &width, &height);
			if (pic) {
				*ename = 0;
				image = R_LoadImageData(lname, pic, width, height, type);
				Mem_Free(pic);
				if (image->type == it_world) {
					image->normalmap = R_FindImage(va("%s_nm", image->name), it_normalmap);
					if (image->normalmap == r_noTexture)
						image->normalmap = NULL;
				}
				break;
			}
		}
		loader++;
	}

	/* no fitting texture found */
	if (!image)
		image = r_noTexture;

	return image;
}

/**
 * @brief Any image that is a mesh or world texture will be removed here
 * @sa R_ShutdownImages
 */
void R_FreeWorldImages (void)
{
	int i;
	image_t *image;

	R_CheckError();
	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (!image->texnum)
			continue;			/* free image slot */
		if (image->type < it_world)
			continue;			/* keep them */

		/* free it */
		R_DeleteImage(image);
	}
}

void R_InitImages (void)
{
	int i;

	r_numImages = 0;

	r_dayandnightTexture = R_LoadImageData("***r_dayandnighttexture***", NULL, DAN_WIDTH, DAN_HEIGHT, it_effect);
	if (r_dayandnightTexture == r_noTexture)
		Com_Error(ERR_FATAL, "Could not create daynight image for the geoscape");

	for (i = 0; i < MAX_ENVMAPTEXTURES; i++) {
		r_envmaptextures[i] = R_FindImage(va("pics/envmaps/envmap_%i", i), it_effect);
		if (r_envmaptextures[i] == r_noTexture)
			Com_Error(ERR_FATAL, "Could not load environment map %i", i);
	}

	R_InitOverlay();
}

/**
 * @sa R_FreeWorldImages
 */
void R_ShutdownImages (void)
{
	int i;
	image_t *image;

	R_CheckError();
	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (!image->texnum)
			continue;			/* free image_t slot */
		R_DeleteImage(image);
	}
	memset(imageHash, 0, sizeof(imageHash));

	R_ShutdownOverlay();
}


typedef struct {
	const char *name;
	int minimize, maximize;
} glTextureMode_t;

static const glTextureMode_t gl_texture_modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};
#define NUM_R_MODES (sizeof(gl_texture_modes) / sizeof(glTextureMode_t))

void R_TextureMode (const char *string)
{
	int i, texturemode;
	image_t *image;

	for (i = 0; i < NUM_R_MODES; i++) {
		if (!Q_strcasecmp(gl_texture_modes[i].name, string))
			break;
	}

	if (i == NUM_R_MODES) {
		Com_Printf("bad filter name\n");
		return;
	}

	texturemode = i;

	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (image->type == it_chars || image->type == it_pic || image->type == it_wrappic)
			continue;

		R_BindTexture(image->texnum);
		if (r_config.anisotropic)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_config.maxAnisotropic);
		R_CheckError();
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_texture_modes[texturemode].minimize);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_texture_modes[texturemode].maximize);
		R_CheckError();
	}
}

typedef struct {
	const char *name;
	int mode;
} gltmode_t;

static const gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_R_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof(gltmode_t))

void R_TextureAlphaMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_ALPHA_MODES; i++) {
		if (!Q_strcasecmp(gl_alpha_modes[i].name, string))
			break;
	}

	if (i == NUM_R_ALPHA_MODES) {
		Com_Printf("bad alpha texture mode name\n");
		return;
	}

	r_config.gl_alpha_format = gl_alpha_modes[i].mode;
}

static const gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_R_SOLID_MODES (sizeof(gl_solid_modes) / sizeof(gltmode_t))

void R_TextureSolidMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_SOLID_MODES; i++) {
		if (!Q_strcasecmp(gl_solid_modes[i].name, string))
			break;
	}

	if (i == NUM_R_SOLID_MODES) {
		Com_Printf("bad solid texture mode name\n");
		return;
	}

	r_config.gl_solid_format = gl_solid_modes[i].mode;
}
