/**
 * @file r_material.c
 * @brief Material related code
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
#include "r_lightmap.h"
#include "../../shared/parse.h"

mBspSurfaces_t r_material_surfaces;

/** @todo load this from file, will make tweaking the game much easier */
material_t defaultMaterial = {
	.flags = 0,
	.time = 0.0f,
	.bump = DEFAULT_BUMP,
	.parallax = DEFAULT_PARALLAX,
	.hardness = DEFAULT_HARDNESS,
	.specular = DEFAULT_SPECULAR,
	.glowscale = DEFAULT_GLOWSCALE,
	.stages = NULL,
	.num_stages = 0
};

#define UPDATE_THRESHOLD 0.02
/**
 * @brief Materials "think" every few milliseconds to advance animations.
 */
static void R_UpdateMaterial (material_t *m)
{
	materialStage_t *s;

	if (refdef.time - m->time < UPDATE_THRESHOLD)
		return;

	m->time = refdef.time;

	for (s = m->stages; s; s = s->next) {
		if (s->flags & STAGE_PULSE) {
			float phase = refdef.time * s->pulse.hz;
			float moduloPhase = phase - floor(phase); /* extract fractional part of phase */

			if (moduloPhase < s->pulse.dutycycle) {
				moduloPhase /= s->pulse.dutycycle;
				s->pulse.dhz = (1.0 - cos(moduloPhase * (2 * M_PI))) / 2.0;
			} else {
				s->pulse.dhz = 0;
			}
		}

		if (s->flags & STAGE_STRETCH) {
			s->stretch.dhz = (1.0 - cos(refdef.time * s->stretch.hz * (2 * M_PI)) ) / 2.0;
			s->stretch.damp = 1.5 - s->stretch.dhz * s->stretch.amp;
		}

		if (s->flags & STAGE_ROTATE)
			s->rotate.deg = refdef.time * s->rotate.hz * 360.0;

		if (s->flags & STAGE_SCROLL_S)
			s->scroll.ds = s->scroll.s * refdef.time;

		if (s->flags & STAGE_SCROLL_T)
			s->scroll.dt = s->scroll.t * refdef.time;

		if (s->flags & STAGE_ANIM) {
			if (refdef.time >= s->anim.dtime) {  /* change frames */
				int frame;
				s->anim.dtime = refdef.time + (1.0 / s->anim.fps);
				s->anim.dframe++;
				switch (s->anim.type) {
				case ANIM_NORMAL:
					frame = s->anim.dframe % s->anim.num_frames;
					break;
				case ANIM_ALTERNATE:
					frame = abs(s->anim.dframe % (s->anim.num_frames + 1) - (s->anim.num_frames / 2));
					break;
				case ANIM_BACKWARDS:
					frame = s->anim.num_frames - 1;
					frame -= s->anim.dframe % s->anim.num_frames;
					break;
				case ANIM_RANDOM:
					frame = rand() % s->anim.num_frames;
					break;
				case ANIM_RANDOMFORCE:
					frame = rand() % s->anim.num_frames;
					if (s->image == s->anim.images[frame])
						frame = (frame + 1) % s->anim.num_frames;
					break;
				default:
					continue;
				}
				assert(frame >= 0);
				assert(frame < s->anim.num_frames);
				s->image = s->anim.images[frame];
			}
		}
	}
}

static void R_StageGlow (const materialStage_t *stage)
{
	image_t *glowmap;

	if (stage->flags & STAGE_GLOWMAPLINK)
		glowmap = stage->image;
	else
		glowmap = stage->image->glowmap;

	if (glowmap) {
		R_EnableGlowMap(glowmap);
		if (r_state.glowmap_enabled)
			R_ProgramParameter1f("GLOWSCALE", stage->glowscale);
	} else {
		R_EnableGlowMap(NULL);
	}
}

/**
 * @brief Manages state for stages supporting static, dynamic, and per-pixel lighting.
 */
static void R_StageLighting (const mBspSurface_t *surf, const materialStage_t *stage)
{
	/* if the surface has a lightmap, and the stage specifies lighting.. */
	if ((surf->flags & MSURF_LIGHTMAP) &&
			(stage->flags & (STAGE_LIGHTMAP | STAGE_LIGHTING))) {
		R_EnableTexture(&texunit_lightmap, qtrue);
		R_BindLightmapTexture(surf->lightmap_texnum);

		/* hardware lighting */
		if ((stage->flags & STAGE_LIGHTING)) {
			R_EnableLighting(r_state.world_program, qtrue);
			R_SetSurfaceBumpMappingParameters(surf, stage->image->normalmap, stage->image->specularmap);
		} else {
			R_SetSurfaceBumpMappingParameters(surf, NULL, NULL);
			R_EnableLighting(NULL, qfalse);
		}
	} else {
		if (!r_state.lighting_enabled)
			return;
		R_EnableLighting(NULL, qfalse);

		R_EnableTexture(&texunit_lightmap, qfalse);
	}
}

/**
 * @brief Vertex deformation
 * @todo implement this
 */
static inline void R_StageVertex (const mBspSurface_t *surf, const materialStage_t *stage, const vec3_t in, vec3_t out)
{
	VectorCopy(in, out);
}

/**
 * @brief Manages texture matrix manipulations for stages supporting rotations,
 * scrolls, and stretches (rotate, translate, scale).
 */
static inline void R_StageTextureMatrix (const mBspSurface_t *surf, const materialStage_t *stage)
{
	static qboolean identity = qtrue;
	float s, t;

	if (!(stage->flags & STAGE_TEXTURE_MATRIX)) {
		if (!identity)
			glLoadIdentity();

		identity = qtrue;
		return;
	}

	glLoadIdentity();

	s = surf->stcenter[0] / surf->texinfo->image->width;
	t = surf->stcenter[1] / surf->texinfo->image->height;

	if (stage->flags & STAGE_STRETCH) {
		glTranslatef(-s, -t, 0.0);
		glScalef(stage->stretch.damp, stage->stretch.damp, 1.0);
		glTranslatef(-s, -t, 0.0);
	}

	if (stage->flags & STAGE_ROTATE) {
		glTranslatef(-s, -t, 0.0);
		glRotatef(stage->rotate.deg, 0.0, 0.0, 1.0);
		glTranslatef(-s, -t, 0.0);
	}

	if (stage->flags & STAGE_SCALE_S)
		glScalef(stage->scale.s, 1.0, 1.0);

	if (stage->flags & STAGE_SCALE_T)
		glScalef(1.0, stage->scale.t, 1.0);

	if (stage->flags & STAGE_SCROLL_S)
		glTranslatef(stage->scroll.ds, 0.0, 0.0);

	if (stage->flags & STAGE_SCROLL_T)
		glTranslatef(0.0, stage->scroll.dt, 0.0);

	identity = qfalse;
}

/**
 * @brief Generates a single texture coordinate for the specified stage and vertex.
 */
static void R_StageTexCoord (const materialStage_t *stage, const vec3_t v, const vec2_t in, vec2_t out)
{
	vec3_t tmp;

	if (stage->flags & STAGE_ENVMAP) {  /* generate texcoords */
		VectorSubtract(v, refdef.viewOrigin, tmp);
		VectorNormalizeFast(tmp);
		Vector2Copy(tmp, out);
	} else {  /* or use the ones we were given */
		Vector2Copy(in, out);
	}
}

/** @brief Array with 'random' alpha values for the dirtmap */
static const float dirtmap[] = {
		0.6, 0.5, 0.3, 0.4, 0.7, 0.3, 0.0, 0.4,
		0.5, 0.2, 0.8, 0.5, 0.3, 0.2, 0.5, 0.3
};

/**
 * @brief Generates a single color for the specified stage and vertex.
 */
static void R_StageColor (const materialStage_t *stage, const vec3_t v, vec4_t color)
{
	if (stage->flags & (STAGE_TERRAIN | STAGE_TAPE)) {
		float a;

		if (stage->flags & STAGE_COLOR)  /* honor stage color */
			VectorCopy(stage->color, color);
		else  /* or use white */
			VectorSet(color, 1.0, 1.0, 1.0);

		/* resolve alpha for vert based on z axis height */
		if (stage->flags & STAGE_TERRAIN) {
			if (v[2] < stage->terrain.floor)
				a = 0.0;
			else if (v[2] > stage->terrain.ceil)
				a = 1.0;
			else
				a = (v[2] - stage->terrain.floor) / stage->terrain.height;
		} else {
			if (v[2] < stage->tape.max && v[2] > stage->tape.min) {
				if (v[2] > stage->tape.center) {
					const float delta = v[2] - stage->tape.center;
					a = 1 - (delta / stage->tape.max);
				} else {
					const float delta = stage->tape.center - v[2];
					a = 1 - (delta / stage->tape.min);
				}
			} else {
				a = 0.0;
			}
		}

		color[3] = a;
	} else if (stage->flags & STAGE_DIRTMAP) {
		/* resolve dirtmap based on vertex position */
		const vec3_t hash_mult = {1.3, 3.1, 7.3};
		const int index = ((int) DotProduct(v, hash_mult)) % lengthof(dirtmap);

		if (stage->flags & STAGE_COLOR)  /* honor stage color */
			VectorCopy(stage->color, color);
		else  /* or use white */
			VectorSet(color, 1.0, 1.0, 1.0);
		color[3] = dirtmap[index] * stage->dirt.intensity;
	} else {  /* simply use white */
		Vector4Set(color, 1.0, 1.0, 1.0, 1.0);
	}
}

/**
 * @brief Manages all state for the specified surface and stage.
 * @sa R_DrawMaterialSurfaces
 */
static void R_SetSurfaceStageState (const mBspSurface_t *surf, const materialStage_t *stage)
{
	vec4_t color;

	/* bind the texture */
	R_BindTexture(stage->image->texnum);

	/* and optionally the lightmap */
	R_StageLighting(surf, stage);

	R_StageGlow(stage);

	/* load the texture matrix for rotations, stretches, etc.. */
	R_StageTextureMatrix(surf, stage);

	/* set the blend function, ensuring a good default */
	if (stage->flags & STAGE_BLEND)
		R_BlendFunc(stage->blend.src, stage->blend.dest);
	else
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* for terrain, enable the color array */
	if (stage->flags & (STAGE_TAPE | STAGE_TERRAIN | STAGE_DIRTMAP))
		R_EnableColorArray(qtrue);
	else
		R_EnableColorArray(qfalse);

	/* when not using the color array, resolve the shade color */
	if (!r_state.color_array_enabled) {
		if (stage->flags & STAGE_COLOR)  /* explicit */
			VectorCopy(stage->color, color);

		else if (stage->flags & STAGE_ENVMAP)  /* implied */
			VectorCopy(surf->color, color);

		else  /* default */
			VectorSet(color, 1.0, 1.0, 1.0);

		/* modulate the alpha value for pulses */
		if (stage->flags & STAGE_PULSE) {
			/* disable fog, since it also sets alpha */
			R_EnableFog(qfalse);
			color[3] = stage->pulse.dhz;
		} else {
			/* ensure fog is available */
			R_EnableFog(qtrue);
			color[3] = 1.0;
		}

		R_Color(color);
	}
}

/**
 * @brief Render the specified stage for the surface. Resolve vertex attributes via
 * helper functions, outputting to the default vertex arrays.
 */
static void R_DrawSurfaceStage (mBspSurface_t *surf, materialStage_t *stage)
{
	int i;

	R_ReallocateStateArrays(surf->numedges);
	R_ReallocateTexunitArray(&texunit_diffuse, surf->numedges);
	R_ReallocateTexunitArray(&texunit_lightmap, surf->numedges);

	for (i = 0; i < surf->numedges; i++) {
		const float *v = &r_mapTiles[surf->tile]->bsp.verts[surf->index * 3 + i * 3];
		const float *st = &r_mapTiles[surf->tile]->bsp.texcoords[surf->index * 2 + i * 2];

		R_StageVertex(surf, stage, v, &r_state.vertex_array_3d[i * 3]);

		R_StageTexCoord(stage, v, st, &texunit_diffuse.texcoord_array[i * 2]);

		if (texunit_lightmap.enabled) {
			st = &r_mapTiles[surf->tile]->bsp.lmtexcoords[surf->index * 2 + i * 2];
			texunit_lightmap.texcoord_array[i * 2 + 0] = st[0];
			texunit_lightmap.texcoord_array[i * 2 + 1] = st[1];
		}

		if (r_state.color_array_enabled)
			R_StageColor(stage, v, &r_state.color_array[i * 4]);

		/* normals and tangents */
		if (r_state.lighting_enabled) {
			const float *n = &r_mapTiles[surf->tile]->bsp.normals[surf->index * 3 + i * 3];
			memcpy(&r_state.normal_array[i * 3], n, sizeof(vec3_t));

			if (r_state.active_normalmap) {
				const float *t = &r_mapTiles[surf->tile]->bsp.tangents[surf->index * 4 + i * 4];
				memcpy(&r_state.tangent_array[i * 4], t, sizeof(vec3_t));
			}
		}
	}

	glDrawArrays(GL_TRIANGLE_FAN, 0, i);

	refdef.batchCount++;

	R_CheckError();
}

/**
 * @brief Iterates the specified surfaces list, updating materials as they are
 * encountered, and rendering all visible stages. State is lazily managed
 * throughout the iteration, so there is a concerted effort to restore the
 * state after all surface stages have been rendered.
 */
void R_DrawMaterialSurfaces (const mBspSurfaces_t *surfs)
{
	int i;

	if (!r_materials->integer || r_wire->integer)
		return;

	if (!surfs->count)
		return;

	assert(r_state.blend_enabled);

	/** @todo - integrate BSP lighting with model lighting */
	R_EnableModelLights(NULL, 0, qfalse, qfalse);

	R_EnableColorArray(qtrue);

	R_ResetArrayState();

	R_EnableColorArray(qfalse);

	R_EnableLighting(NULL, qfalse);

	R_EnableTexture(&texunit_lightmap, qfalse);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.f, -1.f);

	glMatrixMode(GL_TEXTURE);  /* some stages will manipulate texcoords */

	for (i = 0; i < surfs->count; i++) {
		materialStage_t *s;
		mBspSurface_t *surf = surfs->surfaces[i];
		material_t *m = &surf->texinfo->image->material;
		int j = -1;

		if (surf->frame != r_locals.frame)
			continue;

		R_UpdateMaterial(m);

		for (s = m->stages; s; s = s->next, j--) {
			if (!(s->flags & STAGE_RENDER))
				continue;

			R_SetSurfaceStageState(surf, s);

			R_DrawSurfaceStage(surf, s);
		}
	}

	R_Color(NULL);

	/* polygon offset parameters */
	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableFog(qtrue);

	R_EnableColorArray(qfalse);

	R_EnableTexture(&texunit_lightmap, qfalse);

	R_EnableBumpmap(NULL);

	R_EnableLighting(NULL, qfalse);

	R_EnableGlowMap(NULL);

	R_Color(NULL);
}

/**
 * @brief Translate string into glmode
 */
static GLenum R_ConstByName (const char *c)
{
	if (Q_streq(c, "GL_ONE"))
		return GL_ONE;
	if (Q_streq(c, "GL_ZERO"))
		return GL_ZERO;
	if (Q_streq(c, "GL_SRC_ALPHA"))
		return GL_SRC_ALPHA;
	if (Q_streq(c, "GL_ONE_MINUS_SRC_ALPHA"))
		return GL_ONE_MINUS_SRC_ALPHA;
	if (Q_streq(c, "GL_SRC_COLOR"))
		return GL_SRC_COLOR;
	if (Q_streq(c, "GL_DST_COLOR"))
		return GL_DST_COLOR;
	if (Q_streq(c, "GL_ONE_MINUS_SRC_COLOR"))
		return GL_ONE_MINUS_SRC_COLOR;
	if (Q_streq(c, "GL_ONE_MINUS_DST_COLOR"))
		return GL_ONE_MINUS_DST_COLOR;

	Com_Printf("R_ConstByName: Failed to resolve: %s\n", c);
	return GL_ZERO;
}

static void R_CreateMaterialData_ (model_t *mod)
{
	int i;

	for (i = 0; i < mod->bsp.numsurfaces; i++) {
		mBspSurface_t *surf = &mod->bsp.surfaces[i];
		/* create flare */
		R_CreateSurfaceFlare(surf);
	}
}

static void R_CreateMaterialData (void)
{
	int i;

	for (i = 0; i < r_numMapTiles; i++)
		R_CreateMaterialData_(r_mapTiles[i]);

	for (i = 0; i < r_numModelsInline; i++)
		R_CreateMaterialData_(&r_modelsInline[i]);
}

static int R_LoadAnimImages (materialStage_t *s)
{
	char name[MAX_QPATH];
	int i, j;

	if (!s->image) {
		Com_Printf("R_LoadAnimImages: Texture not defined in anim stage.\n");
		return -1;
	}

	Q_strncpyz(name, s->image->name, sizeof(name));
	j = strlen(name);

	if (name[j - 1] != '0') {
		Com_Printf("R_LoadAnimImages: Texture name does not end in 0: %s\n", name);
		return -1;
	}

	/* the first image was already loaded by the stage parse, so just copy
	 * the pointer into the images array */

	s->anim.images[0] = s->image;
	name[j - 1] = 0;

	/* now load the rest */
	for (i = 0; i < s->anim.num_frames; i++) {
		const char *c = va("%s%d", name, i);
		image_t *image = R_FindImage(c, it_material);
		s->anim.images[i] = image;
		if (image == r_noTexture) {
			Com_Printf("R_LoadAnimImages: Failed to resolve texture: %s\n", c);
			return -1;
		}
	}

	return 0;
}

/**
 * @brief Material stage parser
 * @sa R_LoadMaterials
 */
static int R_ParseStage (materialStage_t *s, const char **buffer)
{
	int i;

	while (qtrue) {
		const char *c = Com_Parse(buffer);

		if (c[0] == '\0')
			break;

		if (Q_streq(c, "glowscale")) {
			s->glowscale = atof(Com_Parse(buffer));
			if (s->glowscale < 0.0) {
				Com_Printf("R_LoadMaterials: Invalid glowscale value for %s\n", c);
				s->glowscale = defaultMaterial.glowscale;
			}
			continue;
		}

		if (Q_streq(c, "texture")) {
			c = Com_Parse(buffer);
			s->image = R_FindImage(va("textures/%s", c), it_material);

			if (s->image == r_noTexture) {
				Com_Printf("R_ParseStage: Failed to resolve texture: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (Q_streq(c, "envmap")) {
			c = Com_Parse(buffer);
			i = atoi(c);

			if (i > -1 && i < MAX_ENVMAPTEXTURES)
				s->image = r_envmaptextures[i];
			else
				s->image = R_FindImage(va("pics/envmaps/%s", c), it_material);

			if (s->image == r_noTexture) {
				Com_Printf("R_ParseStage: Failed to resolve envmap: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ENVMAP;
			continue;
		}

		if (Q_streq(c, "blend")) {
			c = Com_Parse(buffer);
			s->blend.src = R_ConstByName(c);

			if (s->blend.src == -1) {
				Com_Printf("R_ParseStage: Failed to resolve blend src: %s\n", c);
				return -1;
			}

			c = Com_Parse(buffer);
			s->blend.dest = R_ConstByName(c);

			if (s->blend.dest == -1) {
				Com_Printf("R_ParseStage: Failed to resolve blend dest: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_BLEND;
			continue;
		}

		if (Q_streq(c, "color")) {
			for (i = 0; i < 3; i++) {
				c = Com_Parse(buffer);
				s->color[i] = atof(c);

				if (s->color[i] < 0.0 || s->color[i] > 1.0) {
					Com_Printf("R_ParseStage: Failed to resolve color: %s\n", c);
					return -1;
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if (Q_streq(c, "pulse")) {
			c = Com_Parse(buffer);
			s->pulse.hz = atof(c);
			s->pulse.dutycycle = 1.0;

			if (s->pulse.hz < 0.0) {
				Com_Printf("R_ParseStage: Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_PULSE;
			continue;
		}

		if (Q_streq(c, "dutycycle")) {
			c = Com_Parse(buffer);
			s->pulse.dutycycle = atof(c);

			if (s->pulse.dutycycle < 0.0 || s->pulse.dutycycle > 1.0) {
				Com_Printf("R_ParseStage: Failed to resolve pulse duty cycle: %s\n", c);
				return -1;
			}

			continue;
		}

		if (Q_streq(c, "stretch")) {
			c = Com_Parse(buffer);
			s->stretch.amp = atof(c);

			if (s->stretch.amp < 0.0) {
				Com_Printf("R_ParseStage: Failed to resolve amplitude: %s\n", c);
				return -1;
			}

			c = Com_Parse(buffer);
			s->stretch.hz = atof(c);

			if (s->stretch.hz < 0.0) {
				Com_Printf("R_ParseStage: Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_STRETCH;
			continue;
		}

		if (Q_streq(c, "rotate")) {
			c = Com_Parse(buffer);
			s->rotate.hz = atof(c);

			if (s->rotate.hz < 0.0) {
				Com_Printf("R_ParseStage: Failed to resolve rotate: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ROTATE;
			continue;
		}

		if (Q_streq(c, "scroll.s")) {
			c = Com_Parse(buffer);
			s->scroll.s = atof(c);

			s->flags |= STAGE_SCROLL_S;
			continue;
		}

		if (Q_streq(c, "scroll.t")) {
			c = Com_Parse(buffer);
			s->scroll.t = atof(c);

			s->flags |= STAGE_SCROLL_T;
			continue;
		}

		if (Q_streq(c, "scale.s")) {
			c = Com_Parse(buffer);
			s->scale.s = atof(c);

			s->flags |= STAGE_SCALE_S;
			continue;
		}

		if (Q_streq(c, "scale.t")) {
			c = Com_Parse(buffer);
			s->scale.t = atof(c);

			s->flags |= STAGE_SCALE_T;
			continue;
		}

		if (Q_streq(c, "terrain")) {
			c = Com_Parse(buffer);
			s->terrain.floor = atof(c);

			c = Com_Parse(buffer);
			s->terrain.ceil = atof(c);
			if (s->terrain.ceil < s->terrain.floor) {
				Com_Printf("R_ParseStage: Inverted terrain ceiling and floor "
					"values for %s\n", (s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->terrain.height = s->terrain.ceil - s->terrain.floor;

			if (s->terrain.height == 0.0) {
				Com_Printf("R_ParseStage: Zero height terrain specified for %s\n",
					(s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->flags |= STAGE_TERRAIN;
			continue;
		}

		if (Q_streq(c, "tape")) {
			c = Com_Parse(buffer);
			s->tape.center = atof(c);

			/* how much downwards? */
			c = Com_Parse(buffer);
			s->tape.floor = atof(c);

			/* how much upwards? */
			c = Com_Parse(buffer);
			s->tape.ceil = atof(c);

			s->tape.min = s->tape.center - s->tape.floor;
			s->tape.max = s->tape.center + s->tape.ceil;
			s->tape.height = s->tape.floor + s->tape.ceil;

			if (s->tape.height == 0.0) {
				Com_Printf("R_ParseStage: Zero height tape specified for %s\n",
					(s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->flags |= STAGE_TAPE;
			continue;
		}

		if (Q_streq(c, "dirtmap")) {
			c = Com_Parse(buffer);
			s->dirt.intensity = atof(c);
			if (s->dirt.intensity <= 0.0 || s->dirt.intensity > 1.0) {
				Com_Printf("R_ParseStage: Invalid dirtmap intensity for %s\n",
					(s->image ? s->image->name : "NULL"));
				return -1;
			}
			s->flags |= STAGE_DIRTMAP;
			continue;
		}

		if (Q_strstart(c, "anim")) {
			switch (c[4]) {
			case 'a':
				s->anim.type = ANIM_ALTERNATE;
				break;
			case 'b':
				s->anim.type = ANIM_BACKWARDS;
				break;
			case 'r':
				s->anim.type = ANIM_RANDOM;
				break;
			case 'f':
				s->anim.type = ANIM_RANDOMFORCE;
				break;
			default:
				s->anim.type = ANIM_NORMAL;
				break;
			}
			c = Com_Parse(buffer);
			s->anim.num_frames = atoi(c);

			if (s->anim.num_frames < 1 || s->anim.num_frames > MAX_ANIM_FRAMES) {
				Com_Printf("R_ParseStage: Invalid number of anim frames for %s (max is %i)\n",
						(s->image ? s->image->name : "NULL"), MAX_ANIM_FRAMES);
				return -1;
			}

			c = Com_Parse(buffer);
			s->anim.fps = atof(c);

			if (s->anim.fps <= 0) {
				Com_Printf("R_ParseStage: Invalid anim fps for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			/* the frame images are loaded once the stage is parsed completely */

			s->flags |= STAGE_ANIM;
			continue;
		}

		if (Q_streq(c, "glowmaplink")) {
			s->flags |= STAGE_GLOWMAPLINK;
			continue;
		}

		if (Q_streq(c, "lightmap")) {
			s->flags |= STAGE_LIGHTMAP;
			continue;
		}

		if (Q_streq(c, "flare")) {
			c = Com_Parse(buffer);
			i = atoi(c);

			if (i > -1 && i < NUM_FLARETEXTURES)
				s->image = r_flaretextures[i];
			else
				s->image = R_FindImage(va("pics/flares/%s", c), it_material);

			if (s->image == r_noTexture) {
				Com_Printf("R_ParseStage: Failed to resolve flare: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_FLARE;
			continue;
		}

		if (*c == '}') {
			Com_DPrintf(DEBUG_RENDERER, "Parsed stage\n"
					"  flags: %d\n"
					"  image: %s\n"
					"  blend: %d %d\n"
					"  color: %3f %3f %3f\n"
					"  pulse: %3f\n"
					"  pulse duty cycle: %1.2f\n"
					"  stretch: %3f %3f\n"
					"  rotate: %3f\n"
					"  scroll.s: %3f\n"
					"  scroll.t: %3f\n"
					"  scale.s: %3f\n"
					"  scale.t: %3f\n"
					"  terrain.floor: %5f\n"
					"  terrain.ceil: %5f\n"
					"  anim.num_frames: %d\n"
					"  anim.fps: %3f\n",
					s->flags, (s->image ? s->image->name : "NULL"),
					s->blend.src, s->blend.dest,
					s->color[0], s->color[1], s->color[2],
					s->pulse.hz, s->pulse.dutycycle, s->stretch.amp, s->stretch.hz,
					s->rotate.hz, s->scroll.s, s->scroll.t,
					s->scale.s, s->scale.t, s->terrain.floor, s->terrain.ceil,
					s->anim.num_frames, s->anim.fps);

			/* a texture or envmap means render it */
			if (s->flags & (STAGE_TEXTURE | STAGE_ENVMAP))
				s->flags |= STAGE_RENDER;

			if (s->flags & (STAGE_TERRAIN | STAGE_DIRTMAP))
				s->flags |= STAGE_LIGHTING;

			return 0;
		}

		Com_Printf("Invalid token: '%s'\n", c);
	}

	Com_Printf("R_ParseStage: Malformed stage\n");
	return -1;
}

/**
 * @brief Load material definitions for each map that has one
 * @param[in] map the base name of the map to load the material for
 */
void R_LoadMaterials (const char *map)
{
	char path[MAX_QPATH];
	byte *fileBuffer;
	const char *buffer;
	qboolean inmaterial;
	image_t *image;
	material_t *m;
	materialStage_t *s, *ss;

	/* clear previously loaded materials */
	R_ImageClearMaterials();

	if (map[0] == '+' || map[0] == '-')
		map++;
	else if (map[0] == '-')
		return;

	/* load the materials file for parsing */
	Com_sprintf(path, sizeof(path), "materials/%s.mat", Com_SkipPath(map));

	if (FS_LoadFile(path, &fileBuffer) < 1) {
		Com_DPrintf(DEBUG_RENDERER, "Couldn't load %s\n", path);
		return;
	} else {
		Com_Printf("load material file: '%s'\n", path);
		if (!r_materials->integer)
			Com_Printf("...ignore materials (r_materials is deactivated)\n");
	}

	buffer = (const char *)fileBuffer;

	inmaterial = qfalse;
	image = NULL;
	m = NULL;

	while (qtrue) {
		const char *c = Com_Parse(&buffer);

		if (c[0] == '\0')
			break;

		if (*c == '{' && !inmaterial) {
			inmaterial = qtrue;
			continue;
		}

		if (Q_streq(c, "material")) {
			c = Com_Parse(&buffer);
			image = R_GetImage(va("textures/%s", c));
			if (image == NULL)
				Com_DPrintf(DEBUG_RENDERER, "R_LoadMaterials: skip texture: %s - not used in the map\n", c);

			continue;
		}

		if (!image)
			continue;

		m = &image->material;

		if (Q_streq(c, "normalmap")){
			c = Com_Parse(&buffer);
			image->normalmap = R_FindImage(va("textures/%s", c), it_normalmap);

			if (image->normalmap == r_noTexture){
				Com_Printf("R_LoadMaterials: Failed to resolve normalmap: %s\n", c);
				image->normalmap = NULL;
			}
		}

		if (Q_streq(c, "glowmap")){
			c = Com_Parse(&buffer);
			image->glowmap = R_FindImage(va("textures/%s", c), it_glowmap);

			if (image->glowmap == r_noTexture){
				Com_Printf("R_LoadMaterials: Failed to resolve glowmap: %s\n", c);
				image->glowmap = NULL;
			}
		}

		if (Q_streq(c, "bump")) {
			m->bump = atof(Com_Parse(&buffer));
			if (m->bump < 0.0) {
				Com_Printf("R_LoadMaterials: Invalid bump value for %s\n", image->name);
				m->bump = defaultMaterial.bump;
			}
		}

		if (Q_streq(c, "parallax")) {
			m->parallax = atof(Com_Parse(&buffer));
			if (m->parallax < 0.0) {
				Com_Printf("R_LoadMaterials: Invalid parallax value for %s\n", image->name);
				m->parallax = defaultMaterial.parallax;
			}
		}

		if (Q_streq(c, "hardness")) {
			m->hardness = atof(Com_Parse(&buffer));
			if (m->hardness < 0.0) {
				Com_Printf("R_LoadMaterials: Invalid hardness value for %s\n", image->name);
				m->hardness = defaultMaterial.hardness;
			}
		}

		if (Q_streq(c, "specular")) {
			m->specular = atof(Com_Parse(&buffer));
			if (m->specular < 0.0) {
				Com_Printf("R_LoadMaterials: Invalid specular value for %s\n", image->name);
				m->specular = defaultMaterial.specular;
			}
		}

		if (Q_streq(c, "glowscale")) {
			m->glowscale = atof(Com_Parse(&buffer));
			if (m->glowscale < 0.0) {
				Com_Printf("R_LoadMaterials: Invalid glowscale value for %s\n", image->name);
				m->glowscale = defaultMaterial.glowscale;
			}
		}

		if (*c == '{' && inmaterial) {
			s = (materialStage_t *)Mem_PoolAlloc(sizeof(*s), vid_imagePool, 0);
			s->glowscale = defaultMaterial.glowscale;

			if (R_ParseStage(s, &buffer) == -1) {
				Mem_Free(s);
				continue;
			}

			/* load animation frame images */
			if (s->flags & STAGE_ANIM) {
				if (R_LoadAnimImages(s) == -1) {
					Mem_Free(s);
					continue;
				}
			}

			/* append the stage to the chain */
			if (!m->stages)
				m->stages = s;
			else {
				ss = m->stages;
				while (ss->next)
					ss = ss->next;
				ss->next = s;
			}

			m->flags |= s->flags;
			m->num_stages++;
			continue;
		}

		if (*c == '}' && inmaterial) {
			Com_DPrintf(DEBUG_RENDERER, "Parsed material %s with %d stages\n", image->name, m->num_stages);
			inmaterial = qfalse;
			image = NULL;
			/* multiply stage glowscale values by material glowscale */
			ss = m->stages;
			while (ss) {
				ss->glowscale *= m->glowscale;
				ss = ss->next;
			}
		}
	}

	FS_FreeFile(fileBuffer);

	R_CreateMaterialData();
}

/**
 * @brief Change listener callback for material value cvars
 */
void R_UpdateDefaultMaterial (const char *cvarName, const char *oldValue, const char *newValue, void *data)
{
	defaultMaterial.specular = r_default_specular->value;
	defaultMaterial.hardness = r_default_hardness->value;
}
