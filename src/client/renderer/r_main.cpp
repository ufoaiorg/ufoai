/**
 * @file
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

#define QGL_EXTERN
#include "r_gl.h"
#include "r_local.h"
#include "r_program.h"
#include "r_sphere.h"
#include "r_draw.h"
#include "r_font.h"
#include "r_light.h"
#include "r_lightmap.h"
#include "r_grass.h"
#include "r_weather.h"
#include "r_main.h"
#include "r_geoscape.h"
#include "r_misc.h"
#include "r_error.h"
#include "../../common/tracing.h"
#include "../ui/ui_windows.h"
#include "../../ports/system.h"
#include "../client.h"

rendererData_t refdef;

rconfig_t r_config;
rstate_t r_state;
rlocals_t r_locals;

image_t* r_noTexture;			/* use for bad textures */
image_t* r_warpTexture;
image_t* r_dummyTexture; /* 1x1 pixel white texture to be used when texturing is required, but texture is not available */

Weather r_battlescapeWeather;

static cvar_t* r_maxtexres;

cvar_t* r_weather;
cvar_t* r_drawentities;
cvar_t* r_drawworld;
cvar_t* r_nocull;
cvar_t* r_isometric;
cvar_t* r_anisotropic;
cvar_t* r_texture_lod;			/* lod_bias */
cvar_t* r_screenshot_format;
cvar_t* r_screenshot_jpeg_quality;
cvar_t* r_lightmap;
cvar_t* r_debug_normals;
cvar_t* r_debug_tangents;
cvar_t* r_debug_lights;
static cvar_t* r_deluxemap;
cvar_t* r_ext_texture_compression;
static cvar_t* r_ext_s3tc_compression;
static cvar_t* r_ext_nonpoweroftwo;
static cvar_t* r_intel_hack;
static cvar_t* r_texturemode;
static cvar_t* r_texturealphamode;
static cvar_t* r_texturesolidmode;
cvar_t* r_materials;
cvar_t* r_overridematerial;
cvar_t* r_default_specular;
cvar_t* r_default_hardness;
cvar_t* r_checkerror;
cvar_t* r_drawbuffer;
cvar_t* r_driver;
cvar_t* r_shadows;
cvar_t* r_stencilshadows;
cvar_t* r_swapinterval;
cvar_t* r_multisample;
cvar_t* r_wire;
cvar_t* r_showbox;
cvar_t* r_threads;
cvar_t* r_vertexbuffers;
cvar_t* r_warp;
cvar_t* r_dynamic_lights;
cvar_t* r_programs;
/** @brief The GLSL version being used (not necessarily a supported version by the OpenGL implementation). Stored as a c-string and integer.*/
cvar_t* r_glsl_version;
cvar_t* r_postprocess;
cvar_t* r_maxlightmap;
cvar_t* r_shownormals;
cvar_t* r_bumpmap;
cvar_t* r_specular;
cvar_t* r_hardness;
cvar_t* r_parallax;
cvar_t* r_fog;
cvar_t* r_flares;
cvar_t* r_coronas;
cvar_t* r_drawtags;

static void R_PrintInfo (const char* pre, const char* msg)
{
	char buf[4096];
	const size_t length = sizeof(buf);
	const size_t maxLength = strlen(msg);

	Com_Printf("%s: ", pre);
	for (int i = 0; i < maxLength; i += length) {
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

#if 0
	/* if we are not drawing world model, we are on the UI code. It break the default UI SCISSOR
	 * Anyway we should merge that code into R_CleanupDepthBuffer (with some rework), it looks better */

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (refdef.rendererFlags & RDF_NOWORLDMODEL) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(viddef.x, viddef.height - viddef.viewHeight - viddef.y, viddef.viewWidth, viddef.viewHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		R_CheckError();
		glDisable(GL_SCISSOR_TEST);
	}
#endif
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
	/* clear the stencil bit if shadows are enabled */
	if (r_stencilshadows->integer)
		glClear(GL_STENCIL_BUFFER_BIT);
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
	refdef.numEntities = refdef.numDynamicLights = refdef.numCoronas = 0;
	R_ClearBspRRefs();
}

/**
 * @sa R_RenderFrame
 * @sa R_EndFrame
 */
void R_BeginFrame (void)
{
	r_locals.frame++;

	/* avoid overflows, negatives and zero are reserved */
	if (r_locals.frame > 0xffff)
		r_locals.frame = 1;

	if (Com_IsRenderModified()) {
		Com_Printf("Modified render related cvars\n");
		if (Cvar_PendingCvars(CVAR_R_PROGRAMS))
			R_RestartPrograms_f();

		/* prevent reloading of some rendering cvars */
		Cvar_ClearVars(CVAR_R_MASK);
		Com_SetRenderModified(false);
	}

	if (r_anisotropic->modified) {
		if (r_anisotropic->integer > r_config.maxAnisotropic) {
			Com_Printf("...max GL_EXT_texture_filter_anisotropic value is %i\n", r_config.maxAnisotropic);
			Cvar_SetValue("r_anisotropic", r_config.maxAnisotropic);
		}
		/*R_UpdateAnisotropy();*/
		r_anisotropic->modified = false;
	}

	/* draw buffer stuff */
#ifndef GL_VERSION_ES_CM_1_0
	if (r_drawbuffer->modified) {
		r_drawbuffer->modified = false;

		if (Q_strcasecmp(r_drawbuffer->string, "GL_FRONT") == 0)
			glDrawBuffer(GL_FRONT);
		else
			glDrawBuffer(GL_BACK);
		R_CheckError();
	}
#endif

	/* texturemode stuff */
	/* Realtime set level of anisotropy filtering and change texture lod bias */
	if (r_texturemode->modified) {
		R_TextureMode(r_texturemode->string);
		r_texturemode->modified = false;
	}

	if (r_texturealphamode->modified) {
		R_TextureAlphaMode(r_texturealphamode->string);
		r_texturealphamode->modified = false;
	}

	if (r_texturesolidmode->modified) {
		R_TextureSolidMode(r_texturesolidmode->string);
		r_texturesolidmode->modified = false;
	}

	/* threads */
	if (r_threads->modified) {
		if (r_threads->integer)
			R_InitThreads();
		else
			R_ShutdownThreads();
		r_threads->modified = false;
	}

	R_EnableMultisample(true);

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
	static int lastFrame = 0;
	int deltaMsec = cl.time - lastFrame;
	lastFrame = cl.time;

	r_battlescapeWeather.update(deltaMsec); /* could do that in separate thread */

	R_Setup3D();

	/* activate wire mode */
	if (r_wire->integer) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	if (!(refdef.rendererFlags & RDF_NOWORLDMODEL)) {
		int tile;
		if (r_threads->integer) {
			while (r_threadstate.state != THREAD_RENDERER)
				Sys_Sleep(0);

			r_threadstate.state = THREAD_CLIENT;
		} else {
			R_SetupFrustum();

			/* draw brushes on current worldlevel */
			R_GetLevelSurfaceLists();
		}

		R_UpdateSustainedLights();

		R_CheckError();

		for (tile = 0; tile < r_numMapTiles; tile++) {
			const model_t* mapTile = r_mapTiles[tile];
			const mBspModel_t* bsp = &mapTile->bsp;

			R_AddBspRRef(bsp, vec3_origin, vec3_origin, false);
		}

		R_GetEntityLists();

		R_EnableFog(true);

		R_RenderOpaqueBspRRefs();
		R_RenderOpaqueWarpBspRRefs();
		R_DrawOpaqueMeshEntities(r_opaque_mesh_entities);

		R_RenderAlphaTestBspRRefs();
		R_DrawGrass();

		R_EnableBlend(true);
		R_RenderMaterialBspRRefs();

		R_EnableFog(false);

		R_RenderBlendBspRRefs();
		R_RenderBlendWarpBspRRefs();
		R_DrawBlendMeshEntities(r_blend_mesh_entities);

		r_battlescapeWeather.render();

		R_EnableFog(true);
		R_RenderFlareBspRRefs();
		R_EnableFog(false);

		if (r_debug_lights->integer) {
			int i;

			for (i = 0; i < refdef.numStaticLights; i++) {
				const light_t* l = &refdef.staticLights[i];
				R_AddCorona(l->origin, l->radius, l->color);
			}
			for (i = 0; i < refdef.numDynamicLights; i++) {
				const light_t* l = &refdef.dynamicLights[i];
				R_AddCorona(l->origin, l->radius, l->color);
			}
		}

		R_DrawCoronas();
		R_EnableBlend(false);

		for (tile = 0; tile < r_numMapTiles; tile++) {
			R_DrawBspNormals(tile);
		}

		R_Color(nullptr);
		R_DrawSpecialEntities(r_special_entities);
		R_DrawNullEntities(r_null_entities);
		R_DrawEntityEffects();
	} else {
		glClear(GL_DEPTH_BUFFER_BIT);

		R_GetEntityLists();

		R_RenderOpaqueBspRRefs();
		R_RenderOpaqueWarpBspRRefs();
		R_DrawOpaqueMeshEntities(r_opaque_mesh_entities);
		R_RenderAlphaTestBspRRefs();

		R_EnableBlend(true);

		R_RenderMaterialBspRRefs();

		R_RenderBlendBspRRefs();
		R_RenderBlendWarpBspRRefs();
		R_DrawBlendMeshEntities(r_blend_mesh_entities);

		R_RenderFlareBspRRefs();

		R_EnableBlend(false);

		R_Color(nullptr);
		R_DrawSpecialEntities(r_special_entities);
		R_DrawNullEntities(r_null_entities);
		R_DrawEntityEffects();
	}

	R_EnableBlend(true);

	R_DrawParticles();

	R_EnableBlend(false);

	/* leave wire mode again */
	if (r_wire->integer) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	R_DrawBloom();

	R_DrawBoundingBoxes();

	R_ResetArrayState();

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
	R_EnableBlend(true);

	R_DrawChars();  /* draw all chars accumulated above */

	/* restore draw color */
	R_Color(nullptr);

	R_EnableBlend(false);

	if (vid_gamma->modified) {
		if (!vid_ignoregamma->integer) {
			const float g = vid_gamma->value;
#if SDL_VERSION_ATLEAST(2,0,0)
			SDL_SetWindowBrightness(cls.window, g);
#else
			SDL_SetGamma(g, g, g);
#endif
		}
		vid_gamma->modified = false;
	}

	R_EnableMultisample(false);

	R_ClearScene();

#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_GL_SwapWindow(cls.window);
#else
	SDL_GL_SwapBuffers();
#endif
}

static const cmdList_t r_commands[] = {
	{"r_listimages", R_ImageList_f, "Show all loaded images on game console"},
	{"r_listfontcache", R_FontListCache_f, "Show information about font cache"},
	{"r_screenshot", R_ScreenShot_f, "Take a screenshot"},
	{"r_listmodels", R_ModModellist_f, "Show all loaded models on game console"},
	{"r_strings", R_Strings_f, "Print openGL vendor and other strings"},
	{"r_restartprograms", R_RestartPrograms_f, "Reloads the shaders"},

	{nullptr, nullptr, nullptr}
};

static bool R_CvarCheckMaxLightmap (cvar_t* cvar)
{
	int maxSize = !r_config.maxTextureSize || LIGHTMAP_MAX_PAGE_SIZE < r_config.maxTextureSize ? LIGHTMAP_MAX_PAGE_SIZE : r_config.maxTextureSize;

	if (cvar->integer > maxSize) {
		Com_Printf("%s exceeded max supported texture size\n", cvar->name);
		Cvar_SetValue(cvar->name, maxSize);
		return false;
	}

	if (!Q_IsPowerOfTwo(cvar->integer)) {
		Com_Printf("%s must be power of two\n", cvar->name);
		Cvar_SetValue(cvar->name, LIGHTMAP_DEFAULT_PAGE_SIZE);
		return false;
	}

	return Cvar_AssertValue(cvar, 128, LIGHTMAP_MAX_PAGE_SIZE, true);
}

static bool R_CvarCheckDynamicLights (cvar_t* cvar)
{
	float maxLights = r_config.maxLights;

	if (maxLights > MAX_ENTITY_LIGHTS)
		maxLights = MAX_ENTITY_LIGHTS;

	return Cvar_AssertValue(cvar, 0, maxLights, true);
}

static bool R_CvarPrograms (cvar_t* cvar)
{
	if (qglUseProgram) {
		return Cvar_AssertValue(cvar, 0, 3, true);
	}

	Cvar_SetValue(cvar->name, 0);
	return true;
}

/**
 * @brief Callback that is called when the r_glsl_version cvar is changed,
 */
static bool R_CvarGLSLVersionCheck (cvar_t* cvar)
{
	int glslVersionMajor, glslVersionMinor;
	sscanf(cvar->string, "%d.%d", &glslVersionMajor, &glslVersionMinor);
	if (glslVersionMajor > r_config.glslVersionMajor) {
		Cvar_Reset(cvar);
		return false;
	}

	if (glslVersionMajor == r_config.glslVersionMajor && glslVersionMinor > r_config.glslVersionMinor) {
		Cvar_Reset(cvar);
		return false;
	}

	return true;
}

static bool R_CvarPostProcess (cvar_t* cvar)
{
	if (r_config.frameBufferObject && r_config.drawBuffers)
		return Cvar_AssertValue(cvar, 0, 1, true);

	Cvar_SetValue(cvar->name, 0);
	return true;
}

static bool R_CvarCheckMultisample (cvar_t* cvar)
{
	return Cvar_AssertValue(cvar, 0, 4, true);
}

static bool R_CvarCheckWeather (cvar_t* cvar)
{
	return Cvar_AssertValue(cvar, (float)Weather::WEATHER_CLEAN, (float)Weather::WEATHER_MAX, true);
}

static void R_UpdateWeather (const char* cvarName, const char* oldValue, const char* newValue, void* data)
{
	r_battlescapeWeather.changeTo((Weather::weatherTypes)atoi(newValue)); /** @todo Some better way? */
}


static void R_RegisterSystemVars (void)
{
	r_driver = Cvar_GetOrCreate("r_driver", "", CVAR_ARCHIVE | CVAR_R_CONTEXT, "You can define the opengl driver you want to use - empty if you want to use the system default");
	r_weather = Cvar_GetOrCreate("r_weather", "0", 0, "Current battlescape weather effect");
	Cvar_SetCheckFunction("r_weather", R_CvarCheckWeather);
	Cvar_RegisterChangeListener("r_weather", R_UpdateWeather);
	r_drawentities = Cvar_GetOrCreate("r_drawentities", "1", 0, "Draw the local entities");
	r_drawworld = Cvar_GetOrCreate("r_drawworld", "1", 0, "Draw the world brushes");
	r_isometric = Cvar_GetOrCreate("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");
	r_nocull = Cvar_GetOrCreate("r_nocull", "0", 0, "Don't perform culling for brushes and entities");
	r_anisotropic = Cvar_GetOrCreate("r_anisotropic", "1", CVAR_ARCHIVE);
	r_texture_lod = Cvar_GetOrCreate("r_texture_lod", "0", CVAR_ARCHIVE);
	r_screenshot_format = Cvar_GetOrCreate("r_screenshot_format", "jpg", CVAR_ARCHIVE, "png, jpg or tga are valid screenshot formats");
	r_screenshot_jpeg_quality = Cvar_GetOrCreate("r_screenshot_jpeg_quality", "75", CVAR_ARCHIVE, "jpeg quality in percent for jpeg screenshots");
	r_threads = Cvar_GetOrCreate("r_threads", "0", CVAR_ARCHIVE, "Activate threads for the renderer");

	r_materials = Cvar_GetOrCreate("r_materials", "1", CVAR_ARCHIVE, "Activate material subsystem");
	r_overridematerial = Cvar_GetOrCreate("r_overridematerial", "", 0, "Used to override the material for a map");
	r_default_specular = Cvar_GetOrCreate("r_default_specular", "0.2", CVAR_R_CONTEXT, "Default specular exponent");
	r_default_hardness = Cvar_GetOrCreate("r_default_hardness", "0.2", CVAR_R_CONTEXT, "Default specular brightness");
	Cvar_RegisterChangeListener("r_default_specular", R_UpdateDefaultMaterial);
	Cvar_RegisterChangeListener("r_default_hardness", R_UpdateDefaultMaterial);
	r_checkerror = Cvar_GetOrCreate("r_checkerror", "0", CVAR_ARCHIVE, "Check for opengl errors");
	r_shadows = Cvar_GetOrCreate("r_shadows", "1", CVAR_ARCHIVE, "Multiplier for the alpha of the shadows");
	r_stencilshadows = Cvar_GetOrCreate("r_stencilshadows", "0", CVAR_ARCHIVE, "Activate or deactivate stencil shadows");
	r_maxtexres = Cvar_GetOrCreate("r_maxtexres", "2048", CVAR_ARCHIVE | CVAR_R_IMAGES, "The maximum texture resolution UFO should use");
	r_texturemode = Cvar_GetOrCreate("r_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE, "change the filtering and mipmapping for textures");
	r_texturealphamode = Cvar_GetOrCreate("r_texturealphamode", "GL_RGBA", CVAR_ARCHIVE);
	r_texturesolidmode = Cvar_GetOrCreate("r_texturesolidmode", "GL_RGB", CVAR_ARCHIVE);
	r_wire = Cvar_GetOrCreate("r_wire", "0", 0, "Draw the scene in wireframe mode");
	r_showbox = Cvar_GetOrCreate("r_showbox", "0", CVAR_ARCHIVE, "1=Shows model bounding box, 2=show also the brushes bounding boxes");
	r_lightmap = Cvar_GetOrCreate("r_lightmap", "0", CVAR_R_PROGRAMS, "Draw only the lightmap");
	r_lightmap->modified = false;
	r_deluxemap = Cvar_GetOrCreate("r_deluxemap", "0", CVAR_R_PROGRAMS, "Draw only the deluxemap");
	r_deluxemap->modified = false;
	r_debug_normals = Cvar_GetOrCreate("r_debug_normals", "0", CVAR_R_PROGRAMS, "Draw dot(normal, light_0 direction)");
	r_debug_normals->modified = false;
	r_debug_tangents = Cvar_GetOrCreate("r_debug_tangents", "0", CVAR_R_PROGRAMS, "Draw tangent, bitangent, and normal dotted with light dir as RGB espectively");
	r_debug_tangents->modified = false;
	r_debug_lights = Cvar_GetOrCreate("r_debug_lights", "0", CVAR_ARCHIVE, "Draw active light sources");
	r_ext_texture_compression = Cvar_GetOrCreate("r_ext_texture_compression", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT);
	r_ext_nonpoweroftwo = Cvar_GetOrCreate("r_ext_nonpoweroftwo", "1", CVAR_ARCHIVE, "Enable or disable the non power of two extension");
	r_ext_s3tc_compression = Cvar_GetOrCreate("r_ext_s3tc_compression", "1", CVAR_ARCHIVE, "Also see r_ext_texture_compression");
	r_intel_hack = Cvar_GetOrCreate("r_intel_hack", "1", CVAR_ARCHIVE, "Intel cards have no shaders until this is set to 0 - to it to a value > 1 to not limit the max texture resolution");
	r_vertexbuffers = Cvar_GetOrCreate("r_vertexbuffers", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls usage of OpenGL Vertex Buffer Objects (VBO) versus legacy vertex arrays.");
	r_maxlightmap = Cvar_GetOrCreate("r_maxlightmap", "2048", CVAR_ARCHIVE | CVAR_LATCH, "Reduce this value on older hardware");
	Cvar_SetCheckFunction("r_maxlightmap", R_CvarCheckMaxLightmap);

	r_drawbuffer = Cvar_GetOrCreate("r_drawbuffer", "GL_BACK");
	r_swapinterval = Cvar_GetOrCreate("r_swapinterval", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls swap interval synchronization (V-Sync). Values between 0 and 2");
	r_multisample = Cvar_GetOrCreate("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls multisampling (anti-aliasing). Values between 0 and 4");
	Cvar_SetCheckFunction("r_multisample", R_CvarCheckMultisample);
	r_warp = Cvar_GetOrCreate("r_warp", "1", CVAR_ARCHIVE, "Activates or deactivates warping surface rendering");
	r_shownormals = Cvar_GetOrCreate("r_shownormals", "0", CVAR_ARCHIVE, "Show normals on bsp surfaces");
	r_bumpmap = Cvar_GetOrCreate("r_bumpmap", "1.0", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate bump mapping");
	r_specular = Cvar_GetOrCreate("r_specular", "1.0", CVAR_ARCHIVE, "Controls specular parameters");
	r_hardness = Cvar_GetOrCreate("r_hardness", "1.0", CVAR_ARCHIVE, "Hardness control for GLSL shaders (specular, bump, ...)");
	r_parallax = Cvar_GetOrCreate("r_parallax", "1.0", CVAR_ARCHIVE, "Controls parallax parameters");
	r_fog = Cvar_GetOrCreate("r_fog", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate or deactivate fog");
	r_flares = Cvar_GetOrCreate("r_flares", "1", CVAR_ARCHIVE, "Activate or deactivate flares");
	r_coronas = Cvar_GetOrCreate("r_coronas", "1", CVAR_ARCHIVE, "Activate or deactivate coronas");
	r_particles = Cvar_GetOrCreate("r_particles", "1", 0, "Activate or deactivate particle rendering");
	r_drawtags = Cvar_GetOrCreate("r_drawtags", "0", 0, "Activate or deactivate tag rendering");

	Cmd_TableAddList(r_commands);
}

/**
 * @note image cvars
 */
static void R_RegisterImageVars (void)
{
}

/**
 * Update the graphical context according to a valid context.
 * All the system is updated according to this new value (viddef and cvars)
 * @param context New graphical context
 */
static void R_UpdateVidDef (const viddefContext_t* context)
{
	viddef.context = *context;

	/* update cvars */
	Cvar_SetValue("vid_width", viddef.context.width);
	Cvar_SetValue("vid_height", viddef.context.height);
	Cvar_SetValue("vid_mode", viddef.context.mode);
	Cvar_SetValue("vid_fullscreen", viddef.context.fullscreen);
	Cvar_SetValue("r_multisample", viddef.context.multisample);
	Cvar_SetValue("r_swapinterval", viddef.context.swapinterval);
	vid_stretch->modified = false;
	vid_fullscreen->modified = false;
	vid_mode->modified = false;
	r_multisample->modified = false;
	r_swapinterval->modified = false;

	/* update cache values */
	if (viddef.stretch) {
		viddef.virtualWidth = VID_NORM_WIDTH;
		viddef.virtualHeight = VID_NORM_HEIGHT;
	} else {
		float normRatio = (float) VID_NORM_WIDTH / (float) VID_NORM_HEIGHT;
		float screenRatio = (float) viddef.context.width / (float) viddef.context.height;

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
	viddef.rx = (float)viddef.context.width / viddef.virtualWidth;
	viddef.ry = (float)viddef.context.height / viddef.virtualHeight;
}

bool R_SetMode (void)
{
	bool result;
	viddefContext_t prevCtx;
	viddefContext_t newCtx;
	vidmode_t vidmode;

	Com_Printf("I: setting mode %d\n", vid_mode->integer);

	/* not values we must restitute */
	r_ext_texture_compression->modified = false;
	viddef.stretch = vid_stretch->integer;

	/* store old values if new ones will fail */
	prevCtx = viddef.context;

	/* new values */
	newCtx = viddef.context;
	newCtx.mode = vid_mode->integer;
	newCtx.fullscreen = vid_fullscreen->integer;
	newCtx.multisample = r_multisample->integer;
	newCtx.swapinterval = r_swapinterval->integer;
	if (!VID_GetModeInfo(newCtx.mode, &vidmode)) {
		Com_Printf("I: invalid mode\n");
		Cvar_Set("vid_mode", "-1");
		Cvar_Set("vid_width", "-1");
		Cvar_Set("vid_height", "-1");
		return false;
	}
	Com_Printf("I: try to get display with %ix%i\n", vidmode.width, vidmode.height);
	newCtx.width = vidmode.width;
	newCtx.height = vidmode.height;
	result = R_InitGraphics(&newCtx);
	if (!result) {
		/* failed, revert */
		Com_Printf("Failed to set video mode %dx%d %s.\n",
				newCtx.width, newCtx.height,
				(newCtx.fullscreen ? "fullscreen" : "windowed"));
		result = R_InitGraphics(&prevCtx);
		if (!result)
			return false;
		newCtx = prevCtx;
	}

	R_UpdateVidDef(&newCtx);
	R_ShutdownFBObjects();
	R_InitFBObjects();
	UI_InvalidateStack();
	Com_Printf("I: %dx%d (fullscreen: %s)\n", viddef.context.width, viddef.context.height, viddef.context.fullscreen ? "yes" : "no");
	return true;
}

/**
 * @note SDL_GL_GetProcAddress returns a void*, which is not on all
 * supported platforms the same size as a function pointer.
 * This wrapper is a workaround until SDL is fixed.
 * It is known to produce the "warning: assignment from incompatible pointer type"
 */
static inline uintptr_t R_GetProcAddress (const char* functionName)
{
	return (uintptr_t)SDL_GL_GetProcAddress(functionName);
}

static uintptr_t R_GetProcAddressExt (const char* functionName)
{
	const char* s = strstr(functionName, "###");
	if (s == nullptr) {
		return R_GetProcAddress(functionName);
	}

	const char* replace[] = {"EXT", "OES", "ARB"};
	char targetBuf[128];
	const size_t length = lengthof(targetBuf);
	const size_t replaceNo = lengthof(replace);
	for (size_t i = 0; i < replaceNo; i++) {
		if (Q_strreplace(functionName, "###", replace[i], targetBuf, length)) {
			uintptr_t funcAdr = R_GetProcAddress(targetBuf);
			if (funcAdr != 0)
				return funcAdr;
		}
	}
	Com_Printf("%s not found\n", functionName);
	return 0;
}

/**
 * @brief Checks for an OpenGL extension that was announced via the OpenGL ext string. If the given extension string
 * includes a placeholder (###), several types are checked. Those from the ARB, those that official extensions (EXT),
 * and those from OpenGL ES (OES)
 * @param extension The extension string to check. Might also contain placeholders. E.g. "GL_###_framebuffer_object",
 * GL_ARB_framebuffer_object
 * @return @c true if the extension was announced by the OpenGL driver, @c false otherwise.
 */
static inline bool R_CheckExtension (const char* extension)
{
	bool found;
#ifdef GL_VERSION_ES_CM_1_0
	if(strcmp(extension, "GL_ARB_texture_non_power_of_two") == 0) {
		extension = "GL_OES_texture_npot";
	}
#endif
	const char* s = strstr(extension, "###");
	if (s == nullptr) {
		found = strstr(r_config.extensionsString, extension) != nullptr;
	} else {
		const char* replace[] = {"ARB", "EXT", "OES"};
		char targetBuf[128];
		const size_t length = lengthof(targetBuf);
		const size_t replaceNo = lengthof(replace);
		size_t i;
		for (i = 0; i < replaceNo; i++) {
			if (Q_strreplace(extension, "###", replace[i], targetBuf, length)) {
				if (strstr(r_config.extensionsString, targetBuf) != nullptr) {
					found = true;
					break;
				}
			}
		}
		if (i == replaceNo)
			found = false;
	}

	if (found)
		Com_Printf("found %s\n", extension);
	else
		Com_Printf("%s not found\n", extension);

#ifdef GL_VERSION_ES_CM_1_0
	if (strcmp(extension, "GL_ARB_multitexture") == 0 ||
		strcmp(extension, "GL_ARB_vertex_buffer_object") == 0) {
		found = true;
		Com_Printf("Overriding %s - it is always present in GLES\n", extension);
	}
#endif

	return found;
}

#define R_CheckGLVersion(max, min) (r_config.glVersionMajor > max || (r_config.glVersionMajor == max && r_config.glVersionMinor >= min))

/**
 * @brief Check and load all needed and supported opengl extensions
 * @sa R_Init
 */
static void R_InitExtensions (void)
{
	GLenum err;
	int tmpInteger;

	/* Get OpenGL version.*/
	if(sscanf(r_config.versionString, "%d.%d", &r_config.glVersionMajor, &r_config.glVersionMinor) != 2) {
		const char*  versionNumbers = r_config.versionString; /* GLES reports version as "OpenGL ES 1.1", so we must skip non-numeric symbols first */
		while(*versionNumbers && strchr("0123456789", *versionNumbers) == nullptr) {
			versionNumbers ++;
		}
		if( *versionNumbers )
			sscanf(versionNumbers, "%d.%d", &r_config.glVersionMajor, &r_config.glVersionMinor);
	}
	Com_Printf("OpenGL version detected: %d.%d\n", r_config.glVersionMajor, r_config.glVersionMinor);

	/* multitexture */
	qglActiveTexture = nullptr;
	qglClientActiveTexture = nullptr;

	/* vertex buffer */
	qglGenBuffers = nullptr;
	qglDeleteBuffers = nullptr;
	qglBindBuffer = nullptr;
	qglBufferData = nullptr;

	/* glsl */
	qglCreateShader = nullptr;
	qglDeleteShader = nullptr;
	qglShaderSource = nullptr;
	qglCompileShader = nullptr;
	qglGetShaderiv = nullptr;
	qglGetShaderInfoLog = nullptr;
	qglCreateProgram = nullptr;
	qglDeleteProgram = nullptr;
	qglAttachShader = nullptr;
	qglDetachShader = nullptr;
	qglLinkProgram = nullptr;
	qglUseProgram = nullptr;
	qglGetActiveUniform = nullptr;
	qglGetProgramiv = nullptr;
	qglGetProgramInfoLog = nullptr;
	qglGetUniformLocation = nullptr;
	qglUniform1i = nullptr;
	qglUniform1f = nullptr;
	qglUniform1fv = nullptr;
	qglUniform2fv = nullptr;
	qglUniform3fv = nullptr;
	qglUniform4fv = nullptr;
	qglGetAttribLocation = nullptr;
	qglUniformMatrix4fv = nullptr;

	/* vertex attribute arrays */
	qglEnableVertexAttribArray = nullptr;
	qglDisableVertexAttribArray = nullptr;
	qglVertexAttribPointer = nullptr;

	/* framebuffer objects */
	qglIsRenderbufferEXT = nullptr;
	qglBindRenderbufferEXT = nullptr;
	qglDeleteRenderbuffersEXT = nullptr;
	qglGenRenderbuffersEXT = nullptr;
	qglRenderbufferStorageEXT = nullptr;
	qglRenderbufferStorageMultisampleEXT = nullptr;
	qglGetRenderbufferParameterivEXT = nullptr;
	qglIsFramebufferEXT = nullptr;
	qglBindFramebufferEXT = nullptr;
	qglDeleteFramebuffersEXT = nullptr;
	qglGenFramebuffersEXT = nullptr;
	qglCheckFramebufferStatusEXT = nullptr;
	qglFramebufferTexture1DEXT = nullptr;
	qglFramebufferTexture2DEXT = nullptr;
	qglFramebufferTexture3DEXT = nullptr;
	qglFramebufferRenderbufferEXT = nullptr;
	qglGetFramebufferAttachmentParameterivEXT = nullptr;
	qglGenerateMipmapEXT = nullptr;
	qglDrawBuffers = nullptr;

	/* multitexture */
	if (R_CheckExtension("GL_ARB_multitexture")) {
		qglActiveTexture = (ActiveTexture_t)R_GetProcAddress("glActiveTexture");
		qglClientActiveTexture = (ClientActiveTexture_t)R_GetProcAddress("glClientActiveTexture");
	}

#ifndef GL_VERSION_ES_CM_1_0
	if (R_CheckExtension("GL_ARB_texture_compression")) {
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
#endif

	if (R_CheckExtension("GL_ARB_texture_non_power_of_two")) {
		if (r_ext_nonpoweroftwo->integer) {
			Com_Printf("using GL_ARB_texture_non_power_of_two\n");
			r_config.nonPowerOfTwo = true;
		} else {
			r_config.nonPowerOfTwo = false;
			Com_Printf("ignoring GL_ARB_texture_non_power_of_two\n");
		}
	} else {
		if (R_CheckGLVersion(2, 0)) {
			r_config.nonPowerOfTwo = r_ext_nonpoweroftwo->integer == 1;
		} else {
			r_config.nonPowerOfTwo = false;
		}
	}

	/* anisotropy */
	if (R_CheckExtension("GL_EXT_texture_filter_anisotropic")) {
		if (r_anisotropic->integer) {
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_config.maxAnisotropic);
			R_CheckError();
			if (r_anisotropic->integer > r_config.maxAnisotropic) {
				Com_Printf("max GL_EXT_texture_filter_anisotropic value is %i\n", r_config.maxAnisotropic);
				Cvar_SetValue("r_anisotropic", r_config.maxAnisotropic);
			}

			if (r_config.maxAnisotropic)
				r_config.anisotropic = true;
		}
	}

	if (R_CheckExtension("GL_EXT_texture_lod_bias"))
		r_config.lod_bias = true;

	/* vertex buffer objects */
	if (R_CheckExtension("GL_ARB_vertex_buffer_object")) {
		qglGenBuffers = (GenBuffers_t)R_GetProcAddress("glGenBuffers");
		qglDeleteBuffers = (DeleteBuffers_t)R_GetProcAddress("glDeleteBuffers");
		qglBindBuffer = (BindBuffer_t)R_GetProcAddress("glBindBuffer");
		qglBufferData = (BufferData_t)R_GetProcAddress("glBufferData");
		r_config.maxVertexBufferSize = 256 * 256 * 256; // This is only recommended value, so we don't really care about it, and set some big number.
#ifndef GL_VERSION_ES_CM_1_0
		glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &r_config.maxVertexBufferSize);
#endif
		Com_Printf("using GL_ARB_vertex_buffer_object\nmax vertex buffer size: %i\n", r_config.maxVertexBufferSize);
	}

	/* glsl vertex and fragment shaders and programs */
	if (R_CheckExtension("GL_ARB_fragment_shader")) {
		qglCreateShader = (CreateShader_t)R_GetProcAddress("glCreateShader");
		qglDeleteShader = (DeleteShader_t)R_GetProcAddress("glDeleteShader");
		qglShaderSource = (ShaderSource_t)R_GetProcAddress("glShaderSource");
		qglCompileShader = (CompileShader_t)R_GetProcAddress("glCompileShader");
		qglGetShaderiv = (GetShaderiv_t)R_GetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = (GetShaderInfoLog_t)R_GetProcAddress("glGetShaderInfoLog");
		qglCreateProgram = (CreateProgram_t)R_GetProcAddress("glCreateProgram");
		qglDeleteProgram = (DeleteProgram_t)R_GetProcAddress("glDeleteProgram");
		qglAttachShader = (AttachShader_t)R_GetProcAddress("glAttachShader");
		qglDetachShader = (DetachShader_t)R_GetProcAddress("glDetachShader");
		qglLinkProgram = (LinkProgram_t)R_GetProcAddress("glLinkProgram");
		qglUseProgram = (UseProgram_t)R_GetProcAddress("glUseProgram");
		qglGetActiveUniform = (GetActiveUniforms_t)R_GetProcAddress("glGetActiveUniform");
		qglGetProgramiv = (GetProgramiv_t)R_GetProcAddress("glGetProgramiv");
		qglGetProgramInfoLog = (GetProgramInfoLog_t)R_GetProcAddress("glGetProgramInfoLog");
		qglGetUniformLocation = (GetUniformLocation_t)R_GetProcAddress("glGetUniformLocation");
		qglUniform1i = (Uniform1i_t)R_GetProcAddress("glUniform1i");
		qglUniform1f = (Uniform1f_t)R_GetProcAddress("glUniform1f");
		qglUniform1fv = (Uniform1fv_t)R_GetProcAddress("glUniform1fv");
		qglUniform2fv = (Uniform2fv_t)R_GetProcAddress("glUniform2fv");
		qglUniform3fv = (Uniform3fv_t)R_GetProcAddress("glUniform3fv");
		qglUniform4fv = (Uniform4fv_t)R_GetProcAddress("glUniform4fv");
		qglGetAttribLocation = (GetAttribLocation_t)R_GetProcAddress("glGetAttribLocation");
		qglUniformMatrix4fv = (UniformMatrix4fv_t)R_GetProcAddress("glUniformMatrix4fv");

		/* vertex attribute arrays */
		qglEnableVertexAttribArray = (EnableVertexAttribArray_t)R_GetProcAddress("glEnableVertexAttribArray");
		qglDisableVertexAttribArray = (DisableVertexAttribArray_t)R_GetProcAddress("glDisableVertexAttribArray");
		qglVertexAttribPointer = (VertexAttribPointer_t)R_GetProcAddress("glVertexAttribPointer");
	}

	if (R_CheckExtension("GL_ARB_shading_language_100") || r_config.glVersionMajor >= 2) {
		/* The GL_ARB_shading_language_100 extension was added to core specification since OpenGL 2.0;
		 * it is ideally listed in the extensions for backwards compatibility.  If it isn't there and OpenGL > v2.0
		 * then enable shaders as the implementation supports the shading language!*/
		const char* shadingVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
		sscanf(shadingVersion, "%d.%d", &r_config.glslVersionMajor, &r_config.glslVersionMinor);
		Com_Printf("GLSL version guaranteed to be supported by OpenGL implementation postfixed by vender supplied info: %i.%i\n",
				r_config.glslVersionMajor, r_config.glslVersionMinor);
	} else {
		/* The shading language is not supported.*/
		Com_Printf("GLSL shaders unsupported by OpenGL implementation.\n");
	}

	/* framebuffer objects */
	if (R_CheckExtension("GL_###_framebuffer_object")) {
		qglIsRenderbufferEXT = (IsRenderbufferEXT_t)R_GetProcAddressExt("glIsRenderbuffer###");
		qglBindRenderbufferEXT = (BindRenderbufferEXT_t)R_GetProcAddressExt("glBindRenderbuffer###");
		qglDeleteRenderbuffersEXT = (DeleteRenderbuffersEXT_t)R_GetProcAddressExt("glDeleteRenderbuffers###");
		qglGenRenderbuffersEXT = (GenRenderbuffersEXT_t)R_GetProcAddressExt("glGenRenderbuffers###");
		qglRenderbufferStorageEXT = (RenderbufferStorageEXT_t)R_GetProcAddressExt("glRenderbufferStorage###");
		qglRenderbufferStorageMultisampleEXT = (RenderbufferStorageMultisampleEXT_t)R_GetProcAddressExt("glRenderbufferStorageMultisample###");
		qglGetRenderbufferParameterivEXT = (GetRenderbufferParameterivEXT_t)R_GetProcAddressExt("glGetRenderbufferParameteriv###");
		qglIsFramebufferEXT = (IsFramebufferEXT_t)R_GetProcAddressExt("glIsFramebuffer###");
		qglBindFramebufferEXT = (BindFramebufferEXT_t)R_GetProcAddressExt("glBindFramebuffer###");
		qglDeleteFramebuffersEXT = (DeleteFramebuffersEXT_t)R_GetProcAddressExt("glDeleteFramebuffers###");
		qglGenFramebuffersEXT = (GenFramebuffersEXT_t)R_GetProcAddressExt("glGenFramebuffers###");
		qglCheckFramebufferStatusEXT = (CheckFramebufferStatusEXT_t)R_GetProcAddressExt("glCheckFramebufferStatus###");
		qglFramebufferTexture1DEXT = (FramebufferTexture1DEXT_t)R_GetProcAddressExt("glFramebufferTexture1D###");
		qglFramebufferTexture2DEXT = (FramebufferTexture2DEXT_t)R_GetProcAddressExt("glFramebufferTexture2D###");
		qglFramebufferTexture3DEXT = (FramebufferTexture3DEXT_t)R_GetProcAddressExt("glFramebufferTexture3D###");
		qglFramebufferRenderbufferEXT = (FramebufferRenderbufferEXT_t)R_GetProcAddressExt("glFramebufferRenderbuffer###");
		qglGetFramebufferAttachmentParameterivEXT = (GetFramebufferAttachmentParameterivEXT_t)R_GetProcAddressExt("glGetFramebufferAttachmentParameteriv###");
		qglGenerateMipmapEXT = (GenerateMipmapEXT_t)R_GetProcAddressExt("glGenerateMipmap###");
		qglDrawBuffers = (DrawBuffers_t)R_GetProcAddress("glDrawBuffers");
		qglBlitFramebuffer = (BlitFramebuffer_t)R_GetProcAddress("glBlitFramebuffer");

		if (qglDeleteRenderbuffersEXT && qglDeleteFramebuffersEXT && qglGenFramebuffersEXT && qglBindFramebufferEXT
		 && qglFramebufferTexture2DEXT && qglBindRenderbufferEXT && qglRenderbufferStorageEXT && qglCheckFramebufferStatusEXT) {
#ifdef GL_VERSION_ES_CM_1_0
			r_config.maxDrawBuffers = 1; /* GLES does not support multiple buffers or color attachments, only COLOR_ATTACHMENT0_OES is available */
			r_config.maxColorAttachments = 1;
#else
			glGetIntegerv(GL_MAX_DRAW_BUFFERS, &r_config.maxDrawBuffers);
			glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &r_config.maxColorAttachments);
#endif
			glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &r_config.maxRenderbufferSize);

			r_config.frameBufferObject = true;

			Com_Printf("using GL_ARB_framebuffer_object\n");
			Com_Printf("max draw buffers: %i\n", r_config.maxDrawBuffers);
			Com_Printf("max render buffer size: %i\n", r_config.maxRenderbufferSize);
			Com_Printf("max color attachments: %i\n", r_config.maxColorAttachments);
		} else {
			Com_Printf("skipping GL_ARB_framebuffer_object - not every needed extension is supported\n");
		}

		if (r_config.maxDrawBuffers > 1 && R_CheckExtension("GL_###_draw_buffers")) {
			Com_Printf("using GL_ARB_draw_buffers\n");
			r_config.drawBuffers = true;
		} else {
			r_config.drawBuffers = false;
		}
	} else {
		Com_Printf("Framebuffer objects unsupported by OpenGL implementation.\n");
	}

	r_programs = Cvar_GetOrCreate("r_programs", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "GLSL shaders level: 0 - disabled, 1 - low, 2 - medium, 3 - high");
	r_programs->modified = false;
	Cvar_SetCheckFunction("r_programs", R_CvarPrograms);

	r_glsl_version = Cvar_GetOrCreate("r_glsl_version", "1.10", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "GLSL Version");
	Cvar_SetCheckFunction("r_glsl_version", R_CvarGLSLVersionCheck);

	r_postprocess = Cvar_GetOrCreate("r_postprocess", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate postprocessing shader effects");
	Cvar_SetCheckFunction("r_postprocess", R_CvarPostProcess);

	/* reset gl error state */
	R_CheckError();

	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB, &r_config.maxVertexTextureImageUnits);
	Com_Printf("max supported vertex texture units: %i\n", r_config.maxVertexTextureImageUnits);

	glGetIntegerv(GL_MAX_LIGHTS, &r_config.maxLights);
	Com_Printf("max supported lights: %i\n", r_config.maxLights);

	r_dynamic_lights = Cvar_GetOrCreate("r_dynamic_lights", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Sets max number of GL lightsources to use in shaders");
	Cvar_SetCheckFunction("r_dynamic_lights", R_CvarCheckDynamicLights);

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &r_config.maxTextureUnits);
	Com_Printf("max texture units: %i\n", r_config.maxTextureUnits);
	if (r_config.maxTextureUnits < 2)
		Com_Error(ERR_FATAL, "You need at least 2 texture units to run " GAME_TITLE);

	glGetIntegerv(GL_MAX_TEXTURE_COORDS, &r_config.maxTextureCoords);
	Com_Printf("max texture coords: %i\n", r_config.maxTextureCoords);
	r_config.maxTextureCoords = std::max(r_config.maxTextureUnits, r_config.maxTextureCoords);

	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &r_config.maxVertexAttribs);
	Com_Printf("max vertex attributes: %i\n", r_config.maxVertexAttribs);

#ifdef GL_VERSION_ES_CM_1_0
	glGetIntegerv(GL_MAX_VARYING_VECTORS, &tmpInteger);
	Com_Printf("max varying floats: %i\n", tmpInteger * 4);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &tmpInteger);
	Com_Printf("max fragment uniform components: %i\n", tmpInteger * 4);
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &tmpInteger);
	Com_Printf("max vertex uniform components: %i\n", tmpInteger * 4);
#else
	glGetIntegerv(GL_MAX_VARYING_FLOATS, &tmpInteger);
	Com_Printf("max varying floats: %i\n", tmpInteger);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &tmpInteger);
	Com_Printf("max fragment uniform components: %i\n", tmpInteger);
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &tmpInteger);
	Com_Printf("max vertex uniform components: %i\n", tmpInteger);
#endif

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

	if (r_config.maxTextureSize > 4096 && R_ImageExists("pics/geoscape/%s/map_earth_season_00", "high")) {
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
}

/**
 * @brief We need at least opengl version 1.2.1
 */
static inline void R_EnforceVersion (void)
{
	int maj, min, rel;

	sscanf(r_config.versionString, "%d.%d.%d ", &maj, &min, &rel);

#ifndef GL_VERSION_ES_CM_1_0
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
#endif
}

/**
 * @brief Searches vendor and renderer GL strings for the given vendor id
 */
static bool R_SearchForVendor (const char* vendor)
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
#else
	if (!Q_strcasecmp((const char*)glGetString(GL_RENDERER), "Software Rasterizer"))
		Com_Error(ERR_FATAL, "No hardware acceleration detected.\n"
			"Update your graphic card drivers.");
#endif
	if (R_SearchForVendor("Intel")) {
		Com_Printf("You might want to activate texture compression if you have visual artifacts.\n");
		if (r_intel_hack->integer) {
			/* HACK: */
			Com_Printf("Disabling shaders for Intel chips - see cvar r_intel_hack\n");
			Cvar_Set("r_programs", "0");
			if (r_intel_hack->integer == 1) {
				if (r_maxtexres->integer > INTEL_TEXTURE_RESOLUTION) {
					Com_Printf("Set max. texture resolution to %i - see cvar r_intel_hack\n", INTEL_TEXTURE_RESOLUTION);
					Cvar_SetValue("r_maxtexres", INTEL_TEXTURE_RESOLUTION);
				}
			}
		}
		r_config.hardwareType = GLHW_INTEL;
	} else if (R_SearchForVendor("NVIDIA")) {
		r_config.hardwareType = GLHW_NVIDIA;
	} else if (R_SearchForVendor("ATI") || R_SearchForVendor("Advanced Micro Devices") || R_SearchForVendor("AMD")) {
		r_config.hardwareType = GLHW_ATI;
	} else if (R_SearchForVendor("mesa") || R_SearchForVendor("gallium") || R_SearchForVendor("nouveau")) {
		r_config.hardwareType = GLHW_MESA;
	} else {
		r_config.hardwareType = GLHW_GENERIC;
	}
	/* disable intel hack for non intel cards */
	if (!R_SearchForVendor("Intel")) {
		Cvar_Set("r_intel_hack", "0");
		Cvar_Set("r_vendor_non_intel", "1");
	}
}

bool R_Init (void)
{
	R_RegisterSystemVars();

	OBJZERO(r_state);
	OBJZERO(r_locals);
	OBJZERO(r_config);

	/* some config default values */
	r_config.gl_solid_format = GL_RGB;
	r_config.gl_alpha_format = GL_RGBA;
	r_config.gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
	r_config.gl_filter_max = GL_LINEAR;
	r_config.maxTextureSize = 256;

	/* initialize OS-specific parts of OpenGL */
	if (!Rimp_Init())
		return false;

	/* get our various GL strings */
	r_config.vendorString = (const char*)glGetString(GL_VENDOR);
	r_config.rendererString = (const char*)glGetString(GL_RENDERER);
	r_config.versionString = (const char*)glGetString(GL_VERSION);
	r_config.extensionsString = (const char*)glGetString(GL_EXTENSIONS);
	R_Strings_f();

	/* sanity checks and card specific hacks */
	R_VerifyDriver();
	R_EnforceVersion();

	R_RegisterImageVars();

	/* prevent reloading of some rendering cvars */
	Cvar_ClearVars(CVAR_R_MASK);

	R_ModelInit();
	R_InitExtensions();
	R_SetDefaultState();
	R_InitPrograms();
	R_InitImages();
	R_InitMiscTexture();
	R_DrawInitLocal();
	R_SphereInit();
	R_FontInit();
	R_InitFBObjects();
	R_UpdateDefaultMaterial("","","", nullptr);
	r_battlescapeWeather.setDefaults();

	R_CheckError();

	return true;
}

/**
 * @sa R_Init
 */
void R_Shutdown (void)
{
	Cmd_TableRemoveList(r_commands);

	R_ShutdownThreads();

	R_ModelShutdown();
	R_ShutdownImages();

	R_ShutdownPrograms();
	R_FontShutdown();
	R_ShutdownFBObjects();

	/* shut down OS specific OpenGL stuff like contexts, etc. */
	Rimp_Shutdown();
}
