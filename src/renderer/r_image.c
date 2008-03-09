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

/* Workaround for a warning about redeclaring the macro. jpeglib sets this macro
 * and SDL, too. */
#undef HAVE_STDLIB_H

#include <jpeglib.h>
#include <png.h>

static char glerrortex[MAX_GLERRORTEX];
static char *glerrortexend;
static image_t gltextures[MAX_GLTEXTURES];
static int numgltextures;

static byte intensitytable[256];

int gl_solid_format = GL_RGB;
int gl_alpha_format = GL_RGBA;

int gl_compressed_solid_format = 0;
int gl_compressed_alpha_format = 0;

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

#if 0
/**
 * @brief Set the anisotropic value for textures
 * @note not used atm
 * @sa R_ScaleTexture
 */
void R_UpdateAnisotropy (void)
{
	int		i;
	image_t	*glt;
	float	value;

	if (!r_state.anisotropic)
		value = 0;
	else
		value = r_state.maxAnisotropic;

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->type != it_pic) {
			R_Bind(glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, value);
			R_CheckError();
		}
	}
}
#endif

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
	int i;
	image_t *glt;

	for (i = 0; i < NUM_R_MODES; i++) {
		if (!Q_stricmp(gl_texture_modes[i].name, string))
			break;
	}

	if (i == NUM_R_MODES) {
		Com_Printf("bad filter name\n");
		return;
	}

	gl_filter_min = gl_texture_modes[i].minimize;
	gl_filter_max = gl_texture_modes[i].maximize;

	/* change all the existing mipmap texture objects */
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->type != it_pic) {
			R_Bind(glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			R_CheckError();
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
			R_CheckError();
		}
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

	for (i = 0, image = gltextures; i < numgltextures; i++, image++) {
		if (image->texnum <= 0)
			continue;
		texels += image->upload_width * image->upload_height;
		switch (image->type) {
		case it_skin:
			Com_Printf("M");
			break;
		case it_sprite:
			Com_Printf("S");
			break;
		case it_wall:
			Com_Printf("W");
			break;
		case it_pic:
			Com_Printf("P");
			break;
		default:
			Com_Printf(" ");
			break;
		}

		Com_Printf(" %3i %3i RGB: %s - shader: %s\n",
				image->upload_width, image->upload_height, image->name,
				(image->shader ? image->shader->name : "NONE"));
	}
	Com_Printf("Total textures: %i (max textures: %i)\n", numgltextures, MAX_GLTEXTURES);
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
static int R_LoadPNG (const char *name, byte **pic, int *width, int *height)
{
	int				rowptr;
	int				samples, color_type, bit_depth;
	png_structp		png_ptr;
	png_infop		info_ptr;
	png_infop		end_info;
	byte			**row_pointers;
	byte			*img;
	uint32_t		i;

	pngBuf_t		PngFileBuffer = {NULL,0};

	if (*pic != NULL)
		Sys_Error("possible mem leak in LoadPNG\n");

	/* Load the file */
	FS_LoadFile(name, (byte **)&PngFileBuffer.buffer);
	if (!PngFileBuffer.buffer)
		return 0;

	/* Parse the PNG file */
	if ((png_check_sig(PngFileBuffer.buffer, 8)) == 0) {
		Com_Printf("LoadPNG: Not a PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
	}

	PngFileBuffer.pos = 0;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,  NULL, NULL);
	if (!png_ptr) {
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
	}

	end_info = png_create_info_struct (png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
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

	img = VID_TagAlloc(vid_imagePool, info_ptr->width * info_ptr->height * 4, 0);
	if (pic)
		*pic = img;

	if (info_ptr->channels == 4) {
		for (i = 0; i < info_ptr->height; i++) {
			memcpy (img + rowptr, row_pointers[i], info_ptr->rowbytes);
			rowptr += info_ptr->rowbytes;
		}
	} else {
		uint32_t	j;

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
	return samples;
}

/**
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_FindImage
 */
void R_WritePNG (FILE *f, byte *buffer, int width, int height)
{
	int			i;
	png_structp	png_ptr;
	png_infop	info_ptr;
	png_bytep	*row_pointers;

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
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

	png_init_io(png_ptr, f);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 9);

	png_write_info(png_ptr, info_ptr);

	row_pointers = VID_TagAlloc(vid_imagePool, height * sizeof(png_bytep), 0);
	for (i = 0; i < height; i++)
		row_pointers[i] = buffer + (height - 1 - i) * 3 * width;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	VID_MemFree(row_pointers);
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
void R_LoadTGA (const char *name, byte ** pic, int *width, int *height)
{
	int i, columns, rows, row_inc, row, col;
	byte *buf_p, *buffer, *pixbuf, *targaRGBA;
	int length, samples, readPixelCount, pixelCount;
	byte palette[256][4], red, green, blue, alpha;
	qboolean compressed;
	targaHeader_t targaHeader;

	if (*pic != NULL)
		Sys_Error("R_LoadTGA: possible mem leak\n");

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

	targaRGBA = VID_TagAlloc(vid_imagePool, columns * rows * 4, 0);
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
					Sys_Error("R_LoadTGA: Unknown tga image type: %i\n", targaHeader.imageType);
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
 */
void R_WriteTGA (FILE *f, byte *buffer, int width, int height)
{
	int i, temp;
	byte *out;
	size_t size;

	/* Allocate an output buffer */
	size = (width * height * 3) + 18;
	out = VID_TagAlloc(vid_imagePool, size, 0);

	/* Fill in header */
	out[2] = 2;		/* Uncompressed type */
	out[12] = width & 255;
	out[13] = width >> 8;
	out[14] = height & 255;
	out[15] = height >> 8;
	out[16] = 24;	/* Pixel size */

	/* Copy to temporary buffer */
	memcpy(out + 18, buffer, width * height * 3);

	/* Swap rgb to bgr */
	for (i = 18; i < size; i += 3) {
		temp = out[i];
		out[i] = out[i+2];
		out[i + 2] = temp;
	}

	if (fwrite(out, 1, size, f) != size)
		Com_Printf("R_WriteTGA: Failed to write the tga file\n");

	VID_MemFree(out);
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
		Sys_Error("possible mem leak in LoadJPG\n");

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
	rgbadata = VID_TagAlloc(vid_imagePool, cinfo.output_width * cinfo.output_height * 4, 0);
	if (!rgbadata) {
		Com_Printf("Insufficient RAM for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}
	/* Allocate Scanline buffer */
	scanline = VID_TagAlloc(vid_imagePool, cinfo.output_width * components, 0);
	if (!scanline) {
		Com_Printf("Insufficient RAM for JPEG scanline buffer\n");
		VID_MemFree(rgbadata);
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
	VID_MemFree(scanline);

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
 */
void R_LoadImage (const char *name, byte **pic, int *width, int *height)
{
	char filename_temp[MAX_QPATH];
	char *ename;
	int len;

	if (!name)
		Sys_Error("R_LoadImage: NULL name");
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
	if (FS_CheckFile(filename_temp) != -1) {
		R_LoadTGA (filename_temp, pic, width, height);
		if (pic)
			return;
	}

	strcpy(ename, ".jpg");
	if (FS_CheckFile(filename_temp) != -1) {
		R_LoadJPG (filename_temp, pic, width, height);
		if (pic)
			return;
	}

	strcpy(ename, ".png");
	if (FS_CheckFile(filename_temp) != -1) {
		R_LoadPNG (filename_temp, pic, width, height);
	}
}

/* Expanded data destination object for stdio output */
typedef struct {
	struct jpeg_destination_mgr pub;	/* public fields */

	byte *outfile;				/* target stream */
	int size;
} my_destination_mgr;

typedef my_destination_mgr *my_dest_ptr;

/**
 * @brief Initialize destination --- called by jpeg_start_compress before any data is actually written
 */
static void init_destination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

	dest->pub.next_output_byte = dest->outfile;
	dest->pub.free_in_buffer = dest->size;
}

static size_t hackSize;

/**
 * @brief Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * @note *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */
static void term_destination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
	size_t datacount = dest->size - dest->pub.free_in_buffer;

	hackSize = datacount;
}

/**
 * @brief Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */
static boolean empty_output_buffer (j_compress_ptr cinfo)
{
	return TRUE;
}

/**
 * @brief Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */
static void jpegDest (j_compress_ptr cinfo, byte * outfile, int size)
{
	my_dest_ptr dest;

	/* The destination object is made permanent so that multiple JPEG images
	 * can be written to the same file without re-executing jpeg_stdio_dest.
	 * This makes it dangerous to use this manager and a different destination
	 * manager serially with the same JPEG object, because their private object
	 * sizes may be different.  Caveat programmer.
	 */
	if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_destination_mgr));
	}

	dest = (my_dest_ptr) cinfo->dest;
	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;
	dest->outfile = outfile;
	dest->size = size;
}

/**
 * @sa R_LoadJPG
 */
size_t R_SaveJPGToBuffer (byte * buffer, int quality, int image_width, int image_height, byte * image_buffer)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	/* pointer to JSAMPLE row[s] */
	JSAMPROW row_pointer[1];

	/* physical row width in image buffer */
	int row_stride;

	/* Step 1: allocate and initialize JPEG compression object */
	cinfo.err = jpeg_std_error(&jerr);
	/* colorspace */
	cinfo.jpeg_color_space = JCS_RGB;

	/* Now we can initialize the JPEG compression object. */
	jpeg_create_compress(&cinfo);

	/* Step 2: specify data destination (eg, a file) */
	/* Note: steps 2 and 3 can be done in either order. */
	jpegDest(&cinfo, buffer, image_width * image_height * RGB_PIXELSIZE);

	/* Step 3: set parameters for compression */
	/* image width and height, in pixels */
	cinfo.image_width = image_width;
	cinfo.image_height = image_height;
	/* # of color components per pixel */
	cinfo.input_components = RGB_PIXELSIZE;
	/* colorspace of input image */
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	/* limit to baseline-JPEG values */
	jpeg_set_quality(&cinfo, quality, TRUE);
	/* If quality is set high, disable chroma subsampling */
	if (quality >= 85) {
		cinfo.comp_info[0].h_samp_factor = 1;
		cinfo.comp_info[0].v_samp_factor = 1;
	}

	/* Step 4: Start compressor */
	jpeg_start_compress(&cinfo, TRUE);

	/* Step 5: while (scan lines remain to be written) */
	/*           jpeg_write_scanlines(...); */
	/* JSAMPLEs per row in image_buffer */
	row_stride = image_width * RGB_PIXELSIZE;

	while (cinfo.next_scanline < cinfo.image_height) {
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
		 * Here the array is only one element long, but you could pass
		 * more than one scanline at a time if that's more convenient. */
		row_pointer[0] = &image_buffer[((cinfo.image_height - 1) * row_stride) - cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	/* Step 6: Finish compression */
	jpeg_finish_compress(&cinfo);

	/* Step 7: release JPEG compression object */
	jpeg_destroy_compress(&cinfo);

	/* And we're done! */
	return hackSize;
}

/**
 * @sa R_ScreenShot_f
 * @sa R_LoadJPG
 */
void R_WriteJPG (FILE *f, byte *buffer, int width, int height, int quality)
{
	int offset, w3;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *s;

	/* Initialise the jpeg compression object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, f);

	/* Setup jpeg parameters */
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	cinfo.progressive_mode = TRUE;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, qtrue);	/* start compression */
	jpeg_write_marker(&cinfo, JPEG_COM, (const byte *) "UFO:AI", (uint32_t) strlen("UFO:AI"));

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

static void R_ScaleTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	int i, j;
	unsigned *inrow, *inrow2;
	unsigned frac, fracstep;
	unsigned p1[1024], p2[1024];
	byte *pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth * 0x10000 / outwidth;

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
		inrow = in + inwidth * (int) ((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int) ((i + 0.75) * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j++) {
			pix1 = (byte *) inrow + p1[j];
			pix2 = (byte *) inrow + p2[j];
			pix3 = (byte *) inrow2 + p1[j];
			pix4 = (byte *) inrow2 + p2[j];
			((byte *) (out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *) (out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *) (out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *) (out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

#define FILTER_SIZE	5
#define BLUR_FILTER	0
#define LIGHT_BLUR	1
#define EDGE_FILTER	2
#define EMBOSS_FILTER	3
#define EMBOSS_FILTER_LOW	4
#define EMBOSS_FILTER_HIGH	5
#define EMBOSS_FILTER_2	6
#define DARKEN_FILTER 7
#define SHARPEN_FILTER 8

static const float FilterMatrix[][FILTER_SIZE][FILTER_SIZE] = {
	/* regular blur */
	{
	 {0, 0, 0, 0, 0},
	 {0, 1, 1, 1, 0},
	 {0, 1, 1, 1, 0},
	 {0, 1, 1, 1, 0},
	 {0, 0, 0, 0, 0},
	 },
	/* light blur */
	{
	 {0, 0, 0, 0, 0},
	 {0, 1, 1, 1, 0},
	 {0, 1, 4, 1, 0},
	 {0, 1, 1, 1, 0},
	 {0, 0, 0, 0, 0},
	 },
	/* find edges */
	{
	 {0, 0, 0, 0, 0},
	 {0, -1, -1, -1, 0},
	 {0, -1, 8, -1, 0},
	 {0, -1, -1, -1, 0},
	 {0, 0, 0, 0, 0},
	 },
	/* emboss */
	{
	 {-1, -1, -1, -1, 0},
	 {-1, -1, -1, 0, 1},
	 {-1, -1, 0, 1, 1},
	 {-1, 0, 1, 1, 1},
	 {0, 1, 1, 1, 1},
	 },
	/* emboss_low */
	{
	 {-0.7, -0.7, -0.7, -0.7, 0},
	 {-0.7, -0.7, -0.7,  0, 0.7},
	 {-0.7, -0.7,  0,  0.7, 0.7},
	 {-0.7,  0,  0.7,  0.7, 0.7},
	 { 0,  0.7,  0.7,  0.7, 0.7},
	 },
	/* emboss_high */
	{
	 {-2, -2, -2, -2, 0},
	 {-2, -2, -2, 0, 2},
	 {-2, -1, 0, 2, 2},
	 {-2, 0, 2, 2, 2},
	 {0, 2, 2, 2, 2},
	 },
	/* emboss2 */
	{
	 {1, 1, 1, 1, 0},
	 {1, 1, 1, 0, -1},
	 {1, 1, 0, -1, -1},
	 {1, 0, -1, -1, -1},
	 {0, -1, -1, -1, -1},
	 },
	/* darken */
	{
	 {0, 0, 0, 0, 0},
	 {0, 0, 0, 0, 0},
	 {0, 0, 0, 0, 0},
	 {0, 0, 0, 0, 0},
	 {0, 0, 0, 0, 0},
	},
	/* sharpen */
	{
	 {1, 2,  0,  -2,   1},
	 {4, 8,  0,  -8,  -4},
	 {6, 12, 0, -12,  -6},
	 {4, 8,  0,  -8,  -4},
	 {1, 2,  0,  -2,  -1}
	}
};

/**
 * @brief Applies a 5 x 5 filtering matrix to the texture, then runs it through a simulated OpenGL texture environment
 * blend with the original data to derive a new texture.  Freaky, funky, and *f--king* *fantastic*.  You can do
 * reasonable enough "fake bumpmapping" with this baby...
 *
 * Filtering algorithm from http://www.student.kuleuven.ac.be/~m0216922/CG/filtering.html
 * All credit due
 */
static void R_FilterTexture (int filterindex, unsigned int *data, int width, int height, float factor, float bias, qboolean greyscale, int blend)
{
	int i;
	int x;
	int y;
	int filterX;
	int filterY;
	unsigned int *temp;
	size_t temp_size = width * height * 4;

	/* allocate a temp buffer */
	temp = VID_TagAlloc(vid_imagePool, temp_size, 0);
	if (!temp)
		Sys_Error("TagMalloc: failed on allocation of "UFO_SIZE_T" bytes", temp_size);

	for (x = 0; x < width; x++) {
		for (y = 0; y < height; y++) {
			float rgbFloat[3] = { 0, 0, 0 };

			for (filterX = 0; filterX < FILTER_SIZE; filterX++) {
				for (filterY = 0; filterY < FILTER_SIZE; filterY++) {
					int imageX = (x - (FILTER_SIZE / 2) + filterX + width) % width;
					int imageY = (y - (FILTER_SIZE / 2) + filterY + height) % height;

					/* casting's a unary operation anyway, so the othermost set of brackets in the left part */
					/* of the rvalue should not be necessary... but i'm paranoid when it comes to C... */
					rgbFloat[0] += ((float) ((byte *) & data[imageY * width + imageX])[0]) * FilterMatrix[filterindex][filterX][filterY];
					rgbFloat[1] += ((float) ((byte *) & data[imageY * width + imageX])[1]) * FilterMatrix[filterindex][filterX][filterY];
					rgbFloat[2] += ((float) ((byte *) & data[imageY * width + imageX])[2]) * FilterMatrix[filterindex][filterX][filterY];
				}
			}

			/* multiply by factor, add bias, and clamp */
			for (i = 0; i < 3; i++) {
				rgbFloat[i] *= factor;
				rgbFloat[i] += bias;

				if (rgbFloat[i] < 0)
					rgbFloat[i] = 0;
				if (rgbFloat[i] > 255)
					rgbFloat[i] = 255;
			}

			if (greyscale) {
				/* NTSC greyscale conversion standard */
				float avg = (rgbFloat[0] * 30 + rgbFloat[1] * 59 + rgbFloat[2] * 11) / 100;

				/* divide by 255 so GL operations work as expected */
				rgbFloat[0] = avg / 255.0;
				rgbFloat[1] = avg / 255.0;
				rgbFloat[2] = avg / 255.0;
			}

			/* write to temp - first, write data in (to get the alpha channel quickly and */
			/* easily, which will be left well alone by this particular operation...!) */
			temp[y * width + x] = data[y * width + x];

			/* now write in each element, applying the blend operator.  blend */
			/* operators are based on standard OpenGL TexEnv modes, and the */
			/* formulae are derived from the OpenGL specs (http://www.opengl.org). */
			for (i = 0; i < 3; i++) {
				/* divide by 255 so GL operations work as expected */
				float TempTarget;
				float SrcData = ((float) ((byte *) & data[y * width + x])[i]) / 255.0;

				switch (blend) {
				case BLEND_ADD:
					TempTarget = rgbFloat[i] + SrcData;
					break;

				case BLEND_BLEND:
					/* default is FUNC_ADD here */
					/* CsS + CdD works out as Src * Dst * 2 */
					TempTarget = rgbFloat[i] * SrcData * 2.0;
					break;

				case BLEND_REPLACE:
					TempTarget = rgbFloat[i];
					break;

				case BLEND_FILTER:
					/* same as default */
				default:
					TempTarget = rgbFloat[i] * SrcData;
					break;
				}

				/* multiply back by 255 to get the proper byte scale */
				TempTarget *= 255.0;

				/* bound the temp target again now, cos the operation may have thrown it out */
				if (TempTarget < 0)
					TempTarget = 0;
				if (TempTarget > 255)
					TempTarget = 255;

				/* and copy it in */
				((byte *) & temp[y * width + x])[i] = (byte) TempTarget;
			}
		}
	}

#if 0
	/* copy temp back to data */
	for (i = 0; i < (width * height); i++)
		data[i] = temp[i];
#else
	memcpy(data, temp, width * height * 4);
#endif
	/* release the temp buffer */
	VID_MemFree(temp);
}

/**
 * @brief Uploads the opengl texture to the server
 */
static void R_UploadTexture (unsigned *data, int width, int height, image_t* image)
{
	unsigned *scaled;
	int samples;
	int scaled_width, scaled_height;
	int i, c;
	byte *scan;
	qboolean mipmap = (image->type != it_pic);
	qboolean clamp = (image->type == it_pic);

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	while (scaled_width > r_config.maxTextureSize || scaled_height > r_config.maxTextureSize) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

	scaled = data;

	if (scaled_width != width || scaled_height != height) {  /* whereas others need to be scaled */
		scaled = (unsigned *)VID_TagAlloc(vid_imagePool, scaled_width * scaled_height * sizeof(unsigned), 0);
		R_ScaleTexture(data, width, height, scaled, scaled_width, scaled_height);
	}

	/* scan the texture for any non-255 alpha */
	c = scaled_width * scaled_height;
	scan = ((byte *) scaled) + 3;
	samples = gl_compressed_solid_format ? gl_compressed_solid_format : gl_solid_format;
	for (i = 0; i < c; i++, scan += 4) {
		if (*scan != 255) {
			samples = gl_compressed_alpha_format ? gl_compressed_alpha_format : gl_alpha_format;
			break;
		}
	}

	image->has_alpha = (samples == gl_alpha_format || samples == gl_compressed_alpha_format);
	image->upload_width = scaled_width;	/* after power of 2 and scales */
	image->upload_height = scaled_height;

	if (r_imagefilter->integer && image->shader) {
		Com_DPrintf(DEBUG_RENDERER, "Using image filter %s\n", image->shader->name);
		/* emboss filter */
		if (image->shader->emboss)
			R_FilterTexture(EMBOSS_FILTER, scaled, scaled_width, scaled_height, 1, 128, qtrue, image->shader->glMode);
		else if (image->shader->emboss2)
			R_FilterTexture(EMBOSS_FILTER_2, scaled, scaled_width, scaled_height, 1, 128, qtrue, image->shader->glMode);
		else if (image->shader->embossHigh)
			R_FilterTexture(EMBOSS_FILTER_HIGH, scaled, scaled_width, scaled_height, 1, 128, qtrue, image->shader->glMode);
		else if (image->shader->embossLow)
			R_FilterTexture(EMBOSS_FILTER_LOW, scaled, scaled_width, scaled_height, 1, 128, qtrue, image->shader->glMode);

		if (image->shader->blur)
			R_FilterTexture(BLUR_FILTER, scaled, scaled_width, scaled_height, 1, 128, qtrue, image->shader->glMode);
		if (image->shader->light)
			R_FilterTexture(LIGHT_BLUR, scaled, scaled_width, scaled_height, 1, 128, qtrue, image->shader->glMode);
		if (image->shader->edge)
			R_FilterTexture(EDGE_FILTER, scaled, scaled_width, scaled_height, 1, 128, qtrue, image->shader->glMode);
	}

	if (mipmap) {
		if (r_anisotropic->integer && r_state.anisotropic) {
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_anisotropic->value);
			R_CheckError();
		}
		if (r_texture_lod->integer && r_state.lod_bias) {
			qglTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, r_texture_lod->value);
			R_CheckError();
		}
	}

	/* and mipmapped */
	if (mipmap) {
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	} else {
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	R_CheckError();

	if (clamp) {
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		R_CheckError();
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		R_CheckError();
	}

	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	R_CheckError();

	if (scaled != data)
		VID_MemFree(scaled);
}

/**
 * @brief Applies blurring to a texture
 * @sa R_BuildLightMap
 */
void R_SoftenTexture (byte *in, int width, int height, int bpp)
{
	byte *out;
	int i, j, k;
	byte *src, *dest;
	byte *u, *d, *l, *r;

	/* soften into a copy of the original image, as in-place would be incorrect */
	out = (byte *)VID_TagAlloc(vid_imagePool, width * height * bpp, 0);
	if (!out)
		Sys_Error("TagMalloc: failed on allocation of %i bytes for R_SoftenTexture", width * height * bpp);

	memcpy(out, in, width * height * bpp);

	for (i = 1; i < height - 1; i++){
		for (j = 1; j < width - 1; j++){

			src = in + ((i * width) + j) * bpp;  /* current input pixel */

			u = (src - (width * bpp));  /* and it's neighbors */
			d = (src + (width * bpp));
			l = (src - (1 * bpp));
			r = (src + (1 * bpp));

			dest = out + ((i * width) + j) * bpp;  /* current output pixel */

			for (k = 0; k < bpp; k++)
				dest[k] = (u[k] + d[k] + l[k] + r[k]) / 4;
		}
	}

	/* copy the softened image over the input image, and free it */
	memcpy(in, out, width * height * bpp);
	VID_MemFree(out);
}

#define DAN_WIDTH	512
#define DAN_HEIGHT	256

#define DAWN		0.03

static byte DaNalpha[DAN_WIDTH * DAN_HEIGHT];
image_t *DaN;

/**
 * @brief Applies alpha values to the night overlay image for 2d geoscape
 * @param[in] q
 */
void R_CalcDayAndNight (float q)
{
	int x, y;
	float phi, dphi, a, da;
	float sin_q, cos_q, root;
	float pos;
	float sin_phi[DAN_WIDTH], cos_phi[DAN_WIDTH];
	byte *px;

	/* get image */
	if (!DaN) {
		if (numgltextures >= MAX_GLTEXTURES)
			Com_Error(ERR_DROP, "MAX_GLTEXTURES");
		DaN = &gltextures[numgltextures++];
		DaN->width = DAN_WIDTH;
		DaN->height = DAN_HEIGHT;
		DaN->type = it_pic;
		DaN->texnum = TEXNUM_IMAGES + (DaN - gltextures);
	}
	R_Bind(DaN->texnum);

	/* init geometric data */
	dphi = (float) 2 * M_PI / DAN_WIDTH;

	da = M_PI / 2 * (HIGH_LAT - LOW_LAT) / DAN_HEIGHT;

	/* precalculate trigonometric functions */
	sin_q = sin(q);
	cos_q = cos(q);
	for (x = 0; x < DAN_WIDTH; x++) {
		phi = x * dphi - q;
		sin_phi[x] = sin(phi);
		cos_phi[x] = cos(phi);
	}

	/* calculate */
	px = DaNalpha;
	for (y = 0; y < DAN_HEIGHT; y++) {
		a = sin(M_PI / 2 * HIGH_LAT - y * da);
		root = sqrt(1 - a * a);
		for (x = 0; x < DAN_WIDTH; x++) {
			pos = sin_phi[x] * root * sin_q - (a * SIN_ALPHA + cos_phi[x] * root * COS_ALPHA) * cos_q;

			if (pos >= DAWN)
				*px++ = 255;
			else if (pos <= -DAWN)
				*px++ = 0;
			else
				*px++ = (byte) (128.0 * (pos / DAWN + 1));
		}
	}

	/* upload alpha map */
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, DAN_WIDTH, DAN_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, DaNalpha);
	R_CheckError();
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	R_CheckError();
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	R_CheckError();
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	R_CheckError();
}


/**
 * @brief This is also used as an entry point for the generated r_notexture
 */
image_t *R_LoadPic (const char *name, byte * pic, int width, int height, imagetype_t type, int bits)
{
	image_t *image;
	int i;
	size_t len;

	/* find a free image_t */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
		if (!image->texnum)
			break;

	if (i == numgltextures) {
		if (numgltextures == MAX_GLTEXTURES)
			Com_Error(ERR_DROP, "MAX_GLTEXTURES");
		numgltextures++;
	}
	image = &gltextures[i];

	image->type = type;

	len = strlen(name);
	if (len >= sizeof(image->name))
		Com_Error(ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
	Q_strncpyz(image->name, name, MAX_QPATH);
	image->registration_sequence = registration_sequence;
	/* drop extension */
	if (len >= 4 && (*image).name[len - 4] == '.')
		(*image).name[len - 4] = '\0';

	image->width = width;
	image->height = height;

	if (image->type == it_pic && strstr(image->name, "_noclamp"))
		image->type = it_wrappic;

	image->shader = R_GetShaderForImage(image->name);
	if (image->shader)
		Com_DPrintf(DEBUG_RENDERER, "R_LoadPic: Shader found: '%s'\n", image->name);

	image->texnum = TEXNUM_IMAGES + (image - gltextures);
	if (pic) {
		R_Bind(image->texnum);
		R_UploadTexture((unsigned *) pic, width, height, image);
	}
	return image;
}

/**
 * @brief Finds an image for a shader
 */
image_t *R_FindImageForShader (const char *name)
{
	int i;
	image_t *image;

	/* look for it */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
		if (!Q_strncmp(name, image->name, MAX_VAR))
			return image;
	return NULL;
}

/**
 * @brief Finds or loads the given image
 * @sa R_RegisterPic
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
	char *etex;
	image_t *image;
	int i;
	size_t len;
	byte *pic = NULL, *palette;
	int width, height;

	if (!pname)
		Sys_Error("R_FindImage: NULL name");
	len = strlen(pname);
	if (len < 5)
		return NULL;

	/* drop extension */
	Q_strncpyz(lname, pname, MAX_QPATH);
	if (lname[len - 4] == '.')
		len -= 4;
	ename = &(lname[len]);
	*ename = 0;

	/* look for it */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
		if (!Q_strncmp(lname, image->name, MAX_QPATH)) {
			image->registration_sequence = registration_sequence;
			return image;
		}

	/* look for it in the error list */
	etex = glerrortex;
	while ((len = strlen(etex)) != 0) {
		if (!Q_strncmp(lname, etex, MAX_QPATH))
			/* it's in the error list */
			return r_notexture;

		etex += len + 1;
	}

	/* load the pic from disk */
	image = NULL;
	pic = NULL;
	palette = NULL;

	strcpy(ename, ".tga");
	if (FS_CheckFile(lname) != -1) {
		R_LoadTGA(lname, &pic, &width, &height);
		if (pic) {
			image = R_LoadPic(lname, pic, width, height, type, 32);
			goto end;
		}
	}

	strcpy(ename, ".png");
	if (FS_CheckFile(lname) != -1) {
		R_LoadPNG(lname, &pic, &width, &height);
		if (pic) {
			image = R_LoadPic(lname, pic, width, height, type, 32);
			goto end;
		}
	}

	strcpy(ename, ".jpg");
	if (FS_CheckFile(lname) != -1) {
		R_LoadJPG(lname, &pic, &width, &height);
		if (pic) {
			image = R_LoadPic(lname, pic, width, height, type, 32);
			goto end;
		}
	}

	/* no fitting texture found */
	/* add to error list */
	image = r_notexture;

	*ename = 0;
#ifdef DEBUG
	Com_Printf("R_FindImage: Can't find %s (%s) - file: %s, line %i\n", lname, pname, file, line);
#else
	Com_Printf("R_FindImage: Can't find %s (%s)\n", lname, pname);
#endif

	if ((glerrortexend - glerrortex) + strlen(lname) < MAX_GLERRORTEX) {
		Q_strncpyz(glerrortexend, lname, MAX_QPATH);
		glerrortexend += strlen(lname) + 1;
	} else {
		Com_Error(ERR_DROP, "MAX_GLERRORTEX");
	}

  end:
	if (pic)
		VID_MemFree(pic);
	if (palette)
		VID_MemFree(palette);

	return image;
}

/**
 * @brief Any image that was not touched on this registration sequence will be freed
 * @sa R_ShutdownImages
 */
void R_FreeUnusedImages (void)
{
	int i;
	image_t *image;

	/* never free r_notexture or particle texture */
	r_notexture->registration_sequence = registration_sequence;

	R_CheckError();
	for (i = 0, image = gltextures; i < numgltextures; i++, image++) {
		if (image->registration_sequence == registration_sequence)
			continue;			/* used this sequence */
		if (!image->registration_sequence)
			continue;			/* free image_t slot */
		if (image->type == it_pic || image->type == it_wrappic || image->type == it_static)
			continue;			/* fix this! don't free pics */
		/* free it */
		qglDeleteTextures(1, (GLuint *) &image->texnum);
		R_CheckError();
		memset(image, 0, sizeof(*image));
	}
}

void R_InitImages (void)
{
	int i, j;

	registration_sequence = 1;
	numgltextures = 0;
	glerrortex[0] = 0;
	glerrortexend = glerrortex;
	DaN = NULL;

	if (r_intensity->value <= 1)
		Cvar_Set("r_intensity", "1");

	r_state.inverse_intensity = 1 / r_intensity->value;

	for (i = 0; i < 256; i++) {
		j = i * r_intensity->value;
		if (j > 255)
			j = 255;
		intensitytable[i] = j;
	}
}

/**
 * @sa R_FreeUnusedImages
 */
void R_ShutdownImages (void)
{
	int i;
	image_t *image;

	R_CheckError();
	for (i = 0, image = gltextures; i < numgltextures; i++, image++) {
		if (!image->registration_sequence)
			continue;			/* free image_t slot */
		/* free it */
		qglDeleteTextures(1, (GLuint *) &image->texnum);
		R_CheckError();
		memset(image, 0, sizeof(*image));
	}
}
