/**
 * @file r_framebuffer.h
 */

/*
 Copyright (C) 2008 Victor Luchits

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

#ifndef R_FRAMEBUFFER_H_
#define R_FRAMEBUFFER_H_

#include "r_viewport.h"
#include "r_state.h"

typedef struct r_framebuffer_s {
	GLsizei width;
	GLsizei height;
	GLfloat clearColor[4];
	r_viewport_t viewport;
	GLenum pixelFormat;
	GLenum byteFormat;
	GLenum type;
	GLuint depth;
	GLuint fbo;
	GLsizei nTextures;
	GLuint *textures;
} r_framebuffer_t;


void R_InitFBObjects(void);
void R_ShutdownFBObjects(void);
void R_RestartFBObjects(void);

void R_DeleteFBObject(r_framebuffer_t *buf);

void R_SetupViewport(r_framebuffer_t *buf, int x, int y, int width, int height);
void R_UseViewport(const r_framebuffer_t *buf);

void R_UseFramebuffer(const r_framebuffer_t *buf);
void R_DrawBuffers(int n);
void R_BindColorAttachments(int n, GLenum *Attachments);
void R_ClearFramebuffer(void);
qboolean R_EnableRenderbuffer(qboolean enable);
qboolean R_RenderbufferEnabled(void);
qboolean R_EnableShadowbuffer(qboolean enable, const r_framebuffer_t *buf);
qboolean R_ShadowbufferEnabled(void);

#endif /* R_FRAMEBUFFER_H_ */

