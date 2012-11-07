/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef __R_PROGRAM_H__
#define __R_PROGRAM_H__

#include "r_gl.h"

/* glsl vertex and fragment shaders */
typedef struct r_shader_s {
	GLenum type;
	GLuint id;
	char name[MAX_QPATH];
} r_shader_t;

#define GL_UNIFORM 1
#define GL_ATTRIBUTE 2
/* program variables */
typedef struct r_progvar_s {
	GLint type;
	char name[MAX_QPATH];
	GLint location;
} r_progvar_t;

/* NOTE: OpenGL spec says we must be able to have at
 * least 64 uniforms; may be more.  glGet(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS) */
#define MAX_PROGRAM_VARS 32

struct r_program_s;

typedef void (*programInitFunc_t)(struct r_program_s *prog);
typedef void (*programUseFunc_t)(struct r_program_s *prog);

/* and glsl programs */
typedef struct r_program_s {
	GLuint id;
	char name[MAX_VAR];
	r_shader_t *v;	/**< vertex shader */
	r_shader_t *f;	/**< fragment shader */
	r_progvar_t vars[MAX_PROGRAM_VARS];
	programInitFunc_t init;
	programUseFunc_t use;
	void *userdata;
} r_program_t;

#define MAX_PROGRAMS 16
#define MAX_SHADERS MAX_PROGRAMS * 2

void R_UseProgram(r_program_t *prog);
void R_ShutdownPrograms(void);
void R_InitPrograms(void);

void R_AttributePointer(const char *name, GLuint size, const GLvoid *array);
void R_EnableAttribute(const char *name);
void R_DisableAttribute(const char *name);

void R_ProgramParameter1f(const char *name, GLfloat value);
void R_ProgramParameter1fvs(const char *name, GLint size, GLfloat *value);
void R_ProgramParameter1i(const char *name, GLint value);
void R_ProgramParameter2fv(const char *name, GLfloat *value);
void R_ProgramParameter2fvs(const char *name, GLint size, GLfloat *value);
void R_ProgramParameter3fv(const char *name, GLfloat *value);
void R_ProgramParameter3fvs(const char *name, GLint size, GLfloat *value);
void R_ProgramParameter4fv(const char *name, GLfloat *value);
void R_ProgramParameter4fvs(const char *name, GLint size, GLfloat *value);
void R_ProgramParameterMatrix4fv(const char *name, GLfloat *value);

void R_InitParticleProgram(r_program_t *prog);
void R_UseParticleProgram(r_program_t *prog);

r_program_t *R_LoadProgram(const char *name, programInitFunc_t init, programUseFunc_t use);
void R_RestartPrograms_f(void);

#endif
