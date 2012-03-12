/**
 * @file r_local.h
 * @brief local graphics definitions
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../../common/common.h"
#include "../../common/qfiles.h"
#include "../cl_renderer.h"
#include "../cl_video.h"

#include "r_gl.h"
#include "r_state.h"
#include "r_array.h"
#include "r_material.h"
#include "r_image.h"
#include "r_model.h"
#include "r_thread.h"
#include "r_framebuffer.h"
#include "r_lightmap.h"
#include "r_corona.h"
#include "r_flare.h"

#define MIN_GL_CONSTANT_ATTENUATION 0.01

void R_DrawSurfaces (const mBspSurfaces_t *surfs);
void R_DrawMaterialSurfaces(const mBspSurfaces_t *surfs);

void R_SetSurfaceBumpMappingParameters(const mBspSurface_t *surf, const image_t *normalMap, const image_t *specularMap);

/*==================================================== */

extern cvar_t *r_drawworld;
extern cvar_t *r_drawentities;
extern cvar_t *r_nocull;
extern cvar_t *r_isometric;
extern cvar_t *r_anisotropic;
extern cvar_t *r_texture_lod;   /* lod_bias */
extern cvar_t *r_materials;
extern cvar_t *r_default_specular;
extern cvar_t *r_default_hardness;
extern cvar_t *r_screenshot_format;
extern cvar_t *r_screenshot_jpeg_quality;
extern cvar_t *r_lightmap;
extern cvar_t *r_debug_normals;
extern cvar_t *r_debug_tangents;
extern cvar_t *r_debug_lights;
extern cvar_t *r_ext_texture_compression;
extern cvar_t *r_checkerror;
extern cvar_t *r_particles;
extern cvar_t *r_showbox;
extern cvar_t *r_shadows;
extern cvar_t *r_stencilshadows;
extern cvar_t *r_soften;
extern cvar_t *r_drawbuffer;
extern cvar_t *r_driver;
extern cvar_t *r_swapinterval;
extern cvar_t *r_multisample;
extern cvar_t *r_threads;
extern cvar_t *r_wire;
extern cvar_t *r_vertexbuffers;
extern cvar_t *r_maxlightmap;
extern cvar_t *r_warp;
extern cvar_t *r_programs;
extern cvar_t *r_glsl_version;
extern cvar_t *r_postprocess;
extern cvar_t *r_shownormals;
extern cvar_t *r_bumpmap;
extern cvar_t *r_specular;
extern cvar_t *r_hardness;
extern cvar_t *r_parallax;
extern cvar_t *r_fog;
extern cvar_t *r_flares;
extern cvar_t *r_coronas;
extern cvar_t *r_dynamic_lights;
extern cvar_t *r_drawtags;

/* private renderer variables */
typedef struct rlocals_s {
	/* view origin angle vectors */
	vec3_t up;
	vec3_t forward;
	vec3_t right;

	/* for box culling */
	cBspPlane_t frustum[4];

	int frame;
	int tracenum;

	float world_matrix[16];

	/**
	 * @brief this is the currently handled bsp model
	 * @note Remember that we can have loaded more than one bsp at the same time
	 */
	const model_t* bufferMapTile;
} rlocals_t;

extern rlocals_t r_locals;

qboolean R_CullMeshModel(const entity_t *e);

void R_DrawModelParticle(modelInfo_t *mi);
void R_DrawBspNormals(int tile);
qboolean R_CullBspModel(const entity_t *e);
qboolean R_CullSphere(const vec3_t centre, const float radius, const unsigned int clipflags);
void R_GetLevelSurfaceLists(void);
void R_GetEntityLists(void);
void R_DrawInitLocal(void);
void R_DrawParticles(void);
void R_SetupFrustum(void);

void R_ClearBspRRefs(void);
void R_AddBspRRef(const mBspModel_t *model, const vec3_t origin, const vec3_t angles, const qboolean forceVisibility);
void R_RenderOpaqueBspRRefs(void);
void R_RenderOpaqueWarpBspRRefs(void);
void R_RenderAlphaTestBspRRefs(void);
void R_RenderMaterialBspRRefs(void);
void R_RenderFlareBspRRefs(void);
void R_RenderBlendBspRRefs(void);
void R_RenderBlendWarpBspRRefs(void);


typedef enum {
	GLHW_GENERIC,
	GLHW_MESA,
	GLHW_INTEL,
	GLHW_ATI,
	GLHW_NVIDIA
} hardwareType_t;

/** @brief GL config stuff */
typedef struct {
	const char *rendererString;
	const char *vendorString;
	const char *versionString;
	const char *extensionsString;

	int glVersionMajor;
	int glVersionMinor;

	int glslVersionMajor;
	int glslVersionMinor;

	int maxTextureSize;
	int maxTextureUnits;
	int maxTextureCoords;
	int maxVertexAttribs;
	int maxVertexBufferSize;
	int maxLights;
	int maxDrawBuffers;
	int maxRenderbufferSize;
	int maxColorAttachments;
	int maxVertexTextureImageUnits;

	char lodDir[8];

	int videoMemory;

	qboolean hwgamma;

	int32_t maxAnisotropic;
	qboolean anisotropic;
	qboolean frameBufferObject;
	qboolean drawBuffers;

	int gl_solid_format;
	int gl_alpha_format;

	int gl_compressed_solid_format;
	int gl_compressed_alpha_format;

	int gl_filter_min;	/**< filter to use if the image is smaller than the original texture or stretched on the screen */
	int gl_filter_max;	/**< filter to use if the image is larger than the original texture or stretched on the screen */

	qboolean lod_bias;

	qboolean nonPowerOfTwo;	/**< support for non power of two textures */

	hardwareType_t hardwareType;
} rconfig_t;

extern rconfig_t r_config;

/*
====================================================================
IMPLEMENTATION SPECIFIC FUNCTIONS
====================================================================
*/

qboolean Rimp_Init(void);
void Rimp_Shutdown(void);
qboolean R_InitGraphics(const viddefContext_t *context);

#endif /* R_LOCAL_H */
