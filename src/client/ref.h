/**
 * @file ref.h
 * @brief Think this may be the header for the renderer that specifies the renderer API, but am not sure.
 *
 * TODO: If this is indeed the header for the render, that specifies the renderer API, it should be with the renderer, not the client.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

15/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.
	Modified inclusion guard.

Original file from Quake 2 v3.21: quake2-2.31/client/ref.h
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

#ifndef CLIENT_REF_H
#define CLIENT_REF_H

#include "../qcommon/qcommon.h"

#ifdef _WIN32
#include <windows.h>
#else
typedef void* HINSTANCE;
typedef void* WNDPROC;
#endif

#define GAME_TITLE "UFO:AI"
#define GAME_TITLE_LONG "UFO:Alien Invasion"

#define VID_NORM_WIDTH		1024
#define VID_NORM_HEIGHT		768

#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	512
#define	MAX_PARTICLES	8192
#define	MAX_LIGHTSTYLES	256
#define MAX_PTL_ART		512
#define MAX_PTLS		2048

#define POWERSUIT_SCALE		4.0F

#define SHELL_RED_COLOR		0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR		0xDC
/*#define SHELL_RB_COLOR		0x86 */
#define SHELL_RB_COLOR		0x68
#define SHELL_BG_COLOR		0x78

/*ROGUE */
#define SHELL_DOUBLE_COLOR	0xDF	/* 223 */
#define	SHELL_HALF_DAM_COLOR	0x90
#define SHELL_CYAN_COLOR	0x72
/*ROGUE */

#define SHELL_WHITE_COLOR	0xD7

#define MAX_ANIMLIST	8

typedef struct animState_s {
	int frame, oldframe;
	float backlerp;
	int time, dt;

	byte list[MAX_ANIMLIST];
	byte lcur, ladd;
	byte change;
} animState_t;

typedef struct entity_s {
	struct model_s *model;		/* opaque type outside refresh */
	float angles[3];

	float origin[3];			/* also used as RF_BEAM's "from" */
	float oldorigin[3];			/* also used as RF_BEAM's "to" */

	/*
	 ** tag positioning
	 */
	struct entity_s *tagent;	/* pointer to the parent entity */
	char *tagname;				/* name of the tag */

	/*
	 ** misc
	 */
	int skinnum;				/* also used as RF_BEAM's palette index */

	float *lightcolor;			/* color for fixed light */
	float *lightambient;		/* ambient color for fixed light */
	float *lightparam;			/* fraction lit by the sun */

	int lightstyle;				/* for flashing entities */
	float alpha;				/* ignore if RF_TRANSLUCENT isn't set */

	struct image_s *skin;		/* NULL for inline skin */
	int flags;

	animState_t as;
} entity_t;

#define ENTITY_FLAGS  68

/*============================================================================= */

#define MAX_SHADERS 255
typedef struct shader_s {
	/* title is internal for finding the shader */

	/* we should use this shader when loading the image */
	char title[MAX_VAR];

	/* filename is for an external filename to load the shader from */
	char filename[MAX_VAR];

	qboolean frag;				/* fragment-shader */
	qboolean vertex;			/* vertex-shader */

	qboolean emboss;			/* active emboss mapping */
	qboolean embossHigh;
	qboolean embossLow;
	qboolean emboss2;
	qboolean blur;
	qboolean light;
	qboolean edge;
	int glMode;
	/* TODO: */

	/* vpid and fpid are vertexpid and fragmentpid for binding */
	unsigned int vpid, fpid;
} shader_t;

typedef struct {
	vec3_t origin;
	int color;
	float alpha;
} particle_t;

typedef struct {
	float rgb[3];				/* 0.0 - 2.0 */
	float white;				/* highest of rgb */
} lightstyle_t;

typedef struct {
	vec3_t dir;
	vec4_t color;
	vec4_t ambient;
} sun_t;

typedef struct {
	vec3_t origin;
	vec3_t color;
	float intensity;
} dlight_t;

typedef struct {
	struct model_s *model;
	char *name;


	float *origin;
	float *angles;
	float *scale;
	float *center;

	int frame, oldframe;
	float backlerp;

	int skin;
	float *color;
} modelInfo_t;

typedef struct ptl_s {
	/* used by ref */
	qboolean inuse;
	int pic, model;
	byte blend;
	byte style;
	vec2_t size;
	vec3_t scale;
	vec4_t color;
	vec3_t s;
	vec3_t angles;
	int levelFlags;

	/* private */
	struct ptlDef_s *ctrl;
	int startTime;
	int frame, endFrame;
	float fps, lastFrame;
	float tps, lastThink;
	byte thinkFade, frameFade;
	float t, dt, life;
	int rounds, roundsCnt;
	vec3_t a, v, omega;
	qboolean light;
} ptl_t;

typedef struct ptlArt_s {
	byte type;
	byte frame;
	char name[MAX_VAR];
	char *art;
} ptlArt_t;

typedef struct {
	int x, y, width, height;	/* in virtual screen coordinates */
	float fov_x, fov_y;
	float vieworg[3];
	float viewangles[3];
	float blend[4];				/* rgba 0-1 full screen blend */
	float time;					/* time is used to auto animate */
	int rdflags;				/* RDF_UNDERWATER, etc */
	int worldlevel;

	byte *areabits;				/* if not NULL, only areas with set bits will be drawn */

	lightstyle_t *lightstyles;	/* [MAX_LIGHTSTYLES] */

	int num_entities;
	entity_t *entities;

	int num_dlights;
	dlight_t *dlights;

	int num_particles;
	particle_t *particles;

	int num_shaders;
	shader_t *shaders;

	int num_ptls;
	ptl_t *ptls;
	ptlArt_t *ptl_art;

	sun_t *sun;
	int num_lights;
	dlight_t *ll;

	float fog;
	float *fogColor;
} refdef_t;



#define	API_VERSION		4

/* these are the functions exported by the refresh module */
typedef struct {
	/* if api_version is different, the dll cannot be used */
	int api_version;

	/* called when the library is loaded */
	qboolean(*Init) (HINSTANCE hinstance, WNDPROC wndproc);

	/* called before the library is unloaded */
	void (*Shutdown) (void);

	/* All data that will be used in a level should be */
	/* registered before rendering any frames to prevent disk hits, */
	/* but they can still be registered at a later time */
	/* if necessary. */

	/* EndRegistration will free any remaining data that wasn't registered. */
	/* Any model_s or skin_s pointers from before the BeginRegistration */
	/* are no longer valid after EndRegistration. */

	/* Skins and images need to be differentiated, because skins */
	/* are flood filled to eliminate mip map edge errors, and pics have */
	/* an implicit "pics/" prepended to the name. (a pic name that starts with a */
	/* slash will not use the "pics/" prefix or the ".pcx" postfix) */
	void (*BeginRegistration) (char *tiles, char *pos);
	struct model_s *(*RegisterModel) (const char *name);
	struct image_s *(*RegisterSkin) (const char *name);

	struct image_s *(*RegisterPic) (const char *name);
	void (*EndRegistration) (void);

	void (*RenderFrame) (refdef_t * fd);
	void (*SetRefDef) (refdef_t * fd);

	void (*DrawModelDirect) (modelInfo_t * mi, modelInfo_t * pmi, char *tag);
	void (*DrawGetPicSize) (int *w, int *h, char *name);	/* will return 0 0 if not found */
	void (*DrawPic) (int x, int y, char *name);
	void (*DrawNormPic) (float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, char *name);
	void (*DrawStretchPic) (int x, int y, int w, int h, char *name);
	void (*DrawChar) (int x, int y, int c);
	void (*FontRegister) (const char *name, int size, char *path, char *style);
	void (*FontLength) (const char *font, char *c, int *width, int *height);
	int (*FontDrawString) (const char *font, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight, const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, qboolean increaseLine);
	void (*DrawTileClear) (int x, int y, int w, int h, char *name);
	void (*DrawFill) (int x, int y, int w, int h, int style, const vec4_t color);
	void (*DrawColor) (const float *rgba);
	void (*DrawFadeScreen) (void);
	void (*DrawDayAndNight) (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char *map);
	void (*DrawLineStrip) (int points, int *verts);
	void (*Draw3DGlobe) (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char *map);
	void (*Draw3DMapMarkers) (float latitude, float longitude, char *image);
	void (*Draw3DMapLine) (int n, float dist, vec2_t * v);

	void (*AnimAppend) (animState_t * as, struct model_s * mod, char *name);
	void (*AnimChange) (animState_t * as, struct model_s * mod, char *name);
	void (*AnimRun) (animState_t * as, struct model_s * mod, int msec);
	char *(*AnimGetName) (animState_t * as, struct model_s * mod);

	void (*LoadTGA) (const char *name, byte ** pic, int *width, int *height);

	/* video mode and refresh state management entry points */
	void (*BeginFrame) (float camera_separation);
	void (*EndFrame) (void);
	void (*AppActivate) (qboolean activate);
	void (*TakeVideoFrame) (int h, int w, byte * captureBuffer, byte * encodeBuffer, qboolean motionJpeg);
} refexport_t;

/* these are the functions imported by the refresh module */
typedef struct {
	void (*Sys_Error) (int err_level, char *str, ...);

	void (*Cmd_AddCommand) (char *name, void (*cmd) (void), char *desc);
	void (*Cmd_RemoveCommand) (const char *name);
	int (*Cmd_Argc) (void);
	char *(*Cmd_Argv) (int i);
	void (*Cmd_ExecuteText) (int exec_when, char *text);

	void (*Con_Printf) (int print_level, char *str, ...);

	/* files will be memory mapped read only */
	/* the returned buffer may be part of a larger pak file, */
	/* or a discrete file from anywhere in the quake search path */
	/* a -1 return means the file does not exist */
	/* NULL can be passed for buf to just determine existance */
	int (*FS_WriteFile) (const void *buffer, int len, const char *filename);
	int (*FS_LoadFile) (const char *name, void **buf);
	void (*FS_FreeFile) (void *buf);
	int (*FS_CheckFile) (const char *name);
	char **(*FS_ListFiles) (const char *findname, int *numfiles, unsigned musthave, unsigned canthave);

	/* will return the size and the path for each font */
	void (*CL_GetFontData) (const char *name, int *size, char *path);

	/* gamedir will be the current directory that generated */
	/* files should be stored to, ie: "f:\quake\id1" */
	char *(*FS_Gamedir) (void);
	char *(*FS_NextPath) (const char *prevpath);

	cvar_t *(*Cvar_Get) (const char *name, const char *value, int flags, char* desc);
	cvar_t *(*Cvar_Set) (const char *name, const char *value);
	void (*Cvar_SetValue) (const char *name, float value);

	qboolean(*Vid_GetModeInfo) (int *width, int *height, int mode);
	void (*Vid_NewWindow) (int width, int height);
	void (*CL_WriteAVIVideoFrame) (const byte * buffer, int size);
} refimport_t;


/* this is the only function actually exported at the linker level */
typedef refexport_t (*GetRefAPI_t) (refimport_t );

#endif /* CLIENT_REF_H */
