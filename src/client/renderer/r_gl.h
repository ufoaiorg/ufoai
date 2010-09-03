/**
 * @file r_gl.h
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

#ifndef __R_GL_H__
#define __R_GL_H__

#include <SDL_opengl.h>

/* internally defined convenience constant */
#define GL_TANGENT_ARRAY -1
#define GL_NEXT_VERTEX_ARRAY -2
#define GL_NEXT_NORMAL_ARRAY -3
#define GL_NEXT_TANGENT_ARRAY -4

/* multitexture */
void (APIENTRY *qglActiveTexture)(GLenum texture);
void (APIENTRY *qglClientActiveTexture)(GLenum texture);

/* vertex buffer objects */
void (APIENTRY *qglGenBuffers)  (GLuint count, GLuint *id);
void (APIENTRY *qglDeleteBuffers)  (GLuint count, GLuint *id);
void (APIENTRY *qglBindBuffer)  (GLenum target, GLuint id);
void (APIENTRY *qglBufferData)  (GLenum target, GLsizei size, const GLvoid *data, GLenum usage);

/* vertex attribute arrays */
void (APIENTRY *qglEnableVertexAttribArray)(GLuint index);
void (APIENTRY *qglDisableVertexAttribArray)(GLuint index);
void (APIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

/* glsl vertex and fragment shaders and programs */
GLuint (APIENTRY *qglCreateShader)(GLenum type);
void (APIENTRY *qglDeleteShader)(GLuint id);
void (APIENTRY *qglShaderSource)(GLuint id, GLuint count, GLchar **sources, GLuint *len);
void (APIENTRY *qglCompileShader)(GLuint id);
void (APIENTRY *qglGetShaderiv)(GLuint id, GLenum field, GLuint *dest);
void (APIENTRY *qglGetShaderInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
GLuint (APIENTRY *qglCreateProgram)(void);
void (APIENTRY *qglDeleteProgram)(GLuint id);
void (APIENTRY *qglAttachShader)(GLuint prog, GLuint shader);
void (APIENTRY *qglDetachShader)(GLuint prog, GLuint shader);
void (APIENTRY *qglLinkProgram)(GLuint id);
void (APIENTRY *qglUseProgram)(GLuint id);
void (APIENTRY *qglGetProgramiv)(GLuint id, GLenum field, GLuint *dest);
void (APIENTRY *qglGetProgramInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
GLint (APIENTRY *qglGetUniformLocation)(GLuint id, const GLchar *name);
void (APIENTRY *qglUniform1i)(GLint location, GLint i);
void (APIENTRY *qglUniform1f)(GLint location, GLfloat f);
void (APIENTRY *qglUniform1fv)(GLint location, int count, GLfloat *f);
void (APIENTRY *qglUniform2fv)(GLint location, int count, GLfloat *f);
void (APIENTRY *qglUniform3fv)(GLint location, int count, GLfloat *f);
void (APIENTRY *qglUniform4fv)(GLint location, int count, GLfloat *f);
GLint(APIENTRY *qglGetAttribLocation)(GLuint id, const GLchar *name);

/* frame buffer objects (fbo) */
GLboolean (APIENTRY *qglIsRenderbufferEXT) (GLuint);
void (APIENTRY *qglBindRenderbufferEXT) (GLenum, GLuint);
void (APIENTRY *qglDeleteRenderbuffersEXT) (GLsizei, const GLuint *);
void (APIENTRY *qglGenRenderbuffersEXT) (GLsizei, GLuint *);
void (APIENTRY *qglRenderbufferStorageEXT) (GLenum, GLenum, GLsizei, GLsizei);
void (APIENTRY *qglGetRenderbufferParameterivEXT) (GLenum, GLenum, GLint *);
GLboolean (APIENTRY *qglIsFramebufferEXT) (GLuint);
void (APIENTRY *qglBindFramebufferEXT) (GLenum, GLuint);
void (APIENTRY *qglDeleteFramebuffersEXT) (GLsizei, const GLuint *);
void (APIENTRY *qglGenFramebuffersEXT) (GLsizei, GLuint *);
GLenum (APIENTRY *qglCheckFramebufferStatusEXT) (GLenum);
void (APIENTRY *qglFramebufferTexture1DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
void (APIENTRY *qglFramebufferTexture2DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
void (APIENTRY *qglFramebufferTexture3DEXT) (GLenum, GLenum, GLenum, GLuint, GLint, GLint);
void (APIENTRY *qglFramebufferRenderbufferEXT) (GLenum, GLenum, GLenum, GLuint);
void (APIENTRY *qglGetFramebufferAttachmentParameterivEXT) (GLenum, GLenum, GLenum, GLint *);
void (APIENTRY *qglGenerateMipmapEXT) (GLenum);
void (APIENTRY *qglDrawBuffers) (GLsizei, const GLenum *);



/* multitexture */
typedef void (*ActiveTexture_t)(GLenum texture);
typedef void (*ClientActiveTexture_t)(GLenum texture);

/* vertex buffer objects */
typedef void (*GenBuffers_t)  (GLuint count, GLuint *id);
typedef void (*DeleteBuffers_t)  (GLuint count, GLuint *id);
typedef void (*BindBuffer_t)  (GLenum target, GLuint id);
typedef void (*BufferData_t)  (GLenum target, GLsizei size, const GLvoid *data, GLenum usage);

/* vertex attribute arrays */
typedef void (*EnableVertexAttribArray_t)(GLuint index);
typedef void (*DisableVertexAttribArray_t)(GLuint index);
typedef void (*VertexAttribPointer_t)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

/* glsl vertex and fragment shaders and programs */
typedef GLuint (*CreateShader_t)(GLenum type);
typedef void (*DeleteShader_t)(GLuint id);
typedef void (*ShaderSource_t)(GLuint id, GLuint count, GLchar **sources, GLuint *len);
typedef void (*CompileShader_t)(GLuint id);
typedef void (*GetShaderiv_t)(GLuint id, GLenum field, GLuint *dest);
typedef void (*GetShaderInfoLog_t)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
typedef GLuint (*CreateProgram_t)(void);
typedef void (*DeleteProgram_t)(GLuint id);
typedef void (*AttachShader_t)(GLuint prog, GLuint shader);
typedef void (*DetachShader_t)(GLuint prog, GLuint shader);
typedef void (*LinkProgram_t)(GLuint id);
typedef void (*UseProgram_t)(GLuint id);
typedef void (*GetProgramiv_t)(GLuint id, GLenum field, GLuint *dest);
typedef void (*GetProgramInfoLog_t)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
typedef GLint (*GetUniformLocation_t)(GLuint id, const GLchar *name);
typedef void (*Uniform1i_t)(GLint location, GLint i);
typedef void (*Uniform1f_t)(GLint location, GLfloat f);
typedef void (*Uniform1fv_t)(GLint location, int count, GLfloat *f);
typedef void (*Uniform2fv_t)(GLint location, int count, GLfloat *f);
typedef void (*Uniform3fv_t)(GLint location, int count, GLfloat *f);
typedef void (*Uniform4fv_t)(GLint location, int count, GLfloat *f);
typedef GLint (*GetAttribLocation_t)(GLuint id, const GLchar *name);

/* frame buffer objects (fbo) */
typedef GLboolean (*IsRenderbufferEXT_t) (GLuint);
typedef void (*BindRenderbufferEXT_t) (GLenum, GLuint);
typedef void (*DeleteRenderbuffersEXT_t) (GLsizei, const GLuint *);
typedef void (*GenRenderbuffersEXT_t) (GLsizei, GLuint *);
typedef void (*RenderbufferStorageEXT_t) (GLenum, GLenum, GLsizei, GLsizei);
typedef void (*GetRenderbufferParameterivEXT_t) (GLenum, GLenum, GLint *);
typedef GLboolean (*IsFramebufferEXT_t) (GLuint);
typedef void (*BindFramebufferEXT_t) (GLenum, GLuint);
typedef void (*DeleteFramebuffersEXT_t) (GLsizei, const GLuint *);
typedef void (*GenFramebuffersEXT_t) (GLsizei, GLuint *);
typedef GLenum (*CheckFramebufferStatusEXT_t) (GLenum);
typedef void (*FramebufferTexture1DEXT_t) (GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (*FramebufferTexture2DEXT_t) (GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (*FramebufferTexture3DEXT_t) (GLenum, GLenum, GLenum, GLuint, GLint, GLint);
typedef void (*FramebufferRenderbufferEXT_t) (GLenum, GLenum, GLenum, GLuint);
typedef void (*GetFramebufferAttachmentParameterivEXT_t) (GLenum, GLenum, GLenum, GLint *);
typedef void (*GenerateMipmapEXT_t) (GLenum);
typedef void (*DrawBuffers_t) (GLsizei, const GLenum *);

#endif
