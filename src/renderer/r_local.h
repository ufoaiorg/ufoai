/**
 * @file r_local.h
 * @brief local graphics definitions
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/ref_gl/gl_local.h
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

#ifndef R_LOCAL_H
#define R_LOCAL_H

#include "../client/ref.h"
#include "../client/vid.h"

#define RGB_PIXELSIZE 3

#include <SDL_ttf.h>

#include "qgl.h"

#include "r_shader.h"

/*
skins will be outline flood filled and mip mapped
pics and sprites with alpha will be outline flood filled
pic won't be mip mapped

model skin
sprite frame
wall texture
pic
*/

typedef enum {
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_wrappic,
	it_static
} imagetype_t;

typedef struct image_s {
	char name[MAX_QPATH];				/**< game path, including extension, must be first */
	imagetype_t type;
	int width, height;					/**< source image dimensions */
	int upload_width, upload_height;	/**< dimensions after power of two and picmip */
	int registration_sequence;			/**< 0 = free */
	struct mBspSurface_s *texturechain;	/**< for sort-by-texture world drawing */
	GLuint texnum;						/**< gl texture binding */
	qboolean has_alpha;
	shader_t *shader;					/**< pointer to shader from refdef_t */
} image_t;

#define TEXNUM_LIGHTMAPS    1024
#define TEXNUM_IMAGES       1281

#define MAX_GLERRORTEX      4096
#define MAX_GLTEXTURES      1024

/* starting offset for font texture numbers (used in font-cache) */
#define TEXNUM_FONTS        TEXNUM_IMAGES + MAX_GLTEXTURES

/*=================================================================== */

typedef struct {
	int width;
	int height;
	byte *captureBuffer;
	byte *encodeBuffer;
	qboolean motionJpeg;
} videoFrameCommand_t;

size_t R_SaveJPGToBuffer(byte * buffer, int quality, int image_width, int image_height, byte * image_buffer);
void RE_TakeVideoFrame(int width, int height, byte * captureBuffer, byte * encodeBuffer, qboolean motionJpeg);
const void *RB_TakeVideoFrameCmd(const void *data);

/*=================================================================== */

void R_WritePNG(FILE *f, byte *buffer, int width, int height);
void R_WriteJPG(FILE *f, byte *buffer, int width, int height, int quality);
void R_WriteTGA(FILE *f, byte *buffer, int width, int height);

/*=================================================================== */

#include "r_model.h"

int RecursiveLightPoint(model_t* mapTile, mBspNode_t * node, vec3_t start, vec3_t end);

void R_SetDefaultState(void);

#define BACKFACE_EPSILON    0.01

/** @brief entity transform */
typedef struct {
	qboolean done;
	qboolean processing;
	float matrix[16];
} transform_t;

extern transform_t trafo[MAX_ENTITIES];

/*==================================================== */

extern int spherelist;	/**< the gl list of the 3d globe */

extern image_t *shadow;	/**< draw this when actor is alive */
extern image_t *blood; /**< draw this when actor is dead */
extern image_t *r_notexture;
extern image_t *DaN;

extern entity_t *currententity;
extern model_t *currentmodel;
extern int r_framecount;
extern cBspPlane_t frustum[4];
extern int gl_filter_min, gl_filter_max;

/* view origin */
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;

extern cvar_t *r_drawclouds;
extern cvar_t *r_drawworld;
extern cvar_t *r_nocull;
extern cvar_t *r_isometric;
extern cvar_t *r_lerpmodels;

extern cvar_t *r_anisotropic;
extern cvar_t *r_texture_lod;   /* lod_bias */
extern cvar_t *r_bitdepth;
extern cvar_t *r_stencilsize;
extern cvar_t *r_colordepth;

extern cvar_t *r_screenshot;
extern cvar_t *r_screenshot_jpeg_quality;

extern cvar_t *r_ext_multitexture;
extern cvar_t *r_ext_combine;
extern cvar_t *r_ext_lockarrays;
extern cvar_t *r_ext_texture_compression;
extern cvar_t *r_ext_s3tc_compression;
extern cvar_t *r_intel_hack;

extern cvar_t *r_checkerror;
extern cvar_t *r_lightmap;

extern cvar_t *r_showbox;

/* shadow stuff */
void R_ShadowBlend(void);
void R_DrawShadowVolume(entity_t * e);
void R_DrawShadow(entity_t * e);
extern cvar_t *r_shadows;
extern cvar_t *r_shadow_debug_shade;
extern cvar_t *r_shadow_debug_volume;
extern cvar_t *r_ati_separate_stencil;
extern cvar_t *r_stencil_two_side;

extern cvar_t *r_dynamic;
extern cvar_t *r_soften;
extern cvar_t *r_showtris;
extern cvar_t *r_flashblend;
extern cvar_t *r_modulate;
extern cvar_t *r_drawbuffer;
extern cvar_t *r_driver;
extern cvar_t *r_swapinterval;
extern cvar_t *r_acceleratedvisuals;
extern cvar_t *r_texturemode;
extern cvar_t *r_texturealphamode;
extern cvar_t *r_texturesolidmode;
extern cvar_t *r_wire;
extern cvar_t *r_fog;
extern cvar_t *r_intensity;
extern cvar_t *r_imagefilter;

extern image_t *draw_chars[2];

extern int gl_lightmap_format;
extern int gl_solid_format;
extern int gl_alpha_format;
extern int gl_compressed_solid_format;
extern int gl_compressed_alpha_format;

extern float r_world_matrix[16];

void R_Bind(int texnum);
void R_MBind(GLenum target, int texnum);
void R_TexEnv(GLenum value);
/*void R_UpdateAnisotropy(void);*/
void R_EnableMultitexture(qboolean enable);
void R_SelectTexture(GLenum);
void R_CalcDayAndNight(float q);

void R_LightPoint(vec3_t p, vec3_t color);

/*==================================================================== */

void QR_UnLink(void);
void QR_Link(void);

/*==================================================================== */

extern model_t *rTiles[MAX_MAPTILES];
extern int rNumTiles;

extern unsigned d_8to24table[256];

extern int registration_sequence;

void R_ScreenShot_f(void);
void R_InterpolateTransform(animState_t * as, int numframes, float *tag, float *interpolated);
void R_DrawModelParticle(modelInfo_t * mi);
void R_DrawBrushModel(entity_t * e);
void R_DrawBox(const entity_t * e);
void R_DrawHighlight(const entity_t * e);
void R_DrawLevelBrushes(void);
void R_DrawOBJModel(entity_t *e);
int R_ModLoadOBJModel(model_t* mod, void* buffer, int bufSize);
void R_RenderDlights(void);
void R_DrawAlphaSurfaces(void);
void R_InitMiscTexture(void);
void R_DrawInitLocal(void);
void R_SubdivideSurface(mBspSurface_t * fa);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_RotateForEntity(entity_t * e);

void R_DrawTurbSurface(mBspSurface_t * fa);
void R_DrawTriangleOutlines(void);
void R_MarkLights(dlight_t * light, int bit, mBspNode_t * node);

#include "r_font.h"

void R_SwapBuffers(int);

void R_SoftenTexture(byte *in, int width, int height, int bpp);

image_t *R_LoadPic(const char *name, byte * pic, int width, int height, imagetype_t type, int bits);
#ifdef DEBUG
image_t *R_FindImageDebug(const char *pname, imagetype_t type, const char *file, int line);
#define R_FindImage(pname,type) R_FindImageDebug(pname, type, __FILE__, __LINE__ )
#else
image_t *R_FindImage(const char *pname, imagetype_t type);
#endif
image_t *R_FindImageForShader(const char *name);
void R_TextureMode(const char *string);
void R_ImageList_f(void);

void R_InitImages(void);
void R_ShutdownImages(void);
void R_ShutdownModels(void);
void R_FreeUnusedImages(void);

void R_TextureAlphaMode(const char *string);
void R_TextureSolidMode(const char *string);

void R_DrawPtls(void);

/*
** GL config stuff
*/
typedef struct {
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;
	int maxTextureSize;
	int maxTextureUnits;
	GLenum envCombine;
} rconfig_t;

typedef struct {
	float inverse_intensity;
	int displayrefresh;
	qboolean fullscreen;

	int lightmap_texnum;

	vec4_t color;

	int currenttextures[2];
	int currenttmu;

	int maxAnisotropic;

	qboolean hwgamma;
	qboolean stencil_two_side;
	qboolean ati_separate_stencil;
	qboolean blend;
	qboolean alpha_test;
	qboolean fog_coord;
	qboolean multitexture;
	qboolean stencil_wrap;
	qboolean anisotropic;
	qboolean lod_bias;
	qboolean arb_fragment_program;
	qboolean glsl_program;
} rstate_t;

extern rconfig_t r_config;
extern rstate_t r_state;

#define RSTATE_DISABLE_ALPHATEST   if (r_state.alpha_test) { qglDisable(GL_ALPHA_TEST); r_state.alpha_test=qfalse; }
#define RSTATE_ENABLE_ALPHATEST    if (!r_state.alpha_test) { qglEnable(GL_ALPHA_TEST); r_state.alpha_test=qtrue; }

#define RSTATE_DISABLE_BLEND       if (r_state.blend) { qglDisable(GL_BLEND); r_state.blend=qfalse; }
#define RSTATE_ENABLE_BLEND        if (!r_state.blend) { qglEnable(GL_BLEND); r_state.blend=qtrue; }

/*
====================================================================
IMPLEMENTATION SPECIFIC FUNCTIONS
====================================================================
*/

qboolean Rimp_Init(void);
void Rimp_Shutdown(void);
qboolean R_InitGraphics(void);

/* 3d globe */

#define PHYSICAL_EARTH_RADIUS 6378137   /* Earth's radius, in meters. (From RFC1876) */
#define SPEED_OF_LIGHT 299792458        /* The big C, in meters/second. */
#define PHYSICAL_EARTH_CIRC (2.0 * M_PI * PHYSICAL_EARTH_RADIUS)
#define NOT_SELECTABLE ~0U              /* For picking reasons, see which_site() */

/* end of 3d globe */

void R_BuildLightMap(mBspSurface_t * surf, byte * dest, int stride);

void R_SetupGL2D(void);
void R_SetupGL3D(void);

#endif /* R_LOCAL_H */
