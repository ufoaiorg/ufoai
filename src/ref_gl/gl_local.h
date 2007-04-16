/**
 * @file gl_local.h
 * @brief local graphics definitions
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

12/04/07, Michael Ploujnikov (Plouj):
	Documented viddef_t.
	Added doxygen file comment.
	Updated copyright notice.
	Changed some image_s struct comments.

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

#ifndef GL_LOCAL_H
#define GL_LOCAL_H

#include "../client/ref.h"

#if !defined __linux__ && !defined __FreeBSD__ && !defined __NetBSD__ && !defined _MSC_VER
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT GL_COLOR_INDEX
#endif
#endif

#if !defined _MSC_VER && !defined __MINGW32__
#include <jpeglib.h>
#include <png.h>
#else
#include "../ports/win32/jpeglib.h"
#include "../ports/win32/png.h"
#endif

/* this was taken from jmorecfg.h */
#define RGB_PIXELSIZE 3

#include "gl_arb_shader.h"

#if !defined _WIN32 || ( defined(__MINGW32__) && !defined(DEVCPP ) )
#	ifdef USE_SDL_FRAMEWORK
#		include <SDL/SDL.h>
#	else
#		include <SDL.h>
#	endif

#	ifdef USE_SDL_TTF_FRAMEWORK
#		include <SDL_ttf/SDL_ttf.h>
#	else
#		include <SDL_ttf.h>
#	endif
#else
#	include <SDL/SDL.h>
#	include <SDL/SDL_ttf.h>
#endif

void GL_ShutdownSDLFonts(void);	/* gl_draw.c */

#include "qgl.h"

#define	REF_VERSION	"GL 0.12"

/* up / down */
#define	PITCH	0

/* left / right */
#define	YAW		1

/* fall over */
#define	ROLL	2


#ifndef __VIDDEF_T
#define __VIDDEF_T
/**
 * @brief Contains the game screen size and drawing scale
 *
 * This is used to rationalize the GUI system rendering box
 * with the actual screen.
 * The width and height are the dimensions of the actual screen,
 * not just the rendering box.
 * The rx, ry positions are the width and height divided by
 * VID_NORM_WIDTH and VID_NORM_HEIGHT respectively.
 * This allows the GUI system to use a "normalized" coordinate system of
 * system of 1024x768 texels.
 *
*/
typedef struct {
	unsigned width;		/**< game screen/window width */
	unsigned height;	/**< game screen/window height */
	float rx;		/**< horizontal screen scale factor */
	float ry;		/**< vertical screen scale factor */
} viddef_t;
#endif

extern viddef_t vid;

#ifdef DEBUG
# ifdef _MSC_VER
#  define GL_CHECK_ERROR \
	do { \
		GLenum error = qglGetError(); \
		if (error != GL_NO_ERROR) \
			ri.Con_Printf(PRINT_ALL, "OpenGL err: %s (%d) 0x%X\n", \
					__FILE__, __LINE__, error); \
	} while(0);
# else
#  define GL_CHECK_ERROR \
	do { \
		GLenum error = qglGetError(); \
		if (error != GL_NO_ERROR) \
			ri.Con_Printf(PRINT_ALL, "OpenGL err: %s (%d): %s 0x%X\n", \
					__FILE__, __LINE__, \
					__PRETTY_FUNCTION__, error); \
	} while(0);
# endif
#else
# define GL_CHECK_ERROR
#endif

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
	it_wrappic
} imagetype_t;

typedef struct image_s {
	char name[MAX_QPATH];		/* game path, including extension, must be first */
	imagetype_t type;
	int width, height;			/* source image dimensions */
	int upload_width, upload_height;	/* dimensions after power of two and picmip */
	int registration_sequence;	/* 0 = free */
	struct msurface_s *texturechain;	/* for sort-by-texture world drawing */
	int texnum;					/* gl texture binding */
	float sl, tl, sh, th;		/* 0,0 - 1,1 unless part of the scrap */
	qboolean scrap;
	qboolean has_alpha;
	shader_t *shader;			/* pointer to shader from refdef_t */
	qboolean paletted;
} image_t;

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_SCRAPS		1280
#define	TEXNUM_IMAGES		1281

#define		MAX_GLERRORTEX	4096
#define		MAX_GLTEXTURES	1024

/* starting offset for font texture numbers (used in font-cache) */
#define	TEXNUM_FONTS		TEXNUM_IMAGES + MAX_GLTEXTURES

/*=================================================================== */

typedef struct {
	int width;
	int height;
	byte *captureBuffer;
	byte *encodeBuffer;
	qboolean motionJpeg;
} videoFrameCommand_t;

size_t SaveJPGToBuffer(byte * buffer, int quality, int image_width, int image_height, byte * image_buffer);
void RE_TakeVideoFrame(int width, int height, byte * captureBuffer, byte * encodeBuffer, qboolean motionJpeg);
const void *RB_TakeVideoFrameCmd(const void *data);

/*=================================================================== */

void WritePNG(FILE *f, byte *buffer, int width, int height);
void WriteJPG(FILE *f, byte *buffer, int width, int height, int quality);
void WriteTGA(FILE *f, byte *buffer, int width, int height);

/*=================================================================== */

typedef enum {
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "gl_model.h"

#define MAX_MODEL_DLIGHTS 3
int RecursiveLightPoint(mnode_t * node, vec3_t start, vec3_t end);

void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);

void GL_SetDefaultState(void);
void GL_UpdateSwapInterval(void);

extern float gldepthmin, gldepthmax;

typedef struct {
	float x, y, z;
	float s, t;
	float r, g, b;
} glvert_t;


#define	MAX_LBM_HEIGHT		1024	/* Was 480 (some standard?), but we used some higher textures now. */

#define BACKFACE_EPSILON	0.01

#define	MAX_MOD_KNOWN	512

/* entity transform */
typedef struct {
	qboolean done;
	qboolean processing;
	float matrix[16];
} transform_t;

extern transform_t trafo[MAX_ENTITIES];

/*==================================================== */

extern image_t *shadow;

extern image_t *r_notexture;
extern image_t *r_particletexture;
extern entity_t *currententity;
extern model_t *currentmodel;
extern int r_framecount;
extern cplane_t frustum[4];
extern int c_brush_polys, c_alias_polys;

extern int gl_filter_min, gl_filter_max;

extern image_t *DaN;

/* view origin */
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

/* screen size info */
extern refdef_t r_newrefdef;

extern cvar_t *r_drawworld;
extern cvar_t *r_fullbright;
extern cvar_t *r_nocull;
extern cvar_t *r_isometric;
extern cvar_t *r_lerpmodels;

extern cvar_t *r_anisotropic;
extern cvar_t *r_ext_max_anisotropy;
extern cvar_t *r_texture_lod;	/* lod_bias */
extern cvar_t *r_displayrefresh;

extern cvar_t *gl_vertex_arrays;

extern cvar_t *gl_screenshot;
extern cvar_t *gl_screenshot_jpeg_quality;

extern cvar_t *gl_ext_swapinterval;
extern cvar_t *gl_ext_multitexture;
extern cvar_t *gl_ext_combine;
extern cvar_t *gl_ext_pointparameters;
extern cvar_t *gl_ext_lockarrays;
extern cvar_t *gl_ext_compiled_vertex_array;
extern cvar_t *gl_ext_texture_compression;
extern cvar_t *gl_ext_s3tc_compression;

extern cvar_t *gl_particle_min_size;
extern cvar_t *gl_particle_max_size;
extern cvar_t *gl_particle_size;
extern cvar_t *gl_particle_att_a;
extern cvar_t *gl_particle_att_b;
extern cvar_t *gl_particle_att_c;

extern cvar_t *gl_bitdepth;
extern cvar_t *gl_mode;
extern cvar_t *gl_log;
extern cvar_t *gl_lightmap;

extern cvar_t *gl_showbox;

extern cvar_t *gl_3dmapradius;

/* shadow stuff */
void R_ShadowBlend(void);
void R_DrawShadowVolume(entity_t * e);
void R_DrawShadow(entity_t * e);
extern cvar_t *gl_shadows;
extern cvar_t *gl_shadow_debug_shade;
extern cvar_t *gl_shadow_debug_volume;
extern cvar_t *gl_ati_separate_stencil;
extern cvar_t *gl_stencil_two_side;

extern cvar_t *gl_dynamic;
extern cvar_t *gl_monolightmap;
extern cvar_t *gl_nobind;
extern cvar_t *gl_round_down;
extern cvar_t *gl_picmip;
extern cvar_t *gl_maxtexres;
extern cvar_t *gl_showtris;
extern cvar_t *gl_finish;
extern cvar_t *gl_ztrick;
extern cvar_t *gl_clear;
extern cvar_t *gl_cull;
extern cvar_t *gl_poly;
extern cvar_t *gl_texsort;
extern cvar_t *gl_polyblend;
extern cvar_t *gl_flashblend;
extern cvar_t *gl_lightmaptype;
extern cvar_t *gl_modulate;
extern cvar_t *gl_drawbuffer;
extern cvar_t *gl_3dlabs_broken;
extern cvar_t *gl_driver;
extern cvar_t *gl_swapinterval;
extern cvar_t *gl_texturemode;
extern cvar_t *gl_texturealphamode;
extern cvar_t *gl_texturesolidmode;
extern cvar_t *gl_saturatelighting;
extern cvar_t *gl_wire;
extern cvar_t *gl_fog;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;
extern cvar_t *vid_grabmouse;

extern cvar_t *intensity;

extern int gl_lightmap_format;
extern int gl_solid_format;
extern int gl_alpha_format;
extern int gl_compressed_solid_format;
extern int gl_compressed_alpha_format;

extern int c_visible_lightmaps;
extern int c_visible_textures;

extern float r_world_matrix[16];

void R_TranslatePlayerSkin(int playernum);
void GL_Bind(int texnum);
void GL_MBind(GLenum target, int texnum);
void GL_TexEnv(GLenum value);
/*void GL_UpdateAnisotropy(void);*/
void GL_EnableMultitexture(qboolean enable);
void GL_SelectTexture(GLenum);
void GL_CalcDayAndNight(float q);

void R_LightPoint(vec3_t p, vec3_t color);

/*==================================================================== */

void QGL_UnLink(void);
void QGL_Link(void);

/*==================================================================== */

extern model_t *rTiles[MAX_MAPTILES];
extern int rNumTiles;

extern unsigned d_8to24table[256];

extern int registration_sequence;

void V_AddBlend(float r, float g, float b, float a, float *v_blend);

struct model_s *R_RegisterModelShort(const char *name);

void GL_ScreenShot_f(void);
void R_InterpolateTransform(animState_t * as, int numframes, float *tag, float *interpolated);
void R_DrawModelDirect(modelInfo_t * mi, modelInfo_t * pmi, const char *tag);
void R_DrawModelParticle(modelInfo_t * mi);
void R_DrawAliasModel(entity_t * e);
void R_DrawBrushModel(entity_t * e);
void R_DrawAllBrushModels(void);
void R_DrawBeam(entity_t * e);
void R_DrawBox(entity_t * e);
void R_DrawLevelBrushes(void);
void R_DrawOBJModel (entity_t *e);
int Mod_LoadOBJModel (model_t* mod, void* buffer);
void R_RenderDlights(void);
void R_DrawAlphaSurfaces(void);
void R_InitParticleTexture(void);
void Draw_InitLocal(void);
void GL_SubdivideSurface(msurface_t * fa);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_RotateForEntity(entity_t * e);

void EmitWaterPolys(msurface_t * fa);
void R_DrawTriangleOutlines(void);
void R_MarkLights(dlight_t * light, int bit, mnode_t * node);
void R_EnableLights(qboolean fixed, float *matrix, float *lightparam, float *lightambient);

#include "gl_font.h"

void Draw_GetPicSize(int *w, int *h, const char *name);
void Draw_Pic(int x, int y, const char *name);
void Draw_NormPic(float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, const char *name);
void Draw_StretchPic(int x, int y, int w, int h, const char *name);
void Draw_Char(int x, int y, int c);
void Draw_TileClear(int x, int y, int w, int h, const char *name);
void Draw_Fill(int x, int y, int w, int h, int style, const vec4_t color);
void Draw_Color(const float *rgba);
void Draw_FadeScreen(void);
void Draw_DayAndNight(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map);
void Draw_Clouds(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map);
void Draw_LineStrip(int points, int *verts);
void Draw_LineLoop(int points, int *verts);
void Draw_Circle (vec3_t mid, float radius, const vec4_t color, int thickness);
void Draw_Polygon(int points, int *verts);

void R_SwapBuffers(int);

int Draw_GetPalette(void);

void R_BeginRegistration(char *tiles, char *pos);
void R_EndRegistration(void);
struct image_s *Draw_FindPic(const char *name);
void LoadTGA(const char *name, byte ** pic, int *width, int *height);

void Anim_Append(animState_t * as, model_t * mod, const char *name);
void Anim_Change(animState_t * as, model_t * mod, const char *name);
void Anim_Run(animState_t * as, model_t * mod, int msec);
char *Anim_GetName(animState_t * as, model_t * mod);

struct image_s *R_RegisterSkin(const char *name);

image_t *GL_LoadPic(const char *name, byte * pic, int width, int height, imagetype_t type, int bits);
image_t *GL_FindImage(const char *pname, imagetype_t type);
image_t *GL_FindImageForShader(const char *name);
void GL_TextureMode(const char *string);
void GL_ImageList_f(void);

void GL_InitImages(void);
void GL_ShutdownImages(void);

void GL_FreeUnusedImages(void);

void GL_TextureAlphaMode(const char *string);
void GL_TextureSolidMode(const char *string);

void R_DrawPtls(void);

/*
** GL config stuff
*/
#define GL_RENDERER_VOODOO		0x00000001
#define GL_RENDERER_VOODOO2   	0x00000002
#define GL_RENDERER_VOODOO_RUSH	0x00000004
#define GL_RENDERER_BANSHEE		0x00000008
#define		GL_RENDERER_3DFX		0x0000000F

#define GL_RENDERER_PCX1		0x00000010
#define GL_RENDERER_PCX2		0x00000020
#define GL_RENDERER_PMX			0x00000040
#define		GL_RENDERER_POWERVR		0x00000070

#define GL_RENDERER_PERMEDIA2	0x00000100
#define GL_RENDERER_GLINT_MX	0x00000200
#define GL_RENDERER_GLINT_TX	0x00000400
#define GL_RENDERER_3DLABS_MISC	0x00000800
#define		GL_RENDERER_3DLABS	0x00000F00

#define GL_RENDERER_REALIZM		0x00001000
#define GL_RENDERER_REALIZM2	0x00002000
#define		GL_RENDERER_INTERGRAPH	0x00003000

#define GL_RENDERER_3DPRO		0x00004000
#define GL_RENDERER_REAL3D		0x00008000
#define GL_RENDERER_RIVA128		0x00010000
#define GL_RENDERER_DYPIC		0x00020000

#define GL_RENDERER_V1000		0x00040000
#define GL_RENDERER_V2100		0x00080000
#define GL_RENDERER_V2200		0x00100000
#define		GL_RENDERER_RENDITION	0x001C0000

#define GL_RENDERER_O2          0x00100000
#define GL_RENDERER_IMPACT      0x00200000
#define GL_RENDERER_RE			0x00400000
#define GL_RENDERER_IR			0x00800000
#define		GL_RENDERER_SGI			0x00F00000

#define GL_RENDERER_MCD			0x01000000
#define GL_RENDERER_OTHER		0x80000000

typedef struct {
	int renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;
	int maxTextureSize;
} glconfig_t;

typedef struct {
	float inverse_intensity;
	int displayrefresh;
	qboolean fullscreen;

	int prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	int currenttextures[2];
	int currenttmu;

	qboolean hwgamma;

	float camera_separation;
	qboolean stereo_enabled;

	qboolean stencil_two_side;
	qboolean ati_separate_stencil;

	int max_lod;

	qboolean blend;
	qboolean alpha_test;
	qboolean fog_coord;

	qboolean stencil_wrap;
	qboolean anisotropic;
	qboolean lod_bias;
	qboolean arb_fragment_program;
	qboolean glsl_program;

	unsigned char originalRedGammaTable[256];
	unsigned char originalGreenGammaTable[256];
	unsigned char originalBlueGammaTable[256];
} glstate_t;

extern glconfig_t gl_config;
extern glstate_t gl_state;

#define GLSTATE_DISABLE_ALPHATEST	if (gl_state.alpha_test) { qglDisable(GL_ALPHA_TEST); gl_state.alpha_test=qfalse; }
#define GLSTATE_ENABLE_ALPHATEST	if (!gl_state.alpha_test) { qglEnable(GL_ALPHA_TEST); gl_state.alpha_test=qtrue; }

#define GLSTATE_DISABLE_BLEND		if (gl_state.blend) { qglDisable(GL_BLEND); gl_state.blend=qfalse; }
#define GLSTATE_ENABLE_BLEND		if (!gl_state.blend) { qglEnable(GL_BLEND); gl_state.blend=qtrue; }

/*
====================================================================
IMPORTED FUNCTIONS
====================================================================
*/

extern refimport_t ri;


/*
====================================================================
IMPLEMENTATION SPECIFIC FUNCTIONS
====================================================================
*/

void GLimp_BeginFrame(float camera_separation);
void GLimp_EndFrame(void);
qboolean GLimp_Init(HINSTANCE hinstance, WNDPROC wndproc);
void GLimp_Shutdown(void);
rserr_t GLimp_SetMode(unsigned int *pwidth, unsigned int *pheight, int mode, qboolean fullscreen);
void GLimp_AppActivate(qboolean active);
void GLimp_EnableLogging(qboolean enable);
void GLimp_LogNewFrame(void);
void GLimp_SetGamma(void);

/* 3d globe */

#define PHYSICAL_EARTH_RADIUS 6378137	/* Earth's radius, in meters. (From RFC1876) */
#define SPEED_OF_LIGHT 299792458		/* The big C, in meters/second. */
#define PHYSICAL_EARTH_CIRC (2.0 * M_PI * PHYSICAL_EARTH_RADIUS)
#define NOT_SELECTABLE ~0U				/* For picking reasons, see which_site() */

void Draw_3DGlobe(int x, int y, int w, int h, float p, float q, vec3_t rotate, float zoom, const char *map);
void Draw_3DMapLine(vec3_t angles, float zoom, int n, float dist, vec2_t * path);
void Draw_3DMapMarkers(vec3_t angles, float zoom, float latitude, float longitude, const char *model);

/* end of 3d globe */


#endif							/* GL_LOCAL_H */
