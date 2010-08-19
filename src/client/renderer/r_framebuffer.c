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

static qboolean frameBufferObjectsInitialized = qfalse;
static int frameBufferObjectCount;
static r_framebuffer_t frameBufferObjects[MAX_FRAMEBUFFER_OBJECTS];
static GLuint frameBufferTextures[TEXNUM_FRAMEBUFFER_TEXTURES];
static const r_framebuffer_t *activeFramebuffer;

static r_framebuffer_t screenBuffer;
static GLenum *colorAttachments;

static qboolean renderbuffer_enabled; /*< renderbuffer vs screen as render target*/
static qboolean shadowbuffer_enabled; /*< shadowmap-buffer vs screen as render target*/

static GLuint R_GetFreeFBOTexture (void)
{
	int i;

	if (!frameBufferObjectsInitialized)
		Com_Error(ERR_FATAL, "Can't allocate framebuffer texture; framebuffers not initialized!\n");

	for (i = 0; i < TEXNUM_FRAMEBUFFER_TEXTURES; i++) {
		if (frameBufferTextures[i] == 0) {
			/* @note we should really use glGenTextures instead of just
			 * picking an arbitrary texnum to avoid conflicts elsewhere;
			 * glGenTextures gurantees us a globally unique texture ID */
			glGenTextures(1, &frameBufferTextures[i]);
			return frameBufferTextures[i];
		}
	}

	Com_Error(ERR_FATAL, "Exceeded max frame buffer textures");
}

static void R_FreeFBOTexture (int texnum)
{
	int i;

	if (!frameBufferObjectsInitialized)
		return;

	for (i = 0; i < TEXNUM_FRAMEBUFFER_TEXTURES; i++) {
		if (frameBufferTextures[i] == texnum)
			break;
	}

	assert(i >= 0);
	assert(i < TEXNUM_FRAMEBUFFER_TEXTURES);
	glDeleteTextures(1, (GLuint *) &frameBufferTextures[i]);
	frameBufferTextures[i] = 0;
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
#define R_CreateFramebuffer_RGBA(w, h, n, f) R_CreateFramebuffer(w, h, n, qtrue, qfalse, qfalse, qfalse, qfalse, qfalse, f)
#define R_CreateFramebuffer_RGBAD(w, h, n, f) R_CreateFramebuffer(w, h, n, qtrue, qtrue, qtrue, qfalse, qfalse, qfalse, f)
#define R_CreateFramebuffer_RGBA_float(w, h, n, f) R_CreateFramebuffer(w, h, n, qtrue, qfalse, qfalse, qtrue, qfalse, qfalse, f)
#define R_CreateFramebuffer_RGBAD_float(w, h, n, f) R_CreateFramebuffer(w, h, n, qtrue, qtrue, qtrue, qtrue, qfalse, qfalse, f)
#define R_CreateFramebuffer_RGBAD_cube_float(w, h, n, f) R_CreateFramebuffer(w, h, n, qtrue, qtrue, qfalse, qtrue, qfalse, qtrue, f)
static r_framebuffer_t * R_CreateFramebuffer (int width, int height, int ntextures, qboolean colorTexture, qboolean depth, qboolean depthTexture, qboolean halfFloat, qboolean mipmap, qboolean cubemap, GLenum *filters)
{
	r_framebuffer_t *buf;
	int i, j;

	if (!frameBufferObjectsInitialized) {
		Com_Printf("Warning: framebuffer creation failed; framebuffers not initialized!\n");
		return 0;
	}

	buf = &frameBufferObjects[frameBufferObjectCount++];

	if (ntextures > r_config.maxDrawBuffers)
		Com_Printf("Couldn't allocate requested number of drawBuffers in R_SetupFramebuffer!\n");

	Vector4Clear(buf->clearColor);

	buf->width = width;
	buf->height = height;
	R_SetupViewport(buf, 0, 0, width, height);

	buf->nTextures = ntextures;
	if (ntextures > 0) {
		buf->textures = Mem_Alloc(sizeof(GLuint) * ntextures);

		buf->pixelFormat = (halfFloat == qtrue) ? GL_RGBA32F_ARB : GL_RGBA8;
		//buf->byteFormat = (halfFloat == qtrue) ? GL_HALF_FLOAT_ARB : GL_UNSIGNED_BYTE;
		buf->byteFormat = (halfFloat == qtrue) ? GL_FLOAT : GL_UNSIGNED_BYTE;
		buf->type = (cubemap == qtrue) ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

		for (i = 0 ; i < buf->nTextures; i++) {
			if (!colorTexture) {
				qglGenRenderbuffersEXT(1, &buf->textures[i]);
				qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, buf->textures[i]);
				qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, buf->pixelFormat, buf->width, buf->height);
			} else {
				vec4_t borderColor = {0.0, 1.0, 0.0, 1.0};
				buf->textures[i] = R_GetFreeFBOTexture();
				glBindTexture(buf->type, buf->textures[i]);

				glTexParameteri(buf->type, GL_TEXTURE_MIN_FILTER, filters[i]);
				glTexParameteri(buf->type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(buf->type, GL_TEXTURE_WRAP_S, GL_CLAMP);
				glTexParameteri(buf->type, GL_TEXTURE_WRAP_T, GL_CLAMP);
				glTexParameterfv(buf->type, GL_TEXTURE_BORDER_COLOR, borderColor);

				if (mipmap) {
					glTexParameteri(buf->type, GL_GENERATE_MIPMAP, GL_TRUE);
				} else {
					glTexParameteri(buf->type, GL_GENERATE_MIPMAP, GL_FALSE);
					glTexParameteri(buf->type, GL_TEXTURE_MAX_LEVEL, 0);
					glTexParameteri(buf->type, GL_TEXTURE_MIN_LOD, 0);
					glTexParameteri(buf->type, GL_TEXTURE_MAX_LOD, 0);
				}

				if (cubemap) {
					glTexParameteri(buf->type, GL_TEXTURE_WRAP_R, GL_CLAMP);
					for (j = 0; j < 6; j++) {
						glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, 
									 buf->pixelFormat, buf->width, buf->height, 0, GL_RGBA, buf->byteFormat, 0);
					}
					
				} else {
					glTexImage2D(GL_TEXTURE_2D, 0, buf->pixelFormat, buf->width, buf->height, 0, GL_RGBA, buf->byteFormat, 0);
				}

			}

			R_CheckError();
		}
	} else { /* we might not want any textures (eg. for basic shadowmapping) */
		buf->textures = NULL;
		buf->pixelFormat = GL_NONE;
		buf->byteFormat = GL_NONE;
	}

	/* create depth renderbuffer or texture if requested */
	if (depth) {
		if (!depthTexture) {
			qglGenRenderbuffersEXT(1, &buf->depth);
			qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, buf->depth);
			qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, buf->width, buf->height);
		} else {
			buf->depth = R_GetFreeFBOTexture();
			glBindTexture(buf->type, buf->depth);

			/* we use GL_LINEAR for variance shadowmapping */
			glTexParameteri(buf->type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(buf->type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(buf->type, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameterf(buf->type, GL_TEXTURE_WRAP_S, GL_CLAMP );
			glTexParameterf(buf->type, GL_TEXTURE_WRAP_T, GL_CLAMP );
			if (cubemap) {
				glTexParameterf(buf->type, GL_TEXTURE_WRAP_R, GL_CLAMP );
				for (j = 0; j < 6; j++) {
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_DEPTH_COMPONENT24, 
							     buf->width, buf->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
				}

			} else {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 
						    buf->width, buf->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
			}
			glBindTexture(buf->type, 0);
		}
		R_CheckError();
	} else {
		buf->depth = 0;
	}

	/* create FBO itself */
	qglGenFramebuffersEXT(1, &buf->fbo);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, buf->fbo);

	/* bind textures to FBO */
	if (buf->nTextures > 0) {
		for (i = 0; i < buf->nTextures; i++) {
			if (colorTexture) {
				glBindTexture(GL_TEXTURE_2D, buf->textures[i]);
				qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, colorAttachments[i], GL_TEXTURE_2D, buf->textures[i], 0);
			} else {
				qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, colorAttachments[i], GL_RENDERBUFFER_EXT, buf->textures[i]);
			}
		}
	} else {
		qglDrawBuffer(GL_NONE);
		qglReadBuffer(GL_NONE);
	}

	/* bind depthbuffer to FBO */
	if (depth) {
		if (!depthTexture) {
			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buf->depth);
		} else { 
			glBindTexture(GL_TEXTURE_2D, buf->depth);
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, buf->depth, 0);
		}
	}

	R_CheckError();

	glBindTexture(GL_TEXTURE_2D, 0);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	return buf;
}


void R_InitFBObjects (void)
{
	unsigned int filters[2];
	float scales[DOWNSAMPLE_PASSES];
	int i;

	if (!r_framebuffers || !r_framebuffers->integer)
		return;

	frameBufferObjectCount = 0;
	memset(frameBufferObjects, 0, sizeof(frameBufferObjects));
	memset(frameBufferTextures, 0, sizeof(frameBufferTextures));

	frameBufferObjectsInitialized = qtrue;

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
	activeFramebuffer = &screenBuffer;

	colorAttachments = Mem_Alloc(sizeof(GLenum) * r_config.maxDrawBuffers);
	for (i = 0; i < r_config.maxDrawBuffers; i++)
		colorAttachments[i] = GL_COLOR_ATTACHMENT0_EXT + i;

	filters[0] = GL_NEAREST;
	//filters[1] = GL_LINEAR_MIPMAP_LINEAR;
	filters[1] = GL_NEAREST;

	/* setup main 3D render target */
	r_state.renderBuffer = R_CreateFramebuffer_RGBAD(viddef.width, viddef.height, 2, filters);

	/* setup buffer render targets */
	fbo_bloom0 = R_CreateFramebuffer_RGBA(viddef.width, viddef.height, 1, filters);
	fbo_bloom1 = R_CreateFramebuffer_RGBA(viddef.width, viddef.height, 1, filters);

	//filters[0] = GL_LINEAR;
	filters[0] = GL_NEAREST;
	/* setup extra framebuffers for postprocessing */
	for (i = 0; i < DOWNSAMPLE_PASSES; i++) {
		const int h = (int)((float)viddef.height / scales[i]);
		const int w = (int)((float)viddef.width / scales[i]);
		r_state.buffers0[i] = R_CreateFramebuffer_RGBA(w, h, 1, filters);
		r_state.buffers1[i] = R_CreateFramebuffer_RGBA(w, h, 1, filters);
		r_state.buffers2[i] = R_CreateFramebuffer_RGBA(w, h, 1, filters);

		R_CheckError();
	}

	/* setup buffers for shadowmapping */
	filters[0] = GL_LINEAR;
	r_state.shadowMapBuffer = R_CreateFramebuffer_RGBAD_float(r_maxlightmap->integer, r_maxlightmap->integer, 1, filters);
	r_state.shadowMapBlur1 = R_CreateFramebuffer_RGBAD_float(r_maxlightmap->integer, r_maxlightmap->integer, 1, filters);
	r_state.shadowCubeMapBuffer = R_CreateFramebuffer_RGBAD_cube_float(r_maxlightmap->integer, r_maxlightmap->integer, 1, filters);
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

	if (!frameBufferObjectsInitialized)
		return;

	for (i = 0; i < frameBufferObjectCount; i++)
		R_DeleteFBObject(&frameBufferObjects[i]);

	R_UseFramebuffer(&screenBuffer);

	frameBufferObjectCount = 0;
	memset(frameBufferObjects, 0, sizeof(frameBufferObjects));
	frameBufferObjectsInitialized = qfalse;

	Mem_Free(colorAttachments);
}


/**
 * @brief Delete any existing framebuffers, then re-initialize them using up-to-date information 
 */
void R_RestartFBObjects (void)
{
	if (!frameBufferObjectsInitialized)
		return;

	R_ShutdownFBObjects();
	R_InitFBObjects();
}



/**
 * @brief bind specified framebuffer object so we render to it
 * @param[in] buf the framebuffer to use, if @c NULL the screen buffer will be used.
 */
void R_UseFramebuffer (const r_framebuffer_t *buf)
{
	if (!r_framebuffers->integer)
		return;

	if (!frameBufferObjectsInitialized) {
		Com_Printf("Can't bind framebuffer: framebuffers not initialized\n");
		return;
	}

	if (!buf)
		buf = &screenBuffer;

	/* don't re-bind if we're already using the requested buffer */
	if (buf == activeFramebuffer)
		return;

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, buf->fbo);

	/* don't call glDrawBuffers for main screenbuffer */
	if (buf != &screenBuffer ) {
		if(buf->nTextures > 0) {
			qglDrawBuffers(buf->nTextures, colorAttachments);
		} else { /* we might only have a depth texture (eg. for shadowmapping) */
			qglDrawBuffer(GL_NONE);
			qglReadBuffer(GL_NONE);
		}
	}

	glClearColor(buf->clearColor[0], buf->clearColor[1], buf->clearColor[2], buf->clearColor[3]);

	activeFramebuffer = buf;

#ifdef PARANOID
	R_CheckError();
#endif
}

void R_ClearFramebuffer (void)
{
	glClear((!activeFramebuffer || activeFramebuffer->nTextures > 0 ? GL_COLOR_BUFFER_BIT : 0) | (!activeFramebuffer || activeFramebuffer->depth ? GL_DEPTH_BUFFER_BIT : 0));
	//glClear(activeFramebuffer->nTextures > 0 ? GL_COLOR_BUFFER_BIT : 0 | (activeFramebuffer->depth ? GL_DEPTH_BUFFER_BIT : 0));
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
	if (!frameBufferObjectsInitialized)
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
	if (!frameBufferObjectsInitialized)
		return;

	if (n >= r_config.maxDrawBuffers) {
		Com_DPrintf(DEBUG_RENDERER, "Max drawbuffers hit\n");
		n = r_config.maxDrawBuffers;
	}

	if (activeFramebuffer && activeFramebuffer->nTextures > 0) {
		n = (n > activeFramebuffer->nTextures) ? activeFramebuffer->nTextures : n;
		qglDrawBuffers(n, attachments);
	}
}

qboolean R_EnableRenderbuffer (qboolean enable)
{
	if (!frameBufferObjectsInitialized)
		return qfalse;

	if (enable != renderbuffer_enabled) {
		renderbuffer_enabled = enable;
		if (enable)
			R_UseFramebuffer(fbo_render);
		else {
			if (shadowbuffer_enabled)
				R_UseFramebuffer(r_state.shadowMapBuffer);
			else
				R_UseFramebuffer(fbo_screen);
		}
	}

	R_DrawBuffers(1);

	return qtrue;
}

qboolean R_EnableShadowbuffer (qboolean enable, const r_framebuffer_t *buf)
{
	if (!frameBufferObjectsInitialized || !r_shadowmapping->integer)
		return qfalse;

	if (enable != shadowbuffer_enabled || buf != activeFramebuffer) {
		if (enable && buf) {
			shadowbuffer_enabled = qtrue;
			R_UseFramebuffer(buf);
			R_DrawBuffers(1);
		} else {
			shadowbuffer_enabled = qfalse;
			if (renderbuffer_enabled)
				R_UseFramebuffer(fbo_render);
			else 
				R_UseFramebuffer(fbo_screen);
		}
	}


	return qtrue;
}

qboolean R_RenderbufferEnabled (void)
{
	return renderbuffer_enabled;
}

qboolean R_ShadowbufferEnabled (void)
{
	return shadowbuffer_enabled;
}

