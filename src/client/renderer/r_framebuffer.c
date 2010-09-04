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

static int frameBufferObjectCount;
static r_framebuffer_t frameBufferObjects[MAX_FRAMEBUFFER_OBJECTS];
static GLuint frameBufferTextures[TEXNUM_FRAMEBUFFER_TEXTURES];

static r_framebuffer_t screenBuffer;
static GLenum *colorAttachments;

static GLuint R_GetFreeFBOTexture (void)
{
	int i;

	for (i = 0; i < TEXNUM_FRAMEBUFFER_TEXTURES; i++) {
		if (frameBufferTextures[i] == 0) {
			frameBufferTextures[i] = 1;
			return TEXNUM_FRAMEBUFFER_TEXTURES + i;
		}
	}

	Com_Error(ERR_FATAL, "Exceeded max frame buffer textures");
}

static void R_FreeFBOTexture (int texnum)
{
	const int i = texnum - TEXNUM_FRAMEBUFFER_TEXTURES;
	assert(i >= 0);
	assert(i < TEXNUM_FRAMEBUFFER_TEXTURES);
	frameBufferTextures[i] = 0;
	glDeleteTextures(1, (GLuint *) &texnum);
}

void R_InitFBObjects (void)
{
	unsigned int filters[2];
	float scales[DOWNSAMPLE_PASSES];
	int i;

	if (!r_config.frameBufferObject || !r_programs->integer)
		return;

	frameBufferObjectCount = 0;
	memset(frameBufferObjects, 0, sizeof(frameBufferObjects));
	memset(frameBufferTextures, 0, sizeof(frameBufferTextures));

	r_state.frameBufferObjectsInitialized = qtrue;

	for (i = 0; i < DOWNSAMPLE_PASSES; i++)
		scales[i] = powf(DOWNSAMPLE_SCALE, i + 1);

	/* setup default screen framebuffer */
	screenBuffer.fbo = 0;
	screenBuffer.depth = 0;
	screenBuffer.nTextures = 0;
	screenBuffer.width = viddef.width;
	screenBuffer.height = viddef.height;
	R_SetupViewport(&screenBuffer, 0, 0, viddef.width, viddef.height);
	Vector4Clear(screenBuffer.clearColor);

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	r_state.activeFramebuffer = &screenBuffer;

	colorAttachments = (GLenum *)Mem_Alloc(sizeof(GLenum) * r_config.maxDrawBuffers);
	for (i = 0; i < r_config.maxDrawBuffers; i++)
		colorAttachments[i] = GL_COLOR_ATTACHMENT0_EXT + i;

	filters[0] = GL_NEAREST;
	filters[1] = GL_LINEAR_MIPMAP_LINEAR;

	/* setup main 3D render target */
	r_state.renderBuffer = R_CreateFramebuffer(viddef.width, viddef.height, 2, qtrue, qfalse, filters);

	/* setup bloom render targets */
	fbo_bloom0 = R_CreateFramebuffer(viddef.width, viddef.height, 1, qfalse, qfalse, filters);
	fbo_bloom1 = R_CreateFramebuffer(viddef.width, viddef.height, 1, qfalse, qfalse, filters);

	filters[0] = GL_LINEAR;
	/* setup extra framebuffers */
	for (i = 0; i < DOWNSAMPLE_PASSES; i++) {
		const int h = (int)((float)viddef.height / scales[i]);
		const int w = (int)((float)viddef.width / scales[i]);
		r_state.buffers0[i] = R_CreateFramebuffer(w, h, 1, qfalse, qfalse, filters);
		r_state.buffers1[i] = R_CreateFramebuffer(w, h, 1, qfalse, qfalse, filters);
		r_state.buffers2[i] = R_CreateFramebuffer(w, h, 1, qfalse, qfalse, filters);

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

	if (buf->textures) {
		int i;
		for (i = 0; i < buf->nTextures; i++)
			R_FreeFBOTexture(buf->textures[i]);
		Mem_Free(buf->textures);
	}
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
	int i;

	if (!r_state.frameBufferObjectsInitialized)
		return;

	for (i = 0; i < frameBufferObjectCount; i++)
		R_DeleteFBObject(&frameBufferObjects[i]);

	R_UseFramebuffer(&screenBuffer);

	frameBufferObjectCount = 0;
	memset(frameBufferObjects, 0, sizeof(frameBufferObjects));
	r_state.frameBufferObjectsInitialized = qfalse;

	Mem_Free(colorAttachments);
}


/**
 * @brief create a new framebuffer object
 * @param[in] width The width of the framebuffer
 * @param[in] height The height of the framebuffer
 * @param[in] ntextures The amount of textures for this framebuffer. See also the filters array.
 * @param[in] depth Also generate a depth buffer
 * @param[in] halfFloat Use half float pixel format
 * @param[in] filters Filters for the textures. Must have @c ntextures entries
 */
r_framebuffer_t * R_CreateFramebuffer (int width, int height, int ntextures, qboolean depth, qboolean halfFloat, unsigned int *filters)
{
	r_framebuffer_t *buf;
	int i;

	if (!r_state.frameBufferObjectsInitialized) {
		Com_Printf("Warning: framebuffer creation failed; framebuffers not initialized!\n");
		return NULL;
	}

	if (frameBufferObjectCount >= lengthof(frameBufferObjects)) {
		Com_Printf("Warning: framebuffer creation failed; already created too many framebuffers!\n");
		return NULL;
	}

	buf = &frameBufferObjects[frameBufferObjectCount++];

	if (ntextures > r_config.maxDrawBuffers) {
		Com_Printf("Couldn't allocate requested number of drawBuffers in R_SetupFramebuffer!\n");
		ntextures = r_config.maxDrawBuffers;
	}

	Vector4Clear(buf->clearColor);

	buf->width = width;
	buf->height = height;
	R_SetupViewport(buf, 0, 0, width, height);

	buf->nTextures = ntextures;
	buf->textures = (unsigned int *)Mem_Alloc(sizeof(unsigned int) * ntextures);

	buf->pixelFormat = (halfFloat == qtrue) ? GL_RGBA16F_ARB : GL_RGBA8;
	buf->byteFormat = (halfFloat == qtrue) ? GL_HALF_FLOAT_ARB : GL_UNSIGNED_BYTE;

	for (i = 0 ; i < buf->nTextures; i++) {
		buf->textures[i] = R_GetFreeFBOTexture();
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

	for (i = 0; i < buf->nTextures; i++) {
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
 * @param[in] buf the framebuffer to use, if @c NULL the screen buffer will be used.
 */
void R_UseFramebuffer (const r_framebuffer_t *buf)
{
	if (!r_config.frameBufferObject || !r_programs->integer || !r_postprocess->integer)
		return;

	if (!r_state.frameBufferObjectsInitialized) {
		Com_Printf("Can't bind framebuffer: framebuffers not initialized\n");
		return;
	}

	if (!buf)
		buf = &screenBuffer;

	/* don't re-bind if we're already using the requested buffer */
	if (buf == r_state.activeFramebuffer)
		return;

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, buf->fbo);

	/* don't call glDrawBuffers for main screenbuffer */
	/** @todo buf != &screenBuffer */
	if (buf->nTextures > 0)
		qglDrawBuffers(buf->nTextures, colorAttachments);

	glClearColor(r_state.activeFramebuffer->clearColor[0], r_state.activeFramebuffer->clearColor[1], r_state.activeFramebuffer->clearColor[2], r_state.activeFramebuffer->clearColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | (buf->depth ? GL_DEPTH_BUFFER_BIT : 0));

	r_state.activeFramebuffer = buf;

	R_CheckError();
}

/**
 * @brief Sets the framebuffer dimensions of the viewport
 * @param[out] buf The framebuffer to initialize the viewport for. If @c NULL the screen buffer will be taken.
 * @sa R_UseViewport
 */
void R_SetupViewport (r_framebuffer_t *buf, int x, int y, int width, int height)
{
	if (!buf)
		buf = &screenBuffer;

	buf->viewport.x = x;
	buf->viewport.y = y;
	buf->viewport.width = width;
	buf->viewport.height = height;
}

/**
 * @brief Set the viewport to the dimensions of the given framebuffer
 * @param[out] buf The framebuffer to set the viewport for. If @c NULL the screen buffer will be taken.
 * @sa R_SetupViewport
 */
void R_UseViewport (const r_framebuffer_t *buf)
{
	if (!r_state.frameBufferObjectsInitialized || !r_config.frameBufferObject || !r_postprocess->integer || !r_programs->integer)
		return;

	if (!buf)
		buf = &screenBuffer;
	glViewport(buf->viewport.x, buf->viewport.y, buf->viewport.width, buf->viewport.height);
}

/** @todo introduce enum or speaking constants for the buffer numbers that are drawn here and elsewhere */
void R_DrawBuffers (int n)
{
	R_BindColorAttachments(n, colorAttachments);
}

void R_BindColorAttachments (int n, unsigned int *attachments)
{
	if (!r_state.frameBufferObjectsInitialized || !r_config.frameBufferObject || !r_postprocess->integer || !r_programs->integer)
		return;

	if (n >= r_config.maxDrawBuffers) {
		Com_DPrintf(DEBUG_RENDERER, "Max drawbuffers hit\n");
		n = r_config.maxDrawBuffers;
	}

	if (r_state.activeFramebuffer && r_state.activeFramebuffer->nTextures > 0)
		qglDrawBuffers(n, attachments);
}

qboolean R_EnableRenderbuffer (qboolean enable)
{
	if (!r_state.frameBufferObjectsInitialized || !r_config.frameBufferObject || !r_postprocess->integer || !r_programs->integer)
		return qfalse;

	if (enable != r_state.renderbuffer_enabled) {
		r_state.renderbuffer_enabled = enable;
		if (enable)
			R_UseFramebuffer(fbo_render);
		else
			R_UseFramebuffer(fbo_screen);
	}

	R_DrawBuffers(1);

	return qtrue;
}

qboolean R_RenderbufferEnabled (void)
{
	return r_state.renderbuffer_enabled;
}
