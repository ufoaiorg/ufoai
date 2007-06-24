/**
 * @file qfiles.h
 * @note This file must be identical in the quake and utils directories
 * @brief Header for various formats like pak, and model formats as well as bsp format
 * @note The .pk3 files are just zip files
 * @note quake file formats: md2, md3, pak, pk3, bsp, sp2, wal
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

#include "unzip.h"

/*========================================================================
PCX files are used for as many images as possible
========================================================================*/

typedef struct {
	char manufacturer;
	char version;
	char encoding;
	char bits_per_pixel;
	unsigned short xmin, ymin, xmax, ymax;
	unsigned short hres, vres;
	unsigned char palette[48];
	char reserved;
	char color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char filler[58];
	unsigned char data;			/**< unbounded */
} pcx_t;


/*========================================================================
.MD2 triangle model file format
========================================================================*/

#define IDALIASHEADER	(('2'<<24)+('P'<<16)+('D'<<8)+'I')
#define IDTAGHEADER		(('2'<<24)+('P'<<16)+('D'<<8)+'J')
#define ALIAS_VERSION	8
#define TAG_VERSION		1

#define MD2_MAX_TRIANGLES	4096
#define MD2_MAX_VERTS		2048
#define MD2_MAX_FRAMES		512
#define MD2_MAX_SKINS	32
#define MD2_MAX_SKINNAME	64
#define MD2_MAX_TAGNAME		64

/**
 * @brief These two shorts are used to map a vertex onto a skin
 * The horizontal axis position is given by s, and the vertical axis position is given by t.
 * The ranges for s and t are greater than or equal to 0 and less than skinWidth and skinHeight
 */
typedef struct {
	short s; /**< (0 <= s < skinWidth) */
	short t; /**< (0 <= t < skinHeight) */
} dstvert_t;

typedef struct {
	short index_xyz[3];	/**< these three shorts are indices into the array of vertices in each frames.
						 * In other words, the number of triangles in a md2 file is fixed, and each
						 * triangle is always made of the same three indices into each frame's array
						 * of vertices. So, in each frame, the triangles themselves stay intact, their
						 * vertices are just moved around
						 */
	short index_st[3];	/**< these three shorts are indices into the array of texture coordinates */
} dtriangle_t;

typedef struct {
	byte v[3];					/**< scaled byte to fit in frame mins/maxs
								 * The three bytes represent the x, y, and z coordinates of this vertex.
								 * This is not the "real" vertex coordinate. This is a scaled version of the
								 * coordinate, scaled so that each of the three numbers fit within one byte.
								 * To scale the vertex back to the "real" coordinate, you need to first
								 * multiply each of the bytes by their respective float scale in the daliasframe_t
								 * structure, and then add the respective float translation, also in the daliasframe_t
								 * structure. This will give you the vertex coordinate relative to the model's
								 * origin, which is at the origin, (0, 0, 0)
								 */
	byte lightnormalindex;		/**< this is an index into a table of normals */
} dtrivertx_t;

/**
 * @brief is a variable sized structure, however all frame_t structures within the same file will
 * have the same size (numVertices in the header)
 */
typedef struct {
	float scale[3];				/**< multiply byte verts by this */
	float translate[3];			/**< then add this */
	char name[16];				/**< frame name from grabbing */
	dtrivertx_t verts[1];		/**< variable sized - an array of num_xyz dtrivertx_t structures.*/
} daliasframe_t;


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
	int ident;					/**< a "magic number" used to identify the file. The magic number is
								 * 844121161 in decimal (0x32504449 in hexadecimal). The magic number
								 * is equal to the int "IDP2" (id polygon 2), which is formed by
								 * ('I' + ('D' << 8) + ('P' << 16) + ('2' << 24))
								 */
	int version;				/**< version number of the file. Always 8 */

	int skinwidth;				/**< width of the skin(s) in pixels */
	int skinheight;				/**< height of the skin(s) in pixels */
	int framesize;				/**< byte size of each frame */

	int num_skins;				/**< Number of skins associated with this model */
	int num_xyz;				/**< number of vertices */
	int num_st;					/**< number of texture coordinates - can be greater than num_xyz */
	int num_tris;				/**< number of triangles in each frame. */
	int num_glcmds;				/**< dwords in strip/fan command list */
	int num_frames;				/**< number of frames for this model */

	int ofs_skins;				/**< each skin is a MD2_MAX_SKINNAME string */
	int ofs_st;					/**< byte offset from start for stverts */
	int ofs_tris;				/**< offset for dtriangles */
	int ofs_frames;				/**< offset for first frame */
	int ofs_glcmds;				/**< offset to the gl command list */
	int ofs_end;				/**< end of file */
} mdl_md2_t;


/** @brief Tag file header structure - 32 byte */
typedef struct {
	int ident;					/**< 844121162 */
	int version;				/**< version of the tag file - @sa TAG_VERSION */

	int num_tags;				/**< number of tags in this tag file */
	int num_frames;				/**< number of frames in this tag file - should be the same as in your
								 * exported model that uses this tag file */

	int ofs_names;
	int ofs_tags;
	int ofs_end;
	int ofs_extractend;
} dtag_t;

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

#ifndef M_TWOPI
#define M_TWOPI		6.28318530717958647692
#endif

/** vertex scales */
#define	MD3_XYZ_SCALE		(1.0/64)

typedef unsigned int index_t;

typedef struct {
	float st[2];
} dmd3coord_t;

typedef struct {
	short point[3];
	short norm;
} dmd3vertex_t;

typedef struct {
	vec3_t point;
	vec3_t normal;
} admd3vertex_t;

typedef struct {
	vec3_t mins;
	vec3_t maxs;
	vec3_t translate;
	float radius;
	char creator[16];
} dmd3frame_t;

typedef struct {
	vec3_t origin;
	float axis[3][3];
} dorientation_t;

typedef struct {
	char name[MD3_MAX_PATH];		/**< tag name */
	dorientation_t orient;
} dmd3tag_t;

typedef struct {
	char name[MD3_MAX_PATH];
	int unused;					/**< shader */
} dmd3skin_t;

typedef struct {
	char id[4];

	char name[MD3_MAX_PATH];

	int flags;

	int num_frames;
	int num_skins;
	int num_verts;
	int num_tris;

	int ofs_tris;
	int ofs_skins;
	int ofs_tcs;
	int ofs_verts;

	int meshsize;
} dmd3mesh_t;

typedef struct {
	int id;
	int version;

	char filename[MD3_MAX_PATH];

	int flags;

	int num_frames;
	int num_tags;
	int num_meshes;
	int num_skins;

	int ofs_frames;
	int ofs_tags;
	int ofs_meshes;
	int ofs_end;
} dmd3_t;

/*========================================================================
.SP2 sprite file format
========================================================================*/

/** little-endian "IDS2" */
#define IDSPRITEHEADER	(('2'<<24)+('S'<<16)+('D'<<8)+'I')
#define SPRITE_VERSION	2
#define SPRITE_MAX_SKINNAME	64
#define SPRITE_MAX_FRAMES	MD2_MAX_SKINS

typedef struct {
	int width, height;
	int origin_x, origin_y;		/**< raster coordinates inside pic */
	char name[SPRITE_MAX_SKINNAME];	/**< name of pcx file */
} dsprframe_t;

typedef struct {
	int ident;					/**< @sa IDSPRITEHEADER*/
	int version;				/**< version is 2 - @sa SPRITE_VERSION */
	int numframes;
	dsprframe_t frames[1];		/**< variable sized */
} dsprite_t;

/*==============================================================================
.WAL texture file format
==============================================================================*/


#define	MIPLEVELS	4
typedef struct miptex_s {
	char name[32];
	unsigned width, height;
	unsigned offsets[MIPLEVELS];	/**< four mip maps stored */
	char animname[32];			/**< next frame in animation chain */
	int flags;
	int contents;
	int value;
} miptex_t;



/*==============================================================================
.BSP file format
==============================================================================*/

/** little-endian "IBSP" */
#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')

#define BSPVERSION	71


/** upper design bounds
 * leaffaces, leafbrushes, planes, and verts are still bounded by
 * 16 bit short limits
 */
#define MAX_MAP_MODELS		1024
#define MAX_MAP_BRUSHES		8192
#define MAX_MAP_ENTITIES	2048
#define MAX_MAP_ENTSTRING	0x40000
#define MAX_MAP_TEXINFO		8192

#define MAX_MAP_AREAS		256
#define MAX_MAP_AREAPORTALS	1024
#define MAX_MAP_PLANES		65536
#define MAX_MAP_NODES		65536
#define MAX_MAP_BRUSHSIDES	65536
#define MAX_MAP_LEAFS		65536
#define MAX_MAP_VERTS		65536
#define MAX_MAP_FACES		65536
#define MAX_MAP_LEAFFACES	65536
#define MAX_MAP_LEAFBRUSHES 65536
#define MAX_MAP_PORTALS		65536
#define MAX_MAP_EDGES		128000
#define MAX_MAP_SURFEDGES	256000
#define MAX_MAP_LIGHTING	0x1000000
/* 256 * 256 * 4 */
#define MAX_MAP_ROUTING		0x40000

#define MAX_MAP_LIGHTS	1024

/** key / value pair sizes */

#define MAX_KEY		32
#define MAX_VALUE	1024

typedef struct {
	int fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_VERTEXES		2
#define LUMP_ROUTING		3
#define LUMP_NODES			4
#define LUMP_TEXINFO		5
#define LUMP_FACES			6
#define LUMP_LIGHTING		7
#define LUMP_LEAFS			8
#define LUMP_LEAFFACES		9
#define LUMP_LEAFBRUSHES	10
#define LUMP_EDGES			11
#define LUMP_SURFEDGES		12
#define LUMP_MODELS			13
#define LUMP_BRUSHES		14
#define LUMP_BRUSHSIDES		15
#define HEADER_LUMPS		19

typedef struct {
	int ident;
	int version;
	lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float mins[3], maxs[3];
	float origin[3];			/**< for sounds or lights */
	int headnode;
	int firstface, numfaces;	/**< submodels just draw faces without walking the bsp tree */
} dmodel_t;

typedef struct {
	float point[3];
} dvertex_t;


/** 0-2 are axial planes */
#define PLANE_X			0
#define PLANE_Y			1
#define PLANE_Z			2

/** 3-5 are non-axial planes snapped to the nearest */
#define PLANE_ANYX		3
#define PLANE_ANYY		4
#define PLANE_ANYZ		5
#define PLANE_NONE		6

/** planes (x&~1) and (x&~1)+1 are always opposites */

typedef struct {
	float normal[3];
	float dist;
	int type;		/**< PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate */
} dplane_t;

/**
 * @note contents flags are seperate bits
 * a given brush can contribute multiple content bits
 * multiple brushes can be in a single leaf
 */

/** these definitions also need to be in q_shared.h! */

/** lower bits are stronger, and will eat weaker brushes completely */
#define CONTENTS_SOLID			1	/**< an eye is never valid in a solid */
#define CONTENTS_WINDOW			2	/**< translucent, but not watery */
#define CONTENTS_WATER			32
#define LAST_VISIBLE_CONTENTS	128

/* remaining contents are non-visible, and don't eat brushes */
#define CONTENTS_ACTORCLIP		0x10000
#define CONTENTS_PASSABLE		0x20000		/**< only used in the compiler */
#define CONTENTS_ORIGIN			0x1000000	/**< removed before bsping an entity */
#define CONTENTS_WEAPONCLIP		0x2000000
#define CONTENTS_DEADMONSTER	0x4000000
#define CONTENTS_DETAIL			0x8000000	/**< brushes to be added after vis leafs */
#define CONTENTS_TRANSLUCENT	0x10000000	/**< auto set if any surface has trans */
#define CONTENTS_STEPON         0x40000000  /**< marks areas elevated passable areas */

#define SURF_LIGHT		0x1		/**< value will hold the light strength */
#define SURF_SLICK		0x2		/**< effects game physics */
#define SURF_WARP		0x8		/**< turbulent water warp */
#define SURF_TRANS33	0x10
#define SURF_TRANS66	0x20
#define SURF_FLOWING	0x40	/**< scroll towards angle */
#define SURF_NODRAW		0x80	/**< don't bother referencing the texture */
#define SURF_HINT		0x100	/* make a primary bsp splitter */
#define SURF_SKIP		0x200	/* completely ignore, allowing non-closed brushes */
#define SURF_ALPHATEST	0x2000000	/* alpha test for transparent textures */

typedef struct {
	int planenum;
	int children[2];			/**< negative numbers are -(leafs+1), not nodes */
	short mins[3];				/**< for frustum culling */
	short maxs[3];
	unsigned short firstface;
	unsigned short numfaces;	/**< counting both sides */
} dnode_t;


typedef struct texinfo_s {
	float vecs[2][4];			/**< [s/t][xyz offset] */
	int flags;					/**< miptex flags + overrides */
	int value;					/**< light emission, etc */
	char texture[32];			/**< texture name (textures\*.wal) */
	int nexttexinfo;			/**< for animations, -1 = end of chain */
} texinfo_t;


/**
 * @note note that edge 0 is never used, because negative edge nums are used for
 * counterclockwise use of the edge in a face
 */
typedef struct {
	unsigned short v[2];		/**< vertex numbers */
} dedge_t;

#define	MAXLIGHTMAPS	4
typedef struct {
	unsigned short planenum;
	short side;

	int firstedge;				/**< we must support > 64k edges */
	short numedges;
	short texinfo;

	/** lighting info */
	byte styles[MAXLIGHTMAPS];
	int lightofs;				/**< start of [numstyles*surfsize] samples */
} dface_t;

typedef struct {
	int contents;				/**< OR of all brushes (not needed?) */

	short cluster;
	short area;

	short mins[3];				/**< for frustum culling */
	short maxs[3];

	unsigned short firstleafface;
	unsigned short numleaffaces;

	unsigned short firstleafbrush;
	unsigned short numleafbrushes;
} dleaf_t;

typedef struct {
	unsigned short planenum;	/**< facing out of the leaf */
	short texinfo;
} dbrushside_t;

typedef struct {
	int firstside;
	int numsides;
	int contents;
} dbrush_t;

#define ANGLE_UP	-1
#define ANGLE_DOWN	-2

#endif /* QFILES_H */
