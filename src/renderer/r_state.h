/**
 * @file r_state.h
 * @brief
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

#ifndef R_STATE_H
#define R_STATE_H

#define MAX_GL_ARRAY_LENGTH 0x40000

typedef struct gltexunit_s {
	GLenum texture;  /* e.g. GL_TEXTURE0_ARB */
	GLint texnum;  /* e.g 123 */
	GLenum texenv;  /* e.g. GL_MODULATE */
} gltexunit_t;

typedef struct {
	qboolean fullscreen;

	/* arrays */
	GLfloat vertex_array_3d[MAX_GL_ARRAY_LENGTH * 3];
	GLshort vertex_array_2d[MAX_GL_ARRAY_LENGTH * 2];
	GLfloat texcoord_array[MAX_GL_ARRAY_LENGTH * 2];
	GLfloat lmtexcoord_array[MAX_GL_ARRAY_LENGTH * 2];
	GLfloat color_array[MAX_GL_ARRAY_LENGTH * 4];
	GLfloat normal_array[MAX_GL_ARRAY_LENGTH * 3];

	/* multitexture texunits */
	gltexunit_t texture_texunit;
	gltexunit_t lightmap_texunit;
	gltexunit_t third_texunit;
	gltexunit_t fourth_texunit;

	/* texunit in use */
	gltexunit_t *active_texunit;

	vec4_t color;

	/* blend function */
	GLenum blend_src, blend_dest;

	int32_t maxAnisotropic;

	qboolean ortho;

	/* states */
	qboolean blend_enabled;
	qboolean alpha_test_enabled;
	qboolean multitexture_enabled;
	qboolean lighting_enabled;
	qboolean warp_enabled;

	qboolean hwgamma;
	qboolean anisotropic;
	qboolean lod_bias;
	qboolean arb_fragment_program;
	qboolean glsl_program;
	qboolean vertex_buffers;
} rstate_t;

extern rstate_t r_state;
extern const float default_texcoords[];

void R_SetDefaultState(void);
void R_SetupGL2D(void);
void R_SetupGL3D(void);
void R_EnableMultitexture(qboolean enable);
void R_SelectTexture(gltexunit_t *texunit);
void R_BindTexture(int texnum);
void R_BindLightmapTexture(GLuint texnum);
void R_BindBuffer(GLenum target, GLenum type, GLuint id);
void R_TexEnv(GLenum value);
void R_BlendFunc(GLenum src, GLenum dest);
void R_EnableBlend(qboolean enable);
void R_EnableAlphaTest(qboolean enable);
void R_EnableLighting(qboolean enable);
void R_EnableWarp(qboolean enable);
void R_DisableEffects(void);
void R_BindArray(GLenum target, GLenum type, void *array);
void R_BindDefaultArray(GLenum target);
void R_StatePrint(void);
#endif
