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

static int frameBufferObjectCount;
static r_framebuffer_t frameBufferObjects[MAX_GL_FRAMEBUFFERS];
static GLuint frameBufferTextures[MAX_GL_FRAMEBUFFERS];

static r_framebuffer_t screenBuffer;
static GLenum *colorAttachments;

static GLuint R_GetFreeFBOTexture (void)
{
	int i;

	for (i = 0; i < MAX_GL_FRAMEBUFFERS; i++) {
		if (frameBufferTextures[i] == 0) {
			glGenTextures(1, &frameBufferTextures[i]);
			return frameBufferTextures[i];
		}
	}

	Com_Error(ERR_FATAL, "Exceeded max frame buffer textures");
}

static void R_FreeFBOTexture (int texnum)
{
	int i;

	for (i = 0; i < MAX_GL_FRAMEBUFFERS; i++) {
		if (frameBufferTextures[i] == texnum)
			break;
	}

	assert(i >= 0);
	assert(i < MAX_GL_FRAMEBUFFERS);
	glDeleteTextures(1, &frameBufferTextures[i]);
	frameBufferTextures[i] = 0;
}

void R_InitFBObjects (void)
{
	unsigned int filters[2];
	float scales[DOWNSAMPLE_PASSES];
	int i;

	if (!r_config.frameBufferObject || !r_programs->integer)
		return;

	frameBufferObjectCount = 0;
	OBJZERO(frameBufferObjects);
	OBJZERO(frameBufferTextures);

	r_state.frameBufferObjectsInitialized = qtrue;

	for (i = 0; i < DOWNSAMPLE_PASSES; i++)
		scales[i] = powf(DOWNSAMPLE_SCALE, i + 1);

	/* setup default screen framebuffer */
	screenBuffer.fbo = 0;
	screenBuffer.depth = 0;
	screenBuffer.nTextures = 0;
	screenBuffer.width = viddef.context.width;
	screenBuffer.height = viddef.context.height;
	R_SetupViewport(&screenBuffer, 0, 0, viddef.context.width, viddef.context.height);
	Vector4Clear(screenBuffer.clearColor);

	/* use default framebuffer */
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	r_state.activeFramebuffer = &screenBuffer;

	colorAttachments = (GLenum *)Mem_Alloc(sizeof(GLenum) * r_config.maxDrawBuffers);
	for (i = 0; i < r_config.maxDrawBuffers; i++)
		colorAttachments[i] = GL_COLOR_ATTACHMENT0_EXT + i;

	filters[0] = GL_NEAREST;
	filters[1] = GL_LINEAR_MIPMAP_LINEAR;

	/* setup main 3D render target */
	r_state.renderBuffer = R_CreateFramebuffer(viddef.context.width, viddef.context.height, 2, qtrue, qfalse, filters);

	/* setup bloom render targets */
	fbo_bloom0 = R_CreateFramebuffer(viddef.context.width, viddef.context.height, 1, qfalse, qfalse, filters);
	fbo_bloom1 = R_CreateFramebuffer(viddef.context.width, viddef.context.height, 1, qfalse, qfalse, filters);

	filters[0] = GL_LINEAR;
	/* setup extra framebuffers */
	for (i = 0; i < DOWNSAMPLE_PASSES; i++) {
		const int h = (int)((float)viddef.context.height / scales[i]);
		const int w = (int)((float)viddef.context.width / scales[i]);
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
	OBJZERO(frameBufferObjects);
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
	OBJZERO(*buf);

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

#ifdef GL_VERSION_ES_CM_1_0
	buf->pixelFormat = GL_RGBA;
	buf->byteFormat = GL_NATIVE_TEXTURE_PIXELFORMAT_ALPHA; /* GL_UNSIGNED_SHORT_5_5_5_1 will give 1-bit alpha but more colorspace */
#else
	buf->pixelFormat = halfFloat ? GL_RGBA16F_ARB : GL_RGBA8;
	buf->byteFormat = halfFloat ? GL_HALF_FLOAT_ARB : GL_UNSIGNED_BYTE;
#endif

	/* Presence of depth buffer indicates render target that could use antialiasing*/
	if (depth) {
		/** @todo also check if we are running on older (SM2.0) hardware, which doesn't support antialiased MRT */
		if (qglRenderbufferStorageMultisampleEXT && qglBlitFramebuffer) {
			int samples = min(4, max(0, r_multisample->integer));
			if (samples>1)
				buf->samples = samples;
		}
	}

	for (i = 0; i < buf->nTextures; i++) {
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
	glBindTexture(GL_TEXTURE_2D, 0);

	/* create FBO itself */
	qglGenFramebuffersEXT(1, &buf->fbo);
	R_CheckError();
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, buf->fbo);

	/* create&attach depth renderbuffer */
	if (depth) {
		qglGenRenderbuffersEXT(1, &buf->depth);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, buf->depth);
		if (buf->samples)
			qglRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, buf->samples, GL_DEPTH_COMPONENT, buf->width, buf->height);
		else
			qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, buf->width, buf->height);
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buf->depth);
	} else {
		buf->depth = 0;
	}

	/* create multisample color buffers if needed */
	if (buf->samples) {
		/* generate color buffers */
		for (i = 0; i < buf->nTextures; i++) {
			unsigned colorbuffer;
			qglGenRenderbuffersEXT(1, &colorbuffer);
			qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, colorbuffer);
			qglRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, buf->samples, buf->pixelFormat, buf->width, buf->height);
			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, colorAttachments[i], GL_RENDERBUFFER_EXT, colorbuffer);
			R_CheckError();
		}
		/* proxy framebuffer object for resolving MSAA */
		qglGenFramebuffersEXT(1, &buf->proxyFBO);
		R_CheckError();
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, buf->proxyFBO);
	}

	/* Whether multisampling was enabled or not, current FBO should be populated with render-to-texture bindings */
	for (i = 0; i < buf->nTextures; i++) {
		qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, colorAttachments[i], GL_TEXTURE_2D, buf->textures[i], 0);
		R_CheckError();
	}

	R_CheckError();

	/* unbind the framebuffer and return to default state */
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	return buf;
}

/**
 * @brief Forces multisample antialiasing resolve on given framebuffer, if needed
 * @param[in] buf the framebuffer to use
 */
void R_ResolveMSAA (const r_framebuffer_t *buf)
{
	int i;

	if (!buf->samples)
		return;

	qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,buf->fbo);
	qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,buf->proxyFBO);
	for (i = 0; i < buf->nTextures; i++)	 {
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + i);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + i);
		qglBlitFramebuffer(0, 0, buf->width, buf-> height, 0, 0, buf->width, buf->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		R_CheckError();
	}
	R_CheckError();

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT,  screenBuffer.fbo);
	R_CheckError();
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

/**
 * @brief Activate draw buffer(s)
 * @param[in] drawBufferNum The number of buffers to activate
 * @todo introduce enum or speaking constants for the buffer numbers that are drawn here and elsewhere
 */
void R_DrawBuffers (unsigned int drawBufferNum)
{
	R_BindColorAttachments(drawBufferNum, colorAttachments);
}

/**
 * @brief Activate draw buffer(s)
 * @param n The number of buffers to activate
 * @param attachments The buffers we are rendering into
 * @note The order of the attachments define the gl_FragData order in the shaders
*/
void R_BindColorAttachments (unsigned int n, unsigned int *attachments)
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

/**
 * @brief Enable the render to the framebuffer
 * @param enable If @c true we are enabling the rendering to fbo_render, if @c false we are rendering
 * to fbo that represents the screen
 * @sa R_DrawBuffers
 * @return @c true if the fbo was bound, @c false if not supported or deactivated
 */
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
