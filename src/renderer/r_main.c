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
#include "r_error.h"

GLenum gl_texture0, gl_texture1, gl_texture2, gl_texture3;

refdef_t refdef;

rconfig_t r_config;
rstate_t r_state;

image_t *r_notexture;			/* use for bad textures */
image_t *r_particletexture;		/* little dot for particles */

entity_t *currententity;
model_t *currentmodel;

cBspPlane_t frustum[4];

int r_framecount;				/* used for dlight push checking */

int c_brush_polys, c_alias_polys;

float v_blend[4];				/* final blending color */

/* entity transform */
transform_t trafo[MAX_ENTITIES];

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

static cvar_t *r_norefresh;
static cvar_t *r_drawentities;
static cvar_t *r_speeds;
cvar_t *r_drawworld;
cvar_t *r_nocull;
cvar_t *r_isometric;
cvar_t *r_lerpmodels;
cvar_t *r_anisotropic;
cvar_t *r_texture_lod;			/* lod_bias */

cvar_t *r_screenshot;
cvar_t *r_screenshot_jpeg_quality;

cvar_t *r_ext_multitexture;
cvar_t *r_ext_combine;
cvar_t *r_ext_lockarrays;
cvar_t *r_ext_texture_compression;
cvar_t *r_ext_s3tc_compression;

cvar_t *r_3dmapradius;

cvar_t *r_checkerror;
cvar_t *r_drawbuffer;
cvar_t *r_driver;
cvar_t *r_lightmap;
cvar_t *r_shadows;
cvar_t *r_shadow_debug_volume;
cvar_t *r_shadow_debug_shade;
cvar_t *r_ati_separate_stencil;
cvar_t *r_stencil_two_side;
cvar_t *r_bitdepth;

cvar_t *r_drawclouds;
cvar_t *r_imagefilter;
cvar_t *r_dynamic;
cvar_t *r_soften;
cvar_t *r_modulate;
cvar_t *r_round_down;
cvar_t *r_picmip;
cvar_t *r_maxtexres;
cvar_t *r_showtris;
cvar_t *r_flashblend;
cvar_t *r_swapinterval;
cvar_t *r_texturemode;
cvar_t *r_texturealphamode;
cvar_t *r_texturesolidmode;
cvar_t *r_wire;
cvar_t *r_fog;
cvar_t *r_showbox;
cvar_t *r_intensity;

/**
 * @brief Prints some OpenGL strings
 */
static void R_Strings_f (void)
{
	Com_Printf("GL_VENDOR: %s\n", r_config.vendor_string);
	Com_Printf("GL_RENDERER: %s\n", r_config.renderer_string);
	Com_Printf("GL_VERSION: %s\n", r_config.version_string);
	Com_Printf("MODE: %i, %d x %d FULLSCREEN: %i\n", vid_mode->integer, viddef.width, viddef.height, vid_fullscreen->integer);
	Com_Printf("GL_EXTENSIONS: %s\n", r_config.extensions_string);
	Com_Printf("GL_MAX_TEXTURE_SIZE: %d\n", r_config.maxTextureSize);
}

/**
 * @sa R_DrawShadowVolume
 * @sa R_DrawShadow
 */
static void R_CastShadow (void)
{
	int i;

	/* no shadows at all */
	if (!r_shadows->integer)
		return;

	for (i = 0; i < refdef.num_entities; i++) {
		currententity = &refdef.entities[i];
		currentmodel = currententity->model;
		if (!currentmodel)
			continue;
		if (currentmodel->type != mod_alias_md2 && currentmodel->type != mod_alias_md3)
			continue;

		if (r_shadows->integer == 2)
			R_DrawShadowVolume(currententity);
		else if (r_shadows->integer == 1)
			R_DrawShadow(currententity);
	}
}

/**
 * @brief Returns true if the box is completely outside the frustum
 */
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int i;

	if (r_nocull->integer)
		return qfalse;

	for (i = 0; i < 4; i++)
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return qtrue;
	return qfalse;
}


void R_RotateForEntity (entity_t * e)
{
	qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef(e->angles[1], 0, 0, 1);
	qglRotatef(-e->angles[0], 0, 1, 0);
	qglRotatef(-e->angles[2], 1, 0, 0);
}


void R_InterpolateTransform (animState_t * as, int numframes, float *tag, float *interpolated)
{
	float *current, *old;
	float bl, fl;
	int i;

	/* range check */
	if (as->frame >= numframes || as->frame < 0)
		as->frame = 0;
	if (as->oldframe >= numframes || as->oldframe < 0)
		as->oldframe = 0;

	/* calc relevant values */
	current = tag + as->frame * 16;
	old = tag + as->oldframe * 16;
	bl = as->backlerp;
	fl = 1.0 - as->backlerp;

	/* right on a frame? */
	if (bl == 0.0) {
		memcpy(interpolated, current, sizeof(float) * 16);
		return;
	}
	if (bl == 1.0) {
		memcpy(interpolated, old, sizeof(float) * 16);
		return;
	}

	/* interpolate */
	for (i = 0; i < 16; i++)
		interpolated[i] = fl * current[i] + bl * old[i];

	/* normalize */
	for (i = 0; i < 3; i++)
		VectorNormalize(interpolated + i * 4);
}


static float *R_CalcTransform (entity_t * e)
{
	vec3_t angles;
	transform_t *t;
	float *mp;
	float mt[16], mc[16];
	int i;

	/* check if this entity is already transformed */
	t = &trafo[e - refdef.entities];

	if (t->processing)
		Sys_Error("Ring in entity transformations!\n");

	if (t->done)
		return t->matrix;

	/* process this matrix */
	t->processing = qtrue;
	mp = NULL;

	/* do parent object trafos first */
	if (e->tagent) {
		/* parent trafo */
		mp = R_CalcTransform(e->tagent);

		/* tag trafo */
		if (e->tagent->model && e->tagent->model->alias.tagdata) {
			dtag_t *taghdr;
			char *name;
			float *tag;
			float interpolated[16];

			taghdr = (dtag_t *) e->tagent->model->alias.tagdata;

			/* find the right tag */
			name = (char *) taghdr + taghdr->ofs_names;
			for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME)
				if (!strcmp(name, e->tagname)) {
					/* found the tag (matrix) */
					tag = (float *) ((byte *) taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					/* do interpolation */
					R_InterpolateTransform(&e->tagent->as, taghdr->num_frames, tag, interpolated);

					/* transform */
					GLMatrixMultiply(mp, interpolated, mt);
					mp = mt;

					break;
				}
		}
	}

	/* fill in edge values */
	mc[3] = mc[7] = mc[11] = 0.0;
	mc[15] = 1.0;

	/* add rotation */
	VectorCopy(e->angles, angles);
/*	angles[YAW] = -angles[YAW]; */

	AngleVectors(angles, &mc[0], &mc[4], &mc[8]);

	/* add translation */
	mc[12] = e->origin[0];
	mc[13] = e->origin[1];
	mc[14] = e->origin[2];

	/* flip an axis */
	VectorScale(&mc[4], -1, &mc[4]);

	/* combine trafos */
	if (mp)
		GLMatrixMultiply(mp, mc, t->matrix);
	else
		memcpy(t->matrix, mc, sizeof(float) * 16);

	/* we're done */
	t->done = qtrue;
	t->processing = qfalse;

	return t->matrix;
}


static void R_TransformEntitiesOnList (void)
{
	int i;

	if (!r_drawentities->integer)
		return;

	/* clear flags */
	for (i = 0; i < refdef.num_entities; i++) {
		trafo[i].done = qfalse;
		trafo[i].processing = qfalse;
	}

	/* calculate all transformations */
	/* possibly recursive */
	for (i = 0; i < refdef.num_entities; i++)
		R_CalcTransform(&refdef.entities[i]);
}


static void R_DrawEntitiesOnList (void)
{
	int i;

	if (!r_drawentities->integer)
		return;

	/* draw non-transparent first */

	for (i = 0; i < refdef.num_entities; i++) {
		currententity = &refdef.entities[i];

		/* find out if and how an entity should be drawn */
		if (currententity->flags & RF_TRANSLUCENT)
			continue;			/* solid */

		if (currententity->flags & RF_BOX)
			R_DrawBox(currententity);
		else {
			currentmodel = currententity->model;
			if (!currentmodel) {
				R_ModDrawNullModel();
				continue;
			}
			switch (currentmodel->type) {
			case mod_alias_md2:
				/* MD2 model */
				R_DrawAliasMD2Model(currententity);
				break;
			case mod_alias_md3:
				/* MD3 model */
				R_DrawAliasMD3Model(currententity);
				break;
			case mod_brush:
				/* draw things like func_breakable */
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			case mod_obj:
				R_DrawOBJModel(currententity);
				break;
			default:
				Sys_Error("Bad %s modeltype: %i", currentmodel->name, currentmodel->type);
				break;
			}
		}
	}

	/* draw transparent entities */
	/* we could sort these if it ever becomes a problem... */
	qglDepthMask(0);			/* no z writes */
	for (i = 0; i < refdef.num_entities; i++) {
		currententity = &refdef.entities[i];
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;			/* solid */

		if (currententity->flags & RF_BOX)
			R_DrawBox(currententity);
		else {
			currentmodel = currententity->model;
			if (!currentmodel) {
				R_ModDrawNullModel();
				continue;
			}
			switch (currentmodel->type) {
			case mod_alias_md2:
				/* MD2 model */
				R_DrawAliasMD2Model(currententity);
				break;
			case mod_alias_md3:
				/* MD3 model */
				R_DrawAliasMD3Model(currententity);
				break;
			case mod_brush:
				R_DrawBrushModel(currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(currententity);
				break;
			default:
				Sys_Error("Bad %s modeltype: %i", currentmodel->name, currentmodel->type);
				break;
			}
		}
	}
	qglDepthMask(1);			/* back to writing */
}

static int SignbitsForPlane (cBspPlane_t * out)
{
	int bits, j;

	/* for fast box on planeside test */
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

static void R_SetFrustum (void)
{
	int i;

	if (r_isometric->integer) {
		/* 4 planes of a cube */
		VectorScale(vright, +1, frustum[0].normal);
		VectorScale(vright, -1, frustum[1].normal);
		VectorScale(vup, +1, frustum[2].normal);
		VectorScale(vup, -1, frustum[3].normal);

		for (i = 0; i < 4; i++) {
			frustum[i].type = PLANE_ANYZ;
			frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
			frustum[i].signbits = SignbitsForPlane(&frustum[i]);
		}
		frustum[0].dist -= 10 * refdef.fov_x;
		frustum[1].dist -= 10 * refdef.fov_x;
		frustum[2].dist -= 10 * refdef.fov_x * ((float) refdef.height / refdef.width);
		frustum[3].dist -= 10 * refdef.fov_x * ((float) refdef.height / refdef.width);
	} else {
		/* rotate VPN right by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - refdef.fov_x / 2));
		/* rotate VPN left by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - refdef.fov_x / 2);
		/* rotate VPN up by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - refdef.fov_y / 2);
		/* rotate VPN down by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - refdef.fov_y / 2));

		for (i = 0; i < 4; i++) {
			frustum[i].type = PLANE_ANYZ;
			frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
			frustum[i].signbits = SignbitsForPlane(&frustum[i]);
		}
	}
}

static void R_SetupFrame (void)
{
	int i;

	r_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(refdef.vieworg, r_origin);

	AngleVectors(refdef.viewangles, vpn, vright, vup);

	for (i = 0; i < 4; i++)
		v_blend[i] = refdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (refdef.rdflags & RDF_NOWORLDMODEL) {
		qglEnable(GL_SCISSOR_TEST);
		qglScissor(refdef.x, viddef.height - refdef.height - refdef.y, refdef.width, refdef.height);
		R_CheckError();
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		R_CheckError();
		qglDisable(GL_SCISSOR_TEST);
	}
}

static void R_Clear (void)
{
	qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	R_CheckError();
	qglDepthFunc(GL_LEQUAL);
	R_CheckError();

	qglDepthRange(0, 1);
	R_CheckError();

	if (r_shadows->integer == 2) {
		/* set the reference stencil value */
		qglClearStencil(1);
		R_CheckError();
		/* reset stencil buffer */
		qglClear(GL_STENCIL_BUFFER_BIT);
		R_CheckError();
	}
}

static void R_Flash (void)
{
	R_ShadowBlend();
}

static void R_RenderView (void)
{
	if (r_norefresh->integer)
		return;

#if 0
	if (!r_worldmodel && !(refdef.rdflags & RDF_NOWORLDMODEL))
		Sys_Error("R_RenderView: NULL worldmodel");
#endif

	if (r_speeds->integer) {
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL3D();

	if (r_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* draw brushes on current worldlevel */
	R_DrawLevelBrushes();
	R_DrawTriangleOutlines();

	R_TransformEntitiesOnList();
	R_DrawEntitiesOnList();

	if (r_shadows->integer == 2)
		R_CastShadow();

	if (r_wire->integer)
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_DrawAlphaSurfaces();

	if (r_shadows->integer == 1)
		R_CastShadow();

	R_DrawPtls();

	R_RenderDlights();

	R_Flash();
}

/**
 * @sa R_RenderFrame
 * @sa R_EndFrame
 */
void R_BeginFrame (void)
{
	/* change modes if necessary */
	if (vid_mode->modified || vid_fullscreen->modified) {
		R_SetMode();
#ifdef _WIN32
		VID_Restart_f();
#endif
	}
	/* we definitly need a restart here */
	if (r_ext_texture_compression->modified)
		VID_Restart_f();

	if (r_anisotropic->modified) {
		if (r_anisotropic->integer > r_state.maxAnisotropic) {
			Com_Printf("...max GL_EXT_texture_filter_anisotropic value is %i\n", r_state.maxAnisotropic);
			Cvar_SetValue("r_anisotropic", r_state.maxAnisotropic);
		}
		/*R_UpdateAnisotropy();*/
		r_anisotropic->modified = qfalse;
	}

	/* draw buffer stuff */
	if (r_drawbuffer->modified) {
		r_drawbuffer->modified = qfalse;

		if (Q_stricmp(r_drawbuffer->string, "GL_FRONT") == 0)
			qglDrawBuffer(GL_FRONT);
		else
			qglDrawBuffer(GL_BACK);
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

	R_SetupGL2D();

	/* clear screen if desired */
	R_Clear();
}

/**
 * @sa R_BeginFrame
 * @sa R_EndFrame
 */
void R_RenderFrame (void)
{
	/* render the view in 3d mode */
	R_RenderView();

	/* go back into 2D mode for hud and the like */
	R_SetupGL2D();
}

/**
 * @sa R_RenderFrame
 * @sa R_BeginFrame
 */
void R_EndFrame (void)
{
	float g;

	if (vid_gamma->modified) {
		g = vid_gamma->value;

		if (g < 0.1)
			g = 0.1;
		if (g > 3.0)
			g = 3.0;

		SDL_SetGamma(g, g, g);

		Cvar_SetValue("vid_gamma", g);
		vid_gamma->modified = qfalse;
	}
	SDL_GL_SwapBuffers();
}

void R_TakeVideoFrame (int w, int h, byte * captureBuffer, byte * encodeBuffer, qboolean motionJpeg)
{
	size_t frameSize;
	int i;

	qglReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, captureBuffer);
	R_CheckError();

	if (motionJpeg) {
		frameSize = R_SaveJPGToBuffer(encodeBuffer, 90, w, h, captureBuffer);
		CL_WriteAVIVideoFrame(encodeBuffer, frameSize);
	} else {
		frameSize = w * h;

		/* Pack to 24bpp and swap R and B */
		for (i = 0; i < frameSize; i++) {
			encodeBuffer[i * 3]     = captureBuffer[i * 4 + 2];
			encodeBuffer[i * 3 + 1] = captureBuffer[i * 4 + 1];
			encodeBuffer[i * 3 + 2] = captureBuffer[i * 4];
		}
		CL_WriteAVIVideoFrame(encodeBuffer, frameSize * 3);
	}
}

static const cmdList_t r_commands[] = {
	{"imagelist", R_ImageList_f, NULL},
	{"fontcachelist", R_FontListCache_f, NULL},
	{"screenshot", R_ScreenShot_f, "Take a screenshot"},
	{"modellist", R_ModModellist_f, NULL},
	{"r_strings", R_Strings_f, NULL},

	{NULL, NULL, NULL}
};

static void R_Register (void)
{
	const cmdList_t *commands;

	r_norefresh = Cvar_Get("r_norefresh", "0", 0, "Fix the screen to the last thing you saw. Only used for debugging.");
	r_drawentities = Cvar_Get("r_drawentities", "1", 0, NULL);
	r_drawworld = Cvar_Get("r_drawworld", "1", 0, NULL);
	r_isometric = Cvar_Get("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");
	r_lerpmodels = Cvar_Get("r_lerpmodels", "1", 0, NULL);
	r_nocull = Cvar_Get("r_nocull", "0", 0, NULL);
	r_speeds = Cvar_Get("r_speeds", "0", 0, NULL);
	r_anisotropic = Cvar_Get("r_anisotropic", "1", CVAR_ARCHIVE, NULL);
	r_texture_lod = Cvar_Get("r_texture_lod", "0", CVAR_ARCHIVE, NULL);
	r_bitdepth = Cvar_Get("r_bitdepth", "24", CVAR_ARCHIVE, "16 or 24 - bitdepth of the display");

	r_screenshot = Cvar_Get("r_screenshot", "jpg", CVAR_ARCHIVE, "png, jpg or tga are valid screenshot formats");
	r_screenshot_jpeg_quality = Cvar_Get("r_screenshot_jpeg_quality", "75", CVAR_ARCHIVE, "jpeg quality in percent for jpeg screenshots");

	r_3dmapradius = Cvar_Get("r_3dmapradius", "8192.0", CVAR_NOSET, "3D geoscape radius");

	r_modulate = Cvar_Get("r_modulate", "1", CVAR_ARCHIVE, NULL);
	r_checkerror = Cvar_Get("r_checkerror", "0", CVAR_ARCHIVE, "Check for opengl errors");
	r_lightmap = Cvar_Get("r_lightmap", "0", 0, NULL);
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE, NULL);
	r_shadow_debug_volume = Cvar_Get("r_shadow_debug_volume", "0", CVAR_ARCHIVE, NULL);
	r_shadow_debug_shade = Cvar_Get("r_shadow_debug_shade", "0", CVAR_ARCHIVE, NULL);
	r_ati_separate_stencil = Cvar_Get("r_ati_separate_stencil", "1", CVAR_ARCHIVE, NULL);
	r_stencil_two_side = Cvar_Get("r_stencil_two_side", "1", CVAR_ARCHIVE, NULL);
	r_drawclouds = Cvar_Get("r_drawclouds", "0", CVAR_ARCHIVE, NULL);
	r_imagefilter = Cvar_Get("r_imagefilter", "1", CVAR_ARCHIVE, NULL);
	r_dynamic = Cvar_Get("r_dynamic", "1", 0, "Render dynamic lightmaps");
	r_soften = Cvar_Get("r_soften", "1", 0, "Apply blur to lightmap");
	r_round_down = Cvar_Get("r_round_down", "1", 0, NULL);
	r_picmip = Cvar_Get("r_picmip", "0", 0, NULL);
	r_maxtexres = Cvar_Get("r_maxtexres", "2048", CVAR_ARCHIVE, NULL);
	r_showtris = Cvar_Get("r_showtris", "0", 0, NULL);
	r_flashblend = Cvar_Get("r_flashblend", "0", 0, "Controls the way dynamic lights are drawn");
	r_driver = Cvar_Get("r_driver", "", CVAR_ARCHIVE, "You can define the opengl driver you want to use - empty if you want to use the system default");
	r_texturemode = Cvar_Get("r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, NULL);
	r_texturealphamode = Cvar_Get("r_texturealphamode", "default", CVAR_ARCHIVE, NULL);
	r_texturesolidmode = Cvar_Get("r_texturesolidmode", "default", CVAR_ARCHIVE, NULL);
	r_wire = Cvar_Get("r_wire", "0", 0, "Draw the scene in wireframe mode");
	r_fog = Cvar_Get("r_fog", "1", CVAR_ARCHIVE, NULL);
	r_showbox = Cvar_Get("r_showbox", "0", CVAR_ARCHIVE, "Shows model bounding box");
	r_intensity = Cvar_Get("r_intensity", "2", CVAR_ARCHIVE, NULL);

	r_ext_multitexture = Cvar_Get("r_ext_multitexture", "1", CVAR_ARCHIVE, NULL);
	r_ext_combine = Cvar_Get("r_ext_combine", "1", CVAR_ARCHIVE, NULL);
	r_ext_lockarrays = Cvar_Get("r_ext_lockarrays", "0", CVAR_ARCHIVE, NULL);
	r_ext_texture_compression = Cvar_Get("r_ext_texture_compression", "0", CVAR_ARCHIVE, NULL);
	r_ext_s3tc_compression = Cvar_Get("r_ext_s3tc_compression", "1", CVAR_ARCHIVE, NULL);

	r_drawbuffer = Cvar_Get("r_drawbuffer", "GL_BACK", 0, NULL);
	r_swapinterval = Cvar_Get("r_swapinterval", "1", CVAR_ARCHIVE, NULL);

	for (commands = r_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function, commands->description);
}

qboolean R_SetMode (void)
{
	Com_Printf("I: setting mode %d:", vid_mode->integer);

	/* store old values if new ones will fail */
	viddef.prev_width = viddef.width;
	viddef.prev_height = viddef.height;
	viddef.prev_fullscreen = viddef.fullscreen;
	viddef.prev_mode = viddef.mode;

	/* new values */
	viddef.mode = vid_mode->integer;
	viddef.fullscreen = vid_fullscreen->integer;
	if (!VID_GetModeInfo()) {
		Com_Printf(" invalid mode\n");
		return qfalse;
	}
	viddef.rx = (float)viddef.width / VID_NORM_WIDTH;
	viddef.ry = (float)viddef.height / VID_NORM_HEIGHT;
	Com_Printf(" %dx%d (fullscreen: %s)\n", viddef.width, viddef.height, viddef.fullscreen ? "yes" : "no");

	if (R_InitGraphics())
		return qtrue;

	Com_Printf("Failed to set video mode %dx%d %s.\n",
			viddef.width, viddef.height,
			(vid_fullscreen->integer ? "fullscreen" : "windowed"));

	Cvar_SetValue("vid_width", viddef.prev_width);  /* failed, revert */
	Cvar_SetValue("vid_height", viddef.prev_height);
	Cvar_SetValue("vid_mode", viddef.prev_mode);
	Cvar_SetValue("vid_fullscreen", viddef.prev_fullscreen);

	viddef.mode = vid_mode->integer;
	viddef.fullscreen = vid_fullscreen->integer;
	if (!VID_GetModeInfo())
		return qfalse;
	viddef.rx = (float)viddef.width / VID_NORM_WIDTH;
	viddef.ry = (float)viddef.height / VID_NORM_HEIGHT;

	return R_InitGraphics();
}

/**
 * @brief Check and load all needed and supported opengl extensions
 * @sa R_Init
 */
static void R_InitExtension (void)
{
	int aniso_level, max_aniso;
	int size;
	GLenum err;

	/* grab extensions */
	if (strstr(r_config.extensions_string, "GL_EXT_compiled_vertex_array") || strstr(r_config.extensions_string, "GL_SGI_compiled_vertex_array")) {
		if (r_ext_lockarrays->integer) {
			Com_Printf("enabling GL_EXT_LockArrays\n");
			qglLockArraysEXT = SDL_GL_GetProcAddress("glLockArraysEXT");
			qglUnlockArraysEXT = SDL_GL_GetProcAddress("glUnlockArraysEXT");
		} else
			Com_Printf("ignoring GL_EXT_LockArrays\n");
	} else
		Com_Printf("GL_EXT_compiled_vertex_array not found\n");

	if (strstr(r_config.extensions_string, "GL_ARB_multitexture")) {
		if (r_ext_multitexture->integer) {
			Com_Printf("using GL_ARB_multitexture\n");
			qglMultiTexCoord2fARB = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
			qglActiveTextureARB = SDL_GL_GetProcAddress("glActiveTextureARB");
			qglClientActiveTextureARB = SDL_GL_GetProcAddress("glClientActiveTextureARB");
			gl_texture0 = GL_TEXTURE0_ARB;
			gl_texture1 = GL_TEXTURE1_ARB;
			gl_texture2 = GL_TEXTURE2_ARB;
			gl_texture3 = GL_TEXTURE3_ARB;
		} else
			Com_Printf("ignoring GL_ARB_multitexture\n");
	} else
		Com_Printf("GL_ARB_multitexture not found\n");

	if (strstr(r_config.extensions_string, "GL_EXT_texture_env_combine") || strstr(r_config.extensions_string, "GL_ARB_texture_env_combine")) {
		if (r_ext_combine->integer) {
			Com_Printf("using GL_EXT_texture_env_combine\n");
			r_config.envCombine = GL_COMBINE_EXT;
		} else {
			Com_Printf("ignoring EXT_texture_env_combine\n");
			r_config.envCombine = 0;
		}
	} else {
		Com_Printf("GL_EXT_texture_env_combine not found\n");
		r_config.envCombine = 0;
	}

	if (strstr(r_config.extensions_string, "GL_SGIS_multitexture")) {
		if (qglActiveTextureARB)
			Com_Printf("GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n");
		else if (r_ext_multitexture->integer) {
			Com_Printf("using GL_SGIS_multitexture\n");
			qglMultiTexCoord2fARB = SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
			qglSelectTextureSGIS = SDL_GL_GetProcAddress("glSelectTextureSGIS");
			gl_texture0 = GL_TEXTURE0_SGIS;
			gl_texture1 = GL_TEXTURE1_SGIS;
			gl_texture2 = GL_TEXTURE2_SGIS;
			gl_texture3 = GL_TEXTURE3_SGIS;
		} else
			Com_Printf("ignoring GL_SGIS_multitexture\n");
	} else
		Com_Printf("GL_SGIS_multitexture not found\n");

	if (strstr(r_config.extensions_string, "GL_ARB_texture_compression")) {
		if (r_ext_texture_compression->integer) {
			Com_Printf("using GL_ARB_texture_compression\n");
			if (r_ext_s3tc_compression->integer && strstr(r_config.extensions_string, "GL_EXT_texture_compression_s3tc")) {
/*				Com_Printf("with s3tc compression\n"); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			} else {
/*				Com_Printf("without s3tc compression\n"); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_ARB;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_ARB;
			}
		} else {
			Com_Printf("ignoring GL_ARB_texture_compression\n");
			gl_compressed_solid_format = 0;
			gl_compressed_alpha_format = 0;
		}
	} else {
		Com_Printf("GL_ARB_texture_compression not found\n");
		gl_compressed_solid_format = 0;
		gl_compressed_alpha_format = 0;
	}

	/* anisotropy */
	r_state.anisotropic = qfalse;

	qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
	R_CheckError();
	r_state.maxAnisotropic = max_aniso;
	if (r_anisotropic->integer > max_aniso) {
		Com_Printf("max GL_EXT_texture_filter_anisotropic value is %i\n", max_aniso);
		Cvar_SetValue("r_anisotropic", max_aniso);
	}

	aniso_level = r_anisotropic->integer;
	if (strstr(r_config.extensions_string, "GL_EXT_texture_filter_anisotropic")) {
		if (!r_anisotropic->integer)
			Com_Printf("ignoring GL_EXT_texture_filter_anisotropic\n");
		else {
			Com_Printf("using GL_EXT_texture_filter_anisotropic [%2i max] [%2i selected]\n", max_aniso, aniso_level);
			r_state.anisotropic = qtrue;
		}
	} else {
		Com_Printf("GL_EXT_texture_filter_anisotropic not found\n");
		Cvar_Set("r_anisotropic", "0");
		r_state.maxAnisotropic = 0;
	}

	if (strstr(r_config.extensions_string, "GL_EXT_texture_lod_bias")) {
		Com_Printf("using GL_EXT_texture_lod_bias\n");
		r_state.lod_bias = qtrue;
	} else {
		Com_Printf("GL_EXT_texture_lod_bias not found\n");
		r_state.lod_bias = qfalse;
	}

	if (strstr(r_config.extensions_string, "GL_EXT_stencil_wrap")) {
		Com_Printf("using GL_EXT_stencil_wrap\n");
		r_state.stencil_wrap = qtrue;
	} else {
		Com_Printf("GL_EXT_stencil_wrap not found\n");
		r_state.stencil_wrap = qfalse;
	}

	if (strstr(r_config.extensions_string, "GL_EXT_fog_coord")) {
		Com_Printf("using GL_EXT_fog_coord\n");
		r_state.fog_coord = qtrue;
	} else {
		Com_Printf("GL_EXT_fog_coord not found\n");
		r_state.fog_coord = qfalse;
	}

	r_state.glsl_program = qfalse;
	r_state.arb_fragment_program = qfalse;
#ifdef HAVE_SHADERS
	if (strstr(r_config.extensions_string, "GL_ARB_fragment_program")) {
		Com_Printf("using GL_ARB_fragment_program\n");
		r_state.arb_fragment_program = qtrue;

		qglProgramStringARB = SDL_GL_GetProcAddress("glProgramStringARB");
		qglBindProgramARB = SDL_GL_GetProcAddress("glBindProgramARB");
		qglDeleteProgramsARB = SDL_GL_GetProcAddress("glDeleteProgramsARB");
		qglGenProgramsARB = SDL_GL_GetProcAddress("glGenProgramsARB");
		qglProgramEnvParameter4dARB = SDL_GL_GetProcAddress("glProgramEnvParameter4dARB");
		qglProgramEnvParameter4dvARB = SDL_GL_GetProcAddress("glProgramEnvParameter4dvARB");
		qglProgramEnvParameter4fARB = SDL_GL_GetProcAddress("glProgramEnvParameter4fARB");
		qglProgramEnvParameter4fvARB = SDL_GL_GetProcAddress("glProgramEnvParameter4fvARB");
		qglProgramLocalParameter4dARB = SDL_GL_GetProcAddress("glProgramLocalParameter4dARB");
		qglProgramLocalParameter4dvARB = SDL_GL_GetProcAddress("glProgramLocalParameter4dvARB");
		qglProgramLocalParameter4fARB = SDL_GL_GetProcAddress("glProgramLocalParameter4fARB");
		qglProgramLocalParameter4fvARB = SDL_GL_GetProcAddress("glProgramLocalParameter4fvARB");
		qglGetProgramEnvParameterdvARB = SDL_GL_GetProcAddress("glGetProgramEnvParameterdvARB");
		qglGetProgramEnvParameterfvARB = SDL_GL_GetProcAddress("glGetProgramEnvParameterfvARB");
		qglGetProgramLocalParameterdvARB = SDL_GL_GetProcAddress("glGetProgramLocalParameterdvARB");
		qglGetProgramLocalParameterfvARB = SDL_GL_GetProcAddress("glGetProgramLocalParameterfvARB");
		qglGetProgramivARB = SDL_GL_GetProcAddress("glGetProgramivARB");
		qglGetProgramStringARB = SDL_GL_GetProcAddress("glGetProgramStringARB");
		qglIsProgramARB = SDL_GL_GetProcAddress("glIsProgramARB");
	} else {
		Com_Printf("GL_ARB_fragment_program not found\n");
		r_state.arb_fragment_program = qfalse;
	}

	/* FIXME: Is this the right extension to check for? */
	if (strstr(r_config.extensions_string, "GL_ARB_shading_language_100")) {
		Com_Printf("using GL_ARB_shading_language_100\n");
		qglCreateShader  = SDL_GL_GetProcAddress("glCreateShaderObjectARB");
		qglShaderSource  = SDL_GL_GetProcAddress("glShaderSourceARB");
		qglCompileShader = SDL_GL_GetProcAddress("glCompileShaderARB");
		qglCreateProgram = SDL_GL_GetProcAddress("glCreateProgramObjectARB");
		qglAttachShader  = SDL_GL_GetProcAddress("glAttachObjectARB");
		qglLinkProgram   = SDL_GL_GetProcAddress("glLinkProgramARB");
		qglUseProgram    = SDL_GL_GetProcAddress("glUseProgramObjectARB");
		qglDeleteShader  = SDL_GL_GetProcAddress("glDeleteObjectARB");
		qglDeleteProgram = SDL_GL_GetProcAddress("glDeleteObjectARB");
		if (!qglCreateShader)
			Sys_Error("Could not load all needed GLSL functions\n");
		r_state.glsl_program = qtrue;
	} else {
		Com_Printf("GL_ARB_shading_language_100 not found\n");
		r_state.glsl_program = qfalse;
	}
#endif	/* HAVE_SHADERS */

	r_state.ati_separate_stencil = qfalse;
	if (strstr(r_config.extensions_string, "GL_ATI_separate_stencil")) {
		if (!r_ati_separate_stencil->integer) {
			Com_Printf("ignoring GL_ATI_separate_stencil\n");
			r_state.ati_separate_stencil = qfalse;
		} else {
			Com_Printf("using GL_ATI_separate_stencil\n");
			r_state.ati_separate_stencil = qtrue;
			qglStencilOpSeparateATI = SDL_GL_GetProcAddress("glStencilOpSeparateATI");
			qglStencilFuncSeparateATI = SDL_GL_GetProcAddress("glStencilFuncSeparateATI");
		}
	} else {
		Com_Printf("GL_ATI_separate_stencil not found\n");
		r_state.ati_separate_stencil = qfalse;
		Cvar_Set("r_ati_separate_stencil", "0");
	}

	r_state.stencil_two_side = qfalse;
	if (strstr(r_config.extensions_string, "GL_EXT_stencil_two_side")) {
		if (!r_stencil_two_side->integer) {
			Com_Printf("ignoring GL_EXT_stencil_two_side\n");
			r_state.stencil_two_side = qfalse;
		} else {
			Com_Printf("using GL_EXT_stencil_two_side\n");
			r_state.stencil_two_side = qtrue;
			qglActiveStencilFaceEXT = SDL_GL_GetProcAddress("glActiveStencilFaceEXT");
		}
	} else {
		Com_Printf("GL_EXT_stencil_two_side not found\n");
		r_state.stencil_two_side = qfalse;
		Cvar_Set("r_stencil_two_side", "0");
	}

	{
		Com_Printf("max texture size: ");

		qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
		r_config.maxTextureSize = size;

		/* stubbed or broken drivers may have reported 0 */
		if (r_config.maxTextureSize <= 0)
			r_config.maxTextureSize = 0;

		if ((err = qglGetError()) != GL_NO_ERROR) {
			Com_Printf("cannot detect!\n");
		} else {
			Com_Printf("detected %d\n", size);
			if (r_maxtexres->integer > size) {
				Com_Printf("downgrading from %i\n", r_maxtexres->integer);
				Cvar_SetValue("r_maxtexres", size);
			} else if (r_maxtexres->integer < size) {
				Com_Printf("but using %i as requested\n", r_maxtexres->integer);
			}
		}
	}
}

qboolean R_Init (void)
{
	int j;
	extern float r_turbsin[256];

	for (j = 0; j < 256; j++)
		r_turbsin[j] *= 0.5;

	R_Register();

	/* set our "safe" modes */
	viddef.prev_mode = 6;

	/* initialize OS-specific parts of OpenGL */
	if (!Rimp_Init())
		return qfalse;

	/* initialize our QGL dynamic bindings */
	QR_Link();

	/* get our various GL strings */
	r_config.vendor_string = (const char *)qglGetString(GL_VENDOR);
	Com_Printf("GL_VENDOR: %s\n", r_config.vendor_string);
	r_config.renderer_string = (const char *)qglGetString(GL_RENDERER);
	Com_Printf("GL_RENDERER: %s\n", r_config.renderer_string);
	r_config.version_string = (const char *)qglGetString(GL_VERSION);
	Com_Printf("GL_VERSION: %s\n", r_config.version_string);
	r_config.extensions_string = (const char *)qglGetString(GL_EXTENSIONS);
	Com_Printf("GL_EXTENSIONS: %s\n", r_config.extensions_string);

	R_InitExtension();
	R_SetDefaultState();
#ifdef HAVE_SHADERS
	R_ShaderInit();
#endif
	R_InitImages();
	R_InitMiscTexture();
	R_DrawInitLocal();
	R_FontInit();
	R_InitModels();

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

	/* in case of a sys error e.g. this value is still -1 */
	if (spherelist != -1)
		qglDeleteLists(spherelist, 1);
	spherelist = -1;

	R_ShutdownModels();
	R_ShutdownImages();

#ifdef HAVE_SHADERS
	R_ShutdownShaders();
#endif
	R_FontShutdown();

	/* shut down OS specific OpenGL stuff like contexts, etc. */
	Rimp_Shutdown();

	/* shutdown our QGL subsystem */
	QR_UnLink();
}
