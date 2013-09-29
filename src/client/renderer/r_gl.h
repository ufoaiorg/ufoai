/**
 * @file
 * @brief OpenGL bindings
 */

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

#pragma once

#ifdef ANDROID
#include "../../ports/android/r_gles_android.h"
#include <stdlib.h>
#else
#include <SDL_opengl.h>
#endif

inline void R_DrawArrays (GLint first, GLsizei count) {
#ifdef GL_VERSION_ES_CM_1_0
	const int cnt = count / 4;
	for (int i = 0; i < cnt; i++)
		glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
#else
	glDrawArrays(GL_QUADS, first, count);
#endif
}

#ifdef GL_VERSION_ES_CM_1_0
/* glDrawElements() cannot be invoked with GL_UNSIGNED_INT on GLES */
typedef GLushort glElementIndex_t;
#define GL_ELEMENT_INDEX_TYPE GL_UNSIGNED_SHORT

#define glOrtho												glOrthof
#define glFrustum											glFrustumf
#define glFogi												glFogx
#define glDepthRange										glDepthRangef
/* Stub, so that we won't clutter the code with #ifdefs */
#define glPolygonMode(x, y)
#else
typedef GLuint glElementIndex_t;
#define GL_ELEMENT_INDEX_TYPE GL_UNSIGNED_INT
#endif

/** @todo update SDL to version that includes these */
#ifndef GL_READ_FRAMEBUFFER_EXT
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#endif

#ifndef GL_DRAW_FRAMEBUFFER_EXT
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#endif

#ifndef QGL_EXTERN
#define QGL_EXTERN extern
#endif

/* internally defined convenience constant */
/* @todo sanitize those  */
#define GL_TANGENT_ARRAY -1
#define GL_NEXT_VERTEX_ARRAY -2
#define GL_NEXT_NORMAL_ARRAY -3
#define GL_NEXT_TANGENT_ARRAY -4

/* multitexture */
QGL_EXTERN void (APIENTRY* qglActiveTexture)(GLenum texture);
QGL_EXTERN void (APIENTRY* qglClientActiveTexture)(GLenum texture);

/* vertex buffer objects */
QGL_EXTERN void (APIENTRY* qglGenBuffers)  (GLuint count, GLuint* id);
QGL_EXTERN void (APIENTRY* qglDeleteBuffers)  (GLuint count, GLuint* id);
QGL_EXTERN void (APIENTRY* qglBindBuffer)  (GLenum target, GLuint id);
QGL_EXTERN void (APIENTRY* qglBufferData)  (GLenum target, GLsizei size, const GLvoid* data, GLenum usage);

/* vertex attribute arrays */
QGL_EXTERN void (APIENTRY* qglEnableVertexAttribArray)(GLuint index);
QGL_EXTERN void (APIENTRY* qglDisableVertexAttribArray)(GLuint index);
QGL_EXTERN void (APIENTRY* qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);

/* glsl vertex and fragment shaders and programs */
QGL_EXTERN GLuint (APIENTRY* qglCreateShader)(GLenum type);
QGL_EXTERN void (APIENTRY* qglDeleteShader)(GLuint id);
QGL_EXTERN void (APIENTRY* qglShaderSource)(GLuint id, GLuint count, GLchar** sources, GLuint* len);
QGL_EXTERN void (APIENTRY* qglCompileShader)(GLuint id);
QGL_EXTERN void (APIENTRY* qglGetShaderiv)(GLuint id, GLenum field, GLuint* dest);
QGL_EXTERN void (APIENTRY* qglGetShaderInfoLog)(GLuint id, GLuint maxlen, GLuint* len, GLchar* dest);
QGL_EXTERN GLuint (APIENTRY* qglCreateProgram)(void);
QGL_EXTERN void (APIENTRY* qglDeleteProgram)(GLuint id);
QGL_EXTERN void (APIENTRY* qglAttachShader)(GLuint prog, GLuint shader);
QGL_EXTERN void (APIENTRY* qglDetachShader)(GLuint prog, GLuint shader);
QGL_EXTERN void (APIENTRY* qglLinkProgram)(GLuint id);
QGL_EXTERN void (APIENTRY* qglUseProgram)(GLuint id);
QGL_EXTERN void (APIENTRY* qglGetProgramiv)(GLuint id, GLenum field, GLuint* dest);
QGL_EXTERN void (APIENTRY* qglGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
QGL_EXTERN void (APIENTRY* qglGetProgramInfoLog)(GLuint id, GLuint maxlen, GLuint* len, GLchar* dest);
QGL_EXTERN GLint (APIENTRY* qglGetUniformLocation)(GLuint id, const GLchar* name);
QGL_EXTERN void (APIENTRY* qglUniform1i)(GLint location, GLint i);
QGL_EXTERN void (APIENTRY* qglUniform1f)(GLint location, GLfloat f);
QGL_EXTERN void (APIENTRY* qglUniform1fv)(GLint location, int count, GLfloat* f);
QGL_EXTERN void (APIENTRY* qglUniform2fv)(GLint location, int count, GLfloat* f);
QGL_EXTERN void (APIENTRY* qglUniform3fv)(GLint location, int count, GLfloat* f);
QGL_EXTERN void (APIENTRY* qglUniform4fv)(GLint location, int count, GLfloat* f);
QGL_EXTERN GLint (APIENTRY* qglGetAttribLocation)(GLuint id, const GLchar* name);
QGL_EXTERN void (APIENTRY* qglUniformMatrix4fv)(GLint location, int count, GLboolean transpose, GLfloat* v);

/* frame buffer objects (fbo) */
QGL_EXTERN GLboolean (APIENTRY* qglIsRenderbufferEXT) (GLuint);
QGL_EXTERN void (APIENTRY* qglBindRenderbufferEXT) (GLenum, GLuint);
QGL_EXTERN void (APIENTRY* qglDeleteRenderbuffersEXT) (GLsizei, const GLuint*);
QGL_EXTERN void (APIENTRY* qglGenRenderbuffersEXT) (GLsizei, GLuint*);
QGL_EXTERN void (APIENTRY* qglRenderbufferStorageEXT) (GLenum, GLenum, GLsizei, GLsizei);
QGL_EXTERN void (APIENTRY* qglRenderbufferStorageMultisampleEXT) (GLenum, GLsizei, GLenum, GLsizei, GLsizei);
QGL_EXTERN void (APIENTRY* qglGetRenderbufferParameterivEXT) (GLenum, GLenum, GLint*);
QGL_EXTERN GLboolean (APIENTRY* qglIsFramebufferEXT) (GLuint);
QGL_EXTERN void (APIENTRY* qglBindFramebufferEXT) (GLenum, GLuint);
QGL_EXTERN void (APIENTRY* qglDeleteFramebuffersEXT) (GLsizei, const GLuint*);
QGL_EXTERN void (APIENTRY* qglGenFramebuffersEXT) (GLsizei, GLuint*);
QGL_EXTERN GLenum (APIENTRY* qglCheckFramebufferStatusEXT) (GLenum);
QGL_EXTERN void (APIENTRY* qglFramebufferTexture1DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
QGL_EXTERN void (APIENTRY* qglFramebufferTexture2DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
QGL_EXTERN void (APIENTRY* qglFramebufferTexture3DEXT) (GLenum, GLenum, GLenum, GLuint, GLint, GLint);
QGL_EXTERN void (APIENTRY* qglFramebufferRenderbufferEXT) (GLenum, GLenum, GLenum, GLuint);
QGL_EXTERN void (APIENTRY* qglGetFramebufferAttachmentParameterivEXT) (GLenum, GLenum, GLenum, GLint*);
QGL_EXTERN void (APIENTRY* qglGenerateMipmapEXT) (GLenum);
QGL_EXTERN void (APIENTRY* qglDrawBuffers) (GLsizei, const GLenum*);
QGL_EXTERN void (APIENTRY* qglBlitFramebuffer)( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

/* multitexture */
typedef void (APIENTRY* ActiveTexture_t)(GLenum texture);
typedef void (APIENTRY* ClientActiveTexture_t)(GLenum texture);

/* vertex buffer objects */
typedef void (APIENTRY* GenBuffers_t)  (GLuint count, GLuint* id);
typedef void (APIENTRY* DeleteBuffers_t)  (GLuint count, GLuint* id);
typedef void (APIENTRY* BindBuffer_t)  (GLenum target, GLuint id);
typedef void (APIENTRY* BufferData_t)  (GLenum target, GLsizei size, const GLvoid* data, GLenum usage);

/* vertex attribute arrays */
typedef void (APIENTRY* EnableVertexAttribArray_t)(GLuint index);
typedef void (APIENTRY* DisableVertexAttribArray_t)(GLuint index);
typedef void (APIENTRY* VertexAttribPointer_t)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);

/* glsl vertex and fragment shaders and programs */
typedef GLuint (APIENTRY* CreateShader_t)(GLenum type);
typedef void (APIENTRY* DeleteShader_t)(GLuint id);
typedef void (APIENTRY* ShaderSource_t)(GLuint id, GLuint count, GLchar** sources, GLuint* len);
typedef void (APIENTRY* CompileShader_t)(GLuint id);
typedef void (APIENTRY* GetShaderiv_t)(GLuint id, GLenum field, GLuint* dest);
typedef void (APIENTRY* GetShaderInfoLog_t)(GLuint id, GLuint maxlen, GLuint* len, GLchar* dest);
typedef GLuint (APIENTRY* CreateProgram_t)(void);
typedef void (APIENTRY* DeleteProgram_t)(GLuint id);
typedef void (APIENTRY* AttachShader_t)(GLuint prog, GLuint shader);
typedef void (APIENTRY* DetachShader_t)(GLuint prog, GLuint shader);
typedef void (APIENTRY* LinkProgram_t)(GLuint id);
typedef void (APIENTRY* UseProgram_t)(GLuint id);
typedef void (APIENTRY* GetActiveUniforms_t)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
typedef void (APIENTRY* GetProgramiv_t)(GLuint id, GLenum field, GLuint* dest);
typedef void (APIENTRY* GetProgramInfoLog_t)(GLuint id, GLuint maxlen, GLuint* len, GLchar* dest);
typedef GLint (APIENTRY* GetUniformLocation_t)(GLuint id, const GLchar* name);
typedef void (APIENTRY* Uniform1i_t)(GLint location, GLint i);
typedef void (APIENTRY* Uniform1f_t)(GLint location, GLfloat f);
typedef void (APIENTRY* Uniform1fv_t)(GLint location, int count, GLfloat* f);
typedef void (APIENTRY* Uniform2fv_t)(GLint location, int count, GLfloat* f);
typedef void (APIENTRY* Uniform3fv_t)(GLint location, int count, GLfloat* f);
typedef void (APIENTRY* Uniform4fv_t)(GLint location, int count, GLfloat* f);
typedef GLint (APIENTRY* GetAttribLocation_t)(GLuint id, const GLchar* name);
typedef void (APIENTRY* UniformMatrix4fv_t)(GLint location, int count, GLboolean transpose, GLfloat* v);

/* frame buffer objects (fbo) */
typedef GLboolean (APIENTRY* IsRenderbufferEXT_t) (GLuint);
typedef void (APIENTRY* BindRenderbufferEXT_t) (GLenum, GLuint);
typedef void (APIENTRY* DeleteRenderbuffersEXT_t) (GLsizei, const GLuint*);
typedef void (APIENTRY* GenRenderbuffersEXT_t) (GLsizei, GLuint*);
typedef void (APIENTRY* RenderbufferStorageEXT_t) (GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY* RenderbufferStorageMultisampleEXT_t) (GLenum, GLsizei, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY* GetRenderbufferParameterivEXT_t) (GLenum, GLenum, GLint*);
typedef GLboolean (APIENTRY* IsFramebufferEXT_t) (GLuint);
typedef void (APIENTRY* BindFramebufferEXT_t) (GLenum, GLuint);
typedef void (APIENTRY* DeleteFramebuffersEXT_t) (GLsizei, const GLuint*);
typedef void (APIENTRY* GenFramebuffersEXT_t) (GLsizei, GLuint*);
typedef GLenum (APIENTRY* CheckFramebufferStatusEXT_t) (GLenum);
typedef void (APIENTRY* FramebufferTexture1DEXT_t) (GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY* FramebufferTexture2DEXT_t) (GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY* FramebufferTexture3DEXT_t) (GLenum, GLenum, GLenum, GLuint, GLint, GLint);
typedef void (APIENTRY* FramebufferRenderbufferEXT_t) (GLenum, GLenum, GLenum, GLuint);
typedef void (APIENTRY* GetFramebufferAttachmentParameterivEXT_t) (GLenum, GLenum, GLenum, GLint*);
typedef void (APIENTRY* GenerateMipmapEXT_t) (GLenum);
typedef void (APIENTRY* DrawBuffers_t) (GLsizei, const GLenum*);
typedef void (APIENTRY* BlitFramebuffer_t)( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
