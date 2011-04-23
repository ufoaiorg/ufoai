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

typedef struct {
	int x;
	int y;
	int width;
	int height;
} r_viewport_t;

typedef struct {
	int width;
	int height;
	int samples;
	vec4_t clearColor;
	r_viewport_t viewport;
	int pixelFormat;
	int byteFormat;
	unsigned int depth;
	unsigned int fbo;
	int nTextures;
	unsigned int *textures;
	unsigned int proxyFBO;
} r_framebuffer_t;


void R_InitFBObjects(void);
void R_ShutdownFBObjects(void);

r_framebuffer_t* R_CreateFramebuffer(int width, int height, int ntextures, qboolean depth, qboolean halfFloat, unsigned int *filters);
void R_DeleteFBObject(r_framebuffer_t *buf);

void R_SetupViewport(r_framebuffer_t *buf, int x, int y, int width, int height);
void R_UseViewport(const r_framebuffer_t *buf);

void R_ResolveMSAA (const r_framebuffer_t *buf);
void R_UseFramebuffer(const r_framebuffer_t *buf);
void R_DrawBuffers(unsigned int n);
void R_BindColorAttachments(unsigned int n, unsigned int *attachments);
qboolean R_EnableRenderbuffer(qboolean enable);
qboolean R_RenderbufferEnabled(void);

#endif /* R_FRAMEBUFFER_H_ */
