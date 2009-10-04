/**
 * @file defines.h
 * @brief Defined CONSTANTS (Macros are elsewhere)
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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

/**
 * @note The comment line specifying files has the following syntax:
 *   <parent dir>/<filename>   OR
 *   [parent dir]              OR
 *   {project name}
 */

/* if a brush just barely pokes onto the other side, let it slide by without chopping */
/* common/tracing.c, ufo2map/brushbsp.c */
#define	PLANESIDE_EPSILON	0.001

/** @sa CONTENTS_NODE - @todo replace this with CONTENTS_NODE? */
#define LEAFNODE -1
#define	PLANENUM_LEAF -1

/* bsp constants */
/* ufo2map/csg.c, ufo2map/portals.c, ufo2map/brushbsp.c */
#define	TEXINFO_NODE		-1		/* side is already on a node */
/* ufo2map/faces.c, shared/typedefs.h (face_t) */
#define	MAXEDGES		20

/* game/g_client.c, client/cl_main.c, client/cl_view.c */
#define	TEAM_DEFAULT 1

/** @sa Com_DeveloperSet_f */
/* {game}, {ufo}, {ufo_ded} */
#define	DEBUG_ALL		(1<<0)
#define	DEBUG_SHARED	(1<<1)
#define	DEBUG_ENGINE	(1<<2)
#define	DEBUG_SYSTEM	(1<<3)
#define	DEBUG_COMMANDS	(1<<4)
#define	DEBUG_CLIENT	(1<<5)
#define	DEBUG_SERVER	(1<<6)
#define	DEBUG_GAME		(1<<7)
#define	DEBUG_RENDERER	(1<<8)
#define	DEBUG_SOUND		(1<<9)
#define DEBUG_EVENTSYS	(1<<10)
#define DEBUG_PATHING	(1<<11)
#define DEBUG_ROUTING	(1<<12)

/* {game}, [client], common/scripts.c */
#define	NONE		-1
/* {game}, [client] */
#define	NONE_AMMO	0

/* Used for saving/transfering (byte) indices */
/* client/campaign/cp_aircraft.c, client/campaign/cp_base.c, client/campaign/cp_campaign.c, client/campaign/cp_transfer.c */
#define	BYTES_NONE	0xFF

/* Move/Routing values */
/* game/g_ai.c, game/g_client.c, client/cl_actor.c, client/battlescape/cl_localentity.c, client/battlescape/cl_parse.c, common/cmodel.c */
#define	MAX_ROUTE		31
/* common/cmodel.c */
#define	MAX_MOVELENGTH	60

/* Timeunits for the various actions. */
/* game/g_client.c, client/cl_actor.c, client/battlescape/cl_localentity.c, client/battlescape/cl_parse.c, common/cmodel.c */
#define	TU_CROUCH			3	/**< Time units for crouching and standing up */
#define	TU_TURN				1	/**< Time units for turning (no matter how far). */
#define	TU_MOVE_STRAIGHT	2	/**< Time units used to move straight to the next field/square. */
#define	TU_MOVE_DIAGONAL	3	/**< Time units used to move to a diagonal field/square. */
#define	TU_MOVE_CLIMB		4	/**< Time units used to climb a ladder. */
#define	TU_MOVE_FALL		0	/**< Time units used to fall. */
#define	TU_DOOR_ACTION		1	/**< Time units used to interact with doors */
#define	TU_CROUCH_MOVING_FACTOR	1.5	/**< The factor the normal movement is multiplied when in crouching mode */
#define	TU_FLYING_MOVING_FACTOR	2	/**< The factor the normal movement is multiplied when flying */

/* door states */
/* game/g_client.c, game/g_func.c */
#define	STATE_OPENED		0
#define	STATE_CLOSED		1

/* client/cl_console.c, client/input/cl_keys.c, client/cl_main.c, common/cmd.c, common/common.c */
#define	MAX_STRING_CHARS    1024    /* max length of a string passed to Cmd_TokenizeString */
/* common/cmd.c */
#define	MAX_STRING_TOKENS   80  /* max tokens resulting from Cmd_TokenizeString */

/* ALL */
#define MAX_TEXPATH         32  /**< max length of filename relative to gamedir, any longer makes the bsp files unnecessarily long */

/* per-level limits */
/* 25 - bases are 5*5 - see BASE_SIZE*/
/* game/q_shared.h, client/battlescape/cl_parse.c, server/sv_ccmds.c, server/sv_init.c */
#define	MAX_TILESTRINGS     25
/* {game}, [client] */
#define	MAX_TEAMS           8
/* game/q_shared.h, client/battlescape/cl_parse.c, client/client.h */
#define	MAX_CLIENTS         256 /* absolute limit */
/* game/g_client.c, game/g_local.h, game/g_main.c, game/g_utils.c, game/game.h, game/q_shared.h,
 * client/battlescape/cl_localentity.c, client/battlescape/cl_localentity.h, client/battlescape/cl_parse.c, server/sv_world.c */
#define	MAX_EDICTS          1024    /* must change protocol to increase more */
/* game/q_shared.h, client/battlescape/cl_localentity.c, client/battlescape/cl_parse.c, client/battlescape/cl_view.c, client/client.h,
 * common/cmodel.c, server/server.h, server/sv_init.c, server/sv_world */
#define	MAX_MODELS          256 /* these are sent over the net as bytes */
/* game/q_shared.h */
#define	MAX_GENERAL         (MAX_CLIENTS*2) /* general config strings */

/* not really max hp - but an initial value */
/* game/g_spawn.c */
#define	MAX_HP 100

/* game print flags */
/* game/g_client.c, server/sv_game.c */
#define	PRINT_NONE		-1  /* suppress printing */
/* game/g_client.c, game/g_cmds.c, client/battlescape/cl_parse.c, server/sv_ccmds.c, server/sv_main.c */
#define	PRINT_CHAT		0   /* chat messages */
/* game/g_client.c, game/g_combat.c, game/g_spawn.c, client/battlescape/cl_parse.c */
#define	PRINT_HUD		1   /* translated hud strings */
/* game/g_client.c, game/g_cmds.c, game/g_combat.c, game/g_svccmds.c, server/sv_ccmds.c */
#define	PRINT_CONSOLE	2   /* critical messages goes to the game console */

/* {game} */
#define	QUIET	(qtrue)
/* {game}, server/sv_user.c */
#define	NOISY	(qfalse)

#define ERR_FATAL           0   /* exit the entire game with a popup window */
#define ERR_DROP            1   /* print to console and disconnect from game */
#define ERR_DISCONNECT      2   /* don't kill server */

/* substract this from the ent->pos[z] to get the ground position */
/* game/g_combat.c, client/cl_actor.c, client/battlescape/cl_parse.c */
#define	GROUND_DELTA	28

/* important units */
#define UNIT_SIZE   32
#define UNIT_HEIGHT 64
/* player height - 12 to be able to walk trough doors
 * UNIT_HEIGHT is the height of one level */
#define PLAYER_HEIGHT		(UNIT_HEIGHT - 16)
#define QUANT	4
/* WALL_SIZE is the distance a wall may protrude from the edge of a cell without interfeeting with an actor entering the cell */
#define WALL_SIZE 5

/** upper design bounds
 * leafbrushes, planes, and verts are still bounded by
 * 16 bit short limits
 */
/* ufo2map/writebsp.c, common/cmodel.c, shared/typedefs.h */
#define	MAX_MAP_MODELS		1024
/* ufo2map/bsp.h, ufo2map/common/bspfile.c, ufo2map/map.c, ufo2map/writebsp.c, common/cmodel.c, shared/typedefs.h */
#define	MAX_MAP_BRUSHES		16384
/* ufo2map/common/bspfile.c, ufo2map/map.c, common/cmodel.c, shared/typedefs.h, ufo2map/common/bspfile.h */
#define	MAX_MAP_ENTITIES	2048
/* common/cmodel.c, common/common.h, shared/typedefs.h */
#define	MAX_MAP_ENTSTRING	0x40000
/* ufo2map/patches.c, common/cmodel.c, shared/typedefs.h */
#define	MAX_MAP_TEXINFO		16384
/* {ufo2map}, common/cmodel.c shared/typedefs.h */
#define	MAX_MAP_PLANES		65536
/* common/tracing.c, ufo2map/writebsp.c, client/battlescape/cl_localentity.c, common/cmodel.c, server/sv_world.c, shared/typedefs.h */
#define	MAX_MAP_NODES		65536
/* ufo2map/map.c, ufo2map/writebsp.c, common/cmodel.c, shared/typedefs.h */
#define	MAX_MAP_BRUSHSIDES	65536
/* ufo2map/writebsp.c, shared/typedefs.h */
#define	MAX_MAP_LEAFS		65536
/* ufo2map/faces.c, ufo2map/lightmap.c, shared/typedefs.h */
#define	MAX_MAP_VERTS		65536
/* ufo2map/lightmap.c, ufo2map/radiosity.c, ufo2map/writebsp.c, shared/typedefs.h */
#define	MAX_MAP_FACES		65536
/* ufo2map/writebsp.c, common/cmodel.c, shared/typedefs.h */
#define	MAX_MAP_LEAFBRUSHES	65536
/* ufo2map/faces.c, ufo2map/lightmap.c, shared/typedefs.h */
#define	MAX_MAP_EDGES		128000
/* ufo2map/writebsp.c, renderer/r_model_brush.c, shared/typedefs.h */
#define	MAX_MAP_SURFEDGES	256000
/* ufo2map/lightmap.c, shared/typedefs.h */
#define	MAX_MAP_LIGHTING	0x1000000
/* WIDTH * WIDTH * 4 */
/* shared/typedefs.h */
#define	MAX_MAP_ROUTING		0x100000

/* common/tracing.c */
#define	MAX_LEAFS		1024

/** key / value pair sizes */
/* ufo2map/common/bspfile.c, client/cl_input.c */
#define	MAX_KEY		32
/* ufo2map/common/bspfile.c, client/menu/m_nodes.h, client/menu/m_parse.c */
#define	MAX_VALUE	1024

/** Lump ID constants (directory in the bsp header) */
/* ufo2map/bspfile.c, common/cmodel.c, common/qfiles.h, renderer/r_model_brush.c */
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
#define LUMP_NORMALS		16
#define HEADER_LUMPS		17

/* ufo2map/common/polylib.c */
#define	SIDE_FRONT		0
#define	SIDE_ON			2
#define	SIDE_BACK		1

/** 0-2 are axial planes */
/* common/tracing.c, ufo2map/common/trace.c, ufo2map/map.c, renderer/r_bsp.c, renderer/r_lightmap.c */
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

/** 3-5 are non-axial planes snapped to the nearest */
/* ufo2map/map.c, renfderer/r_main.c */
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5
/* common/tracing.c, ufo2map/common/trace.c */
#define	PLANE_NONE		6

#define AXIAL(p) ((p)->type < PLANE_ANYX)

/** planes (x & ~1) and (x & ~1) + 1 are always opposites */

/* ufo2map/lightmap.c */
#define	ANGLE_UP	-1
#define	ANGLE_DOWN	-2

/*
==============================================================
COLLISION DETECTION
==============================================================
*/

/**
 * @note contents flags are separate bits
 * a given brush can contribute multiple content bits
 * multiple brushes can be in a single leaf.
 * Please update DisplayContentFlags, if these are updated.
 * @sa DisplayContentFlags
 */

/** lower bits are stronger, and will eat weaker brushes completely */
/* {game}, {ufo2map}, client/battlescape/cl_localentity.c, client/battlescape/cl_parse.c, common/cmodel.c, renderer/r_bsp.c */
#define	CONTENTS_SOLID	0x0001 /**< an eye is never valid in a solid */
#define	CONTENTS_WINDOW	0x0002 /**< translucent, but not watery */
#define	CONTENTS_LADDER	0x0004
#define	CONTENTS_WATER	0x0020
/** max 16 bit please - otherwise change EV_ACTOR_MOVE to send a long and not a short */

/* ufo2map/map.c, ufo2map/portals.c */
#define	LAST_VISIBLE_CONTENTS	0x80

/* {game}, {ufo2map}, client/battlescape/cl_localentity.c, client/battlescape/cl_parse.c, common/cmodel.c, renderer/r_bsp.c */
#define	CONTENTS_LEVEL_ALL	0xFF00
#define	CONTENTS_LEVEL_1	0x0100
#define	CONTENTS_LEVEL_2	0x0200
#define	CONTENTS_LEVEL_3	0x0400
#define	CONTENTS_LEVEL_4	0x0800
#define	CONTENTS_LEVEL_5	0x1000
#define	CONTENTS_LEVEL_6	0x2000
#define	CONTENTS_LEVEL_7	0x4000
#define	CONTENTS_LEVEL_8	0x8000

/** remaining contents are non-visible, and don't eat brushes */
/* {game}, {ufo2map}, client/battlescape/cl_localentity.c, client/battlescape/cl_parse.c, common/cmodel.c, renderer/r_bsp.c */
#define	CONTENTS_ACTORCLIP		0x00010000
#define	CONTENTS_PASSABLE		0x00020000
#define CONTENTS_TERRAIN		0x00040000 /**<  */
#define	CONTENTS_ACTOR			0x00800000 /**< should never be on a brush, only in game */
#define	CONTENTS_ORIGIN			0x01000000 /**< removed before bsping an entity */
#define	CONTENTS_WEAPONCLIP		0x02000000 /**< stop bullets */
#define	CONTENTS_DEADACTOR		0x04000000
#define	CONTENTS_DETAIL			0x08000000 /**< brushes to be added after vis leafs also used for debugging local entities */
#define	CONTENTS_TRANSLUCENT	0x10000000 /**< auto set if any surface has trans */

/* {ufo2map}, [renderer] */
#define	SURF_LIGHT		0x00000001 /**< value will hold the light strength */
#define	SURF_SLICK		0x00000002 /**< effects game physics */
#define	SURF_WARP		0x00000008 /**< turbulent water warp */
#define	SURF_BLEND33	0x00000010 /**< 0.33 alpha blending */
#define	SURF_BLEND66	0x00000020 /**< 0.66 alpha blending */
#define	SURF_FLOWING	0x00000040 /**< scroll towards angle */
#define	SURF_NODRAW		0x00000080 /**< don't bother referencing the texture */
#define	SURF_HINT		0x00000100 /**< make a primary bsp splitter */
#define	SURF_SKIP		0x00000200 /**< completely ignored, allowing non-closed brushes */
#define	SURF_PHONG		0x00000400 /**< phong interpolated lighting at compile time */
#define	SURF_BURN		0x00000800 /**< will keep burning when flamed */
#define	SURF_FOOTSTEP	0x00001000 /**< only for grabbing footsteps textures */
#define	SURF_ORIGIN		0x00002000 /**< use this to identity those submodel surfaces that must not be translated in case of rma */
#define	SURF_ALPHATEST	0x02000000 /**< alpha test for transparent textures */

/* content masks */
/* game/g_client.c, game/g_combat.c, game/g_local.h, game/g_phys.c, game/game.h, common/routing.c, common/tracing.c,
 * ufo2map/csg.c, client/cl_actor.c, client/input/cl_input.c, client/battlescape/cl_localentity.c, client/battlescape/cl_particle.c, common/cmodel.c,
 * server/sv_world.c */
#define	MASK_ALL			(-1)
#define	MASK_SOLID			(CONTENTS_SOLID | CONTENTS_WINDOW)
#define	MASK_IMPASSABLE		(MASK_SOLID | CONTENTS_ACTORCLIP)
#define MASK_PASSABLE		(CONTENTS_PASSABLE | CONTENTS_WATER)
#define	MASK_SHOT			(CONTENTS_SOLID | CONTENTS_ACTOR | CONTENTS_WEAPONCLIP | CONTENTS_WINDOW | CONTENTS_DEADACTOR)
#define	MASK_VISIBILILITY	(CONTENTS_SOLID | CONTENTS_WATER)
#define	MASK_CLIP			(CONTENTS_ACTORCLIP | CONTENTS_WEAPONCLIP)

/*============================================================== */

/* game/g_ai.c, game/g_client.c, ufo2map/routing.c, client/cl_actor.c, common/cmodel.c */
#define	ROUTING_NOT_REACHABLE	0xFF	/**< (byte) Indicates a cell that is not reachable.  A TU value. */
#define	ROUTING_UNREACHABLE		-1		/**< (int) Indicates that a cell is not reachable.  A DV value. */

/* Battlescape map dimensions (WIDTH*WIDTH*HEIGHT) */
/** @brief -MAX_WORLD_WIDTH up tp +MAX_WORLD_WIDTH */
#define MAX_WORLD_WIDTH 4096
/** @brief absolute max - -GRID_WIDTH up tp +GRID_WIDTH @sa PATHFINDING_WIDTH */
#define GRID_WIDTH (MAX_WORLD_WIDTH / UNIT_SIZE)
/** @brief absolute max */
#define PATHFINDING_WIDTH (GRID_WIDTH * 2)
/** @brief 15 max, adjusting above 8 will require a rewrite to the DV code */
#define PATHFINDING_HEIGHT 8
/** @brief A cell's height in QUANT sized units */
#define CELL_HEIGHT (UNIT_HEIGHT / QUANT)

/* Maximum falling distance in QUANT units (model units / QUANT) */
/* common/routing.c */
#define	PATHFINDING_MAX_FALL	16
/* The height of the box where we don't need the full actor's torso width */
#define	PATHFINDING_LEGROOMHEIGHT	4
/* Minimum step-up height in QUANT units (model units/QUANT)
 * Note that 4 is the minimum to rise one cell every 4 moved horizontally. */
/* common/routing.c */
#define	PATHFINDING_MIN_STEPUP		2
/* Minimum step-up height in QUANT units (model units/QUANT)
 * Note that 4 is the minimum to rise one cell every 4 moved horizontally. */
/* common/routing.c */
#define	PATHFINDING_MAX_STEPUP		4
/* A stepup value indicating that there is no way to enter the cell. */
/* common/routing.c */
#define	PATHFINDING_NO_STEPUP		(2 * CELL_HEIGHT)
/* A special bit mask indicating that the stepup causes the actor to rise a cell. */
/* common/routing.c */
#define	PATHFINDING_BIG_STEPUP		0x80
/* A special bit mask indicating that the stepup causes the actor to walk down a cell. */
/* common/routing.c */
#define	PATHFINDING_BIG_STEPDOWN		0x40
/* Minimum height for an opening to be an opening in step units (model units/QUANT)
 * Must be larger than PATHFINDING_MAX_STEPUP!!
 */
/* common/routing.c */
#define	PATHFINDING_MIN_OPENING	6
/* common/routing.c */
/** @brief The size (in model units) of a microstep.  Must be a power of 2 and less than UNIT_SIZE. */
#define PATHFINDING_MICROSTEP_SIZE	4
/** @brief The number of microsteps that can be stepped over by an actor.
 *  Used to allow an actor to stepup when the floor is not level or consitent. */
#define PATHFINDING_MICROSTEP_SKIP	2

/* DIRECTION constants- define "odd" directions */
#define DIRECTION_FALL			13
#define DIRECTION_CLIMB_UP		8
#define DIRECTION_CLIMB_DOWN	9
#define DIRECTION_STAND_UP		10
#define DIRECTION_CROUCH		11


/* Actor size and states defines */
/* common/cmodel.c */
#define	ACTOR_MAX_SIZE			2
#define	ACTOR_MAX_STATES		2

/* common/tracing.c, ufo2map/common/trace.c, ufo2map/csg.c, ufo2map/radiosity.c, ufo2map/routing.c, client/cl_input.c,
 * common/cmodel.c, renderer/r_bsp.c, shared/typedefs.h */
/**
 * @note The bsp trees are generated based on the levels that a given brush is supposed to be in.
 * So a brush that is tagged for viewing in levels 1, 2, and 3 will be in bsp tree 7 (1 + 2 + 4)
 * a brush that is viewable in all levels will be in bsp tree level 255, and a bush that is not
 * tagged for viewing by level will be in tree 0.
 * Also, a brush will only be in one bsp tree - the brush tagged for levels 1, 2, and 3 will only
 * be in tree 7, saving memory
 */
#define	LEVEL_LASTVISIBLE		255
#define	LEVEL_WEAPONCLIP		256
#define	LEVEL_ACTORCLIP			257
#define	LEVEL_MAX				258
/* ufo2map/bsp.c, common/cmodel.c, renderer/r_model_brush.c */
#define	NUM_REGULAR_MODELS		(LEVEL_ACTORCLIP + 1)

/* TestLine level masks */
/* game/g_client.c, game/g_combat.c, common/routing.c, common/tracing.c, ufo2map/lightmap.c, ufo2map/radiosity.c,
 * client/cl_actor.c, client/battlescape/cl_localentity.c */
#define	TL_FLAG_NONE			0x0000
#define	TL_FLAG_REGULAR_LEVELS	0x00FF
#define	TL_FLAG_ACTORCLIP		0x0100
#define	TL_FLAG_WEAPONCLIP		0x0200
#define	TL_FLAG_ALL				0x0300

/* ufo2map/common/bspfile.c, renderer/r_lightmap.c, renderer/r_lightmap.h, renderer/r_main.c, renderer/r_model_brush.c,
 * shared/typedefs.h */
#define	LIGHTMAP_NIGHT	0
#define	LIGHTMAP_DAY	1
#define	LIGHTMAP_MAX	2

/* common/tracing.c, ufo2map/brushbsp.c, renderer/r_main.c */
#define	PSIDE_FRONT			1
#define	PSIDE_BACK			2
#define	PSIDE_BOTH			(PSIDE_FRONT|PSIDE_BACK)
#define	PSIDE_FACING		4

/* shared/shared.c, ufo2map/common/scriplib.c, ufo2map/common/scriplib.h, client/battlescape/cl_parse.c, client/client.h,
 * common/common.c, server/server.h, server/sv_ccmds.c, server/sv_game.c, server/sv_init.c */
#define	MAX_TOKEN_CHARS		256 /* max length of an individual token */

/* common/tracing.c, ufo2map/common/polylib.c, ufo2map/faces.c, ufo2map/patches.c, ufo2map/portals.c, ufo2map/common/polylib.h */
#define	ON_EPSILON	0.1

/* 1/32 epsilon to keep floating point happy */
/* common/tracing.c, ufo2map/map.c, client/cl_actor.c, common/cmodel.c */
#define	DIST_EPSILON	(0.03125)
#define	DIST_EPSILON2	(0.0625125) /* DIST_EPSILON * 2 + small bit more */

/* ufo2map/map.c */
#define	MAP_DIST_EPSILON	0.01
#define	NORMAL_EPSILON	0.00001
/*
From tools/ufo2map/map.c
#define	DIST_EPSILON	0.01
*/

/* ufo2map/bsp.h, ufo2map/map.c */
#define	MAX_MAP_SIDES (MAX_MAP_BRUSHES*6)
/* ufo2map/bsp.h, ufo2map/textures.c */
#define	MAX_MAP_TEXTURES	1024

#define MAX_MAP_LIGHTMAP	(512 * 512)

#define MAP_SIZE_OFFSET 100

#endif /* SHARED_DEFINES_H */
