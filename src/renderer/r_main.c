/**
 * @file r_main.c
 * @brief
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

GLenum gl_texture0, gl_texture1, gl_texture2, gl_texture3;

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

/* screen size info */
refdef_t r_newrefdef;

static cvar_t *r_norefresh;
static cvar_t *r_drawentities;
static cvar_t *r_speeds;
cvar_t *r_drawworld;
cvar_t *r_nocull;
cvar_t *r_isometric;
cvar_t *r_lerpmodels;
cvar_t *r_displayrefresh;
cvar_t *r_anisotropic;
cvar_t *r_ext_max_anisotropy;
cvar_t *r_texture_lod;			/* lod_bias */

cvar_t *r_screenshot;
cvar_t *r_screenshot_jpeg_quality;

cvar_t *r_ext_swapinterval;
cvar_t *r_ext_multitexture;
cvar_t *r_ext_combine;
cvar_t *r_ext_lockarrays;
cvar_t *r_ext_texture_compression;
cvar_t *r_ext_s3tc_compression;

cvar_t *r_bitdepth;
cvar_t *r_drawbuffer;
cvar_t *r_driver;
cvar_t *r_lightmap;
cvar_t *r_shadows;
cvar_t *r_shadow_debug_volume;
cvar_t *r_shadow_debug_shade;
cvar_t *r_ati_separate_stencil;
cvar_t *r_stencil_two_side;

cvar_t *r_drawclouds;
cvar_t *r_imagefilter;
cvar_t *r_mode;
cvar_t *r_dynamic;
cvar_t *r_soften;
cvar_t *r_modulate;
cvar_t *r_round_down;
cvar_t *r_picmip;
cvar_t *r_maxtexres;
cvar_t *r_showtris;
cvar_t *r_finish;
cvar_t *r_flashblend;
cvar_t *r_swapinterval;
cvar_t *r_texturemode;
cvar_t *r_texturealphamode;
cvar_t *r_texturesolidmode;
cvar_t *r_wire;
cvar_t *r_fog;
cvar_t *r_showbox;
cvar_t *r_intensity;

cvar_t *r_3dmapradius;

cvar_t *con_font;
cvar_t *con_fontWidth;
cvar_t *con_fontHeight;
cvar_t *con_fontShift;

/**
 * @brief Reset to initial state
 */
static void R_Reset_f (void)
{
	R_EnableMultitexture(qfalse);
	RSTATE_DISABLE_ALPHATEST
	RSTATE_DISABLE_BLEND
}

/**
 * @brief Prints some OpenGL strings
 */
static void R_Strings_f (void)
{
	Com_Printf("GL_VENDOR: %s\n", r_config.vendor_string);
	Com_Printf("GL_RENDERER: %s\n", r_config.renderer_string);
	Com_Printf("GL_VERSION: %s\n", r_config.version_string);
	Com_Printf("MODE: %i, %d x %d FULLSCREEN: %i\n", r_mode->integer, viddef.width, viddef.height, vid_fullscreen->integer);
	Com_Printf("GL_EXTENSIONS: %s\n", r_config.extensions_string);
	Com_Printf("GL_MAX_TEXTURE_SIZE: %d\n", r_config.maxTextureSize);
}

/**
 * @brief
 * @sa R_DrawShadowVolume
 * @sa R_DrawShadow
 */
static void R_CastShadow (void)
{
	int i;

	/* no shadows at all */
	if (!r_shadows->integer)
		return;

	for (i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];
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


/**
 * @brief
 */
void R_RotateForEntity (entity_t * e)
{
	qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef(e->angles[1], 0, 0, 1);
	qglRotatef(-e->angles[0], 0, 1, 0);
	qglRotatef(-e->angles[2], 1, 0, 0);
}


/**
 * @brief
 */
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


/**
 * @brief
 */
static float *R_CalcTransform (entity_t * e)
{
	vec3_t angles;
	transform_t *t;
	float *mp;
	float mt[16], mc[16];
	int i;

	/* check if this entity is already transformed */
	t = &trafo[e - r_newrefdef.entities];

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


/**
 * @brief
 */
static void R_TransformEntitiesOnList (void)
{
	int i;

	if (!r_drawentities->integer)
		return;

	/* clear flags */
	for (i = 0; i < r_newrefdef.num_entities; i++) {
		trafo[i].done = qfalse;
		trafo[i].processing = qfalse;
	}

	/* calculate all transformations */
	/* possibly recursive */
	for (i = 0; i < r_newrefdef.num_entities; i++)
		R_CalcTransform(&r_newrefdef.entities[i]);
}


/**
 * @brief
 */
static void R_DrawEntitiesOnList (void)
{
	int i;

	if (!r_drawentities->integer)
		return;

	/* draw non-transparent first */

	for (i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];

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
	for (i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];
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

/**
 * @brief
 */
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

/**
 * @brief
 */
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
		frustum[0].dist -= 10 * r_newrefdef.fov_x;
		frustum[1].dist -= 10 * r_newrefdef.fov_x;
		frustum[2].dist -= 10 * r_newrefdef.fov_x * ((float) r_newrefdef.height / r_newrefdef.width);
		frustum[3].dist -= 10 * r_newrefdef.fov_x * ((float) r_newrefdef.height / r_newrefdef.width);
	} else {
		/* rotate VPN right by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - r_newrefdef.fov_x / 2));
		/* rotate VPN left by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - r_newrefdef.fov_x / 2);
		/* rotate VPN up by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - r_newrefdef.fov_y / 2);
		/* rotate VPN down by FOV_X/2 degrees */
		RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - r_newrefdef.fov_y / 2));

		for (i = 0; i < 4; i++) {
			frustum[i].type = PLANE_ANYZ;
			frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
			frustum[i].signbits = SignbitsForPlane(&frustum[i]);
		}
	}
}

/**
 * @brief
 */
static void R_SetupFrame (void)
{
	int i;

	r_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	for (i = 0; i < 4; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) {
		qglEnable(GL_SCISSOR_TEST);
		qglScissor(r_newrefdef.x, viddef.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		qglDisable(GL_SCISSOR_TEST);
	}
}

/**
 * @brief
 */
static void R_Clear (void)
{
	qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	qglDepthFunc(GL_LEQUAL);

	qglDepthRange(0, 1);

	if (r_shadows->integer == 2) {
		/* set the reference stencil value */
		qglClearStencil(1);
		/* reset stencil buffer */
		qglClear(GL_STENCIL_BUFFER_BIT);
	}
}

/**
 * @brief
 */
static void R_Flash (void)
{
	R_ShadowBlend();
}

/**
 * @brief r_newrefdef must be set before the first call
 */
static void R_RenderView (refdef_t * fd)
{
	if (r_norefresh->integer)
		return;

	r_newrefdef = *fd;

/*	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL)) */
/*		Sys_Error("R_RenderView: NULL worldmodel"); */

	if (r_speeds->integer) {
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (r_finish->integer)
		qglFinish();

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
 * @brief
 * @sa R_BeginFrame
 * @sa R_EndFrame
 */
void R_RenderFrame (refdef_t * fd)
{
	R_RenderView(fd);
	R_SetupGL2D();

	if (r_speeds->integer) {
		fd->c_brush_polys = c_brush_polys;
		fd->c_alias_polys = c_alias_polys;
	}
}

static const cmdList_t r_commands[] = {
	{"imagelist", R_ImageList_f, NULL},
	{"fontcachelist", R_FontListCache_f, NULL},
	{"screenshot", R_ScreenShot_f, "Take a screenshot"},
	{"modellist", R_ModModellist_f, NULL},
	{"r_strings", R_Strings_f, NULL},
	{"r_reset", R_Reset_f, "Reset to initial state"},

	{NULL, NULL, NULL}
};

/**
 * @brief
 */
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
	r_displayrefresh = Cvar_Get("r_displayrefresh", "0", CVAR_ARCHIVE, NULL);
	r_anisotropic = Cvar_Get("r_anisotropic", "1", CVAR_ARCHIVE, NULL);
	r_ext_max_anisotropy = Cvar_Get("r_ext_max_anisotropy", "0", 0, NULL);
	r_texture_lod = Cvar_Get("r_texture_lod", "0", CVAR_ARCHIVE, NULL);

	r_screenshot = Cvar_Get("r_screenshot", "jpg", CVAR_ARCHIVE, "png, jpg or tga are valid screenshot formats");
	r_screenshot_jpeg_quality = Cvar_Get("r_screenshot_jpeg_quality", "75", CVAR_ARCHIVE, "jpeg quality in percent for jpeg screenshots");

	r_modulate = Cvar_Get("r_modulate", "1", CVAR_ARCHIVE, NULL);
	r_bitdepth = Cvar_Get("r_bitdepth", "0", CVAR_ARCHIVE, NULL);
	r_mode = Cvar_Get("r_mode", "6", CVAR_ARCHIVE, "Display resolution");
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
	r_finish = Cvar_Get("r_finish", "0", CVAR_ARCHIVE, NULL);
	r_flashblend = Cvar_Get("r_flashblend", "0", 0, "Controls the way dynamic lights are drawn");
	r_driver = Cvar_Get("r_driver", "", CVAR_ARCHIVE, "You can define the opengl driver you want to use - empty if you want to use the system default");
	r_texturemode = Cvar_Get("r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, NULL);
	r_texturealphamode = Cvar_Get("r_texturealphamode", "default", CVAR_ARCHIVE, NULL);
	r_texturesolidmode = Cvar_Get("r_texturesolidmode", "default", CVAR_ARCHIVE, NULL);
	r_wire = Cvar_Get("r_wire", "0", 0, "Draw the scene in wireframe mode");
	r_fog = Cvar_Get("r_fog", "1", CVAR_ARCHIVE, NULL);
	r_showbox = Cvar_Get("r_showbox", "0", CVAR_ARCHIVE, "Shows model bounding box");
	r_intensity = Cvar_Get("r_intensity", "2", CVAR_ARCHIVE, NULL);

	r_ext_swapinterval = Cvar_Get("r_ext_swapinterval", "1", CVAR_ARCHIVE, NULL);
	r_ext_multitexture = Cvar_Get("r_ext_multitexture", "1", CVAR_ARCHIVE, NULL);
	r_ext_combine = Cvar_Get("r_ext_combine", "1", CVAR_ARCHIVE, NULL);
	r_ext_lockarrays = Cvar_Get("r_ext_lockarrays", "0", CVAR_ARCHIVE, NULL);
	r_ext_texture_compression = Cvar_Get("r_ext_texture_compression", "0", CVAR_ARCHIVE, NULL);
	r_ext_s3tc_compression = Cvar_Get("r_ext_s3tc_compression", "1", CVAR_ARCHIVE, NULL);

	r_drawbuffer = Cvar_Get("r_drawbuffer", "GL_BACK", 0, NULL);
	r_swapinterval = Cvar_Get("r_swapinterval", "1", CVAR_ARCHIVE, NULL);

	r_3dmapradius = Cvar_Get("r_3dmapradius", "8192.0", CVAR_NOSET, NULL);

	vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE, NULL);
	vid_gamma = Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE, NULL);

	con_font = Cvar_Get("con_font", "0", CVAR_ARCHIVE, NULL);
	con_fontWidth = Cvar_Get("con_fontWidth", "8", CVAR_NOSET, NULL);
	con_fontHeight = Cvar_Get("con_fontHeight", "16", CVAR_NOSET, NULL);
	con_fontShift = Cvar_Get("con_fontShift", "3", CVAR_NOSET, NULL);

	for (commands = r_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function, commands->description);
}

/**
 * @brief
 */
qboolean R_SetMode (void)
{
	rserr_t err;
	qboolean fullscreen;

	fullscreen = vid_fullscreen->integer;

	vid_fullscreen->modified = qfalse;
	r_mode->modified = qfalse;
	r_ext_texture_compression->modified = qfalse;

	if ((err = Rimp_SetMode(&viddef.width, &viddef.height, r_mode->integer, fullscreen)) == rserr_ok)
		r_state.prev_mode = r_mode->integer;
	else {
		if (err == rserr_invalid_fullscreen) {
			Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = qfalse;
			Com_Printf("renderer::R_SetMode() - fullscreen unavailable in this mode\n");
			if ((err = Rimp_SetMode(&viddef.width, &viddef.height, r_mode->integer, qfalse)) == rserr_ok)
				return qtrue;
		} else if (err == rserr_invalid_mode) {
			Cvar_SetValue("r_mode", r_state.prev_mode);
			r_mode->modified = qfalse;
			Com_Printf("renderer::R_SetMode() - invalid mode\n");
		}

		/* try setting it back to something safe */
		if ((err = Rimp_SetMode(&viddef.width, &viddef.height, r_state.prev_mode, qfalse)) != rserr_ok) {
			Com_Printf("renderer::R_SetMode() - could not revert to safe mode\n");
			return qfalse;
		}
	}
	return qtrue;
}

/**
 * @brief
 */
qboolean R_Init (void)
{
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	int j;
	extern float r_turbsin[256];
	int aniso_level, max_aniso;

	for (j = 0; j < 256; j++)
		r_turbsin[j] *= 0.5;

	R_Register();

	/* initialize OS-specific parts of OpenGL */
	if (!Rimp_Init())
		return qfalse;

	/* initialize our QGL dynamic bindings */
	QR_Link();

	/* set our "safe" modes */
	r_state.prev_mode = 3;

	/* create the window and set up the context */
	if (!R_SetMode()) {
		QR_UnLink();
		Com_Printf("renderer::R_Init() - could not R_SetMode()\n");
		return qfalse;
	}

	/* get our various GL strings */
	r_config.vendor_string = (const char *)qglGetString(GL_VENDOR);
	Com_Printf("GL_VENDOR: %s\n", r_config.vendor_string);
	r_config.renderer_string = (const char *)qglGetString(GL_RENDERER);
	Com_Printf("GL_RENDERER: %s\n", r_config.renderer_string);
	r_config.version_string = (const char *)qglGetString(GL_VERSION);
	Com_Printf("GL_VERSION: %s\n", r_config.version_string);
	r_config.extensions_string = (const char *)qglGetString(GL_EXTENSIONS);
	Com_Printf("GL_EXTENSIONS: %s\n", r_config.extensions_string);

	Q_strncpyz(renderer_buffer, r_config.renderer_string, sizeof(renderer_buffer));
	renderer_buffer[sizeof(renderer_buffer)-1] = 0;
	Q_strlwr(renderer_buffer);

	Q_strncpyz(vendor_buffer, r_config.vendor_string, sizeof(vendor_buffer));
	vendor_buffer[sizeof(vendor_buffer)-1] = 0;
	Q_strlwr(vendor_buffer);

	if (strstr(renderer_buffer, "voodoo")) {
		if (!strstr(renderer_buffer, "rush"))
			r_config.renderer = GL_RENDERER_VOODOO;
		else
			r_config.renderer = GL_RENDERER_VOODOO_RUSH;
	} else if (strstr(vendor_buffer, "sgi"))
		r_config.renderer = GL_RENDERER_SGI;
	else if (strstr(renderer_buffer, "permedia"))
		r_config.renderer = GL_RENDERER_PERMEDIA2;
	else if (strstr(renderer_buffer, "glint"))
		r_config.renderer = GL_RENDERER_GLINT_MX;
	else if (strstr(renderer_buffer, "glzicd"))
		r_config.renderer = GL_RENDERER_REALIZM;
	else if (strstr(renderer_buffer, "gdi"))
		r_config.renderer = GL_RENDERER_MCD;
	else if (strstr(renderer_buffer, "pcx2"))
		r_config.renderer = GL_RENDERER_PCX2;
	else if (strstr(renderer_buffer, "verite"))
		r_config.renderer = GL_RENDERER_RENDITION;
	else
		r_config.renderer = GL_RENDERER_OTHER;

#if defined (__linux__) || defined (__FreeBSD__) || defined (__NetBSD__)
	Cvar_SetValue("r_finish", 1);
#else
	/* MCD has buffering issues */
	if (r_config.renderer == GL_RENDERER_MCD)
		Cvar_SetValue("r_finish", 1);
#endif

	/* grab extensions */
	if (strstr(r_config.extensions_string, "GL_EXT_compiled_vertex_array") || strstr(r_config.extensions_string, "GL_SGI_compiled_vertex_array")) {
		if (r_ext_lockarrays->integer) {
			Com_Printf("...enabling GL_EXT_LockArrays\n");
			qglLockArraysEXT = SDL_GL_GetProcAddress("glLockArraysEXT");
			qglUnlockArraysEXT = SDL_GL_GetProcAddress("glUnlockArraysEXT");
		} else
			Com_Printf("...ignoring GL_EXT_LockArrays\n");
	} else
		Com_Printf("...GL_EXT_compiled_vertex_array not found\n");

	if (strstr(r_config.extensions_string, "GL_ARB_multitexture")) {
		if (r_ext_multitexture->integer) {
			Com_Printf("...using GL_ARB_multitexture\n");
			qglMTexCoord2fSGIS = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
			qglActiveTextureARB = SDL_GL_GetProcAddress("glActiveTextureARB");
			qglClientActiveTextureARB = SDL_GL_GetProcAddress("glClientActiveTextureARB");
			gl_texture0 = GL_TEXTURE0_ARB;
			gl_texture1 = GL_TEXTURE1_ARB;
			gl_texture2 = GL_TEXTURE2_ARB;
			gl_texture3 = GL_TEXTURE3_ARB;
		} else
			Com_Printf("...ignoring GL_ARB_multitexture\n");
	} else
		Com_Printf("...GL_ARB_multitexture not found\n");

	if (strstr(r_config.extensions_string, "GL_EXT_texture_env_combine") || strstr(r_config.extensions_string, "GL_ARB_texture_env_combine")) {
		if (r_ext_combine->integer) {
			Com_Printf("...using GL_EXT_texture_env_combine\n");
			r_config.envCombine = GL_COMBINE_EXT;
		} else {
			Com_Printf("...ignoring EXT_texture_env_combine\n");
			r_config.envCombine = 0;
		}
	} else {
		Com_Printf("...GL_EXT_texture_env_combine not found\n");
		r_config.envCombine = 0;
	}

	if (strstr(r_config.extensions_string, "GL_SGIS_multitexture")) {
		if (qglActiveTextureARB)
			Com_Printf("...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n");
		else if (r_ext_multitexture->integer) {
			Com_Printf("...using GL_SGIS_multitexture\n");
			qglMTexCoord2fSGIS = SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
			qglSelectTextureSGIS = SDL_GL_GetProcAddress("glSelectTextureSGIS");
			gl_texture0 = GL_TEXTURE0_SGIS;
			gl_texture1 = GL_TEXTURE1_SGIS;
			gl_texture2 = GL_TEXTURE2_SGIS;
			gl_texture3 = GL_TEXTURE3_SGIS;
		} else
			Com_Printf("...ignoring GL_SGIS_multitexture\n");
	} else
		Com_Printf("...GL_SGIS_multitexture not found\n");

	if (strstr(r_config.extensions_string, "GL_ARB_texture_compression")) {
		if (r_ext_texture_compression->integer) {
			Com_Printf("...using GL_ARB_texture_compression\n");
			if (r_ext_s3tc_compression->integer && strstr(r_config.extensions_string, "GL_EXT_texture_compression_s3tc")) {
/*				Com_Printf("   with s3tc compression\n"); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			} else {
/*				Com_Printf("   without s3tc compression\n"); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_ARB;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_ARB;
			}
		} else {
			Com_Printf("...ignoring GL_ARB_texture_compression\n");
			gl_compressed_solid_format = 0;
			gl_compressed_alpha_format = 0;
		}
	} else {
		Com_Printf("...GL_ARB_texture_compression not found\n");
		gl_compressed_solid_format = 0;
		gl_compressed_alpha_format = 0;
	}

	/* anisotropy */
	r_state.anisotropic = qfalse;

	qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
	Cvar_SetValue("r_ext_max_anisotropy", max_aniso);
	if (r_anisotropic->integer > r_ext_max_anisotropy->integer) {
		Com_Printf("...max GL_EXT_texture_filter_anisotropic value is %i\n", max_aniso);
		Cvar_SetValue("r_anisotropic", r_ext_max_anisotropy->integer);
	}

	aniso_level = r_anisotropic->integer;
	if (strstr(r_config.extensions_string, "GL_EXT_texture_filter_anisotropic")) {
		if (!r_anisotropic->integer)
			Com_Printf("...ignoring GL_EXT_texture_filter_anisotropic\n");
		else {
			Com_Printf("...using GL_EXT_texture_filter_anisotropic [%2i max] [%2i selected]\n", max_aniso, aniso_level);
			r_state.anisotropic = qtrue;
		}
	} else {
		Com_Printf("...GL_EXT_texture_filter_anisotropic not found\n");
		Cvar_Set("r_anisotropic", "0");
		Cvar_Set("r_ext_max_anisotropy", "0");
	}

	if (strstr(r_config.extensions_string, "GL_EXT_texture_lod_bias")) {
		Com_Printf("...using GL_EXT_texture_lod_bias\n");
		r_state.lod_bias = qtrue;
	} else {
		Com_Printf("...GL_EXT_texture_lod_bias not found\n");
		r_state.lod_bias = qfalse;
	}

#if 0
	if (strstr(r_config.extensions_string, "GL_SGIS_generate_mipmap")) {
		Com_Printf("...using GL_SGIS_generate_mipmap\n");
		r_state.sgis_mipmap = qtrue;
	} else {
		Com_Printf("...GL_SGIS_generate_mipmap not found\n");
		Sys_Error("GL_SGIS_generate_mipmap not found!");
		r_state.sgis_mipmap = qfalse;
	}
#endif

	if (strstr(r_config.extensions_string, "GL_EXT_stencil_wrap")) {
		Com_Printf("...using GL_EXT_stencil_wrap\n");
		r_state.stencil_wrap = qtrue;
	} else {
		Com_Printf("...GL_EXT_stencil_wrap not found\n");
		r_state.stencil_wrap = qfalse;
	}

	if (strstr(r_config.extensions_string, "GL_EXT_fog_coord")) {
		Com_Printf("...using GL_EXT_fog_coord\n");
		r_state.fog_coord = qtrue;
	} else {
		Com_Printf("...GL_EXT_fog_coord not found\n");
		r_state.fog_coord = qfalse;
	}

	r_state.glsl_program = qfalse;
	r_state.arb_fragment_program = qfalse;
#ifdef HAVE_SHADERS
	if (strstr(r_config.extensions_string, "GL_ARB_fragment_program")) {
		Com_Printf("...using GL_ARB_fragment_program\n");
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
		Com_Printf("...GL_ARB_fragment_program not found\n");
		r_state.arb_fragment_program = qfalse;
	}

	/* FIXME: Is this the right extension to check for? */
	if (strstr(r_config.extensions_string, "GL_ARB_shading_language_100")) {
		Com_Printf("...using GL_ARB_shading_language_100\n");
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
		Com_Printf("...GL_ARB_shading_language_100 not found\n");
		r_state.glsl_program = qfalse;
	}
#endif							/* HAVE_SHADERS */

	r_state.ati_separate_stencil = qfalse;
	if (strstr(r_config.extensions_string, "GL_ATI_separate_stencil")) {
		if (!r_ati_separate_stencil->integer) {
			Com_Printf("...ignoring GL_ATI_separate_stencil\n");
			r_state.ati_separate_stencil = qfalse;
		} else {
			Com_Printf("...using GL_ATI_separate_stencil\n");
			r_state.ati_separate_stencil = qtrue;
			qglStencilOpSeparateATI = SDL_GL_GetProcAddress("glStencilOpSeparateATI");
			qglStencilFuncSeparateATI = SDL_GL_GetProcAddress("glStencilFuncSeparateATI");
		}
	} else {
		Com_Printf("...GL_ATI_separate_stencil not found\n");
		r_state.ati_separate_stencil = qfalse;
		Cvar_Set("r_ati_separate_stencil", "0");
	}

	r_state.stencil_two_side = qfalse;
	if (strstr(r_config.extensions_string, "GL_EXT_stencil_two_side")) {
		if (!r_stencil_two_side->integer) {
			Com_Printf("...ignoring GL_EXT_stencil_two_side\n");
			r_state.stencil_two_side = qfalse;
		} else {
			Com_Printf("...using GL_EXT_stencil_two_side\n");
			r_state.stencil_two_side = qtrue;
			qglActiveStencilFaceEXT = SDL_GL_GetProcAddress("glActiveStencilFaceEXT");
		}
	} else {
		Com_Printf("...GL_EXT_stencil_two_side not found\n");
		r_state.stencil_two_side = qfalse;
		Cvar_Set("r_stencil_two_side", "0");
	}

	{
		int size;
		GLenum err;

		Com_Printf("...max texture size:\n");

		qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
		r_config.maxTextureSize = size;

		/* stubbed or broken drivers may have reported 0 */
		if (r_config.maxTextureSize <= 0)
			r_config.maxTextureSize = 0;

		if ((err = qglGetError()) != GL_NO_ERROR) {
			Com_Printf("......cannot detect !\n");
		} else {
			Com_Printf("......detected %d\n", size);
			if (r_maxtexres->integer > size) {
				Com_Printf("......downgrading from %i\n", r_maxtexres->integer);
				Cvar_SetValue("r_maxtexres", size);
			} else if (r_maxtexres->integer < size) {
				Com_Printf("......but using %i as requested\n", r_maxtexres->integer);
			}
		}
	}

	R_SetDefaultState();

	R_InitImages();
	R_InitMiscTexture();
	R_DrawInitLocal();
	R_FontInit();

	R_CHECK_ERROR

	return qtrue;
}

/**
 * @brief
 */
void R_Restart (void)
{
	qglDeleteLists(spherelist, 1);
	spherelist = -1;

	R_ShutdownImages();
	R_FontCleanCache();

	R_InitMiscTexture();
	R_DrawInitLocal();
}

/**
 * @brief
 * @sa R_Init
 */
void R_Shutdown (void)
{
	const cmdList_t *commands;

	for (commands = r_commands; commands->name; commands++)
		Cmd_RemoveCommand(commands->name);

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

/**
 * @brief
 * @sa R_RenderFrame
 */
void R_BeginFrame (void)
{
	/* change modes if necessary */
	if (r_mode->modified || vid_fullscreen->modified || r_ext_texture_compression->modified) {
		R_SetMode();
#ifdef _WIN32
		VID_Restart_f();
#endif
	}

	if (r_anisotropic->modified) {
		if (r_anisotropic->integer > r_ext_max_anisotropy->integer) {
			Com_Printf("...max GL_EXT_texture_filter_anisotropic value is %i\n", r_ext_max_anisotropy->integer);
			Cvar_SetValue("r_anisotropic", r_ext_max_anisotropy->value);
		}
		/*R_UpdateAnisotropy();*/
		r_anisotropic->modified = qfalse;
	}

	if (con_font->modified) {
		if (con_font->integer == 0) {
			Cvar_ForceSet("con_fontWidth", "8");
			Cvar_ForceSet("con_fontHeight", "16");
			Cvar_ForceSet("con_fontShift", "3");
			con_font->modified = qfalse;
		} else if (con_font->integer == 1 && draw_chars[1]) {
			Cvar_ForceSet("con_fontWidth", "8");
			Cvar_ForceSet("con_fontHeight", "8");
			Cvar_ForceSet("con_fontShift", "3");
			con_font->modified = qfalse;
		} else
			Cvar_ForceSet("con_font", "1");
	}

	/* go into 2D mode */
	R_SetupGL2D();

	/* draw buffer stuff */
	if (r_drawbuffer->modified) {
		r_drawbuffer->modified = qfalse;

		if (Q_stricmp(r_drawbuffer->string, "GL_FRONT") == 0)
			qglDrawBuffer(GL_FRONT);
		else
			qglDrawBuffer(GL_BACK);
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

	/* vsync stuff */
	if (r_swapinterval->modified) {
		if (qglSwapInterval)
			qglSwapInterval(r_swapinterval->integer);
		r_swapinterval->modified = qfalse;
	}

	/* clear screen if desired */
	R_Clear();
}

/**
 * @brief
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

/**
 * @brief
 */
void R_TakeVideoFrame (int w, int h, byte * captureBuffer, byte * encodeBuffer, qboolean motionJpeg)
{
	size_t frameSize;
	int i;

	qglReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, captureBuffer);

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
