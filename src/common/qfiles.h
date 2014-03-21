/**
 * @file
 * @note This file must be identical in the quake and utils directories
 * @brief Header for various formats like pak, and model formats as well as bsp format
 * @note The .pk3 files are just zip files
 * @note quake file formats: md2, md3, pk3, bsp
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

#pragma once

#include "../shared/defines.h"

#define IDMDXHEADER "UFOMDX"
#define MDX_VERSION		1

/*========================================================================
.MD2 triangle model file format
========================================================================*/

#define IDALIASHEADER	(('2'<<24)+('P'<<16)+('D'<<8)+'I')
#define IDTAGHEADER		(('2'<<24)+('P'<<16)+('D'<<8)+'J')
#define MD2_ALIAS_VERSION	8
#define TAG_VERSION		1

#define MD2_MAX_TRIANGLES	4096
#define MD2_MAX_VERTS		2048
#define MD2_MAX_FRAMES		1024
#define MD2_MAX_SKINS	32
#define MD2_MAX_SKINNAME	64
#define MD2_MAX_TAGNAME		64

/**
 * @brief These two shorts are used to map a vertex onto a skin
 * The horizontal axis position is given by s, and the vertical axis position is given by t.
 * The ranges for s and t are greater than or equal to 0 and less than skinWidth and skinHeight
 */
typedef struct {
	uint16_t s; /**< (0 <= s < skinWidth) */
	uint16_t t; /**< (0 <= t < skinHeight) */
} dMD2Coord_t;

typedef struct {
	uint16_t index_verts[3];	/**< these three shorts are indices into the array of vertices in each frames.
								 * In other words, the number of triangles in a md2 file is fixed, and each
								 * triangle is always made of the same three indices into each frame's array
								 * of vertices. So, in each frame, the triangles themselves stay intact, their
								 * vertices are just moved around
								 */
	uint16_t index_st[3];	/**< these three shorts are indices into the array of texture coordinates */
} dMD2Triangle_t;

typedef struct {
	byte v[3];	/**< scaled byte to fit in frame mins/maxs
				 * The three bytes represent the x, y, and z coordinates of this vertex.
				 * This is not the "real" vertex coordinate. This is a scaled version of the
				 * coordinate, scaled so that each of the three numbers fit within one byte.
				 * To scale the vertex back to the "real" coordinate, you need to first
				 * multiply each of the bytes by their respective float scale in the dMD2Frame_t
				 * structure, and then add the respective float translation, also in the dMD2Frame_t
				 * structure. This will give you the vertex coordinate relative to the model's
				 * origin, which is at the origin, (0, 0, 0)
				 */
	byte unused;
} dMD2TriangleVertex_t;

/**
 * @brief is a variable sized structure, however all frame_t structures within the same file will
 * have the same size (numVertices in the header)
 */
typedef struct dAliasFrame_s {
	float scale[3];				/**< multiply byte verts by this */
	float translate[3];			/**< then add this */
	char name[16];				/**< frame name from grabbing */
	dMD2TriangleVertex_t verts[1];		/**< variable sized - an array of num_verts dMD2TriangleVertex_t structures.*/
} dMD2Frame_t;


/**
 * @brief the glcmd format:
 * a positive integer starts a tristrip command, followed by that many
 * vertex structures.
 * a negative integer starts a trifan command, followed by -x vertexes
 * a zero indicates the end of the command list.
 * a vertex consists of a floating point s, a floating point t,
 * and an integer vertex index.
 */

/** @brief model file header structure - 68 bytes */
typedef struct {
	uint32_t ident;					/**< a "magic number" used to identify the file. The magic number is
									 * 844121161 in decimal (0x32504449 in hexadecimal). The magic number
									 * is equal to the int "IDP2" (id polygon 2), which is formed by
									 * ('I' + ('D' << 8) + ('P' << 16) + ('2' << 24))
									 */
	uint32_t version;				/**< version number of the file. Always 8 */

	uint32_t skinwidth;				/**< width of the skin(s) in pixels */
	uint32_t skinheight;			/**< height of the skin(s) in pixels */
	uint32_t framesize;				/**< byte size of each frame */

	uint32_t num_skins;				/**< Number of skins associated with this model */
	uint32_t num_verts;				/**< number of vertices */
	uint32_t num_st;				/**< number of texture coordinates - can be greater than num_verts */
	uint32_t num_tris;				/**< number of triangles in each frame. */
	uint32_t num_glcmds;			/**< dwords in strip/fan command list */
	uint32_t num_frames;			/**< number of frames for this model */

	uint32_t ofs_skins;				/**< each skin is a MD2_MAX_SKINNAME string */
	uint32_t ofs_st;				/**< byte offset from start for stverts */
	uint32_t ofs_tris;				/**< offset for dtriangles */
	uint32_t ofs_frames;			/**< offset for first frame */
	uint32_t ofs_glcmds;			/**< offset to the gl command list */
	uint32_t ofs_end;				/**< end of file */
} dMD2Model_t;


/** @brief Tag file header structure - 32 byte
 * A tag for a model must have the same amount of frames as the model. A tag is
 * nothing more than a coordinate. It's used to e.g. determine the hand of a model when
 * the walking animation is played. The renderer has to know where to place the weapon. */
typedef struct {
	uint32_t ident;					/**< 844121162 */
	uint32_t version;				/**< version of the tag file - @sa TAG_VERSION */

	uint32_t num_tags;				/**< number of tags in this tag file */
	uint32_t num_frames;			/**< number of frames in this tag file - should be the same as in your
									 * exported model that uses this tag file */

	uint32_t ofs_names;
	uint32_t ofs_tags;
	uint32_t ofs_end;
	uint32_t ofs_extractend;
} dMD2tag_t;

/*========================================================================
.MD3 model file format
========================================================================*/

#define IDMD3HEADER		(('3'<<24)+('P'<<16)+('D'<<8)+'I')

#define MD3_ALIAS_VERSION	15
#define MD3_ALIAS_MAX_LODS	4

#define	MD3_MAX_TRIANGLES	8192	/* per mesh */
#define MD3_MAX_VERTS		4096	/* per mesh */
#define MD3_MAX_SHADERS		256	/* per mesh */
#define MD3_MAX_FRAMES		1024	/* per model */
#define	MD3_MAX_MESHES		32	/* per model */
#define MD3_MAX_TAGS		16	/* per frame */
#define MD3_MAX_PATH		64

/** vertex scales */
#define	MD3_XYZ_SCALE		(1.0f/64.0f)

typedef struct {
	float st[2];
} dmd3coord_t;

typedef struct {
	int16_t point[3];
	int16_t norm;
} dmd3vertex_t;

typedef struct {
	float mins[3];
	float maxs[3];
	float translate[3];
	float radius;
	char creator[16];
} dmd3frame_t;

typedef struct {
	float origin[3];
	float axis[3][3];
} dorientation_t;

typedef struct {
	char name[MD3_MAX_PATH];		/**< tag name */
	dorientation_t orient;
} dmd3tag_t;

typedef struct {
	char name[MD3_MAX_PATH];
	uint32_t unused;					/**< shader */
} dmd3skin_t;

typedef struct {
	char id[4];

	char name[MD3_MAX_PATH];

	uint32_t flags;

	uint32_t num_frames;
	uint32_t num_skins;
	uint32_t num_verts;
	uint32_t num_tris;

	uint32_t ofs_tris;
	uint32_t ofs_skins;
	uint32_t ofs_tcs;
	uint32_t ofs_verts;

	uint32_t meshsize;
} dmd3mesh_t;

typedef struct {
	uint32_t id;
	uint32_t version;

	char filename[MD3_MAX_PATH];

	uint32_t flags;

	uint32_t num_frames;
	uint32_t num_tags;
	uint32_t num_meshes;
	uint32_t num_skins;

	uint32_t ofs_frames;
	uint32_t ofs_tags;
	uint32_t ofs_meshes;
	uint32_t ofs_end;
} dmd3_t;

/*==============================================================================
.BSP file format
==============================================================================*/

/** little-endian "IBSP" */
#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')

#define BSPVERSION	79

/** @brief Directory of the different data blocks */
typedef struct {
	uint32_t fileofs;	/**< the offset of the data block (from the start of the file) */
	uint32_t filelen;	/**< the length of the data block */
} lump_t;

/** @brief The BSP header definition with the data block directory */
typedef struct {
	uint32_t ident;		/**< the magic number of the BSP file @sa IDBSPHEADER */
	uint32_t version;	/**< the version of the BSP file @sa BSPVERSION */
	lump_t lumps[HEADER_LUMPS];	/**< the data directories */
} dBspHeader_t;


#define BSP_SwapHeader(header, name) { \
	unsigned int i; \
	(header)->ident = LittleLong((header)->ident); \
	(header)->version = LittleLong((header)->version); \
	for (i = 0; i < HEADER_LUMPS; i++) { \
		lump_t* l = &(header)->lumps[i]; \
		l->filelen = LittleLong(l->filelen); \
		l->fileofs = LittleLong(l->fileofs); \
		if (l->fileofs == (uint32_t) -1) \
			Sys_Error("Invalid bsp header found (lump overflow %i): '%s'", i, (name)); \
	} \
}
