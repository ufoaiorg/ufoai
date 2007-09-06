/**
 * @file ref.h
 * @brief Think this may be the header for the renderer that specifies the renderer API, but am not sure.
 *
 * @todo: If this is indeed the header for the render, that specifies the renderer API, it should be with the renderer, not the client.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "../common/common.h"

#include <SDL.h>
#include <SDL_syswm.h>

#ifdef _WIN32
#include <windows.h>
#else
typedef void* HINSTANCE;
typedef void* WNDPROC;
#endif

#include "vid.h"

#define GAME_TITLE "UFO:AI"
#define GAME_TITLE_LONG "UFO:Alien Invasion"

#define VID_NORM_WIDTH		1024
#define VID_NORM_HEIGHT		768

#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	512
#define	MAX_LIGHTSTYLES	256
#define MAX_PTL_ART		512
#define MAX_PTLS		2048

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
	struct model_s *model;		/**< opaque type outside refresh */
	float angles[3];

	float origin[3];
	float oldorigin[3];

	/*
	 ** tag positioning
	 */
	struct entity_s *tagent;	/**< pointer to the parent entity */
	const char *tagname;				/**< name of the tag */

	/*
	 ** misc
	 */
	int skinnum;

	float *lightcolor;			/**< color for fixed light */
	float *lightambient;		/**< ambient color for fixed light */
	float *lightparam;			/**< fraction lit by the sun */

	int lightstyle;				/**< for flashing entities */
	float alpha;				/**< ignore if RF_TRANSLUCENT isn't set */

	struct image_s *skin;		/**< NULL for inline skin */
	int flags;

	animState_t as;
} entity_t;

/*============================================================================= */

#define MAX_SHADERS 64
typedef struct shader_s {
	/* title is internal for finding the shader */

	/** we should use this shader when loading the image */
	char *name;

	/** filename is for an external filename to load the shader from */
	char *filename;

	/** image filenames */
	char *distort;
	char *normal;

	qboolean glsl;				/**< glsl shader */
	qboolean frag;				/**< fragment-shader */
	qboolean vertex;			/**< vertex-shader */

	qboolean emboss;			/**< active emboss mapping */
	qboolean embossHigh;
	qboolean embossLow;
	qboolean emboss2;
	qboolean blur;
	qboolean light;
	qboolean edge;
	byte glMode;
	/* @todo: */

	/* vpid and fpid are vertexpid and fragmentpid for binding */
	unsigned int vpid, fpid, glslpid;
} shader_t;

typedef struct {
	float rgb[3];				/**< 0.0 - 2.0 */
	float white;				/**< highest of rgb */
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
	vec3_t direction;   /**< used by MD3 */
} dlight_t;

typedef struct {
	struct model_s *model;
	char *name;				/**< model path */

	float *origin;			/**< pointer to node/menumodel origin */
	float *angles;			/**< pointer to node/menumodel angles */
	float *scale;			/**< pointer to node/menumodel scale */
	float *center;			/**< pointer to node/menumodel center */

	int frame, oldframe;	/**< animation frames */
	float backlerp;

	int skin;				/**< skin number */
	float *color;
} modelInfo_t;


typedef struct ptlCmd_s {
	byte cmd;
	byte type;
	int ref;
} ptlCmd_t;

typedef struct ptlDef_s {
	char name[MAX_VAR];
	ptlCmd_t *init, *run, *think, *round, *physics;
} ptlDef_t;

typedef struct ptl_s {
	/* used by ref */
	qboolean inuse;			/**< particle active? */
	qboolean invis;			/**< is this particle invisible */
	int pic, model;			/**< index of pic or model */
	byte blend;				/**< blend mode */
	byte style;				/**< style mode */
	vec2_t size;
	vec3_t scale;
	vec4_t color;
	vec3_t s;			/**< current position */
	vec3_t origin;		/**< start position - set initial s position to get this value */
	vec3_t offset;
	vec3_t angles;
	int levelFlags;

	int skin;		/**< model skin to use for this particle */

	struct ptl_s* children;	/**< list of children */
	struct ptl_s* next;		/**< next peer in list */
	struct ptl_s* parent;   /**< pointer to parent */

	/* private */
	ptlDef_t *ctrl;
	int startTime;
	int frame, endFrame;
	float fps, lastFrame;
	float tps, lastThink;
	byte thinkFade, frameFade;
	float t, dt, life;
	int rounds, roundsCnt;
	vec3_t a, v, omega;
	int light;
	qboolean physics;	/**< basic physics */
	qboolean autohide;	/**< only draw the particle if the current position is
						 * not higher than the current level (useful for weather
						 * particles) */
	qboolean stayalive;	/**< used for physics particles that hit the ground */
	qboolean weather;	/**< used to identify weather particles (can be switched
						 * off via cvar cl_particleweather) */
} ptl_t;

typedef struct ptlArt_s {
	byte type;
	byte frame;
	char name[MAX_VAR];
	int skin;
	char *art;
} ptlArt_t;

typedef struct {
	int x, y, width, height;	/**< in virtual screen coordinates */
	float fov_x, fov_y;
	float vieworg[3];
	float viewangles[3];
	float blend[4];				/**< rgba 0-1 full screen blend */
	float time;					/**< time is used to auto animate */
	int rdflags;				/**< RDF_NOWORLDMODEL, etc */
	int worldlevel;

	lightstyle_t *lightstyles;	/**< [MAX_LIGHTSTYLES] */

	int num_entities;
	entity_t *entities;

	int num_dlights;
	dlight_t *dlights;

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

	const char *mapZone;	/**< used to replace textures in base assembly */

	int c_brush_polys, c_alias_polys;
} refdef_t;

void R_BeginFrame(void);
void R_EndFrame(void);
void R_RenderFrame(refdef_t *fd);

void R_AnimAppend(animState_t * as, struct model_s *mod, const char *name);
void R_AnimChange(animState_t * as, struct model_s *mod, const char *name);
void R_AnimRun(animState_t * as, struct model_s *mod, int msec);
char *R_AnimGetName(animState_t * as, struct model_s *mod);

struct model_s *R_RegisterModelShort(const char *name);
struct image_s *R_RegisterPic(const char *name);

void R_DrawPic(int x, int y, const char *name);
void R_DrawNormPic(float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, const char *name);
void R_DrawChar(int x, int y, int c);
void R_DrawFill(int x, int y, int w, int h, int align, const vec4_t color);
void R_DrawColor(const float *rgba);
void R_Draw3DGlobe(int x, int y, int w, int h, float p, float q, vec3_t rotate, float zoom, const char *map);
void R_Draw3DMapMarkers(vec3_t angles, float zoom, vec3_t position, const char *model);
void R_DrawImagePixelData(const char *name, byte *frame, int width, int height);
void R_DrawGetPicSize(int *w, int *h, const char *name);
void R_DrawModelDirect(modelInfo_t * mi, modelInfo_t * pmi, const char *tag);
void R_DrawDayAndNight(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map);
void R_DrawClouds(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map);
void R_DrawLineStrip(int points, int *verts);
void R_DrawLineLoop(int points, int *verts);
void R_DrawCircle (vec3_t mid, float radius, const vec4_t color, int thickness);
void R_DrawPolygon(int points, int *verts);

void R_ModBeginLoading(const char *tiles, const char *pos);
void R_ModEndLoading(void);

void R_TakeVideoFrame(int w, int h, byte * captureBuffer, byte * encodeBuffer, qboolean motionJpeg);

void R_LoadTGA(const char *name, byte ** pic, int *width, int *height);

void R_FontRegister(const char *name, int size, const char *path, const char *style);
int R_FontDrawString(const char *fontID, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight, const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, qboolean increaseLine);
void R_FontLength(const char *font, char *c, int *width, int *height);

qboolean R_Init(void);
qboolean R_SetMode(void);
void R_Shutdown(void);
void R_Restart(void);

extern SDL_Surface *r_surface;

#endif /* CLIENT_REF_H */
