/**
 * @file
 * @brief
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "r_program.h"
#include "r_material.h"
#include "r_framebuffer.h"
#include "r_light.h"
#include "r_image.h"

/**
 * @brief Center position of skybox along z-axis. This is used to make sure we see only the inside of Skybox.
 * @sa R_DrawStarfield
 * @sa R_Setup2D
 */
#define SKYBOX_DEPTH -9999.0f

/* vertex arrays are used for many things */
#define GL_ARRAY_LENGTH_CHUNK 4096
extern const vec2_t default_texcoords[4];

/** @brief texunits maintain multitexture state */
typedef struct gltexunit_s {
	bool enabled;		/**< GL_TEXTURE_2D on / off */
	GLenum texture;		/**< e.g. GL_TEXTURE0 */
	GLint texnum;		/**< e.g 123 */
	GLenum texenv;		/**< e.g. GL_MODULATE */
	GLfloat* texcoord_array;
	/* Size of the array above - it's dynamically reallocated */
	int array_size;
} gltexunit_t;

#define MAX_GL_TEXUNITS		6

/* these are defined for convenience */
#define texunit_0	        r_state.texunits[0]
#define texunit_1	        r_state.texunits[1]
#define texunit_2	        r_state.texunits[2]
#define texunit_3	        r_state.texunits[3]
#define texunit_4	        r_state.texunits[4]
#define texunit_5	        r_state.texunits[5]

#define texunit_diffuse			texunit_0
#define texunit_lightmap		texunit_1
#define texunit_deluxemap		texunit_2
#define texunit_normalmap		texunit_3
#define texunit_glowmap			texunit_4
#define texunit_specularmap		texunit_5
#define texunit_roughnessmap	texunit_2

#define COMPONENTS_VERTEX_ARRAY3D	3
#define COMPONENTS_VERTEX_ARRAY2D	2
#define COMPONENTS_COLOR_ARRAY		4
#define COMPONENTS_INDEX_ARRAY		1
#define COMPONENTS_NORMAL_ARRAY		3
#define COMPONENTS_TANGENT_ARRAY	4
#define COMPONENTS_TEXCOORD_ARRAY	2

#define DOWNSAMPLE_PASSES	5
#define DOWNSAMPLE_SCALE	2

#define fbo_screen			nullptr
#define fbo_render			r_state.renderBuffer
#define fbo_bloom0			r_state.bloomBuffer0
#define fbo_bloom1			r_state.bloomBuffer1

#define default_program		nullptr

struct mAliasMesh_s;

typedef struct rstate_s {
	bool fullscreen;

	/* arrays */
	GLfloat* vertex_array_3d;
	GLshort* vertex_array_2d;
	GLfloat* color_array;
	GLint* index_array;
	GLfloat* normal_array;
	GLfloat* tangent_array;
	GLfloat* next_vertex_array_3d;
	GLfloat* next_normal_array;
	GLfloat* next_tangent_array;

	/* Size of all arrays above - it's dynamically reallocated */
	int array_size;

	/* multitexture texunits */
	gltexunit_t texunits[MAX_GL_TEXUNITS];

	/* texunit in use */
	gltexunit_t* active_texunit;

	/* framebuffer objects*/
	r_framebuffer_t* renderBuffer;
	r_framebuffer_t* bloomBuffer0;
	r_framebuffer_t* bloomBuffer1;
	r_framebuffer_t* buffers0[DOWNSAMPLE_PASSES];
	r_framebuffer_t* buffers1[DOWNSAMPLE_PASSES];
	r_framebuffer_t* buffers2[DOWNSAMPLE_PASSES];
	bool frameBufferObjectsInitialized;
	const r_framebuffer_t* activeFramebuffer;

	/* shaders */
	r_shader_t shaders[MAX_SHADERS];
	r_program_t programs[MAX_PROGRAMS];
	r_program_t* world_program;
	r_program_t* model_program;
	r_program_t* warp_program;
	r_program_t* geoscape_program;
	r_program_t* convolve_program;
	r_program_t* combine2_program;
	r_program_t* atmosphere_program;
	r_program_t* simple_glow_program;
	r_program_t* active_program;

	/* blend function */
	GLenum blend_src, blend_dest;

	const material_t* active_material;

	/* states */
	bool shell_enabled;
	bool blend_enabled;
	bool multisample_enabled;
	bool color_array_enabled;
	bool alpha_test_enabled;
	bool stencil_test_enabled;
	bool lighting_enabled;
	bool warp_enabled;
	bool fog_enabled;
	bool blur_enabled;
	bool glowmap_enabled;
	bool draw_glow_enabled;
	bool dynamic_lighting_enabled;
	bool specularmap_enabled;
	bool roughnessmap_enabled;
	bool animation_enabled;
	bool renderbuffer_enabled; /**< renderbuffer vs screen as render target*/

	const struct image_s* active_normalmap;
} rstate_t;

extern rstate_t r_state;

void R_SetDefaultState(void);
void R_Setup2D(void);
void R_Setup3D(void);
void R_ReallocateStateArrays(int size);
void R_ReallocateTexunitArray(gltexunit_t* texunit, int size);

void R_TexEnv(GLenum value);
void R_TexOverride (vec4_t rgba);
void R_BlendFunc(GLenum src, GLenum dest);

bool R_SelectTexture(gltexunit_t* texunit);

void R_BindTextureDebug(int texnum, const char* file, int line, const char* function);
#define R_BindTexture(tn) R_BindTextureDebug(tn, __FILE__, __LINE__, __PRETTY_FUNCTION__)
void R_BindTextureForTexUnit(GLuint texnum, gltexunit_t* texunit);
void R_BindLightmapTexture(GLuint texnum);
void R_BindDeluxemapTexture(GLuint texnum);
void R_BindNormalmapTexture(GLuint texnum);
void R_BindBuffer(GLenum target, GLenum type, GLuint id);
void R_BindArray(GLenum target, GLenum type, const void* array);
void R_BindDefaultArray(GLenum target);
void R_EnableStencilTest(bool enable);
void R_EnableTexture(gltexunit_t* texunit, bool enable);
void R_EnableMultisample(bool enable);
void R_EnableBlend(bool enable);
void R_EnableAlphaTest(bool enable);
void R_EnableColorArray(bool enable);
bool R_EnableLighting(r_program_t* program, bool enable);
void R_EnableBumpmap(const struct image_s* normalmap);
void R_EnableWarp(r_program_t* program, bool enable);
void R_EnableBlur(r_program_t* program, bool enable, r_framebuffer_t* source, r_framebuffer_t* dest, int dir);
void R_EnableShell(bool enable);
void R_EnableFog(bool enable);
void R_EnableDrawAsGlow(bool enable);
void R_EnableGlowMap(const struct image_s* image);
void R_EnableSpecularMap(const struct image_s* image, bool enable);
void R_EnableRoughnessMap(const struct image_s* image, bool enable);
void R_SetupSpotLight(int index, const light_t* light);
void R_DisableSpotLight(int index);
void R_EnableAnimation(const struct mAliasMesh_s* mesh, float backlerp, bool enable);

void R_UseMaterial (const material_t* material);
