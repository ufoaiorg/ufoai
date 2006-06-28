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

#ifndef GL_LOCAL_H
#define GL_LOCAL_H

#ifdef _WIN32
#  include <windows.h>
#endif

#include <stdio.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <ctype.h>

#ifndef __linux__
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT GL_COLOR_INDEX
#endif
#endif

#ifndef _WIN32
#include <jpeglib.h>
#else
#ifdef DEVCPP
#include <jpeglib.h>
#else
#include "../win32/jpeglib.h"
#endif
#endif
#include "../client/ref.h"
/* this was taken from jmorecfg.h */
#define RGB_PIXELSIZE 3

#include "gl_arb_shader.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
void GL_ShutdownSDLFonts ( void ); /* gl_draw.c */

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

typedef struct
{
	unsigned		width, height;			/* coordinates from main game */
	float			rx, ry;
} viddef_t;
#endif

extern	viddef_t	vid;


/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_wrappic
} imagetype_t;

typedef struct image_s
{
	char	name[MAX_QPATH];			/* game path, including extension, must be first */
	imagetype_t	type;
	int		width, height;				/* source image */
	int		upload_width, upload_height;	/* after power of two and picmip */
	int		registration_sequence;		/* 0 = free */
	struct msurface_s	*texturechain;	/* for sort-by-texture world drawing */
	int		texnum;						/* gl texture binding */
	float	sl, tl, sh, th;				/* 0,0 - 1,1 unless part of the scrap */
	qboolean	scrap;
	qboolean	has_alpha;
	shader_t	*shader;	/* pointer to shader from refdef_t */

	qboolean paletted;
} image_t;

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_SCRAPS		1280
#define	TEXNUM_IMAGES		1281

#define		MAX_GLERRORTEX	4096
#define		MAX_GLTEXTURES	1024

/*=================================================================== */

typedef struct {
	int	width;
	int	height;
	byte	*captureBuffer;
	byte	*encodeBuffer;
	qboolean	motionJpeg;
} videoFrameCommand_t;

int SaveJPGToBuffer( byte *buffer, int quality, int image_width, int image_height, byte *image_buffer );
void RE_TakeVideoFrame( int width, int height, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg );
const void *RB_TakeVideoFrameCmd( const void *data );

/*=================================================================== */

typedef enum
{
	rserr_ok,


	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "gl_model.h"

#define MAX_MODEL_DLIGHTS 3
int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end);

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

void GL_SetDefaultState( void );
void GL_UpdateSwapInterval( void );

extern	float	gldepthmin, gldepthmax;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;


#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01

#define	MAX_MOD_KNOWN	512
extern model_t	mod_inline[MAX_MOD_KNOWN];
extern int		numInline;

/* entity transform */
typedef struct
{
	qboolean	done;
	qboolean	processing;
	float		matrix[16];
} transform_t;

extern transform_t	trafo[MAX_ENTITIES];

/*==================================================== */

extern	image_t		*shadow;

extern	char		glerrortex[MAX_GLERRORTEX];
extern	char		*glerrortexend;
extern	image_t		gltextures[MAX_GLTEXTURES];
extern	int			numgltextures;


extern	image_t		*r_notexture;
extern	image_t		*r_particletexture;
extern	entity_t	*currententity;
extern	model_t		*currentmodel;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;

extern	int			gl_filter_min, gl_filter_max;

extern	image_t		*DaN;

/* */
/* view origin */
/* */
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

/* */
/* screen size info */
/* */
extern	refdef_t	r_newrefdef;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_isometric;
extern	cvar_t	*r_lerpmodels;

extern	cvar_t	*r_lightlevel;	/* FIXME: This is a HACK to get the client's light level */

extern  cvar_t  *r_anisotropic;
extern  cvar_t  *r_ext_max_anisotropy;
extern  cvar_t  *r_texture_lod; /* lod_bias */
extern  cvar_t  *r_displayrefresh;

extern cvar_t	*gl_vertex_arrays;

extern cvar_t	*gl_ext_swapinterval;
extern cvar_t	*gl_ext_multitexture;
extern cvar_t	*gl_ext_combine;
extern cvar_t	*gl_ext_pointparameters;
extern cvar_t	*gl_ext_compiled_vertex_array;
extern cvar_t	*gl_ext_texture_compression;
extern cvar_t	*gl_ext_s3tc_compression;

extern cvar_t	*gl_particle_min_size;
extern cvar_t	*gl_particle_max_size;
extern cvar_t	*gl_particle_size;
extern cvar_t	*gl_particle_att_a;
extern cvar_t	*gl_particle_att_b;
extern cvar_t	*gl_particle_att_c;

extern	cvar_t	*gl_nosubimage;
extern	cvar_t	*gl_bitdepth;
extern	cvar_t	*gl_mode;
extern	cvar_t	*gl_log;
extern	cvar_t	*gl_lightmap;
/* shadow stuff */
void R_ShadowBlend( void );
void R_DrawShadowVolume (entity_t *e);
void GL_DrawAliasShadowVolume (dmdl_t *paliashdr, int posenumm);
void R_DrawShadow (entity_t *e);
extern	cvar_t	*gl_shadows;
extern	cvar_t	*gl_shadow_debug_shade;
extern	cvar_t	*gl_shadow_debug_volume;
extern  cvar_t	*r_ati_separate_stencil;
extern  cvar_t	*r_stencil_two_side;

extern	cvar_t	*gl_dynamic;
extern  cvar_t	*gl_monolightmap;
extern	cvar_t	*gl_nobind;
extern	cvar_t	*gl_round_down;
extern	cvar_t	*gl_picmip;
extern	cvar_t	*gl_maxtexres;
extern	cvar_t	*gl_showtris;
extern	cvar_t	*gl_finish;
extern	cvar_t	*gl_ztrick;
extern	cvar_t	*gl_clear;
extern	cvar_t	*gl_cull;
extern	cvar_t	*gl_poly;
extern	cvar_t	*gl_texsort;
extern	cvar_t	*gl_polyblend;
extern	cvar_t	*gl_flashblend;
extern	cvar_t	*gl_lightmaptype;
extern	cvar_t	*gl_modulate;
extern	cvar_t	*gl_playermip;
extern	cvar_t	*gl_drawbuffer;
extern	cvar_t	*gl_3dlabs_broken;
extern  cvar_t	*gl_driver;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_texturealphamode;
extern	cvar_t	*gl_texturesolidmode;
extern  cvar_t	*gl_saturatelighting;
extern  cvar_t	*gl_lockpvs;
extern	cvar_t	*gl_wire;
extern	cvar_t	*gl_fog;

extern	cvar_t	*vid_fullscreen;
extern	cvar_t	*vid_gamma;
extern	cvar_t	*vid_grabmouse;

extern	cvar_t	*intensity;

extern	int		gl_lightmap_format;
extern	int		gl_solid_format;
extern	int		gl_alpha_format;
extern	int		gl_compressed_solid_format;
extern	int		gl_compressed_alpha_format;

extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern	float	r_world_matrix[16];

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (int texnum);
void GL_MBind( GLenum target, int texnum );
void GL_TexEnv( GLenum value );
void GL_EnableMultitexture( qboolean enable );
void GL_SelectTexture( GLenum );

void R_LightPoint (vec3_t p, vec3_t color);

/*==================================================================== */

extern	model_t	*rTiles[MAX_MAPTILES];
extern	int		rNumTiles;

extern	unsigned	d_8to24table[256];

extern	int		registration_sequence;

void V_AddBlend (float r, float g, float b, float a, float *v_blend);

qboolean 	R_Init( HINSTANCE hinstance, WNDPROC wndproc );
void	R_Shutdown( void );

struct model_s *R_RegisterModelShort( char *name );

void R_RenderView (refdef_t *fd);
void GL_ScreenShot_f (void);
void R_InterpolateTransform( animState_t *as, int numframes, float *tag, float *interpolated );
void R_DrawModelDirect (modelInfo_t *mi, modelInfo_t *pmi, char *tag);
void R_DrawModelParticle (modelInfo_t *mi);
void R_DrawAliasModel (entity_t *e);
void R_DrawBrushModel (entity_t *e);
void R_DrawAllBrushModels (void);
void R_DrawSpriteModel (entity_t *e);
void R_DrawBeam( entity_t *e );
void R_DrawBox( entity_t *e );
void R_DrawWorld( mnode_t *nodes ) ;
void R_DrawLevelBrushes (void);
void R_RenderDlights (void);
void R_DrawAlphaSurfaces (void);
void R_RenderBrushPoly (msurface_t *fa);
void R_InitParticleTexture (void);
void Draw_InitLocal (void);
void GL_SubdivideSurface (msurface_t *fa);
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
void R_RotateForEntity (entity_t *e);

void EmitWaterPolys (msurface_t *fa);
void R_DrawTriangleOutlines (void);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);

void R_AddMapTile( char *name, int sX, int sY, int sZ );

void	R_SetGL2D (void);
void	R_LeaveGL2D (void);

#include "gl_font.h"

void	COM_StripExtension (char *in, char *out);

void	Draw_GetPicSize (int *w, int *h, char *name);
void	Draw_Pic (int x, int y, char *name);
void	Draw_NormPic (float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, char *name);
void	Draw_StretchPic (int x, int y, int w, int h, char *name);
void	Draw_Char (int x, int y, int c);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_Fill (int x, int y, int w, int h, int style, vec4_t color);
void	Draw_Color (float *rgba);
void	Draw_FadeScreen (void);
void	Draw_DayAndNight (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char* map );
void	Draw_LineStrip( int points, int *verts );

void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);
/*void	Draw_Model (int x, int y, ); */

void	R_BeginFrame( float camera_separation );
void	R_SwapBuffers( int );
void	R_SetPalette ( const unsigned char *palette);

int	Draw_GetPalette (void);

void	Anim_Append( animState_t *as, model_t *mod, char *name );
void	Anim_Change( animState_t *as, model_t *mod, char *name );
void	Anim_Run( animState_t *as, model_t *mod, int msec );
char	*Anim_GetName( animState_t *as, model_t *mod );

void	GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight);

struct image_s *R_RegisterSkin (char *name);

void	WriteTGA (char *filename, byte *data, int width, int height);

void	LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height);
image_t	*GL_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits);
image_t	*GL_FindImage (char *pname, imagetype_t type);
image_t *GL_FindImageForShader ( char *name );
void	GL_TextureMode( char *string );
void	GL_ImageList_f (void);
void	GL_CalcDayAndNight ( float q );

void	GL_InitImages (void);
void	GL_ShutdownImages (void);

void	GL_FreeUnusedImages (void);

void	GL_TextureAlphaMode( char *string );
void	GL_TextureSolidMode( char *string );

void	R_DrawPtls( void );

/*
** GL extension emulation functions
*/
void	GL_DrawParticles( int n, const particle_t particles[], const unsigned colortable[768] );

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

typedef struct
{
	int renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;
	int maxTextureSize;
} glconfig_t;

typedef struct
{
	float inverse_intensity;
	int displayrefresh;
	qboolean fullscreen;

	int     prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	int	currenttextures[2];
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

	qboolean stencil_warp;
	qboolean anisotropic;
	qboolean lod_bias;
	qboolean arb_fragment_program;

	unsigned char originalRedGammaTable[256];
	unsigned char originalGreenGammaTable[256];
	unsigned char originalBlueGammaTable[256];
} glstate_t;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

#define GLSTATE_DISABLE_ALPHATEST	if (gl_state.alpha_test) { qglDisable(GL_ALPHA_TEST); gl_state.alpha_test=qfalse; }
#define GLSTATE_ENABLE_ALPHATEST	if (!gl_state.alpha_test) { qglEnable(GL_ALPHA_TEST); gl_state.alpha_test=qtrue; }

#define GLSTATE_DISABLE_BLEND		if (gl_state.blend) { qglDisable(GL_BLEND); gl_state.blend=qfalse; }
#define GLSTATE_ENABLE_BLEND		if (!gl_state.blend) { qglEnable(GL_BLEND); gl_state.blend=qtrue; }

/*
====================================================================

IMPORTED FUNCTIONS

====================================================================
*/

extern	refimport_t	ri;


/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_BeginFrame( float camera_separation );
void		GLimp_EndFrame( void );
qboolean 	GLimp_Init( HINSTANCE hinstance, WNDPROC wndproc );
void		GLimp_Shutdown( void );
rserr_t     	GLimp_SetMode( unsigned int *pwidth, unsigned int *pheight, int mode, qboolean fullscreen );
void		GLimp_AppActivate( qboolean active );
void		GLimp_EnableLogging( qboolean enable );
void		GLimp_LogNewFrame( void );

/* NOTE TTimo linux works with float gamma value, not the gamma table */
/*   the params won't be used, getting the r_gamma cvar directly */
void		GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );


/*
====================================================================

3D Globe functions

Note: This code was taken from xtraceroute and adopted

====================================================================
*/

typedef struct globe_triangle_s
{
	vec3_t vec[3];
} globe_triangle_t;


/* for icosahedron */
#define CZ (0.866025403)        /* cos(30) */
#define SZ (0.5)                /* sin(30) */
#define C1 (0.951056516)        /* cos(18) */
#define S1 (0.309016994)        /* sin(18) */
#define C2 (0.587785252)        /* cos(54) */
#define S2 (0.809016994)        /* sin(54) */
#define X1 (C1*CZ)
#define Y1 (S1*CZ)
#define X2 (C2*CZ)
#define Y2 (S2*CZ)

#define Ip0     {0.,    1.,     0.}
#define Ip1     {-X2,   SZ,    -Y2}
#define Ip2     {X2,    SZ,    -Y2}
#define Ip3     {X1,    SZ,     Y1}
#define Ip4     {0,     SZ,     CZ}
#define Ip5     {-X1,   SZ,     Y1}

#define Im0     {-X1,   -SZ,    -Y1}
#define Im1     {0,     -SZ,    -CZ}
#define Im2     {X1,    -SZ,    -Y1}
#define Im3     {X2,    -SZ,     Y2}
#define Im4     {-X2,   -SZ,     Y2}
#define Im5     {0.,    -1.,     0.}

#define MAX_ICOSAHEDRON 20

#define GLOBE_TORAD (M_PI/180.0f)
#define GLOBE_TODEG (180.0f/M_PI)
#define MAX_SITES  40
#define Z_OF_EYE 4              /* s.g. distance between eye and object on z axis */
#define SITE_MARKER_SIZE  0.03
#define EARTH_SIZE 1.0
#define PHYSICAL_EARTH_RADIUS 6378137  /* Earth's radius, in meters. (From RFC1876) */
#define SPEED_OF_LIGHT 299792458       /* The big C, in meters/second. */
#define MAXSELECT 100
#define PHYSICAL_EARTH_CIRC (2.0 * M_PI * PHYSICAL_EARTH_RADIUS)
#define GEOSYNC_SAT_ALT 36000000 /* Approx. altitude of geosynchronous */
                                 /* satellites, in meters. */
#define NOT_SELECTABLE ~0U       /* For picking reasons, see which_site() */

void Draw_3DGlobe ( int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, char* map );
void Draw_3DMapLine ( int n, float dist, vec2_t *path );
void Draw_3DMapMarkers ( float latitude, float longitude, char* image );

/* end of 3d globe */


#endif /* GL_LOCAL_H */
