/**
 * @file qfiles.h
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

#ifndef QFILES_H
#define QFILES_H

#include "../shared/defines.h"
#include "unzip.h"

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
dpm file format
==============================================================================*/

#define DPMHEADER		(('K'<<24)+('R'<<16)+('A'<<8)+'D')

/** @brief
 * type 2 model (hierarchical skeletal pose)
 * within this specification, int is assumed to be 32bit, float is assumed to be 32bit, char is assumed to be 8bit, text is assumed to be an array of chars with NULL termination
 * all values are big endian (also known as network byte ordering), NOT x86 little endian
 * @note general notes:
 * a pose is a 3x4 matrix (rotation matrix, and translate vector)
 * parent bones must always be lower in number than their children, models will be rejected if this is not obeyed (can be fixed by modelling utilities)
 * @note utility notes:
 * if a hard edge is desired (faceted lighting, or a jump to another set of skin coordinates), vertices must be duplicated
 * ability to visually edit groupids of triangles is highly recommended
 * bones should be markable as 'attach' somehow (up to the utility) and thus protected from culling of unused resources
 * frame 0 is always the base pose (the one the skeleton was built for)
 * @note game notes:
 * the loader should be very thorough about error checking, all vertex and bone indices should be validated, etc
 * the gamecode can look up bone numbers by name using a builtin function, for use in attachment situations (the client should have the same model as the host of the gamecode in question - that is to say if the server gamecode is setting the bone number, the client and server must have vaguely compatible models so the client understands, and if the client gamecode is setting the bone number, the server could have a completely different model with no harm done)
 * the triangle groupid values are up to the gamecode, it is recommended that gamecode process this in an object-oriented fashion (I.E. bullet hits entity, call that entity's function for getting properties of that groupid)
 * frame 0 should be usable, not skipped
 * speed optimizations for the saver to do:
 * remove all unused data (unused bones, vertices, etc, be sure to check if bones are used for attachments however)
 * sort triangles into strips
 * sort vertices according to first use in a triangle (caching benefits) after sorting triangles
 * speed optimizations for the loader to do:
 * if the model only has one frame, process it at load time to create a simple static vertex mesh to render (this is a hassle, but it is rewarding to optimize all such models)
 * @note rendering process:
 * 1*. one or two poses are looked up by number
 * 2*. boneposes (matrices) are interpolated, building bone matrix array
 * 3. bones are parsed sequentially, each bone's matrix is transformed by it's parent bone (which can be -1; the model to world matrix)
 * 4. meshs are parsed sequentially, as follows:
 *   1. vertices are parsed sequentially and may be influenced by more than one bone (the results of the 3x4 matrix transform will be added together - weighting is already built into these)
 *   2. shader is looked up and called, passing vertex buffer (temporary) and triangle indices (which are stored in the mesh)
 * 5. rendering is complete
 * * - these stages can be replaced with completely dynamic animation instead of pose animations.
 */

/** @brief header for the entire file */
typedef struct dpmheader_s {
	char id[16]; /**< "DARKPLACESMODEL\0", length 16 */
	uint32_t type; /**< 2 (hierarchical skeletal pose) */
	uint32_t filesize; /**< size of entire model file */
	float mins[3], maxs[3], yawradius, allradius; /**< for clipping uses */

	/** these offsets are relative to the file */
	uint32_t num_bones;
	uint32_t num_meshs;
	uint32_t num_frames;
	uint32_t ofs_bones; /**< dpmbone_t bone[num_bones]; */
	uint32_t ofs_meshs; /**< dpmmesh_t mesh[num_meshs]; */
	uint32_t ofs_frames; /**< dpmframe_t frame[num_frames]; */
} dpmheader_t;

/** @brief there may be more than one of these */
typedef struct dpmmesh_s {
	/** these offsets are relative to the file */
	char shadername[32]; /**< name of the shader to use */
	uint32_t num_verts;
	uint32_t num_tris;
	uint32_t ofs_verts; /**< dpmvertex_t vert[numvertices]; see vertex struct */
	uint32_t ofs_texcoords; /**< float texcoords[numvertices][2]; */
	uint32_t ofs_indices; /**< uint32_t indices[numtris*3]; designed for glDrawElements (each triangle is 3 uint32_t indices) */
	uint32_t ofs_groupids; /**< uint32_t groupids[numtris]; the meaning of these values is entirely up to the gamecode and modeler */
} dpmmesh_t;

/** @brief if set on a bone, it must be protected from removal */
#define DPMBONEFLAG_ATTACHMENT 1

/** @brief one per bone */
typedef struct dpmbone_s {
	/** name examples: upperleftarm leftfinger1 leftfinger2 hand, etc */
	char name[32];
	/** parent bone number */
	int32_t parent;
	/** flags for the bone */
	uint32_t flags;
} dpmbone_t;

/** @brief a bonepose matrix is intended to be used like this:
 * (n = output vertex, v = input vertex, m = matrix, f = influence)
 * n[0] = v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2] + f * m[0][3];
 * n[1] = v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2] + f * m[1][3];
 * n[2] = v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2] + f * m[2][3]; */
typedef struct dpmbonepose_s {
	float matrix[3][4];
} dpmbonepose_t;

/** @brief immediately followed by bone positions for the frame */
typedef struct dpmframe_s {
	/** name examples: idle_1 idle_2 idle_3 shoot_1 shoot_2 shoot_3, etc */
	char name[32];
	float mins[3], maxs[3], yawradius, allradius;
	uint32_t ofs_bonepositions; /**< dpmbonepose_t bonepositions[bones]; */
} dpmframe_t;

/** @brief one or more of these per vertex */
typedef struct dpmbonevert_s {
	/** this pairing of origin and influence is intentional
	 * (in SSE or 3DNow! assembly it can be done as a quad vector op
	 * (or two dual vector ops) very easily) */
	float origin[3]; /**< vertex location (these blend) */
	float influence; /**< influence fraction (these must add up to 1) */
	/** this pairing of normal and bonenum is intentional
	 * (in SSE or 3DNow! assembly it can be done as a quad vector op
	 * (or two dual vector ops) very easily, the bonenum is ignored) */
	float normal[3]; /**< surface normal (these blend) */
	uint32_t bonenum; /**< number of the bone */
} dpmbonevert_t;

/** @brief variable size, parsed sequentially */
typedef struct dpmvertex_s {
	uint32_t numbones;
	/** immediately followed by 1 or more dpmbonevert_t structures */
} dpmvertex_t;

/*==============================================================================
.BSP file format
==============================================================================*/

/** little-endian "IBSP" */
#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')

#define BSPVERSION	78

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
		lump_t *l = &(header)->lumps[i]; \
		l->filelen = LittleLong(l->filelen); \
		l->fileofs = LittleLong(l->fileofs); \
		if (l->fileofs == (uint32_t) -1) \
			Sys_Error("Invalid bsp header found (lump overflow %i): '%s'", i, (name)); \
	} \
}

#endif /* QFILES_H */
