/**
 * @file defines.h
 * @brief Defined CONSTANTS (Macros are elsewhere)
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef SHARED_DEFINES_H
#define SHARED_DEFINES_H

/* if a brush just barely pokes onto the other side, let it slide by without chopping */
#define	PLANESIDE_EPSILON	0.001

/* bsp constants */
#define	CLIP_EPSILON	0.1
#define	TEXINFO_NODE		-1		/* side is already on a node */
#define	MAX_NODE_BRUSHES	8
#define	MAXEDGES		20

/* Maximum maps constant */
#define MAX_MAPS 400

/* valid -noradiosity parameters */
#define RADIOSITY_NONE			1
#define RADIOSITY_DAY_ONLY		2
#define RADIOSITY_NIGHT_ONLY	3

#define UFO_EPSILON 0.00001f

#define DEFAULT_TEAMNUM 1

/** @sa Com_DeveloperSet_f */
#define DEBUG_ALL		(1<<0)
#define DEBUG_SHARED	(1<<1)
#define DEBUG_ENGINE	(1<<2)
#define DEBUG_SYSTEM	(1<<3)
#define DEBUG_COMMANDS	(1<<4)
#define DEBUG_CLIENT	(1<<5)
#define DEBUG_SERVER	(1<<6)
#define DEBUG_GAME		(1<<7)
#define DEBUG_RENDERER	(1<<8)
#define DEBUG_SOUND		(1<<9)

#define NONE		-1
#define NONE_AMMO	0

/* Used for saving/transfering (byte) indices */
#define BYTES_NONE	0xFF

/* Move/Routing values */
#define MAX_ROUTE       31
#define MAX_MOVELENGTH  60

/* Timeunits for the various actions. */
#define TU_CROUCH	3	/**< Time units for crouching and standing up */
#define TU_TURN		1	/**< Time units for turning (no matter how far). */
#define TU_MOVE_STRAIGHT 2	/**< Time units used to move straight to the next field/square. */
#define TU_MOVE_DIAGONAL 3	/**< Time units used to move to a diagonal field/square. */
#define TU_DOOR_ACTION 1	/**< Time units used to interact with doors */
#define TU_CROUCH_WALKING_FACTOR 1.5	/**< The factor the normal movement is multiplied with in crouching mode */

/* door states */
#define STATE_OPENED		0
#define STATE_CLOSED		1

#define MAX_STRING_CHARS    1024    /* max length of a string passed to Cmd_TokenizeString */
#define MAX_STRING_TOKENS   80  /* max tokens resulting from Cmd_TokenizeString */
#define MAX_TOKEN_CHARS     256 /* max length of an individual token */

#define MAX_QPATH           64  /* max length of the ufo virtual filesystem */
/* windows + linux 256, macosx 32 */
#define MAX_OSPATH          256 /* max length of a filesystem pathname */
#define MAX_FILES           512 /* max files in a directory */

/* per-level limits */
/* 25 - bases are 5*5 - see BASE_SIZE*/
#define MAX_TILESTRINGS     25
#define MAX_TEAMS           8
#define MAX_CLIENTS         256 /* absolute limit */
#define MAX_EDICTS          1024    /* must change protocol to increase more */
#define MAX_MODELS          256 /* these are sent over the net as bytes */
#define MAX_GENERAL         (MAX_CLIENTS*2) /* general config strings */

/* not really max hp - but an initial value */
#define MAX_HP 100

/* game print flags */
#define PRINT_NONE          -1  /* suppress printing */
#define PRINT_CHAT          0   /* chat messages */
#define PRINT_HUD           1   /* translated hud strings */
#define PRINT_CONSOLE       2   /* critical messages goes to the game console */

#define QUIET   (qtrue)
#define NOISY   (qfalse)

#define ERR_FATAL           0   /* exit the entire game with a popup window */
#define ERR_DROP            1   /* print to console and disconnect from game */
#define ERR_DISCONNECT      2   /* don't kill server */

/* substract this from the ent->pos[z] to get the ground position */
#define GROUND_DELTA        28

/** upper design bounds
 * leafbrushes, planes, and verts are still bounded by
 * 16 bit short limits
 */
#define MAX_MAP_MODELS		1024
#define MAX_MAP_BRUSHES		8192
#define MAX_MAP_ENTITIES	2048
#define MAX_MAP_ENTSTRING	0x40000
#define MAX_MAP_TEXINFO		8192

#define MAX_MAP_PLANES		65536
#define MAX_MAP_NODES		65536
#define MAX_MAP_BRUSHSIDES	65536
#define MAX_MAP_LEAFS		65536
#define MAX_MAP_VERTS		65536
#define MAX_MAP_FACES		65536
#define MAX_MAP_LEAFBRUSHES 65536
#define MAX_MAP_PORTALS		65536
#define MAX_MAP_EDGES		128000
#define MAX_MAP_SURFEDGES	256000
#define MAX_MAP_LIGHTING	0x1000000
/* WIDTH * WIDTH * 4 */
#define MAX_MAP_ROUTING		0x40000

#define MAX_MAP_LIGHTS	1024

#define MAX_LEAFS 1024

/** key / value pair sizes */
#define MAX_KEY		32
#define MAX_VALUE	1024

/** Lump ID constants */
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_VERTEXES		2
#define LUMP_ROUTING		3
#define LUMP_NODES			4
#define LUMP_TEXINFO		5
#define LUMP_FACES			6
#define LUMP_LIGHTING_NIGHT	7
#define LUMP_LIGHTING_DAY	8
#define LUMP_LEAFS			9
#define LUMP_LEAFBRUSHES	10
#define LUMP_EDGES			11
#define LUMP_SURFEDGES		12
#define LUMP_MODELS			13
#define LUMP_BRUSHES		14
#define LUMP_BRUSHSIDES		15
#define HEADER_LUMPS		16

#define	SIDE_FRONT		0
#define	SIDE_ON			2
#define	SIDE_BACK		1

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

#define ANGLE_UP	-1
#define ANGLE_DOWN	-2

/*
==============================================================
COLLISION DETECTION
==============================================================
*/

/**
 * @note contents flags are seperate bits
 * a given brush can contribute multiple content bits
 * multiple brushes can be in a single leaf
 */

/** lower bits are stronger, and will eat weaker brushes completely */
#define CONTENTS_SOLID  0x0001 /**< an eye is never valid in a solid */
#define CONTENTS_WINDOW 0x0002 /**< translucent, but not watery */
#define CONTENTS_BURN   0x0008 /**< will keep burning when flamed */
#define CONTENTS_WATER  0x0020
/** max 16 bit please - otherwise change EV_ACTOR_MOVE to send a long and not a short */

#define LAST_VISIBLE_CONTENTS 0x80

#define CONTENTS_LEVEL_ALL 0xFF00
#define CONTENTS_LEVEL_1   0x0100
#define CONTENTS_LEVEL_2   0x0200
#define CONTENTS_LEVEL_3   0x0400
#define CONTENTS_LEVEL_4   0x0800
#define CONTENTS_LEVEL_5   0x1000
#define CONTENTS_LEVEL_6   0x2000
#define CONTENTS_LEVEL_7   0x4000
#define CONTENTS_LEVEL_8   0x8000

/** remaining contents are non-visible, and don't eat brushes */
#define CONTENTS_ACTORCLIP   0x00010000
#define CONTENTS_PASSABLE    0x00020000
#define CONTENTS_FOOTSTEP    0x00040000 /**< only for grabbing footsteps textures */
#define CONTENTS_ACTOR       0x00800000 /**< should never be on a brush, only in game */
#define CONTENTS_ORIGIN      0x01000000 /**< removed before bsping an entity */
#define CONTENTS_WEAPONCLIP  0x02000000 /**< stop bullets */
#define CONTENTS_DEADACTOR   0x04000000
#define CONTENTS_DETAIL      0x08000000 /**< brushes to be added after vis leafs also used for debugging local entities */
#define CONTENTS_TRANSLUCENT 0x10000000 /**< auto set if any surface has trans */
#define CONTENTS_STEPON      0x40000000 /**< marks areas elevated passable areas */

#define SURF_LIGHT     0x00000001 /**< value will hold the light strength */
#define SURF_SLICK     0x00000002 /**< effects game physics */
#define SURF_WARP      0x00000008 /**< turbulent water warp */
#define SURF_TRANS33   0x00000010 /**< 0.33 alpha blending */
#define SURF_TRANS66   0x00000020 /**< 0.66 alpha blending */
#define SURF_FLOWING   0x00000040 /**< scroll towards angle */
#define SURF_NODRAW    0x00000080 /**< don't bother referencing the texture */
#define SURF_HINT      0x00000100 /**< make a primary bsp splitter */
#define SURF_SKIP      0x00000200 /**< completely ignored, allowing non-closed brushes */
#define SURF_PHONG     0x00000400 /**< phong interpolated lighting at compile time */
#define SURF_ALPHATEST 0x02000000 /**< alpha test for transparent textures */

/* content masks */
#define MASK_ALL          (-1)
#define MASK_SOLID        (CONTENTS_SOLID | CONTENTS_WINDOW)
#define MASK_VERY_SOLID   (MASK_SOLID | CONTENTS_STEPON | CONTENTS_ACTORCLIP)
#define MASK_SHOT         (CONTENTS_SOLID | CONTENTS_ACTOR | CONTENTS_WEAPONCLIP | CONTENTS_WINDOW | CONTENTS_DEADACTOR)
#define MASK_VISIBILILITY (CONTENTS_SOLID | CONTENTS_WATER)
#define MASK_CLIP         (CONTENTS_ACTORCLIP | CONTENTS_WEAPONCLIP | CONTENTS_STEPON)

/*============================================================== */

#define ROUTING_NOT_REACHABLE 0xFF

/* Battlescape map dimensions (WIDTH*WIDTH*HEIGHT) */
#define PATHFINDING_WIDTH       256         /* absolute max */
#define PATHFINDING_HEIGHT      8           /* 15 max */

#define LEVEL_LASTVISIBLE 255
#define LEVEL_WEAPONCLIP 256
#define LEVEL_ACTORCLIP 257
#define LEVEL_STEPON 258
#define LEVEL_MAX 259
#define NUM_REGULAR_MODELS (LEVEL_ACTORCLIP + 1)

/* TestLineMask level masks */
#define TL_FLAG_NONE        0x00
#define TL_FLAG_ACTORCLIP   0x01
#define TL_FLAG_WEAPONCLIP  0x02
#define TL_FLAG_STEPON      0x04
#define TL_FLAG_ALL         0x07

#define LIGHTMAP_NIGHT 0
#define LIGHTMAP_DAY 1
#define LIGHTMAP_MAX 2

#define	PSIDE_FRONT			1
#define	PSIDE_BACK			2
#define	PSIDE_BOTH			(PSIDE_FRONT|PSIDE_BACK)
#define	PSIDE_FACING		4

#define COLORED_GREEN 1

#define MAX_TOKEN_CHARS     256 /* max length of an individual token */

#define	ON_EPSILON	0.1
#define GRENADE_ALPHAFAC	0.7
#define GRENADE_MINALPHA	M_PI/6
#define GRENADE_MAXALPHA	M_PI*7/16

/* 1/32 epsilon to keep floating point happy */
#define	DIST_EPSILON	(0.03125)
#define	MAP_DIST_EPSILON	0.01
#define	NORMAL_EPSILON	0.00001
/*
From tools/ufo2map/map.c
#define	DIST_EPSILON	0.01
*/

#define	MAX_MAP_SIDES (MAX_MAP_BRUSHES*6)
#define	MAX_MAP_TEXTURES	1024


#endif /* SHARED_DEFINES_H */
