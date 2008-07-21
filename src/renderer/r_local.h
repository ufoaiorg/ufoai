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

#include "../client/cl_renderer.h"
#include "../client/cl_video.h"

#include <SDL_ttf.h>

#include "qgl.h"

#include "r_state.h"
#include "r_material.h"
#include "r_image.h"
#include "r_model.h"

void R_DrawBlendSurfaces(const mBspSurfaces_t *list);
void R_DrawOpaqueSurfaces(const mBspSurfaces_t *surfs);
void R_DrawOpaqueWarpSurfaces(mBspSurfaces_t *surfs);
void R_DrawBlendWarpSurfaces(mBspSurfaces_t *surfs);
void R_DrawAlphaTestSurfaces(mBspSurfaces_t *surfs);
void R_DrawMaterialSurfaces(mBspSurfaces_t *surfs);

/* surface lists */
extern mBspSurfaces_t r_opaque_surfaces;
extern mBspSurfaces_t r_opaque_warp_surfaces;
extern mBspSurfaces_t r_blend_surfaces;
extern mBspSurfaces_t r_blend_warp_surfaces;
extern mBspSurfaces_t r_alpha_test_surfaces;
extern mBspSurfaces_t r_material_surfaces;

/* temporary space used to group surfaces by texture */
extern mBspSurfaces_t r_sorted_surfaces[MAX_GL_TEXTURES];
extern int numgltextures;

/*==================================================== */

/* view origin */
extern vec3_t r_vup;
extern vec3_t r_vpn;
extern vec3_t r_vright;

extern cvar_t *r_brightness;
extern cvar_t *r_contrast;
extern cvar_t *r_invert;
extern cvar_t *r_monochrome;

extern cvar_t *r_drawworld;
extern cvar_t *r_drawspecialbrushes;
extern cvar_t *r_drawentities;
extern cvar_t *r_nocull;
extern cvar_t *r_isometric;
extern cvar_t *r_anisotropic;
extern cvar_t *r_texture_lod;   /* lod_bias */
extern cvar_t *r_light;
extern cvar_t *r_materials;
extern cvar_t *r_screenshot;
extern cvar_t *r_screenshot_jpeg_quality;
extern cvar_t *r_lightmap;
extern cvar_t *r_ext_texture_compression;
extern cvar_t *r_ext_s3tc_compression;
extern cvar_t *r_intel_hack;
extern cvar_t *r_checkerror;
extern cvar_t *r_showbox;
extern cvar_t *r_shadows;
extern cvar_t *r_soften;
extern cvar_t *r_modulate;
extern cvar_t *r_drawbuffer;
extern cvar_t *r_driver;
extern cvar_t *r_swapinterval;
extern cvar_t *r_multisample;
extern cvar_t *r_texturemode;
extern cvar_t *r_texturealphamode;
extern cvar_t *r_texturesolidmode;
extern cvar_t *r_wire;
extern cvar_t *r_vertexbuffers;
extern cvar_t *r_maxlightmap;

extern int gl_solid_format;
extern int gl_alpha_format;
extern int gl_compressed_solid_format;
extern int gl_compressed_alpha_format;
extern int gl_filter_min;
extern int gl_filter_max;

extern float r_world_matrix[16];

/*==================================================================== */

void QR_UnLink(void);
void QR_Link(void);

/*==================================================================== */

extern int registration_sequence;

void R_ScreenShot_f(void);
void R_DrawModelParticle(modelInfo_t *mi);
void R_DrawBrushModel(const entity_t *e);
void R_GetLevelSurfaceLists(void);
void R_SortSurfaces(void);
void R_InitMiscTexture(void);
void R_DrawEntities(void);
void R_DrawInitLocal(void);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_DrawParticles(void);

/** @brief GL config stuff */
typedef struct {
	const char *rendererString;
	const char *vendorString;
	const char *versionString;
	const char *extensionsString;
	int maxTextureSize;
	int maxTextureUnits;

	int videoMemory;
} rconfig_t;

extern rconfig_t r_config;

/*
====================================================================
IMPLEMENTATION SPECIFIC FUNCTIONS
====================================================================
*/

qboolean Rimp_Init(void);
void Rimp_Shutdown(void);
qboolean R_InitGraphics(void);

#endif /* R_LOCAL_H */
