/**
 * @file r_main.c
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

#include "r_local.h"
#include "r_program.h"
#include "r_sphere.h"
#include "r_draw.h"
#include "r_font.h"
#include "r_light.h"
#include "r_lightmap.h"
#include "r_main.h"
#include "r_overlay.h"
#include "r_misc.h"
#include "r_error.h"
#include "../../common/tracing.h"
#include "../menu/m_windows.h"
#include "../battlescape/cl_localentity.h"

rendererData_t refdef;

rconfig_t r_config;
rstate_t r_state;
rlocals_t r_locals;

image_t *r_noTexture;			/* use for bad textures */
image_t *r_warpTexture;

static cvar_t *r_maxtexres;

cvar_t *r_brightness;
cvar_t *r_contrast;
cvar_t *r_invert;
cvar_t *r_monochrome;
cvar_t *r_saturation;
cvar_t *r_drawentities;
cvar_t *r_drawworld;
cvar_t *r_nocull;
cvar_t *r_isometric;
cvar_t *r_anisotropic;
cvar_t *r_texture_lod;			/* lod_bias */
cvar_t *r_screenshot_format;
cvar_t *r_screenshot_jpeg_quality;
cvar_t *r_ext_texture_compression;
static cvar_t *r_ext_s3tc_compression;
static cvar_t *r_ext_nonpoweroftwo;
static cvar_t *r_intel_hack;
cvar_t *r_materials;
cvar_t *r_checkerror;
cvar_t *r_drawbuffer;
cvar_t *r_driver;
cvar_t *r_shadows;
cvar_t *r_soften;
cvar_t *r_modulate;
cvar_t *r_swapinterval;
cvar_t *r_multisample;
cvar_t *r_texturemode;
cvar_t *r_texturealphamode;
cvar_t *r_texturesolidmode;
cvar_t *r_wire;
cvar_t *r_showbox;
cvar_t *r_threads;
cvar_t *r_vertexbuffers;
cvar_t *r_warp;
cvar_t *r_lights;
cvar_t *r_dynamic_lights;
cvar_t *r_programs;
cvar_t *r_framebuffers;
cvar_t *r_postprocess;
cvar_t *r_maxlightmap;
cvar_t *r_geoscape_overlay;
cvar_t *r_shownormals;
cvar_t *r_bumpmap;
cvar_t *r_specular;
cvar_t *r_hardness;
cvar_t *r_parallax;
cvar_t *r_fog;
cvar_t *r_flares;
cvar_t *r_coronas;
cvar_t *r_shadowmapping;
cvar_t *r_softshadows;
cvar_t *r_shadowmap_width;
cvar_t *r_map_maxlevel;
cvar_t *cl_worldlevel;

cvar_t *r_debug_normals;
cvar_t *r_debug_tangents;
cvar_t *r_debug_lightmaps;
cvar_t *r_debug_deluxemaps;
cvar_t *r_debug_normalmaps;
cvar_t *r_debug_shadows;

static void R_PrintInfo (const char *pre, const char *msg)
{
	char buf[4096];
	const size_t length = sizeof(buf);
	const size_t maxLength = strlen(msg);
	int i;

	Com_Printf("%s: ", pre);
	for (i = 0; i < maxLength; i += length) {
		Q_strncpyz(buf, msg + i, sizeof(buf));
		Com_Printf("%s", buf);
	}
	Com_Printf("\n");
}

/**
 * @brief Prints some OpenGL strings
 */
static void R_Strings_f (void)
{
	R_PrintInfo("GL_VENDOR", r_config.vendorString);
	R_PrintInfo("GL_RENDERER", r_config.rendererString);
	R_PrintInfo("GL_VERSION", r_config.versionString);
	R_PrintInfo("GL_EXTENSIONS", r_config.extensionsString);
}

void R_SetupFrustum (void)
{
	int i;

	/* build the transformation matrix for the given view angles */
	AngleVectors(refdef.viewAngles, r_locals.forward, r_locals.right, r_locals.up);

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (refdef.rendererFlags & RDF_NOWORLDMODEL) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(viddef.x, viddef.height - viddef.viewHeight - viddef.y, viddef.viewWidth, viddef.viewHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		R_CheckError();
		glDisable(GL_SCISSOR_TEST);
	}

	if (r_isometric->integer) {
		/* 4 planes of a cube */
		VectorScale(r_locals.right, +1, r_locals.frustum[0].normal);
		VectorScale(r_locals.right, -1, r_locals.frustum[1].normal);
		VectorScale(r_locals.up, +1, r_locals.frustum[2].normal);
		VectorScale(r_locals.up, -1, r_locals.frustum[3].normal);

		for (i = 0; i < 4; i++) {
			r_locals.frustum[i].type = PLANE_ANYZ;
			r_locals.frustum[i].dist = DotProduct(refdef.viewOrigin, r_locals.frustum[i].normal);
		}
		r_locals.frustum[0].dist -= 10 * refdef.fieldOfViewX;
		r_locals.frustum[1].dist -= 10 * refdef.fieldOfViewX;
		r_locals.frustum[2].dist -= 10 * refdef.fieldOfViewX * ((float) viddef.viewHeight / viddef.viewWidth);
		r_locals.frustum[3].dist -= 10 * refdef.fieldOfViewX * ((float) viddef.viewHeight / viddef.viewWidth);
	} else {
		/* rotate VPN right by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[0].normal, r_locals.up, r_locals.forward, -(90 - refdef.fieldOfViewX / 2));
		/* rotate VPN left by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[1].normal, r_locals.up, r_locals.forward, 90 - refdef.fieldOfViewX / 2);
		/* rotate VPN up by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[2].normal, r_locals.right, r_locals.forward, 90 - refdef.fieldOfViewY / 2);
		/* rotate VPN down by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[3].normal, r_locals.right, r_locals.forward, -(90 - refdef.fieldOfViewY / 2));

		for (i = 0; i < 4; i++) {
			r_locals.frustum[i].type = PLANE_ANYZ;
			r_locals.frustum[i].dist = DotProduct(refdef.viewOrigin, r_locals.frustum[i].normal);
		}
	}
}

/**
 * @brief Clears the screens color and depth buffer
 */
static inline void R_Clear (void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	R_CheckError();
	glDepthFunc(GL_LEQUAL);
	R_CheckError();

	glDepthRange(0.0f, 1.0f);
	R_CheckError();
}

/**
 * @sa CL_ClearState
 */
static inline void R_ClearScene (void)
{
	/* lights and coronas are populated as ents are added */
	refdef.numEntities = refdef.numLights = refdef.numCoronas = 0;
}

/**
 * @sa R_RenderFrame
 * @sa R_EndFrame
 */
void R_BeginFrame (void)
{
	/* change modes if necessary */
	if (vid_mode->modified || vid_fullscreen->modified || vid_strech->modified) {
		R_SetMode();
#if defined(_WIN32) || defined(__APPLE__)
		VID_Restart_f();
#endif
	}

	if (Com_IsRenderModified()) {
		Com_Printf("Modified render related cvars\n");
		if (Cvar_PendingCvars(CVAR_R_CONTEXT))
			VID_Restart_f();
		else if (Cvar_PendingCvars(CVAR_R_PROGRAMS))
			R_RestartPrograms_f();

		/* @todo - restart FBOs if required */
		//R_RestartFBObjects();

		Com_SetRenderModified(qfalse);
	}

	if (r_anisotropic->modified) {
		if (r_anisotropic->integer > r_config.maxAnisotropic) {
			Com_Printf("...max GL_EXT_texture_filter_anisotropic value is %i\n", r_config.maxAnisotropic);
			Cvar_SetValue("r_anisotropic", r_config.maxAnisotropic);
		}
		/*R_UpdateAnisotropy();*/
		r_anisotropic->modified = qfalse;
	}


	/* draw buffer stuff */
	if (r_drawbuffer->modified) {
		r_drawbuffer->modified = qfalse;

		if (Q_strcasecmp(r_drawbuffer->string, "GL_FRONT") == 0)
			glDrawBuffer(GL_FRONT);
		else
			glDrawBuffer(GL_BACK);
		R_CheckError();
	}

	/* texturemode stuff */
	/* Realtime set level of anisotropy filtering and change texture lod bias */
	if (r_texturemode->modified) {
		R_TextureMode(r_texturemode->string);
		r_texturemode->modified = qfalse;
	}

	if (r_texturealphamode->modified) {
		R_TextureAlphaMode(r_texturealphamode->string);
		r_texturealphamode->modified = qfalse;
	}

	if (r_texturesolidmode->modified) {
		R_TextureSolidMode(r_texturesolidmode->string);
		r_texturesolidmode->modified = qfalse;
	}


	if (r_framebuffers->modified || r_maxlightmap->modified) {
		R_RestartFBObjects();
		r_framebuffers->modified = qfalse;
		r_maxlightmap->modified = qfalse;
	}

	/* threads */
	if (r_threads->modified) {
		if (r_threads->integer)
			R_InitThreads();
		else
			R_ShutdownThreads();
		r_threads->modified = qfalse;
	}

	R_Setup2D();

	/* clear screen if desired */
	R_Clear();
}

/**
 * @sa R_BeginFrame
 * @sa R_EndFrame
 */
void R_RenderFrame (void)
{
	//int tile;
	entityListNode_t *allEntities;
	entityListNode_t *levelEntities;

	R_Setup3D();
	R_SetupCameraViewpoint();

#if 1
	if (!(refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		/* @todo - this only needs to be updated when models are added/removed (eg. due to visibility) */
		allEntities = R_ListAllEntities();
		R_SetupFrustum();
		levelEntities = R_ListLevelEntities();
		r_state.camera.ents = levelEntities;
		/* @todo - per-viewpoint lists; eg. objects that cast shadows, objects visible on the current level, etc. */
		/* @todo - per-viewpoint lists don't need to be updated every frame, but when things might have changed (eg. camera has moved more than some delta) */
		/* @todo - sort lists; first opaque things front to back, then transparent things back to front.  Again, doesn't need to happen every frame. */
		/* @todo - move all of this list generating/sorting/etc. into another thread with lower priority */
		/* @todo - use KD-tree to build/update lists */

		if (r_shadowmapping->integer) {
			int i, j;

			/* do a first pass rendering scene to depth buffer only */
			R_UseViewpoint(&r_state.camera);
			/* @todo - we don't need the full lighting program here, just
			 * the vertex LERPing; should make a separate shader for this */
			R_EnableLighting(r_state.lighting_program, qtrue);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glDepthMask(GL_TRUE);
			/* @todo - try to render opaque objects front to back, then transparent
			 * ones back to front.  If we disable writing to depth-mask for transparent
			 * objects, this will result in the best quality result */
			R_DrawEntityList(r_state.camera.ents);
			R_DrawBuffers(2);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			R_EnableLighting(NULL, qfalse);


			/* render lighting using up to r_dynamic_lights sources */
			for (i = 0, j = 0; i < r_dynamic_lights->integer; i++, j++) {
				/* skip lights that are disabled */
				for (; !r_state.dynamicLights[j].enabled && j < MAX_DYNAMIC_LIGHTS; j++);

				/* @todo - remove this when more lights are being properly set up */
				if (j > 1)
					continue;

				/* build shadowmap for this lightsource */
				R_SetupViewpoint(&r_state.dynamicLights[j]);
				/* @todo - don't use allEntities, use light-specific list.
				 * we only need to render things that generate shadows here; 
				 * this means no material stages, no "special" entities, etc. 
				 * as well as things that can't be illuminated by a given light 
				 * due to being too far away. */

				r_state.dynamicLights[j].viewpoint.ents = allEntities;
				R_EnableBuildShadowmap(qtrue, &r_state.dynamicLights[j]);
				R_ClearFramebuffer();

				/* @todo - for point sources, use shadowmap_cube_program */
				R_EnableLighting(r_state.build_shadowmap_flat_program, qtrue);
				R_DrawEntityList(r_state.dynamicLights[j].viewpoint.ents);
				R_EnableLighting(NULL, qfalse);

				R_EnableBuildShadowmap(qfalse, NULL);

				/* blur the shadowmap to get smooth, soft shadow edges */
				if (r_softshadows->integer) {
					R_Blur(r_state.shadowMapBuffer, r_state.shadowMapBlur1, 0, 0);
					R_Blur(r_state.shadowMapBlur1, r_state.shadowMapBuffer, 0, 1);
				}

				/* render scene using this lightsource */
				R_UseViewpoint(&r_state.camera);
				R_EnableUseShadowmap(qtrue);
				R_EnableBlend(qtrue);
				R_BlendFunc(GL_ONE, GL_ONE);
				R_EnableLighting(r_state.lighting_program, qtrue);
				R_EnableDynamicLights(&r_state.dynamicLights[j], qtrue);
				glDepthMask(GL_FALSE);
				R_DrawEntityList(r_state.camera.ents);
				glDepthMask(GL_TRUE);
				R_EnableDynamicLights(NULL, qfalse);
				R_EnableLighting(NULL, qfalse);
				R_EnableUseShadowmap(qfalse);
				R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				R_EnableBlend(qfalse);
			}

			/* render glow channel in a separate pass to avoid it getting applied multiple times */
			R_UseViewpoint(&r_state.camera);
			R_EnableBlend(qtrue);
			R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			/* @todo - again, we don't need a full lighting pass here, just LERPing and textures,
			 * so this should be done using a different shader program for efficiency */
			R_EnableLighting(r_state.lighting_program, qtrue);
			R_EnableDynamicLights(&r_state.dynamicLights[j], qtrue);

			R_ProgramParameter1i("DRAW_GLOW", 1);
			glDepthMask(GL_FALSE);
			R_DrawEntityList(r_state.camera.ents);
			glDepthMask(GL_TRUE);
			R_ProgramParameter1i("DRAW_GLOW", 0);

			R_EnableDynamicLights(NULL, qfalse);
			R_EnableLighting(NULL, qfalse);
			R_EnableBlend(qfalse);



		} else {
			/* if shadows are disabled, we do what we can using the fixed pipeline */

			R_UseViewpoint(&r_state.camera);

			R_EnableLighting(r_state.world_program, qtrue);
			R_EnableStaticLights(qtrue);
			R_DrawEntityList(r_state.camera.ents);
			R_EnableStaticLights(qfalse);
			R_EnableLighting(NULL, qfalse);

		}

		
		/* @todo - move this "free" somewhere else when making the list persist across frames */
		R_FreeEntityList(allEntities);
		R_FreeEntityList(levelEntities);
		R_CheckError();
	}
#else 

	/* build shadowmaps */
	if (r_shadowmapping->integer) {
	    /** @todo - make this thread-safe!  right now, if threads are
	     *  enabled, upper levels of the map (eg. roofs, tree tops)
	     *  flicker in and out of existance when looking at lower levels.
	     *  Turning off r_threads fixes this, but eventually this should
	     *  be properly handled with threads enabled. */
		int i, curLevel;

		for (i = 0; i < 3; i++) {
			r_locals.mins[i] = HUGE_VAL;
			r_locals.maxs[i] = -HUGE_VAL;
		}
		/* fix entity list so we draw all worldlevels for the shadowmap*/
		r_locals.framecheck = qfalse;
		refdef.numEntities = 0;
		LM_AddToScene();
		LE_AddToScene();
		R_GetAllSurfaceLists();

		R_SetupViewpoint(&r_state.dynamicLights[0]);
		R_EnableBuildShadowmap(qtrue, &r_state.dynamicLights[0]);

		for (tile = 0; tile < r_numMapTiles; tile++) {

			R_UpdateLightList(&r_state.world_entity);
			R_EnableDynamicLights(&r_state.world_entity, qtrue);
			R_DrawOpaqueSurfaces(r_mapTiles[tile]->bsp.opaque_surfaces);
		}

		R_DrawEntities();

		R_EnableBlend(qtrue);
		R_DrawParticles();
		R_EnableBlend(qfalse);

		R_EnableBuildShadowmap(qfalse, NULL);
		/* fix entity list so we don't draw all worldlevels in the actual scene */
		r_locals.framecheck = qtrue;
		refdef.numEntities = 0;
		LM_AddToScene();
		LE_AddToScene();


		/* blur the shadowmap to get smooth, soft shadow edges */
		if (r_softshadows->integer) {
			R_Blur(r_state.shadowMapBuffer, r_state.shadowMapBlur1, 0, 0);
			R_Blur(r_state.shadowMapBlur1, r_state.shadowMapBuffer, 0, 1);
		}

	}

	R_UseViewpoint(&r_state.camera);

	/* @todo - we shouldn't really need to call this a second time if we do things right */
	//R_Setup3D();
	R_CheckError();
	
	if ( r_shadowmapping->integer)
		R_EnableUseShadowmap(qtrue);
	//r_locals.framecheck = qtrue;

	/* render scene */

	/* activate wire mode */
	if (r_wire->integer)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (!(refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		if (r_threads->integer) {
			while (r_threadstate.state != THREAD_RENDERER)
				Sys_Sleep(0);

			r_threadstate.state = THREAD_CLIENT;
		} else {
			R_SetupFrustum();

			/* draw brushes on current worldlevel */
			R_GetLevelSurfaceLists();
		}

		R_EnableLights();

		R_CheckError();

		/* @todo: this is sort of a hack so we have something to be an "entity"
		 * for models which otherwise lack one (eg. map tiles); there may be
		 * a better way of handling this */
		R_UpdateLightList(&r_state.world_entity);

		for (tile = 0; tile < r_numMapTiles; tile++) {
			R_EnableFog(qtrue);

			R_EnableDynamicLights(&r_state.world_entity, qtrue);

			R_DrawOpaqueSurfaces(r_mapTiles[tile]->bsp.opaque_surfaces);

			R_DrawOpaqueWarpSurfaces(r_mapTiles[tile]->bsp.opaque_warp_surfaces);

			R_DrawAlphaTestSurfaces(r_mapTiles[tile]->bsp.alpha_test_surfaces);

			R_EnableBlend(qtrue);

			R_DrawMaterialSurfaces(r_mapTiles[tile]->bsp.material_surfaces);

			R_DrawFlareSurfaces(r_mapTiles[tile]->bsp.flare_surfaces);

			R_EnableFog(qfalse);

			R_DrawCoronas();

			R_DrawBlendSurfaces(r_mapTiles[tile]->bsp.blend_surfaces);
			R_DrawBlendWarpSurfaces(r_mapTiles[tile]->bsp.blend_warp_surfaces);

			R_EnableBlend(qfalse);

			R_DrawBspNormals(tile);
		}
	}
	R_CheckError();

	R_DrawEntities();

#endif
	

	R_EnableBlend(qtrue);

	R_DrawParticles();

	R_EnableBlend(qfalse);

	//R_EnableDynamicLights(NULL, qfalse);

	R_EnableUseShadowmap(qfalse);

	R_DrawBloom();

	/* leave wire mode again */
	if (r_wire->integer)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_ResetArrayState();


#if 0
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0, viddef.width, viddef.height, 0, -1000.0f, 10000.0f);

		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);

		R_UseProgram(default_program);
		R_UseFramebuffer(fbo_screen);

		glColor4f(1,1,1,1);
		glActiveTextureARB(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, r_state.renderBuffer->textures[1]);
		//glBindTexture(GL_TEXTURE_2D, r_state.renderBuffer->depth);
		//glBindTexture(GL_TEXTURE_2D, r_state.shadowMapBuffer->textures[0]);
		//glBindTexture(GL_TEXTURE_2D, r_state.shadowMapBuffer->depth);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

		glEnable(GL_TEXTURE_2D);
		//glTranslated(0,0,-1);

		R_UseViewport(fbo_render);

		glBegin(GL_QUADS);
		glTexCoord2i(0, 1);
		glVertex2i(0, 0);
		glTexCoord2i(1, 1);
		glVertex2i(fbo_render->width, 0);
		glTexCoord2i(1, 0);
		glVertex2i(fbo_render->width, fbo_render->height);
		glTexCoord2i(0, 0);
		glVertex2i(0, fbo_render->height);
		glEnd();

		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		R_CheckError();

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif



	/* go back into 2D mode for hud and the like */
	R_Setup2D();

	R_CheckError();
}

/**
 * @sa R_RenderFrame
 * @sa R_BeginFrame
 */
void R_EndFrame (void)
{
	if (vid_gamma->modified) {
		if (!vid_ignoregamma->integer) {
			const float g = vid_gamma->value;
			SDL_SetGamma(g, g, g);
		}
		vid_gamma->modified = qfalse;
	}

	R_ClearScene();

	SDL_GL_SwapBuffers();
}

static const cmdList_t r_commands[] = {
	{"r_listimages", R_ImageList_f, "Show all loaded images on game console"},
	{"r_listfontcache", R_FontListCache_f, "Show information about font cache"},
	{"r_screenshot", R_ScreenShot_f, "Take a screenshot"},
	{"r_listmodels", R_ModModellist_f, "Show all loaded models on game console"},
	{"r_strings", R_Strings_f, "Print openGL vendor and other strings"},
	{"r_restartprograms", R_RestartPrograms_f, "Reloads the shaders"},

	{NULL, NULL, NULL}
};

static qboolean R_CvarCheckMaxLightmap (cvar_t *cvar)
{
	if (r_config.maxTextureSize && cvar->integer > r_config.maxTextureSize) {
		Com_Printf("%s exceeded max supported texture size\n", cvar->name);
		Cvar_SetValue(cvar->name, r_config.maxTextureSize);
		return qfalse;
	}

	if (!Q_IsPowerOfTwo(cvar->integer)) {
		Com_Printf("%s must be power of two\n", cvar->name);
		Cvar_SetValue(cvar->name, LIGHTMAP_BLOCK_WIDTH);
		return qfalse;
	}

	return Cvar_AssertValue(cvar, 128, LIGHTMAP_BLOCK_WIDTH, qtrue);
}

static qboolean R_CvarCheckLights (cvar_t *cvar)
{
	if (!cvar->integer)
		Cvar_SetValue("r_dynamic_lights", 0);

	return Cvar_AssertValue(cvar, 0, 1, qtrue);
}

static qboolean R_CvarCheckDynamicLights (cvar_t *cvar)
{
	if (!r_lights->integer){
		Cvar_SetValue(cvar->name, 0);
		return qfalse;
	}
	return Cvar_AssertValue(cvar, 1, r_config.maxLights, qtrue);
}


static qboolean R_CvarCheckShadowmapping (cvar_t *cvar)
{
	if (!r_dynamic_lights->integer || !r_framebuffers->integer){
		Cvar_SetValue(cvar->name, 0);
		return qfalse;
	}
	return Cvar_AssertValue(cvar, 0, 1, qtrue);
}

static qboolean R_CvarCheckPrograms (cvar_t *cvar)
{
	if (qglUseProgram) {
		if (!cvar->integer || !r_config.drawBuffers) {
			Cvar_SetValue("r_framebuffers", 0);
			Cvar_SetValue("r_postprocess", 0);
			Cvar_SetValue("r_shadowmapping", 0);
		}
		return Cvar_AssertValue(cvar, 0, 1, qtrue);
	}

	Cvar_SetValue(cvar->name, 0);
	return qtrue;
}

static qboolean R_CvarCheckFramebuffers (cvar_t *cvar)
{
	if (r_config.frameBufferObject && r_programs->integer && r_config.drawBuffers)
		return Cvar_AssertValue(cvar, 0, 1, qtrue);

	Cvar_SetValue("r_postprocess", 0);
	Cvar_SetValue("r_shadowmapping", 0);
	Cvar_SetValue(cvar->name, 0);
	return qtrue;
}

static qboolean R_CvarCheckPostProcess (cvar_t *cvar)
{
	if (r_framebuffers->integer)
		return Cvar_AssertValue(cvar, 0, 1, qtrue);

	Cvar_SetValue(cvar->name, 0);
	return qtrue;
}

static void R_RegisterSystemVars (void)
{
	const cmdList_t *commands;

	r_driver = Cvar_Get("r_driver", "", CVAR_ARCHIVE | CVAR_R_CONTEXT, "You can define the opengl driver you want to use - empty if you want to use the system default");
	r_drawentities = Cvar_Get("r_drawentities", "1", 0, "Draw the local entities");
	r_drawworld = Cvar_Get("r_drawworld", "1", 0, "Draw the world brushes");
	r_isometric = Cvar_Get("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");
	r_nocull = Cvar_Get("r_nocull", "0", 0, "Don't perform culling for brushes and entities");
	r_anisotropic = Cvar_Get("r_anisotropic", "1", CVAR_ARCHIVE, NULL);
	r_texture_lod = Cvar_Get("r_texture_lod", "0", CVAR_ARCHIVE, NULL);
	r_screenshot_format = Cvar_Get("r_screenshot_format", "jpg", CVAR_ARCHIVE, "png, jpg or tga are valid screenshot formats");
	r_screenshot_jpeg_quality = Cvar_Get("r_screenshot_jpeg_quality", "75", CVAR_ARCHIVE, "jpeg quality in percent for jpeg screenshots");
	r_threads = Cvar_Get("r_threads", "0", CVAR_ARCHIVE, "Activate threads for the renderer");

	r_geoscape_overlay = Cvar_Get("r_geoscape_overlay", "0", 0, "Geoscape overlays - Bitmask");
	r_materials = Cvar_Get("r_materials", "1", CVAR_ARCHIVE, "Activate material subsystem");
	r_checkerror = Cvar_Get("r_checkerror", "0", CVAR_ARCHIVE, "Check for opengl errors");
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE, "Activate or deactivate shadows");
	r_maxtexres = Cvar_Get("r_maxtexres", "2048", CVAR_ARCHIVE | CVAR_R_IMAGES, "The maximum texture resolution UFO should use");
	r_texturemode = Cvar_Get("r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, NULL);
	r_texturealphamode = Cvar_Get("r_texturealphamode", "default", CVAR_ARCHIVE, NULL);
	r_texturesolidmode = Cvar_Get("r_texturesolidmode", "default", CVAR_ARCHIVE, NULL);
	r_wire = Cvar_Get("r_wire", "0", 0, "Draw the scene in wireframe mode");
	r_showbox = Cvar_Get("r_showbox", "0", CVAR_ARCHIVE, "1=Shows model bounding box, 2=show also the brushes bounding boxes");
	r_debug_lightmaps = Cvar_Get("r_debug_lightmaps", "0", CVAR_R_PROGRAMS, "Draw only the lightmap");
	r_debug_lightmaps->modified = qfalse;
	r_debug_deluxemaps = Cvar_Get("r_debug_deluxemaps", "0", CVAR_R_PROGRAMS, "Draw only the deluxemap");
	r_debug_deluxemaps->modified = qfalse;
	r_debug_normals = Cvar_Get("r_debug_normals", "0", CVAR_R_PROGRAMS, "Draw dot(normal, light_0 direction)");
	r_debug_normals->modified = qfalse;
	r_debug_normalmaps = Cvar_Get("r_debug_normalmaps", "0", CVAR_R_PROGRAMS, "Draw dot(normalmap, light_0 direction)");
	r_debug_normalmaps->modified = qfalse;
	r_debug_tangents = Cvar_Get("r_debug_tangents", "0", CVAR_R_PROGRAMS, "Draw tangent, bitangent, and normal dotted with light dir as RGB espectively");
	r_debug_tangents->modified = qfalse;
	r_debug_shadows = Cvar_Get("r_debug_shadows", "0", CVAR_R_PROGRAMS, "Draw shadowmap values only");
	r_debug_shadows->modified = qfalse;
	r_ext_texture_compression = Cvar_Get("r_ext_texture_compression", "0", CVAR_ARCHIVE, NULL);
	r_ext_nonpoweroftwo = Cvar_Get("r_ext_nonpoweroftwo", "1", CVAR_ARCHIVE, "Enable or disable the non power of two extension");
	r_ext_s3tc_compression = Cvar_Get("r_ext_s3tc_compression", "1", CVAR_ARCHIVE, "Also see r_ext_texture_compression");
	r_intel_hack = Cvar_Get("r_intel_hack", "1", CVAR_ARCHIVE, "Intel cards have activated texture compression until this is set to 0");
	r_vertexbuffers = Cvar_Get("r_vertexbuffers", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls usage of OpenGL Vertex Buffer Objects (VBO) versus legacy vertex arrays.");
	r_maxlightmap = Cvar_Get("r_maxlightmap", "2048", CVAR_ARCHIVE | CVAR_LATCH, "Reduce this value on older hardware");
	Cvar_SetCheckFunction("r_maxlightmap", R_CvarCheckMaxLightmap);

	r_drawbuffer = Cvar_Get("r_drawbuffer", "GL_BACK", 0, NULL);
	r_swapinterval = Cvar_Get("r_swapinterval", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls swap interval synchronization (V-Sync). Values between 0 and 2");
	r_multisample = Cvar_Get("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls multisampling (anti-aliasing). Values between 0 and 4");
	r_lights = Cvar_Get("r_lights", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activates or deactivates hardware lighting");
	Cvar_SetCheckFunction("r_lights", R_CvarCheckLights);
	r_warp = Cvar_Get("r_warp", "1", CVAR_ARCHIVE, "Activates or deactivates warping surface rendering");
	r_shownormals = Cvar_Get("r_shownormals", "0", CVAR_ARCHIVE, "Show normals on bsp surfaces");
	r_bumpmap = Cvar_Get("r_bumpmap", "1.0", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate bump mapping");
	r_specular = Cvar_Get("r_specular", "1.0", CVAR_ARCHIVE, "Controls specular parameters");
	r_hardness = Cvar_Get("r_hardness", "1.0", CVAR_ARCHIVE, "Hardness controll for GLSL shaders (specular, bump, ...)");
	r_parallax = Cvar_Get("r_parallax", "1.0", CVAR_ARCHIVE, "Controls parallax parameters");
	r_fog = Cvar_Get("r_fog", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate or deactivate fog");
	r_flares = Cvar_Get("r_flares", "1", CVAR_ARCHIVE, "Activate or deactivate flares");
	r_coronas = Cvar_Get("r_coronas", "1", CVAR_ARCHIVE, "Activate or deactivate coronas");

	for (commands = r_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function, commands->description);
}

/**
 * @note image cvars
 * @sa R_FilterTexture
 */
static void R_RegisterImageVars (void)
{
	r_brightness = Cvar_Get("r_brightness", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Brightness for images");
	r_contrast = Cvar_Get("r_contrast", "1.5", CVAR_ARCHIVE | CVAR_R_IMAGES, "Contrast for images");
	r_saturation = Cvar_Get("r_saturation", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Saturation for images");
	r_monochrome = Cvar_Get("r_monochrome", "0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Monochrome world - Bitmask - 1, 2");
	r_invert = Cvar_Get("r_invert", "0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Invert images - Bitmask - 1, 2");
	if (r_config.hardwareType == GLHW_NVIDIA)
		r_modulate = Cvar_Get("r_modulate", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Scale lightmap values");
	else
		r_modulate = Cvar_Get("r_modulate", "2.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Scale lightmap values");
	r_soften = Cvar_Get("r_soften", "0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Apply blur to lightmap");
}

static void R_UpdateVidDef (const vidmode_t *vidmode)
{
	viddef.width = vidmode->width;
	viddef.height = vidmode->height;

	if (viddef.strech) {
		viddef.virtualWidth = VID_NORM_WIDTH;
		viddef.virtualHeight = VID_NORM_HEIGHT;
	} else {
		float normRatio = (float) VID_NORM_WIDTH / (float) VID_NORM_HEIGHT;
		float screenRatio = (float) viddef.width / (float) viddef.height;

		/* wide screen */
		if (screenRatio > normRatio) {
			viddef.virtualWidth = VID_NORM_HEIGHT * screenRatio;
			viddef.virtualHeight = VID_NORM_HEIGHT;
		/* 5:4 or low */
		} else if (screenRatio < normRatio) {
			viddef.virtualWidth = VID_NORM_WIDTH;
			viddef.virtualHeight = VID_NORM_WIDTH / screenRatio;
		/* 4:3 */
		} else {
			viddef.virtualWidth = VID_NORM_WIDTH;
			viddef.virtualHeight = VID_NORM_HEIGHT;
		}
	}
	viddef.rx = (float)viddef.width / viddef.virtualWidth;
	viddef.ry = (float)viddef.height / viddef.virtualHeight;
}

qboolean R_SetMode (void)
{
	qboolean result;
	unsigned prevWidth;
	unsigned prevHeight;
	int prevMode;
	qboolean prevFullscreen;
	vidmode_t vidmode;

	Com_Printf("I: setting mode %d\n", vid_mode->integer);

	/* store old values if new ones will fail */
	prevWidth = viddef.width;
	prevHeight = viddef.height;
	prevFullscreen = viddef.fullscreen;
	prevMode = viddef.mode;

	/* new values */
	viddef.mode = vid_mode->integer;
	viddef.fullscreen = vid_fullscreen->integer;
	viddef.strech = vid_strech->integer;
	if (!VID_GetModeInfo(viddef.mode, &vidmode)) {
		Com_Printf("I: invalid mode\n");
		return qfalse;
	}

	result = R_InitGraphics(viddef.fullscreen, vidmode.width, vidmode.height);
	R_ShutdownFBObjects();
	R_InitFBObjects();
	R_UpdateVidDef(&vidmode);
	MN_InvalidateStack();

	Cvar_SetValue("vid_width", viddef.width);
	Cvar_SetValue("vid_height", viddef.height);

	Com_Printf("I: %dx%d (fullscreen: %s)\n", vidmode.width, vidmode.height, viddef.fullscreen ? "yes" : "no");
	if (result)
		return qtrue;

	Com_Printf("Failed to set video mode %dx%d %s.\n",
			viddef.width, viddef.height,
			(vid_fullscreen->integer ? "fullscreen" : "windowed"));

	Cvar_SetValue("vid_width", prevWidth);  /* failed, revert */
	Cvar_SetValue("vid_height", prevHeight);
	Cvar_SetValue("vid_mode", prevMode);
	Cvar_SetValue("vid_fullscreen", prevFullscreen);

	viddef.mode = vid_mode->integer;
	viddef.fullscreen = vid_fullscreen->integer;
	if (!VID_GetModeInfo(viddef.mode, &vidmode))
		return qfalse;

	result = R_InitGraphics(viddef.fullscreen, vidmode.width, vidmode.height);
	R_ShutdownFBObjects();
	R_InitFBObjects();
	R_UpdateVidDef(&vidmode);
	MN_InvalidateStack();
	return result;
}

/**
 * @brief Check and load all needed and supported opengl extensions
 * @sa R_Init
 */
static qboolean R_InitExtensions (void)
{
	GLenum err;

	/* multitexture */
	qglActiveTexture = NULL;
	qglClientActiveTexture = NULL;

	/* vertex buffer */
	qglGenBuffers = NULL;
	qglDeleteBuffers = NULL;
	qglBindBuffer = NULL;
	qglBufferData = NULL;

	/* glsl */
	qglCreateShader = NULL;
	qglDeleteShader = NULL;
	qglShaderSource = NULL;
	qglCompileShader = NULL;
	qglGetShaderiv = NULL;
	qglGetShaderInfoLog = NULL;
	qglCreateProgram = NULL;
	qglDeleteProgram = NULL;
	qglAttachShader = NULL;
	qglDetachShader = NULL;
	qglLinkProgram = NULL;
	qglUseProgram = NULL;
	qglGetProgramiv = NULL;
	qglProgramParameteriEXT = NULL;
	qglGetProgramInfoLog = NULL;
	qglGetUniformLocation = NULL;
	qglUniform1i = NULL;
	qglUniform1f = NULL;
	qglUniform2fv = NULL;
	qglUniform3fv = NULL;
	qglUniform4fv = NULL;
	qglUniformMatrix4fv = NULL;

	/* vertex attribute arrays */
	qglEnableVertexAttribArray = NULL;
	qglDisableVertexAttribArray = NULL;
	qglVertexAttribPointer = NULL;

	/* framebuffer objects */
	qglIsRenderbufferEXT = NULL;
	qglBindRenderbufferEXT = NULL;
	qglDeleteRenderbuffersEXT = NULL;
	qglGenRenderbuffersEXT = NULL;
	qglRenderbufferStorageEXT = NULL;
	qglGetRenderbufferParameterivEXT = NULL;
	qglIsFramebufferEXT = NULL;
	qglBindFramebufferEXT = NULL;
	qglDeleteFramebuffersEXT = NULL;
	qglGenFramebuffersEXT = NULL;
	qglCheckFramebufferStatusEXT = NULL;
	qglFramebufferTexture1DEXT = NULL;
	qglFramebufferTexture2DEXT = NULL;
	qglFramebufferTexture3DEXT = NULL;
	qglFramebufferRenderbufferEXT = NULL;
	qglGetFramebufferAttachmentParameterivEXT = NULL;
	qglGenerateMipmapEXT = NULL;
	qglDrawBuffers = NULL;
	qglDrawBuffer = NULL;
	qglReadBuffer = NULL;

	/* multitexture */
	if (strstr(r_config.extensionsString, "GL_ARB_multitexture")) {
		qglActiveTexture = SDL_GL_GetProcAddress("glActiveTexture");
		qglClientActiveTexture = SDL_GL_GetProcAddress("glClientActiveTexture");
	}

	if (strstr(r_config.extensionsString, "GL_ARB_texture_compression")) {
		if (r_ext_texture_compression->integer) {
			Com_Printf("using GL_ARB_texture_compression\n");
			if (r_ext_s3tc_compression->integer && strstr(r_config.extensionsString, "GL_EXT_texture_compression_s3tc")) {
				r_config.gl_compressed_solid_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				r_config.gl_compressed_alpha_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			} else {
				r_config.gl_compressed_solid_format = GL_COMPRESSED_RGB_ARB;
				r_config.gl_compressed_alpha_format = GL_COMPRESSED_RGBA_ARB;
			}
		}
	}

	if (strstr(r_config.extensionsString, "GL_ARB_texture_non_power_of_two")) {
		if (r_ext_nonpoweroftwo->integer) {
			Com_Printf("using GL_ARB_texture_non_power_of_two\n");
			r_config.nonPowerOfTwo = qtrue;
		} else {
			r_config.nonPowerOfTwo = qfalse;
			Com_Printf("ignoring GL_ARB_texture_non_power_of_two\n");
		}
	} else {
		r_config.nonPowerOfTwo = qfalse;
	}

	/* anisotropy */
	if (strstr(r_config.extensionsString, "GL_EXT_texture_filter_anisotropic")) {
		if (r_anisotropic->integer) {
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_config.maxAnisotropic);
			R_CheckError();
			if (r_anisotropic->integer > r_config.maxAnisotropic) {
				Com_Printf("max GL_EXT_texture_filter_anisotropic value is %i\n", r_config.maxAnisotropic);
				Cvar_SetValue("r_anisotropic", r_config.maxAnisotropic);
			}

			if (r_config.maxAnisotropic)
				r_config.anisotropic = qtrue;
		}
	}

	if (strstr(r_config.extensionsString, "GL_EXT_texture_lod_bias"))
		r_config.lod_bias = qtrue;

	/* vertex buffer objects */
	if (strstr(r_config.extensionsString, "GL_ARB_vertex_buffer_object")) {
		qglGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
		qglDeleteBuffers = SDL_GL_GetProcAddress("glDeleteBuffers");
		qglBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
		qglBufferData = SDL_GL_GetProcAddress("glBufferData");
	}

	/* glsl vertex and fragment shaders and programs */
	if (strstr(r_config.extensionsString, "GL_ARB_fragment_shader")) {
		qglCreateShader = SDL_GL_GetProcAddress("glCreateShader");
		qglDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
		qglShaderSource = SDL_GL_GetProcAddress("glShaderSource");
		qglCompileShader = SDL_GL_GetProcAddress("glCompileShader");
		qglGetShaderiv = SDL_GL_GetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = SDL_GL_GetProcAddress("glGetShaderInfoLog");
		qglCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
		qglDeleteProgram = SDL_GL_GetProcAddress("glDeleteProgram");
		qglAttachShader = SDL_GL_GetProcAddress("glAttachShader");
		qglDetachShader = SDL_GL_GetProcAddress("glDetachShader");
		qglLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
		qglUseProgram = SDL_GL_GetProcAddress("glUseProgram");
		qglGetProgramiv = SDL_GL_GetProcAddress("glGetProgramiv");
		qglProgramParameteriEXT = SDL_GL_GetProcAddress("glProgramParameteriEXT");
		qglGetProgramInfoLog = SDL_GL_GetProcAddress("glGetProgramInfoLog");
		qglGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
		qglUniform1i = SDL_GL_GetProcAddress("glUniform1i");
		qglUniform1f = SDL_GL_GetProcAddress("glUniform1f");
		qglUniform1fv = SDL_GL_GetProcAddress("glUniform1fv");
		qglUniform2fv = SDL_GL_GetProcAddress("glUniform2fv");
		qglUniform3fv = SDL_GL_GetProcAddress("glUniform3fv");
		qglUniform4fv = SDL_GL_GetProcAddress("glUniform4fv");
		qglUniformMatrix4fv = SDL_GL_GetProcAddress("glUniformMatrix4fv");
		qglGetAttribLocation = SDL_GL_GetProcAddress("glGetAttribLocation");

		/* vertex attribute arrays */
		qglEnableVertexAttribArray = SDL_GL_GetProcAddress("glEnableVertexAttribArray");
		qglDisableVertexAttribArray = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
		qglVertexAttribPointer = SDL_GL_GetProcAddress("glVertexAttribPointer");
	}

	if (strstr(r_config.extensionsString, "GL_ARB_shading_language_100")) {
		r_config.shadingLanguageVersion = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION_ARB);
		Com_Printf("using GL_ARB_shading_language_100\n");
		Com_Printf("GLSL Version: %s\n", r_config.shadingLanguageVersion);
	}

	/* framebuffer objects */
	if (strstr(r_config.extensionsString, "GL_ARB_framebuffer_object")
	 || strstr(r_config.extensionsString, "GL_EXT_framebuffer_object")) {
		qglIsRenderbufferEXT = SDL_GL_GetProcAddress("glIsRenderbufferEXT");
		qglBindRenderbufferEXT = SDL_GL_GetProcAddress("glBindRenderbufferEXT");
		qglDeleteRenderbuffersEXT = SDL_GL_GetProcAddress("glDeleteRenderbuffersEXT");
		qglGenRenderbuffersEXT = SDL_GL_GetProcAddress("glGenRenderbuffersEXT");
		qglRenderbufferStorageEXT = SDL_GL_GetProcAddress("glRenderbufferStorageEXT");
		qglGetRenderbufferParameterivEXT = SDL_GL_GetProcAddress("glGetRenderbufferParameterivEXT");
		qglIsFramebufferEXT = SDL_GL_GetProcAddress("glIsFramebufferEXT");
		qglBindFramebufferEXT = SDL_GL_GetProcAddress("glBindFramebufferEXT");
		qglDeleteFramebuffersEXT = SDL_GL_GetProcAddress("glDeleteFramebuffersEXT");
		qglGenFramebuffersEXT = SDL_GL_GetProcAddress("glGenFramebuffersEXT");
		qglCheckFramebufferStatusEXT = SDL_GL_GetProcAddress("glCheckFramebufferStatusEXT");
		qglFramebufferTexture1DEXT = SDL_GL_GetProcAddress("glFramebufferTexture1DEXT");
		qglFramebufferTexture2DEXT = SDL_GL_GetProcAddress("glFramebufferTexture2DEXT");
		qglFramebufferTexture3DEXT = SDL_GL_GetProcAddress("glFramebufferTexture3DEXT");
		qglFramebufferRenderbufferEXT = SDL_GL_GetProcAddress("glFramebufferRenderbufferEXT");
		qglGetFramebufferAttachmentParameterivEXT = SDL_GL_GetProcAddress("glGetFramebufferAttachmentParameterivEXT");
		qglGenerateMipmapEXT = SDL_GL_GetProcAddress("glGenerateMipmapEXT");
		qglDrawBuffers = SDL_GL_GetProcAddress("glDrawBuffers");
		qglDrawBuffer = SDL_GL_GetProcAddress("glDrawBuffer");
		qglReadBuffer = SDL_GL_GetProcAddress("glReadBuffer");

		if (qglBindFramebufferEXT && qglDeleteRenderbuffersEXT && qglDeleteFramebuffersEXT && qglGenFramebuffersEXT
		 && qglBindFramebufferEXT && qglFramebufferTexture2DEXT && qglBindRenderbufferEXT && qglRenderbufferStorageEXT
		 && qglCheckFramebufferStatusEXT) {
			glGetIntegerv(GL_MAX_DRAW_BUFFERS, &r_config.maxDrawBuffers);
			glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &r_config.maxRenderbufferSize);
			glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &r_config.maxColorAttachments);

			r_config.frameBufferObject = qtrue;

			Com_Printf("using GL_ARB_framebuffer_object\n");
			Com_Printf("max draw buffers: %i\n", r_config.maxDrawBuffers);
			Com_Printf("max render buffer size: %i\n", r_config.maxRenderbufferSize);
			Com_Printf("max color attachments: %i\n", r_config.maxColorAttachments);
		}

		if ((strstr(r_config.extensionsString, "GL_ARB_draw_buffers")
		  || strstr(r_config.extensionsString, "GL_EXT_draw_buffers"))
		  && r_config.maxDrawBuffers > 1) {
			Com_Printf("using GL_ARB_draw_buffers\n");
			r_config.drawBuffers = qtrue;
		} else {
			r_config.drawBuffers = qfalse;
		}
	}

	r_programs = Cvar_Get("r_programs", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Use GLSL shaders");
	r_programs->modified = qfalse;
	Cvar_SetCheckFunction("r_programs", R_CvarCheckPrograms);

	r_framebuffers = Cvar_Get("r_framebuffers", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate framebuffer rendering");
	r_framebuffers->modified = qfalse;
	Cvar_SetCheckFunction("r_framebuffers", R_CvarCheckFramebuffers);

	r_postprocess = Cvar_Get("r_postprocess", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate postprocessing shader effects");
	r_postprocess->modified = qfalse;
	Cvar_SetCheckFunction("r_postprocess", R_CvarCheckPostProcess);

	/* reset gl error state */
	R_CheckError();

	glGetIntegerv(GL_MAX_LIGHTS, &r_config.maxLights);
	Com_Printf("max supported lights: %i\n", r_config.maxLights);

	r_dynamic_lights = Cvar_Get("r_dynamic_lights", "4", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Sets max number of GL lightsources to use in shaders");
	r_dynamic_lights->modified = qfalse;
	Cvar_SetCheckFunction("r_dynamic_lights", R_CvarCheckDynamicLights);

	r_shadowmapping = Cvar_Get("r_shadowmapping", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate shadowmapping");
	r_shadowmapping->modified = qfalse;
	Cvar_SetCheckFunction("r_shadowmapping", R_CvarCheckShadowmapping);

	r_softshadows = Cvar_Get("r_softshadows", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate soft shadow boundaries");
	Cvar_SetCheckFunction("r_softshadows", R_CvarCheckShadowmapping);


	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &r_config.maxTextureUnits);
	Com_Printf("max texture units: %i\n", r_config.maxTextureUnits);
	if (r_config.maxTextureUnits < 2)
		Com_Error(ERR_FATAL, "You need at least 2 texture units to run "GAME_TITLE);

	glGetIntegerv(GL_MAX_TEXTURE_COORDS, &r_config.maxTextureCoords);
	Com_Printf("max texture coords: %i\n", r_config.maxTextureCoords);
	r_config.maxTextureCoords = max(r_config.maxTextureUnits, r_config.maxTextureCoords);

	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &r_config.maxTextureImageUnits);
	Com_Printf("max combined texture image units: %i\n", r_config.maxTextureImageUnits);
	r_config.maxTextureCoords = max(r_config.maxTextureImageUnits, r_config.maxTextureCoords);

	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &r_config.maxVertexAttribs);
	Com_Printf("max vertex attributes: %i\n", r_config.maxVertexAttribs);

	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &r_config.maxUniforms);
	Com_Printf("max shader uniforms: %i\n", r_config.maxUniforms);


	/* reset gl error state */
	R_CheckError();

	/* check max texture size */
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &r_config.maxTextureSize);
	/* stubbed or broken drivers may have reported 0 */
	if (r_config.maxTextureSize <= 0)
		r_config.maxTextureSize = 256;

	if ((err = glGetError()) != GL_NO_ERROR) {
		Com_Printf("max texture size: cannot detect - using %i! (%s)\n", r_config.maxTextureSize, R_TranslateError(err));
		Cvar_SetValue("r_maxtexres", r_config.maxTextureSize);
	} else {
		Com_Printf("max texture size: detected %d\n", r_config.maxTextureSize);
		if (r_maxtexres->integer > r_config.maxTextureSize) {
			Com_Printf("...downgrading from %i\n", r_maxtexres->integer);
			Cvar_SetValue("r_maxtexres", r_config.maxTextureSize);
		/* check for a minimum */
		} else if (r_maxtexres->integer >= 128 && r_maxtexres->integer < r_config.maxTextureSize) {
			Com_Printf("...but using %i as requested\n", r_maxtexres->integer);
			r_config.maxTextureSize = r_maxtexres->integer;
		}
	}

#if 0
	/* @note - currently we use r_maxlightmap for this */
	r_shadowmap_width = Cvar_Get("r_shadowmap_width", "1024", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Set shadowmap resolution");
	r_shadowmap_width->modified = qfalse;
	Cvar_SetCheckFunction("r_shadowmap_width", R_CvarCheckMaxLightmap);
#endif


	if (r_config.maxTextureSize > 4096 && R_ImageExists(va("pics/geoscape/%s/map_earth_season_00", "high"))) {
		Q_strncpyz(r_config.lodDir, "high", sizeof(r_config.lodDir));
		Com_Printf("Using high resolution globe textures as requested.\n");
	} else if (r_config.maxTextureSize > 2048 && R_ImageExists("pics/geoscape/med/map_earth_season_00")) {
		if (r_config.maxTextureSize > 4096) {
			Com_Printf("Warning: high resolution globe textures requested, but could not be found; falling back to medium resolution globe textures.\n");
		} else {
			Com_Printf("Using medium resolution globe textures as requested.\n");
		}
		Q_strncpyz(r_config.lodDir, "med", sizeof(r_config.lodDir));
	} else {
		if (r_config.maxTextureSize > 2048) {
			Com_Printf("Warning: medium resolution globe textures requested, but could not be found; falling back to low resolution globe textures.\n");
		} else {
			Com_Printf("Using low resolution globe textures as requested.\n");
		}
		Q_strncpyz(r_config.lodDir, "low", sizeof(r_config.lodDir));
	}

	/* multitexture is the only one we absolutely need */
	if (!qglActiveTexture || !qglClientActiveTexture)
		return qfalse;

	return qtrue;
}

/**
 * @brief We need at least opengl version 1.2.1
 */
static inline void R_EnforceVersion (void)
{
	int maj, min, rel;

	sscanf(r_config.versionString, "%d.%d.%d ", &maj, &min, &rel);

	if (maj > 1)
		return;

	if (maj < 1)
		Com_Error(ERR_FATAL, "OpenGL version %s is less than 1.2.1", r_config.versionString);

	if (min > 2)
		return;

	if (min < 2)
		Com_Error(ERR_FATAL, "OpenGL Version %s is less than 1.2.1", r_config.versionString);

	if (rel > 1)
		return;

	if (rel < 1)
		Com_Error(ERR_FATAL, "OpenGL version %s is less than 1.2.1", r_config.versionString);
}

/**
 * @brief Searches vendor and renderer GL strings for the given vendor id
 */
static qboolean R_SearchForVendor (const char *vendor)
{
	return Q_stristr(r_config.vendorString, vendor)
		|| Q_stristr(r_config.rendererString, vendor);
}

#define INTEL_TEXTURE_RESOLUTION 1024

/**
 * @brief Checks whether we have hardware acceleration
 */
static inline void R_VerifyDriver (void)
{
#ifdef _WIN32
	if (!Q_strcasecmp((const char*)glGetString(GL_RENDERER), "gdi generic"))
		Com_Error(ERR_FATAL, "No hardware acceleration detected.\n"
			"Update your graphic card drivers.");
#endif
	if (r_intel_hack->integer && R_SearchForVendor("Intel")) {
		/* HACK: */
		Com_Printf("Activate texture compression for Intel chips - see cvar r_intel_hack\n");
		Cvar_Set("r_ext_texture_compression", "1");
		if (r_maxtexres->integer > INTEL_TEXTURE_RESOLUTION) {
			Com_Printf("Set max. texture resolution to %i - see cvar r_intel_hack\n", INTEL_TEXTURE_RESOLUTION);
			Cvar_SetValue("r_maxtexres", INTEL_TEXTURE_RESOLUTION);
		}
		r_ext_texture_compression->modified = qfalse;
		r_config.hardwareType = GLHW_INTEL;
	} else if (R_SearchForVendor("NVIDIA")) {
		r_config.hardwareType = GLHW_NVIDIA;
	} else if (R_SearchForVendor("ATI") || R_SearchForVendor("Advanced Micro Devices") || R_SearchForVendor("AMD")) {
		r_config.hardwareType = GLHW_ATI;
	} else if (R_SearchForVendor("mesa")) {
		r_config.hardwareType = GLHW_MESA;
	} else {
		r_config.hardwareType = GLHW_GENERIC;
	}
}

qboolean R_Init (void)
{
	R_RegisterSystemVars();

	memset(&r_state, 0, sizeof(r_state));
	memset(&r_locals, 0, sizeof(r_locals));
	memset(&r_config, 0, sizeof(r_config));

	/* some config default values */
	r_config.gl_solid_format = GL_RGB;
	r_config.gl_alpha_format = GL_RGBA;
	r_config.gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
	r_config.gl_filter_max = GL_LINEAR;
	r_config.maxTextureSize = 256;

	/* initialize OS-specific parts of OpenGL */
	if (!Rimp_Init())
		return qfalse;

	/* get our various GL strings */
	r_config.vendorString = (const char *)glGetString(GL_VENDOR);
	r_config.rendererString = (const char *)glGetString(GL_RENDERER);
	r_config.versionString = (const char *)glGetString(GL_VERSION);
	r_config.extensionsString = (const char *)glGetString(GL_EXTENSIONS);
	R_Strings_f();

	/* sanity checks and card specific hacks */
	R_VerifyDriver();
	R_EnforceVersion();

	R_RegisterImageVars();

	/* prevent reloading of some rendering cvars */
	Cvar_ClearVars(CVAR_R_MASK);

	R_InitExtensions();
	R_SetDefaultState();
	R_InitPrograms();
	R_InitImages();
	R_InitMiscTexture();
	R_DrawInitLocal();
	R_SphereInit();
	R_FontInit();
	R_InitFBObjects();

	R_CheckError();

	return qtrue;
}

/**
 * @sa R_Init
 */
void R_Shutdown (void)
{
	const cmdList_t *commands;

	for (commands = r_commands; commands->name; commands++)
		Cmd_RemoveCommand(commands->name);

	R_ShutdownModels(qtrue);
	R_ShutdownImages();

	R_ShutdownPrograms();
	R_FontShutdown();
	R_ShutdownFBObjects();

	/* shut down OS specific OpenGL stuff like contexts, etc. */
	Rimp_Shutdown();

	R_ShutdownThreads();
}
