/**
 * @brief
 * @file shared.h
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
#include "../../../shared/shared.h"
#include "../../../shared/mathlib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>

#ifndef errno
extern int errno;
#endif

/*========================================================================
The .pak files are just a linear collapse of a directory tree
========================================================================*/

#define MAX_QPATH 64
#define MAX_OSPATH 128

typedef struct qFILE_s {
	void *z; /**< in case of the file being a zip archive */
	FILE *f; /**< in case the file being part of a pak or the actual file */
	char name[MAX_OSPATH];
	unsigned long filepos;
	unsigned long size;
} qFILE;

/* valid -noradiosity parameters */
#define RADIOSITY_NONE			1
#define RADIOSITY_DAY_ONLY		2
#define RADIOSITY_NIGHT_ONLY	3

typedef struct mapConfig_s {
	float subdivideSize;
	int nice;
	qboolean verbose;
	qboolean noprune;
	qboolean nodetail;
	qboolean fulldetail;
	qboolean onlyents;
	qboolean nomerge;
	qboolean nowater;
	qboolean nofill;
	qboolean nocsg;
	qboolean noweld;
	qboolean noshare;
	qboolean nosubdiv;
	qboolean notjunc;
	qboolean verboseentities;
	qboolean nobackclip;
	int noradiosity;
	int block_xl, block_xh, block_yl, block_yh;
	float microvolume;
	unsigned int numbounce;
	qboolean extrasamples;
	float subdiv;
	qboolean info;
	qboolean generateFootstepFile;
	qboolean generateMaterialFile;

	float day_ambient_red;
	float day_ambient_green;
	float day_ambient_blue;
	float day_sun_intensity;
	float day_sun_pitch;
	float day_sun_yaw;
	vec3_t day_sun_color;
	vec3_t day_sun_dir;

	float night_ambient_red;
	float night_ambient_green;
	float night_ambient_blue;
	float night_sun_intensity;
	float night_sun_pitch;
	float night_sun_yaw;
	vec3_t night_sun_color;
	vec3_t night_sun_dir;

	float maxlight;
	float lightscale;
	byte lightquant;
	float direct_scale;
	float entity_scale;

	int compile_for_day;	/**< set this to 1 if you want to compile the day version of the lightmap */
} mapConfig_t;

extern mapConfig_t config;

void U2M_ProgressBar(void (*func) (unsigned int cnt), unsigned int count, qboolean showProgress, const char *id);

#include "../../../common/qfiles.h"

extern qboolean verbose;

#define SYS_VRB 0 /* verbose support (on/off) */
#define SYS_STD 1 /* standard print level */
#define SYS_WRN 2 /* warnings */
#define SYS_ERR 3 /* error */

/* the dec offsetof macro doesnt work very well... */
#define myoffsetof(type,identifier) ((size_t)&((type *)0)->identifier)

extern char gamedir[1024];

void Sys_Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Com_Printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void Sys_FPrintf(int flag, const char *text, ...) __attribute__((format(printf, 2, 3)));

#endif /* _SHARED_H */
