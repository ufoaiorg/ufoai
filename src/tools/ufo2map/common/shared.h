/**
 * @file src/tools/ufo2map/common/shared.h
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

#ifndef _SHARED_H
#define _SHARED_H

#include "../../../shared/ufotypes.h"
#include "../../../common/unzip.h"
#include "../../../common/mem.h"
#include "../../../common/filesys.h"
#include "../../../shared/shared.h"
#include "../../../shared/mathlib.h"
#include "../../../shared/defines.h"
#include "../../../shared/typedefs.h"
#include "../../../shared/byte.h"

/** @brief verbosity levels for use in calls to Verb_Printf
 *  and on the command line -v \<int\> */
typedef enum {
	VERB_SILENT_EXCEPT_ERROR,
	VERB_CHECK, /**< print nothing, except check/fix messages */
	VERB_MAPNAME, /**< print mapname only. in check/fix, only do this if a problem is found */
	VERB_LESS,
	VERB_NORMAL,
	VERB_EXTRA,
	VERB_DUMP, /**< the highest verbosity level */
	VERB_NUM /**< the number of verbosity levels */
} verbosityLevel_t;

qboolean AbortPrint(const verbosityLevel_t importance);
void Verb_Printf(const verbosityLevel_t importance, const char *format, ...) __attribute__((format(printf, 2, 3)));

typedef struct mapConfig_s {
	float subdivideSize;
	int nice;
	qboolean verbose;
	verbosityLevel_t verbosity;
	qboolean noprune;		/**< If not set to true the brushlists of child nodes are combined. The bsp file will get larger
							 * and the compile time will increase */
	qboolean nodetail;		/**< do not put any of the detail brushes in the bsp */
	qboolean fulldetail;	/**< everything with content will get a detail brush */
	qboolean onlyents;		/**< will only update the entity data without recalculating the pathfinding or the lightmaps */
	qboolean exportLightmaps;	/**< will export the lightmaps and direction data into tga images */
	qboolean nomerge;		/**< If set, faces that share the same edge on the same plane are not merged */
	qboolean nowater;		/**< does not add any CONTENTS_WATER brush to the bsp */
	qboolean nocsg;			/**< If set, the call to chop the brushes is deactivated - there will be no reduction of intersecting brushes
							 * with non-intersecting brushes. The resulting bsp file will also be larger with this option turned on. */
	qboolean noweld;		/**< use vertices from curTile->vertexes array or if false, create vertices from a hash table */
	qboolean noshare;		/**< If true, keep identical edges in the bsp */
	qboolean nosubdiv;		/**< If this is set to false faces that are larger than the given chop size will get subdivided */
	qboolean notjunc;		/**< If true, do not break edges on tjunctions. This will decrease the compile times and should not be used on final compiles */
	qboolean verboseentities;	/**< prints entity sub model informations to the console */
	qboolean nobackclip;	/**< does not filter out back clipping brushes - results in larger bsp files */
	int nolighting;
	int block_xl, block_xh, block_yl, block_yh;
	float microvolume; /**< -micro test after CSG */
	qboolean extrasamples;	/**< apply sampling to the lightmap- makes lightmap smoother */
	qboolean soft; /**< use different supersampling pattern for lightmap, which produces softer shadows */
	qboolean info;
	qboolean generateFootstepFile;
	qboolean generateMaterialFile;

	qboolean performMapCheck;
	qboolean chkEntities;
	qboolean chkContained;
	qboolean chkFillLevelFlags;
	qboolean chkIntersection;
	qboolean chkNodraws;
	qboolean chkBrushes;
	qboolean chkLevelFlags;
	qboolean chkTextures;
	qboolean chkMixedFaceContents;
	qboolean chkMMicro;
	qboolean chkZFight;
	float mapMicrovol; /**< -check mbr micro test before CSG */
	qboolean chkAll;
	qboolean fixMap;
	qboolean stats; /**< print stats and quit. implemented as a kind of check */
	qboolean generateTraceFile;
	qboolean generateDebugTrace;

	int sun_intensity[LIGHTMAP_MAX];
	vec3_t sun_ambient_color[LIGHTMAP_MAX];
	vec3_t sun_color[LIGHTMAP_MAX];
	vec3_t sun_angles[LIGHTMAP_MAX];
	vec3_t sun_normal[LIGHTMAP_MAX];

	char gamedir[MAX_VAR];

	float saturation;
	float contrast;
	float brightness;		/**< global light scale factor */
	byte lightquant;
	float surface_scale;	/**< surface lighting scale factor */
	float entity_scale;		/**< entity lighting scale factor */

	int onlynewer;	/**< only compile (update) the bsp if the map file is newer */

	int compile_for_day;	/**< set this to 1 if you want to compile the day version of the lightmap */
} mapConfig_t;

extern mapConfig_t config;
extern char baseFilename[MAX_OSPATH]; /**< This is used for extra file output functions */

typedef struct threadstate_s {
	int numthreads;		/**< spawned threads */
	int workindex;		/**< current work cycle */
	int workcount;		/**< total work cycles */
	int workfrac;		/**< last fraction of work completed (tenths) */
	int worktick;		/**< Number of iterations before the progress spinner is spun */
	qboolean progress;	/**< are we reporting progress */
} threadstate_t;

extern threadstate_t threadstate;

extern memPool_t *com_genericPool;
extern memPool_t *com_fileSysPool;

void ThreadLock(void);
void ThreadUnlock(void);
void RunThreadsOn(void (*func)(unsigned int), unsigned int workcount, qboolean progress, const char *id);
void RunSingleThreadOn(void (*func)(unsigned int), unsigned int workcount, qboolean progress, const char *id);

#define SYS_VRB 0 /* verbose support (on/off) */
#define SYS_STD 1 /* standard print level */
#define SYS_WRN 2 /* warnings */
#define SYS_ERR 3 /* error */

void Sys_Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Com_Printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif /* _SHARED_H */
