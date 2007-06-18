/**
 * @file lbmlib.c
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

#include <limits.h>

#include "cmdlib.h"
#include "lbmlib.h"


/*
============================================================================
LBM STUFF
============================================================================
*/


typedef unsigned char	UBYTE;
/*conflicts with windows typedef short			WORD; */
typedef unsigned short	UWORD;
typedef long			LONG;

typedef enum
{
	ms_none,
	ms_mask,
	ms_transcolor,
	ms_lasso
} mask_t;

typedef enum
{
	cm_none,
	cm_rle1
} compress_t;

typedef struct
{
	UWORD		w,h;
	short		x,y;
	UBYTE		nPlanes;
	UBYTE		masking;
	UBYTE		compression;
	UBYTE		pad1;
	UWORD		transparentColor;
	UBYTE		xAspect,yAspect;
	short		pageWidth,pageHeight;
} bmhd_t;

static bmhd_t bmhd;						/* will be in native byte order */

#define FORMID ('F'+('O'<<8)+((int)'R'<<16)+((int)'M'<<24))
#define ILBMID ('I'+('L'<<8)+((int)'B'<<16)+((int)'M'<<24))
#define PBMID  ('P'+('B'<<8)+((int)'M'<<16)+((int)' '<<24))
#define BMHDID ('B'+('M'<<8)+((int)'H'<<16)+((int)'D'<<24))
#define BODYID ('B'+('O'<<8)+((int)'D'<<16)+((int)'Y'<<24))
#define CMAPID ('C'+('M'<<8)+((int)'A'<<16)+((int)'P'<<24))


/**
 * @brief
 */
int Align (int l)
{
	if (l&1)
		return l+1;
	return l;
}



/**
 * @brief
 * @note Source must be evenly aligned!
 */
static byte *LBMRLEDecompress (byte *source, byte *unpacked, int bpwidth)
{
	int count;
	byte b,rept;

	count = 0;

	do {
		rept = *source++;

		if (rept > 0x80) {
			rept = (rept^0xff)+2;
			b = *source++;
			memset(unpacked,b,rept);
			unpacked += rept;
		} else if (rept < 0x80) {
			rept++;
			memcpy(unpacked,source,rept);
			unpacked += rept;
			source += rept;
		} else
			rept = 0;               /* rept of 0x80 is NOP */

		count += rept;
	} while (count<bpwidth);

	if (count>bpwidth)
		Error ("Decompression exceeded width!\n");

	return source;
}


/**
 * @brief
 * @sa WriteLBMfile
 */
void LoadLBM (const char *filename, byte **picture, byte **palette)
{
	byte *LBMbuffer, *picbuffer, *cmapbuffer;
	byte *LBM_P, *LBMEND_P, *pic_p, *body_p;
	int formtype,formlength, chunktype,chunklength, y;

	/* qiet compiler warnings */
	picbuffer = NULL;
	cmapbuffer = NULL;

	/* load the LBM */
	LoadFile (filename, (void **)&LBMbuffer);

	/* parse the LBM header */
	LBM_P = LBMbuffer;
	if ( *(int *)LBMbuffer != LittleLong(FORMID) )
		Error ("No FORM ID at start of file!\n");

	LBM_P += 4;
	formlength = BigLong( *(int *)LBM_P );
	LBM_P += 4;
	LBMEND_P = LBM_P + Align(formlength);

	formtype = LittleLong(*(int *)LBM_P);

	if (formtype != ILBMID && formtype != PBMID)
		Error ("Unrecognized form type: %c%c%c%c\n", formtype&0xff
		,(formtype>>8)&0xff,(formtype>>16)&0xff,(formtype>>24)&0xff);

	LBM_P += 4;

	/* parse chunks */
	while (LBM_P < LBMEND_P) {
		chunktype = LBM_P[0] + (LBM_P[1]<<8) + (LBM_P[2]<<16) + (LBM_P[3]<<24);
		LBM_P += 4;
		chunklength = LBM_P[3] + (LBM_P[2]<<8) + (LBM_P[1]<<16) + (LBM_P[0]<<24);
		LBM_P += 4;

		switch (chunktype) {
		case BMHDID:
			memcpy (&bmhd,LBM_P,sizeof(bmhd));
			bmhd.w = BigShort(bmhd.w);
			bmhd.h = BigShort(bmhd.h);
			bmhd.x = BigShort(bmhd.x);
			bmhd.y = BigShort(bmhd.y);
			bmhd.pageWidth = BigShort(bmhd.pageWidth);
			bmhd.pageHeight = BigShort(bmhd.pageHeight);
			break;

		case CMAPID:
			cmapbuffer = malloc (768);
			memset (cmapbuffer, 0, 768);
			memcpy (cmapbuffer, LBM_P, chunklength);
			break;

		case BODYID:
			body_p = LBM_P;

			pic_p = picbuffer = malloc(bmhd.w*bmhd.h);
			if (formtype == PBMID) {
				/* unpack PBM */
				for (y = 0; y < bmhd.h; y++, pic_p += bmhd.w) {
					if (bmhd.compression == cm_rle1)
						body_p = LBMRLEDecompress((byte *)body_p
						, pic_p , bmhd.w);
					else if (bmhd.compression == cm_none) {
						memcpy(pic_p,body_p,bmhd.w);
						body_p += Align(bmhd.w);
					}
				}
			} else {
				/* unpack ILBM */
				Error("%s is an interlaced LBM, not packed", filename);
			}
			break;
		}
		LBM_P += Align(chunklength);
	}
	free (LBMbuffer);
	*picture = picbuffer;

	if (palette)
		*palette = cmapbuffer;
}


/*
============================================================================
WRITE LBM
============================================================================
*/

/**
 * @brief
 * @sa LoadLBM
 */
void WriteLBMfile (const char *filename, byte *data, int width, int height, byte *palette)
{
	byte *lbm, *lbmptr;
	int *formlength, *bmhdlength, *cmaplength, *bodylength, length;
	bmhd_t basebmhd;

	lbm = lbmptr = malloc (width*height+1000);

	/* start FORM */
	*lbmptr++ = 'F';
	*lbmptr++ = 'O';
	*lbmptr++ = 'R';
	*lbmptr++ = 'M';

	formlength = (int*)lbmptr;
	lbmptr+=4;                      /* leave space for length */

	*lbmptr++ = 'P';
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = ' ';

	/* write BMHD */
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = 'H';
	*lbmptr++ = 'D';

	bmhdlength = (int *)lbmptr;
	lbmptr+=4;                      /* leave space for length */

	memset (&basebmhd,0,sizeof(basebmhd));
	basebmhd.w = BigShort((short)width);
	basebmhd.h = BigShort((short)height);
	basebmhd.nPlanes = BigShort(8);
	basebmhd.xAspect = BigShort(5);
	basebmhd.yAspect = BigShort(6);
	basebmhd.pageWidth = BigShort((short)width);
	basebmhd.pageHeight = BigShort((short)height);

	memcpy (lbmptr,&basebmhd,sizeof(basebmhd));
	lbmptr += sizeof(basebmhd);

	length = lbmptr-(byte *)bmhdlength-4;
	*bmhdlength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          /* pad chunk to even offset */

	/* write CMAP */
	*lbmptr++ = 'C';
	*lbmptr++ = 'M';
	*lbmptr++ = 'A';
	*lbmptr++ = 'P';

	cmaplength = (int *)lbmptr;
	lbmptr+=4;                      /* leave space for length */

	memcpy (lbmptr,palette,768);
	lbmptr += 768;

	length = lbmptr-(byte *)cmaplength-4;
	*cmaplength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          /* pad chunk to even offset */

	/* write BODY */
	*lbmptr++ = 'B';
	*lbmptr++ = 'O';
	*lbmptr++ = 'D';
	*lbmptr++ = 'Y';

	bodylength = (int *)lbmptr;
	lbmptr+=4;                      /* leave space for length */

	memcpy (lbmptr,data,width*height);
	lbmptr += width*height;

	length = lbmptr-(byte *)bodylength-4;
	*bodylength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          /* pad chunk to even offset */

	/* done */
	length = lbmptr-(byte *)formlength-4;
	*formlength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          /* pad chunk to even offset */

	/* write output file */
	SaveFile (filename, lbm, lbmptr-lbm);
	free (lbm);
}


/*
============================================================================
LOAD PCX
============================================================================
*/

/**
 * @brief
 * @sa WritePCXfile
 */
void LoadPCX (const char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;

	/* load the file */
	len = LoadFile(filename, (void **)&raw);

	/* parse the PCX file */
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

	pcx->xmin = LittleShort(pcx->xmin);
	pcx->ymin = LittleShort(pcx->ymin);
	pcx->xmax = LittleShort(pcx->xmax);
	pcx->ymax = LittleShort(pcx->ymax);
	pcx->hres = LittleShort(pcx->hres);
	pcx->vres = LittleShort(pcx->vres);
	pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
	pcx->palette_type = LittleShort(pcx->palette_type);

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
		Error("Bad pcx file %s", filename);

	if (palette) {
		*palette = malloc(768);
		memcpy(*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax+1;
	if (height)
		*height = pcx->ymax+1;

	if (!pic) {
		/* free palette in CalcTextureReflectivity */
		return;
	}

	out = malloc((pcx->ymax + 1) * (pcx->xmax + 1));
	if (!out)
		Error("Skin_Cache: couldn't allocate");

	*pic = out;

	pix = out;

	for (y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1) {
		for (x = 0; x <= pcx->xmax; ) {
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			} else
				runLength = 1;

			while (runLength-- > 0)
				pix[x++] = dataByte;
		}
	}

	if (raw - (byte *)pcx > len)
		Error("PCX file %s was malformed", filename);

	free(pcx);
}

/**
 * @brief
 * @sa LoadPCX
 */
void WritePCXfile (const char *filename, byte *data, int width, int height, byte *palette)
{
	int		i, j, length;
	pcx_t	*pcx;
	byte		*pack;

	pcx = malloc(width * height * 2 + 1000);
	memset(pcx, 0, sizeof(*pcx));

	pcx->manufacturer = 0x0a;	/* PCX id */
	pcx->version = 5;			/* 256 color */
 	pcx->encoding = 1;		/* uncompressed */
	pcx->bits_per_pixel = 8;		/* 256 color */
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	pcx->color_planes = 1;		/* chunky image */
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);		/* not a grey scale */

	/* pack the image */
	pack = &pcx->data;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			if ((*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else {
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}
	}

	/* write the palette */
	*pack++ = 0x0c;	/* palette ID byte */
	for (i = 0; i < 768; i++)
		*pack++ = *palette++;

	/* write output file */
	length = pack - (byte *)pcx;
	SaveFile(filename, pcx, length);

	free(pcx);
}


/*
============================================================================
LOAD IMAGE
============================================================================
*/

/**
 * @brief Will load either an lbm or pcx, depending on extension.
 * @note Any of the return pointers can be NULL if you don't want them.
 * @sa LoadPCX
 * @sa LoadLBM
 * @sa Save256Image
 * @note LoadPCX and LoadLBM will allocate pixles and palette - make sure you free them
 * after using them
 */
void Load256Image (const char *name, byte **pixels, byte **palette, int *width, int *height)
{
	char	ext[128];

	ExtractFileExtension(name, ext);
	if (!Q_strcasecmp(ext, "lbm")) {
		LoadLBM(name, pixels, palette);
		if (width)
			*width = bmhd.w;
		if (height)
			*height = bmhd.h;
	} else if (!Q_strcasecmp (ext, "pcx")) {
		LoadPCX(name, pixels, palette, width, height);
	} else
		Error("%s doesn't have a known image extension", name);
}


/**
 * @brief Will save either an lbm or pcx, depending on extension.
 * @sa Load256Image
 * @sa WriteLBMfile
 * @sa WritePCXfile
 */
void Save256Image (const char *name, byte *pixels, byte *palette, int width, int height)
{
	char ext[128];

	ExtractFileExtension (name, ext);
	if (!Q_strcasecmp (ext, "lbm"))
		WriteLBMfile (name, pixels, width, height, palette);
	else if (!Q_strcasecmp (ext, "pcx"))
		WritePCXfile (name, pixels, width, height, palette);
	else
		Error ("%s doesn't have a known image extension", name);
}


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

/**
 * @brief
 */
extern void LoadTGA (const char *name, byte ** pic, int *width, int *height)
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
		Error("possible mem leak in LoadTGA\n");

	/* load the file */
	length = TryLoadFile(name, (void **) &buffer);
	if (!buffer)
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
		Error("LoadTGA: Only type 2 and 10 targa RGB images supported (%s) (type: %i)\n", name, targa_header.image_type);

	if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		Error("LoadTGA: Only 32 or 24 bit images supported (no colormaps) (%s) (pixel_size: %i)\n", name, targa_header.pixel_size);

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = malloc(numPixels * 4);
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

	FreeFile(buffer);
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

	size = sizeof(miptex_t) + width * height * 4;
	*mt = (miptex_t *)malloc(size);

	memset(*mt, 0, size);

	dest = (byte*)(*mt) + sizeof(miptex_t);
	memcpy(dest, pic, width * height * 4);  /* stuff RGBA into this opaque space */
	free(pic);

	/* copy relevant header fields to miptex_t */
	(*mt)->width = width;
	(*mt)->height = height;
	(*mt)->offsets[0] = sizeof(miptex_t);
	(*mt)->value = 0;
	(*mt)->flags = 0;
	(*mt)->contents = 0;
	(*mt)->animname[0] = 0;

	return 0;
}


/*
=================================================================
JPEG LOADING
By Robert 'Heffo' Heffernan
=================================================================
*/

/**
 * @brief
 */
static void jpg_null (j_decompress_ptr cinfo)
{
}

/**
 * @brief
 */
static boolean jpg_fill_input_buffer (j_decompress_ptr cinfo)
{
	Sys_FPrintf(SYS_VRB, "Premature end of JPEG data\n");
	return 1;
}

/**
 * @brief
 */
static void jpg_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	if (cinfo->src->bytes_in_buffer < (size_t) num_bytes)
		Sys_FPrintf(SYS_VRB, "Premature end of JPEG data\n");

	cinfo->src->next_input_byte += (size_t) num_bytes;
	cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

/**
 * @brief
 */
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
 * @brief
 * @sa LoadPCX
 * @sa LoadTGA
 * @sa LoadPNG
 * @sa GL_FindImage
 */
extern void LoadJPG (const char *filename, byte ** pic, int *width, int *height)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *rawdata, *rgbadata, *scanline, *p, *q;
	int rawsize, i;

	if (*pic != NULL)
		Error("possible mem leak in LoadJPG\n");

	/* Load JPEG file into memory */
	rawsize = TryLoadFile(filename, (void **) &rawdata);

	if (!rawdata)
		return;

	/* Knightmare- check for bad data */
	if (rawdata[6] != 'J' || rawdata[7] != 'F' || rawdata[8] != 'I' || rawdata[9] != 'F') {
		Sys_FPrintf(SYS_VRB, "Bad jpg file %s\n", filename);
		free(rawdata);
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

	/* Check Colour Components */
	if (cinfo.output_components != 3 && cinfo.output_components != 4) {
		Sys_FPrintf(SYS_VRB, "Invalid JPEG colour components\n");
		jpeg_destroy_decompress(&cinfo);
		free(rawdata);
		return;
	}

	/* Allocate Memory for decompressed image */
	rgbadata = malloc(cinfo.output_width * cinfo.output_height * 4);
	if (!rgbadata) {
		Sys_FPrintf(SYS_VRB, "Insufficient RAM for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		free(rawdata);
		return;
	}

	/* Pass sizes to output */
	*width = cinfo.output_width;
	*height = cinfo.output_height;

	/* Allocate Scanline buffer */
	scanline = malloc(cinfo.output_width * 4);
	if (!scanline) {
		Sys_FPrintf(SYS_VRB, "Insufficient RAM for JPEG scanline buffer\n");
		free(rgbadata);
		jpeg_destroy_decompress(&cinfo);
		free(rawdata);
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
	free(scanline);

	/* Finish Decompression */
	jpeg_finish_decompress(&cinfo);

	/* Destroy JPEG object */
	jpeg_destroy_decompress(&cinfo);

	/* Return the 'rgbadata' */
	*pic = rgbadata;
	free(rawdata);
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

	size = sizeof(miptex_t) + width * height * 4;
	*mt = (miptex_t *)malloc(size);

	memset(*mt, 0, size);

	dest = (byte*)(*mt) + sizeof(miptex_t);
	memcpy(dest, pic, width * height * 4);  /* stuff RGBA into this opaque space */
	free(pic);

	/* copy relevant header fields to miptex_t */
	(*mt)->width = width;
	(*mt)->height = height;
	(*mt)->offsets[0] = sizeof(miptex_t);
	(*mt)->animname[0] = 0;

	return 0;
}
