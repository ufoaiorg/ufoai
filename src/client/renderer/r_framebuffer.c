/**
 * @file r_framebuffer.c
 * @brief Framebuffer Objects support
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

#include "r_local.h"
#include "r_framebuffer.h"
#include "r_error.h"

#define MAX_FRAMEBUFFER_OBJECTS	    64

static qboolean r_framebuffer_objects_initialized;
static int FBObjectCount;
static r_framebuffer_t FBObjects[MAX_FRAMEBUFFER_OBJECTS];
static r_framebuffer_t *activeFramebuffer;

static r_framebuffer_t screenBuffer;
static GLenum *colorAttachments;

void R_InitFBObjects (void)
{
	GLint maxDrawBuffers;
	GLenum filters[2];
	float scales[DOWNSAMPLE_PASSES];
	int i;

	Com_Printf("Initializing framebuffer objects...\n");
	if (!r_config.frameBufferObject)
		return;

	FBObjectCount = 0;
	memset(FBObjects, 0, sizeof(FBObjects));

	r_framebuffer_objects_initialized = qtrue;

	for (i = 0; i < DOWNSAMPLE_PASSES; i++)
		scales[i] = powf(DOWNSAMPLE_SCALE, i + 1);

	/* setup default screen framebuffer */
	screenBuffer.fbo = 0;
	screenBuffer.depth = 0;
	screenBuffer.nTextures = 0;
	screenBuffer.width = viddef.width;
	screenBuffer.height = viddef.height;
	R_SetupViewport(&screenBuffer, 0, 0, viddef.width, viddef.height);
	R_SetClearColor(&screenBuffer, 0, 0, 0, 0);

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	activeFramebuffer = &screenBuffer;

	glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
	colorAttachments = malloc(sizeof(GLenum) * maxDrawBuffers);
	for (int i = 0; i < maxDrawBuffers; i++){
		colorAttachments[i] = GL_COLOR_ATTACHMENT0_EXT + i;
	}

	filters[0] = GL_NEAREST;
	filters[1] = GL_LINEAR_MIPMAP_LINEAR;

	/* setup main 3D render target */
	r_state.renderBuffer = R_CreateFramebuffer(viddef.width, viddef.height, 2, qtrue, qfalse, filters);

	/* setup bloom render targets */
	fbo_bloom0 = R_CreateFramebuffer(viddef.width, viddef.height, 1, qfalse, qfalse, filters);
	fbo_bloom1 = R_CreateFramebuffer(viddef.width, viddef.height, 1, qfalse, qfalse, filters);

	filters[0] = GL_LINEAR;
	/* setup extra framebuffers */
	for (int i = 0; i < DOWNSAMPLE_PASSES; i++) {
		int h = (int)((float)viddef.height / scales[i]);
		int w = (int)((float)viddef.width / scales[i]);
		r_state.buffers0[i] = R_CreateFramebuffer(w, h, 1, qfalse, qfalse, filters);
		r_state.buffers1[i] = R_CreateFramebuffer(w, h, 1, qfalse, qfalse, filters);

		R_CheckError();
	}
}


/**
 * @brief Delete framebuffer object along with attached render buffer
 */
void R_DeleteFBObject (r_framebuffer_t *buf)
{
	if (buf->depth)
		qglDeleteRenderbuffersEXT(1, &buf->depth);
	buf->depth = 0;

	if (buf->textures)
		glDeleteTextures(buf->nTextures, buf->textures);
	buf->textures = 0;

	if (buf->fbo)
		qglDeleteFramebuffersEXT(1, &buf->fbo);
	buf->fbo = 0;
}


/**
 * @brief Delete all registered framebuffer and render buffer objects, clear memory
 */
void R_ShutdownFBObjects (void)
{
	Com_Printf("Shutting down framebuffer objects...\n");
	if (!r_framebuffer_objects_initialized)
		return;

	for (int i = 0; i < FBObjectCount; i++)
		R_DeleteFBObject(&FBObjects[i]);

	R_UseFramebuffer(&screenBuffer);

	FBObjectCount = 0;
	memset(FBObjects, 0, sizeof(FBObjects));
	r_framebuffer_objects_initialized = qfalse;
}


/**
 * @brief create a new framebuffer object
 */
r_framebuffer_t * R_CreateFramebuffer (int width, int height, int ntextures, qboolean depth, qboolean halfFloat, GLenum *filters)
{
	GLint maxDrawBuffers;
	r_framebuffer_t *buf;
	if (!r_framebuffer_objects_initialized) {
		Com_Printf("Warning: framebuffer creation failed; framebuffers not initialized!\n");
		return 0;
	}

	buf = &FBObjects[FBObjectCount++];

	glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
	if (ntextures > maxDrawBuffers)
		Com_Printf("Couldn't allocate requested number of drawBuffers in R_SetupFramebuffer!\n");

	for (int i = 0; i < 4; i++)
		buf->clearColor[i] = 0.0;

	buf->width = width;
	buf->height = height;
	buf->viewport.x = 0;
	buf->viewport.y = 0;
	buf->viewport.width = width;
	buf->viewport.height = height;

	buf->nTextures = ntextures;
	buf->textures = malloc(sizeof(GLuint) * (ntextures + 1));

	buf->pixelFormat = (halfFloat == qtrue) ? GL_RGBA16F_ARB : GL_RGBA8;
	buf->byteFormat = (halfFloat == qtrue) ? GL_HALF_FLOAT_ARB : GL_UNSIGNED_BYTE;

	glGenTextures(buf->nTextures, buf->textures);

	for (int i = 0 ; i < buf->nTextures; i++) {
		glBindTexture(GL_TEXTURE_2D, buf->textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, buf->pixelFormat, buf->width, buf->height, 0, GL_RGBA, buf->byteFormat, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filters[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		qglGenerateMipmapEXT(GL_TEXTURE_2D);
		if (r_config.anisotropic)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_config.maxAnisotropic);

		R_CheckError();
	}

	/* create depth renderbuffer */
	if (depth == qtrue) {
		qglGenRenderbuffersEXT(1, &buf->depth);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, buf->depth);
		qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, buf->width, buf->height);
		R_CheckError();
	} else {
		buf->depth = 0;
	}

	R_CheckError();

	/* create FBO itself */
	qglGenFramebuffersEXT(1, &buf->fbo);
	R_CheckError();
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, buf->fbo);

	for (int i = 0; i < buf->nTextures; i++) {
		glBindTexture(GL_TEXTURE_2D, buf->textures[i]);
		qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, colorAttachments[i], GL_TEXTURE_2D, buf->textures[i], 0);
		R_CheckError();
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	if (depth)
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buf->depth);

	R_CheckError();

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	return buf;
}

/**
 * @brief bind specified framebuffer object so we render to it
 */
void R_UseFramebuffer (r_framebuffer_t *buf)
{
	if (!r_framebuffer_objects_initialized){
		Com_Printf("Can't bind framebuffer: framebuffers not initialized\n");
		return;
	}

	if (!buf)
		buf = &screenBuffer;

	/* don't re-bind if we're already using the requested buffer */
	if (buf == activeFramebuffer)
		return;

    qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, buf->fbo);

	if (buf->nTextures > 0) /* don't call glDrawBuffers for main screenbuffer */
		qglDrawBuffers(buf->nTextures, colorAttachments);

	glClearColor(activeFramebuffer->clearColor[0], activeFramebuffer->clearColor[1], activeFramebuffer->clearColor[2], activeFramebuffer->clearColor[3]);

	activeFramebuffer = buf;

	R_CheckError();
}

void R_SetupViewport (r_framebuffer_t *buf, int x, int y, int width, int height)
{
	if (!buf)
		buf = &screenBuffer;

	buf->viewport.x = x;
	buf->viewport.y = y;
	buf->viewport.width = width;
	buf->viewport.height = height;
}

void R_UseViewport (r_framebuffer_t *buf)
{
	if (!buf)
		buf = &screenBuffer;
	glViewport(buf->viewport.x, buf->viewport.y, buf->viewport.width, buf->viewport.height);
}

void R_SetClearColor (r_framebuffer_t *buf, float r, float g, float b, float a)
{
	buf->clearColor[0] = r;
	buf->clearColor[1] = g;
	buf->clearColor[2] = b;
	buf->clearColor[3] = a;
}

void R_ClearBuffer (void)
{
	glClear(GL_COLOR_BUFFER_BIT | ((activeFramebuffer->depth) ? GL_DEPTH_BUFFER_BIT : 0));
}


void R_DrawBuffers (int n)
{
	if (r_framebuffer_objects_initialized && activeFramebuffer && activeFramebuffer->nTextures > 0)
		qglDrawBuffers(n, colorAttachments);
}
