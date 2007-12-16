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

extern rstate_t r_state;

#define RSTATE_DISABLE_ALPHATEST   if (r_state.alpha_test) { qglDisable(GL_ALPHA_TEST); r_state.alpha_test=qfalse; }
#define RSTATE_ENABLE_ALPHATEST    if (!r_state.alpha_test) { qglEnable(GL_ALPHA_TEST); r_state.alpha_test=qtrue; }

#define RSTATE_DISABLE_BLEND       if (r_state.blend) { qglDisable(GL_BLEND); r_state.blend=qfalse; }
#define RSTATE_ENABLE_BLEND        if (!r_state.blend) { qglEnable(GL_BLEND); r_state.blend=qtrue; }

void R_SetDefaultState(void);
void R_SetupGL2D(void);
void R_SetupGL3D(void);
void R_EnableMultitexture(qboolean enable);
void R_SelectTexture(GLenum);
void R_Bind(int texnum);
void R_MBind(GLenum target, int texnum);
void R_TexEnv(GLenum value);
void R_TextureAlphaMode(const char *string);
void R_TextureSolidMode(const char *string);


#endif
