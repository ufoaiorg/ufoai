/**
 * @file r_image.c
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
#include "r_error.h"

/* useful for particles, pics, etc.. */
const float default_texcoords[] = {
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0
};

/**
 * @brief Returns qfalse if the texunit is not supported
 */
qboolean R_SelectTexture (gltexunit_t *texunit)
{
	if (texunit == r_state.active_texunit)
		return qtrue;

	/* not supported */
	if (texunit->texture >= r_config.maxTextureCoords + GL_TEXTURE0_ARB)
		return qfalse;

	r_state.active_texunit = texunit;

	qglActiveTexture(texunit->texture);
	qglClientActiveTexture(texunit->texture);
	return qtrue;
}

static void R_BindTexture_ (int texnum)
{
	if (texnum == r_state.active_texunit->texnum)
		return;

	assert(texnum > 0);

	r_state.active_texunit->texnum = texnum;

	glBindTexture(GL_TEXTURE_2D, texnum);
#ifdef PARANOID
	R_CheckError();
#endif
}

void R_BindTextureDebug (int texnum, const char *file, int line, const char *function)
{
	if (texnum <= 0) {
		Com_Printf("Bad texnum (%d) in: %s (%d): %s\n", texnum, file, line, function);
	}
	R_BindTexture_(texnum);
}

void R_BindTextureForTexUnit (GLuint texnum, gltexunit_t *texunit)
{
	/* small optimization to save state changes */
	if (texnum == texunit->texnum)
		return;

	R_SelectTexture(texunit);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);
}

void R_BindLightmapTexture (GLuint texnum)
{
	R_BindTextureForTexUnit(texnum, &texunit_lightmap);
}

void R_BindDeluxemapTexture (GLuint texnum)
{
	R_BindTextureForTexUnit(texnum, &texunit_deluxemap);
}

void R_BindNormalmapTexture (GLuint texnum)
{
	R_BindTextureForTexUnit(texnum, &texunit_normalmap);
}

void R_BindArray (GLenum target, GLenum type, const void *array)
{
	switch (target) {
	case GL_VERTEX_ARRAY:
		glVertexPointer(3, type, 0, array);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		glTexCoordPointer(2, type, 0, array);
		break;
	case GL_COLOR_ARRAY:
		glColorPointer(4, type, 0, array);
		break;
	case GL_NORMAL_ARRAY:
		glNormalPointer(type, 0, array);
		break;
	case GL_TANGENT_ARRAY:
		R_AttributePointer("TANGENTS", 4, array);
		break;
	case GL_NEXT_VERTEX_ARRAY:
		R_AttributePointer("NEXT_FRAME_VERTS", 3, array);
		break;
	case GL_NEXT_NORMAL_ARRAY:
		R_AttributePointer("NEXT_FRAME_NORMALS", 3, array);
		break;
	case GL_NEXT_TANGENT_ARRAY:
		R_AttributePointer("NEXT_FRAME_TANGENTS", 4, array);
		break;
	}
}

/**
 * @brief Binds the appropriate shared vertex array to the specified target.
 */
void R_BindDefaultArray (GLenum target)
{
	switch (target) {
	case GL_VERTEX_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.vertex_array_3d);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.active_texunit->texcoord_array);
		break;
	case GL_COLOR_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.color_array);
		break;
	case GL_NORMAL_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.normal_array);
		break;
	case GL_TANGENT_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.tangent_array);
		break;
	case GL_NEXT_VERTEX_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.next_vertex_array_3d);
		break;
	case GL_NEXT_NORMAL_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.next_normal_array);
		break;
	case GL_NEXT_TANGENT_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.next_tangent_array);
		break;
	}
}

void R_BindBuffer (GLenum target, GLenum type, GLuint id)
{
	if (!qglBindBuffer)
		return;

	if (!r_vertexbuffers->integer)
		return;

	qglBindBuffer(GL_ARRAY_BUFFER, id);

	if (type && id)  /* assign the array pointer as well */
		R_BindArray(target, type, NULL);
}

void R_BlendFunc (GLenum src, GLenum dest)
{
	if (r_state.blend_src == src && r_state.blend_dest == dest)
		return;

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	glBlendFunc(src, dest);
}

void R_EnableBlend (qboolean enable)
{
	if (r_state.blend_enabled == enable)
		return;

	r_state.blend_enabled = enable;

	if (enable) {
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
	} else {
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
}

void R_EnableAlphaTest (qboolean enable)
{
	if (r_state.alpha_test_enabled == enable)
		return;

	r_state.alpha_test_enabled = enable;

	if (enable)
		glEnable(GL_ALPHA_TEST);
	else
		glDisable(GL_ALPHA_TEST);
}

void R_EnableTexture (gltexunit_t *texunit, qboolean enable)
{
	if (enable == texunit->enabled)
		return;

	texunit->enabled = enable;

	R_SelectTexture(texunit);

	if (enable) {
		/* activate texture unit */
		glEnable(GL_TEXTURE_2D);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		if (texunit == &texunit_lightmap) {
			if (r_debug_lightmaps->integer)
				R_TexEnv(GL_REPLACE);
			else
				R_TexEnv(GL_MODULATE);
		}
	} else {
		/* disable on the second texture unit */
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	R_SelectTexture(&texunit_diffuse);
}

void R_EnableColorArray (qboolean enable)
{
	if (r_state.color_array_enabled == enable)
		return;

	r_state.color_array_enabled = enable;

	if (enable)
		glEnableClientState(GL_COLOR_ARRAY);
	else
		glDisableClientState(GL_COLOR_ARRAY);
}

/**
 * @brief Enables hardware-accelerated lighting with the specified program.  This
 * should be called after any texture units which will be active for lighting
 * have been enabled.
 */
qboolean R_EnableLighting (r_program_t *program, qboolean enable)
{
	if (!r_programs->integer)
		return qfalse;

	if (enable && (!program || !program->id))
		return r_state.lighting_enabled;

	//if (program == r_state.world_program)
	//	program = r_state.battlescape_program;


	if (!r_lights->integer || (r_state.lighting_enabled == enable && r_state.active_program == program))
		return r_state.lighting_enabled;

	r_state.lighting_enabled = enable;

	if (enable) {  /* toggle state */
		R_UseProgram(program);

		glEnableClientState(GL_NORMAL_ARRAY);

		if (r_state.active_program == r_state.world_program || r_state.battlescape_program) {
			R_EnableDynamicLights(r_state.active_entity, qtrue);
			if (r_state.build_shadowmap_enabled)
				R_ProgramParameter1i("BUILD_SHADOWMAP", 1);
			else 
				R_ProgramParameter1i("BUILD_SHADOWMAP", 0);
		}
	} else {
		glDisableClientState(GL_NORMAL_ARRAY);

		if (r_state.active_program == r_state.world_program || r_state.battlescape_program)
			R_EnableDynamicLights(NULL, qfalse);

		R_UseProgram(NULL);
	}

	return r_state.lighting_enabled;
}

void R_EnableLightmap (qboolean enable)
{
	if (!r_state.lighting_enabled)
		return;
	if (enable)
		R_ProgramParameter1i("LIGHTMAP", 1);
	else
		R_ProgramParameter1i("LIGHTMAP", 0);
}

/**  
 * @brief Enable or disable realtime dynamic lighting
 * @param ent The entity to enable/disable lighting for
 * @param enable Whether to turn realtime lighting on or off
 */
void R_EnableDynamicLights (entity_t *ent, qboolean enable)
{
	int i, j;
	r_light_t *l;
	int maxLights = r_dynamic_lights->integer;

	if (enable)
		r_state.active_entity = ent;

	if (!enable || !r_state.lighting_enabled || r_state.numActiveLights == 0 || !ent) {
		if (r_state.lighting_enabled && r_shadowmapping->integer) {
			R_ProgramParameter1i("SHADOWMAP", 0);
		}
		if (!r_state.bumpmap_enabled && r_state.lighting_enabled)
			R_DisableAttribute("TANGENTS");
		glDisable(GL_LIGHTING);
		for (i = 0; i < maxLights; i++) {
			glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
			glDisable(GL_LIGHT0 + i);
		}

		r_state.dynamic_lighting_enabled = qfalse;
		return;
	}

	if (!ent)
		ent = &r_state.world_entity;

	r_state.dynamic_lighting_enabled = qtrue;

	R_EnableAttribute("TANGENTS");

	R_ProgramParameter1f("HARDNESS", 0.1);
	R_ProgramParameter1f("SPECULAR", 0.25);

	if (r_state.use_shadowmap_enabled) {
		R_ProgramParameter1i("SHADOWMAP", 1);
		R_BindTextureForTexUnit(r_state.shadowmapBuffer->textures[0], &texunit_shadowmap);
		//R_BindTextureForTexUnit(r_state.shadowmapBuffer->depth, &texunit_shadowmap);
	}

	glEnable(GL_LIGHTING);

	for (i = 0, j = 0; i < maxLights && (i + j) < ent->numLights; i++) {
		l = ent->lights[i + j];
		if (!l->enabled) {
			j++;
			continue;
		}

		glEnable(GL_LIGHT0 + i);
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, l->constantAttenuation);
		glLightf(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, l->linearAttenuation);
		glLightf(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, l->quadraticAttenuation);

		glLightfv(GL_LIGHT0 + i, GL_POSITION, l->loc);
		glLightfv(GL_LIGHT0 + i, GL_AMBIENT, l->ambientColor);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, l->diffuseColor);
		glLightfv(GL_LIGHT0 + i, GL_SPECULAR, l->specularColor);
	}

	/* if there aren't enough active lights, turn off the rest */
	while (i < maxLights) {
		glDisable(GL_LIGHT0 + i);
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
		glLightf(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, 0.0);
		glLightf(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, 0.0);
		i++;
	}
}

/** 
 * @brief Enables animation using keyframe interpolation on the GPU
 * @param mesh The mesh to animate
 * @param backlerp The temporal proximity to the previous keyframe (in the range 0.0 to 1.0)
 * @param enable Whether to turn animation on or off
 */
void R_EnableAnimation (const mAliasMesh_t *mesh, float backlerp, qboolean enable)
{
	/* @todo fix this for shadowmaps */
	if (!r_programs->integer || !r_state.lighting_enabled)
		return;

	r_state.animation_enabled = enable;
	
	if (enable) {
		R_EnableAttribute("NEXT_FRAME_VERTS");
		R_EnableAttribute("NEXT_FRAME_NORMALS");
		R_EnableAttribute("NEXT_FRAME_TANGENTS");
		R_ProgramParameter1i("ANIMATE", 1);

		R_ProgramParameter1f("TIME", backlerp);

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mesh->texcoords);
		R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mesh->verts);
		R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mesh->normals);
		R_BindArray(GL_TANGENT_ARRAY, GL_FLOAT, mesh->tangents);
		R_BindArray(GL_NEXT_VERTEX_ARRAY, GL_FLOAT, mesh->next_verts);
		R_BindArray(GL_NEXT_NORMAL_ARRAY, GL_FLOAT, mesh->next_normals);
		R_BindArray(GL_NEXT_TANGENT_ARRAY, GL_FLOAT, mesh->next_tangents);
	} else {
		R_DisableAttribute("NEXT_FRAME_VERTS");
		R_DisableAttribute("NEXT_FRAME_NORMALS");
		R_DisableAttribute("NEXT_FRAME_TANGENTS");
		R_ProgramParameter1i("ANIMATE", 0);
	}
}

/**
 * @brief Enables bumpmapping and binds the given normalmap.
 * @note Don't forget to bind the deluxe map, too.
 * @sa R_BindDeluxemapTexture
 */
void R_EnableBumpmap (const image_t *normalmap, qboolean enable)
{
	if (!r_state.lighting_enabled)
		return;

	if (!r_bumpmap->value)
		return;

	if (enable)
		R_BindNormalmapTexture(normalmap->texnum);

	if (r_state.bumpmap_enabled == enable)
		return;

	r_state.bumpmap_enabled = enable;

	if (enable) {  /* toggle state */
		assert(normalmap);
		R_EnableAttribute("TANGENTS");
		R_ProgramParameter1i("BUMPMAP", 1);
		/* default parameters to use if no material gets loaded */
		R_ProgramParameter1f("HARDNESS", 0.1);
		R_ProgramParameter1f("SPECULAR", 0.25);
		R_ProgramParameter1f("BUMP", 1.0);
		R_ProgramParameter1f("PARALLAX", 0.5);
	} else {
		R_DisableAttribute("TANGENTS");
		R_ProgramParameter1i("BUMPMAP", 0);
	}
}

void R_EnableWarp (r_program_t *program, qboolean enable)
{
	if (!r_programs->integer)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_warp->integer || r_state.warp_enabled == enable)
		return;

	r_state.warp_enabled = enable;

	R_SelectTexture(&texunit_lightmap);

	if (enable) {
		glEnable(GL_TEXTURE_2D);
		R_BindTexture(r_warpTexture->texnum);
		R_UseProgram(program);
	} else {
		glDisable(GL_TEXTURE_2D);
		R_UseProgram(NULL);
	}

	R_SelectTexture(&texunit_diffuse);
}

void R_EnableBlur (r_program_t *program, qboolean enable, r_framebuffer_t *source, r_framebuffer_t *dest, int dir)
{
	if (!r_programs->integer || !r_postprocess->integer)
		return;

	if (enable && (!program || !program->id))
		return;


	R_SelectTexture(&texunit_lightmap);

	if (enable) {
		float userdata[] = { source->width, dir };
		R_UseFramebuffer(dest);
		program->userdata = userdata;
		R_UseProgram(program);
	} else {
		R_UseFramebuffer(fbo_screen);
		R_UseProgram(NULL);
	}

	R_SelectTexture(&texunit_diffuse);
	r_state.blur_enabled = enable;
}

void R_EnableShell (qboolean enable)
{
	if (enable == r_state.shell_enabled)
		return;

	r_state.shell_enabled = enable;

	if (enable) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0, 1.0);

		R_EnableDrawAsGlow(qtrue);
		R_EnableBlend(qtrue);
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

		if (r_state.lighting_enabled)
			R_ProgramParameter1f("OFFSET", refdef.time / 3.0);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		R_EnableBlend(qfalse);
		R_EnableDrawAsGlow(qfalse);

		glPolygonOffset(0.0, 0.0);
		glDisable(GL_POLYGON_OFFSET_FILL);

		if (r_state.lighting_enabled)
			R_ProgramParameter1f("OFFSET", 0.0);
	}
}

#define FOG_START	300.0
#define FOG_END		2500.0

void R_EnableFog (qboolean enable)
{
	if (!r_fog->integer || r_state.fog_enabled == enable)
		return;

	r_state.fog_enabled = qfalse;

	if (enable) {
		if ((refdef.weather & WEATHER_FOG) || r_fog->integer == 2) {
			r_state.fog_enabled = qtrue;

			glFogfv(GL_FOG_COLOR, refdef.fogColor);
			glFogf(GL_FOG_DENSITY, 1.0);
			glEnable(GL_FOG);
		}
	} else {
		glFogf(GL_FOG_DENSITY, 0.0);
		glDisable(GL_FOG);
	}
}

void R_EnableGlowMap (const image_t *image, qboolean enable)
{
	static GLenum glowRenderTarget = GL_COLOR_ATTACHMENT1_EXT;

	if (!r_postprocess->integer || r_state.build_shadowmap_enabled)
		return;

	if (enable && image != NULL)
		R_BindTextureForTexUnit(image->texnum, &texunit_glowmap);

	if (r_state.glowmap_enabled == enable)
		return;

	r_state.glowmap_enabled = enable;

	if (enable) {
		if (!r_state.active_program)
			R_UseProgram(r_state.simple_glow_program);

		if (image == NULL)
			R_ProgramParameter1f("GLOWSCALE", 0.0);
		else
			R_ProgramParameter1f("GLOWSCALE", 1.0);

		R_DrawBuffers(2);
	} else {
		if (r_state.active_program == r_state.simple_glow_program)
			R_UseProgram(NULL);
		else
			R_ProgramParameter1f("GLOWSCALE", 0.0);

		if (r_state.draw_glow_enabled)
			R_BindColorAttachments(1, &glowRenderTarget);
		else
			R_DrawBuffers(1);
	}
}

void R_EnableDrawAsGlow (qboolean enable)
{
	static GLenum glowRenderTarget = GL_COLOR_ATTACHMENT1_EXT;

	if (!r_postprocess->integer || r_state.draw_glow_enabled == enable || r_state.build_shadowmap_enabled)
		return;

	r_state.draw_glow_enabled = enable;

	if (enable) {
		R_BindColorAttachments(1, &glowRenderTarget);
	} else {
		if (r_state.glowmap_enabled) {
			R_DrawBuffers(2);
		} else {
			R_DrawBuffers(1);
		}
	}
}

void R_EnableSpecularMap (const image_t *image, qboolean enable)
{
	if (!r_state.lighting_enabled)
		return;

	if (enable && image != NULL) {
		R_BindTextureForTexUnit(image->texnum, &texunit_specularmap);
		R_ProgramParameter1i("SPECULARMAP", 1);
		r_state.specularmap_enabled = enable;
	} else {
		R_ProgramParameter1i("SPECULARMAP", 0);
		r_state.specularmap_enabled = qfalse;
	}
}

void R_EnableRoughnessMap (const image_t *image, qboolean enable)
{
	if (!r_state.lighting_enabled)
		return;

	if (enable && image != NULL) {
		R_BindTextureForTexUnit(image->texnum, &texunit_roughnessmap);
		R_ProgramParameter1i("ROUGHMAP", 1);
		r_state.roughnessmap_enabled = enable;
	} else {
		R_ProgramParameter1i("ROUGHMAP", 0);
		r_state.roughnessmap_enabled = qfalse;
	}
}


/**
 * @sa R_Setup3D
 * @sa R_EnableBuildShadowmap
 */
static void MYgluPerspective (GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax, yaspect = (double) viddef.viewHeight / viddef.viewWidth;

	if (r_isometric->integer) {
		glOrtho(-10 * refdef.fieldOfViewX, 10 * refdef.fieldOfViewX, -10 * refdef.fieldOfViewX * yaspect, 10 * refdef.fieldOfViewX * yaspect, -zFar, zFar);
	} else {
		xmax = zNear * tan(refdef.fieldOfViewX * (M_PI / 360.0));
		xmin = -xmax;

		ymin = xmin * yaspect;
		ymax = xmax * yaspect;

		glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}


/**
 * @sa R_EnableBuildShadowmap
 */
static void MYgluLookAt (const vec3_t eye, const vec3_t center, const vec3_t up)
{
	GLfloat m[16];
	vec3_t x, y, z;

	/* Make rotation matrix */
	VectorSubtract(eye, center, z);
	VectorNormalize(z);
	CrossProduct(up, z, x);
	VectorNormalize(x);
	CrossProduct(z, x, y);
	VectorNormalize(y);

	/* build 4x4 transform matrix*/
#define M(row,col)  m[col*4+row]
	M(0, 0) = x[0];
	M(0, 1) = x[1];
	M(0, 2) = x[2];
	M(0, 3) = 0.0;
	M(1, 0) = y[0];
	M(1, 1) = y[1];
	M(1, 2) = y[2];
	M(1, 3) = 0.0;
	M(2, 0) = z[0];
	M(2, 1) = z[1];
	M(2, 2) = z[2];
	M(2, 3) = 0.0;
	M(3, 0) = 0.0;
	M(3, 1) = 0.0;
	M(3, 2) = 0.0;
	M(3, 3) = 1.0;
#undef M
	glMultMatrixf(m);

	/* Translate Eye to Origin */
	glTranslatef(-eye[0], -eye[1], -eye[2]);
}

#define MAX_SHADOW_SIZE 1600
#define SHADOW_SPLITS 5
#define MAT_I4  { 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0 }

void R_EnableBuildShadowmap (qboolean enable, const r_light_t *light) {
	static vec3_t viewOriginCamera;
	int i, j, k;

	if (!r_shadowmapping->integer)
		return;

	if (r_state.lighting_enabled) {
		Com_Printf("Error: R_EnableBuildShadowmap called when lighting was active!\n");
		return;
	}

	if (r_state.build_shadowmap_enabled == enable)
		return;

	r_state.build_shadowmap_enabled = enable;

	if (enable) {
		float modelView[16];
		float projection[16];
		float M1[16] = MAT_I4;
		float M2[16] = MAT_I4;
		float M3[16] = MAT_I4;
		const GLfloat bias[16] = {	0.5, 0.0, 0.0, 0.0, 
									0.0, 0.5, 0.0, 0.0,
									0.0, 0.0, 0.5, 0.0,
									0.5, 0.5, 0.5, 1.0 };
		vec3_t up = {0.0, 0.0, 1.0};
		vec3_t target, lightDir, lightPos;
		vec3_t left, right, midpoint, v;
		float scale, dist;
		vec3_t R1[3];
		vec3_t R2[3];
		GLdouble zNear, zFar;
		vec3_t maxs[SHADOW_SPLITS];
		vec3_t mins[SHADOW_SPLITS];
		float clip;

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glRotatef(-90.0, 1.0, 0.0, 0.0);	/* put Z going up */
		glRotatef(90.0, 0.0, 0.0, 1.0);	/* put Z going up */
		glRotatef(-refdef.viewAngles[2], 1.0, 0.0, 0.0);
		glRotatef(-refdef.viewAngles[0], 0.0, 1.0, 0.0);
		glRotatef(-refdef.viewAngles[1], 0.0, 0.0, 1.0);
		glTranslatef(-refdef.viewOrigin[0], -refdef.viewOrigin[1], -refdef.viewOrigin[2]);

		/* retrieve the resulting matrix for other manipulations  */
		glGetFloatv(GL_MODELVIEW_MATRIX, r_locals.world_matrix);
		glPopMatrix();


		if (r_isometric->integer) {
			//glOrtho(-10 * refdef.fieldOfViewX, 10 * refdef.fieldOfViewX, -10 * refdef.fieldOfViewX * yaspect, 10 * refdef.fieldOfViewX * yaspect, -zFar, zFar);
			/* @todo - handle this case */
			Com_Printf("TODO: handle isometric case for shadowmaps\n");
		} else {
			zNear = 4.0;
			zFar = MAX_WORLD_WIDTH; /*< @todo - try to make this tighter if possible */

			for (i = 0; i < SHADOW_SPLITS; i++) {
				float Clin = zNear + (zFar - zNear) * ((float)i / (SHADOW_SPLITS - 1.0));
				float Clog = zNear * pow((zFar / zNear), (float)i / (SHADOW_SPLITS - 1.0));
				float weight = 0.9;
				vec3_t point;

				clip = weight * Clin + (1.0 - weight) * Clog;

				VectorSet(maxs[i], -HUGE_VAL, -HUGE_VAL, -HUGE_VAL);
				VectorSet(mins[i], HUGE_VAL, HUGE_VAL, HUGE_VAL);
				VectorMul(zNear, r_locals.frustum[j].normal, point);
				for (k = 0; k < 3; k++) {
					maxs[i][k] = (maxs[i][k] > point[k]) ? maxs[i][k] : point[k];
					mins[i][k] = (mins[i][k] < point[k]) ? mins[i][k] : point[k];
					maxs[i][k] = (maxs[i][k] > point[k]) ? maxs[i][k] : point[k];
					mins[i][k] = (mins[i][k] < point[k]) ? mins[i][k] : point[k];
				}

				for (j = 0; j < 4; j++) {
					VectorMul(clip, r_locals.frustum[j].normal, point);
					for (k = 0; k < 3; k++) {
						maxs[i][k] = (maxs[i][k] > point[k]) ? maxs[i][k] : point[k];
						mins[i][k] = (mins[i][k] < point[k]) ? mins[i][k] : point[k];
						maxs[i][k] = (maxs[i][k] > point[k]) ? maxs[i][k] : point[k];
						mins[i][k] = (mins[i][k] < point[k]) ? mins[i][k] : point[k];
					}
					
				}

				
				//Com_Printf("clip=%f\n", clip);
				//Print3Vector(mins[i]);
				//Print3Vector(maxs[i]);
				//glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);

			}
		}


		R_UseProgram(r_state.world_program);
		R_ProgramParameter3fvs("SHADOW_SPLIT_MINS", SHADOW_SPLITS, mins);
		R_ProgramParameter2fvs("SHADOW_SPLIT_MAXS", SHADOW_SPLITS, maxs);


		R_EnableShadowbuffer(qtrue);
		//R_UseProgram(r_state.build_shadowmap_program);
		glEnable(GL_DEPTH_TEST);
		/* don't render front faces to reduce aliasing artifacts */
		//glEnable(GL_CULL_FACE);
		//glCullFace(GL_FRONT);

		scale = - refdef.viewOrigin[2] / r_locals.frustum[0].normal[2];
		for (i = 0; i < 3; i++) 
			right[i] = refdef.viewOrigin[i] + scale * r_locals.frustum[0].normal[i];

		scale = - refdef.viewOrigin[2] / r_locals.frustum[1].normal[2];
		for (i = 0; i < 3; i++) 
			left[i] = refdef.viewOrigin[i] + scale * r_locals.frustum[1].normal[i];

		scale = - refdef.viewOrigin[2] / r_locals.frustum[3].normal[2];
		for (i = 0; i < 3; i++) 
			midpoint[i] = refdef.viewOrigin[i] + scale * r_locals.frustum[3].normal[i];

		scale = - refdef.viewOrigin[2] / r_locals.forward[2];
		for (i = 0; i < 3; i++) 
			target[i] = refdef.viewOrigin[i] + scale * r_locals.forward[i];

		VectorSubtract(right, left, v);
		dist = VectorLength(v) / 3.0;

		dist = (dist > MAX_SHADOW_SIZE) ? MAX_SHADOW_SIZE : dist;

		VectorSubtract(target, midpoint, v);
		VectorNormalize(v);
		VectorMul(dist, v, target);
		VectorAdd(midpoint, target, target);
		
		/* set up matrixes for the light source as viewpoint */
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		if (light->loc[3] == 0) {
			//glOrtho(-1000, 1000, -0000, 2000,  -1000, 1000);
			//glOrtho(-MAX_WORLD_WIDTH, MAX_WORLD_WIDTH, -MAX_WORLD_WIDTH, MAX_WORLD_WIDTH,  -MAX_WORLD_WIDTH, MAX_WORLD_WIDTH);
			//glOrtho(-dist, dist, -dist, dist,  -dist/4.0, dist/4.0);
			glOrtho(-dist, dist, -dist, dist,  -200.0, 10.0);
			//glOrtho(mins[4][0], maxs[4][0], mins[4][1], maxs[4][1], mins[4][2], maxs[4][2]);
			glRotatef(90.0 - refdef.viewAngles[1], 0.0, 0.0, 1.0);
			R_UseViewport(r_state.shadowmapBuffer);
		} else {
			MYgluPerspective(4.0, MAX_WORLD_WIDTH);
		}

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		VectorCopy(light->loc, lightPos);
		if (light->loc[3] == 0) {
			VectorMul(-1.0, lightPos, lightPos);
			VectorNormalize2(lightPos, lightDir);
			VectorAdd(lightPos, target, lightPos);
		} else {
			VectorSubtract(target, lightPos, lightDir);
			VectorNormalize(lightDir);
		}

		if (DotProduct(lightDir, up) > 0.75)
			VectorSet(up, 0.0, 1.0, 0.0);
		MYgluLookAt(lightPos, target, up); 

		/* save the camera origin, and then use the light position as the effective view origin */
		VectorCopy(refdef.viewOrigin, viewOriginCamera);
		VectorCopy(lightPos, refdef.viewOrigin);

		//VectorCopy(lightDir, r_locals.forward);


		/* @todo fix frustum stuff */
		VectorSet(r_locals.frustum[0].normal, 1.0, 0.0, 0.0);
		r_locals.frustum[0].type = PLANE_ANYZ;
		VectorSet(r_locals.frustum[1].normal, -1.0, 0.0, 0.0);
		r_locals.frustum[1].type = PLANE_ANYZ;
		VectorSet(r_locals.frustum[2].normal, 0.0, 1.0, 0.0);
		r_locals.frustum[2].type = PLANE_ANYZ;
		VectorSet(r_locals.frustum[3].normal, 0.0, -1.0, 0.0);
		r_locals.frustum[3].type = PLANE_ANYZ;

		//VectorScale(r_locals.right, +1, r_locals.frustum[0].normal);
		//VectorScale(r_locals.right, -1, r_locals.frustum[1].normal);
		//VectorScale(r_locals.up, +1, r_locals.frustum[2].normal);
		//VectorScale(r_locals.up, -1, r_locals.frustum[3].normal);

		VectorSet(up, 0.0, 0.0, 1.0);
		for (i = 0; i < 4; i++) {
			RotatePointAroundVector(r_locals.frustum[i].normal, lightDir, r_locals.frustum[i].normal, -(90 - refdef.viewAngles[1]));
			//r_locals.frustum[i].dist = -DotProduct(target, r_locals.frustum[i].normal);
			r_locals.frustum[i].dist = -DotProduct(target, r_locals.frustum[i].normal);
			//r_locals.frustum[i].dist = 0.0001;
			//r_locals.frustum[i].dist = -dist;
			//r_locals.frustum[i].dist = -MAX_WORLD_WIDTH;
		}
		
		/* @todo make modelView be the mat. to take camera coords to light coords */

		glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
		glGetFloatv(GL_PROJECTION_MATRIX, projection);

		//Com_Printf("M1=:\n");
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				M1[i*4+j] = r_locals.world_matrix[j*4+i];
				//M2[i*4+j] = modelView[i*4+j];
				//R1[i][j] = r_locals.world_matrix[j*4+i];
				//R2[i][j] = modelView[i*4+j];
				//Com_Printf("%f, ", M1[i*4+j]);
			}
			//Com_Printf("\n");
		}

		//GLMatrixMultiply(M1, M2, M3);

		//VectorSubtract(lightPos, viewOriginCamera, v);
		for (i = 0; i < 3; i++) {
			//M1[(i*4)-1] = v[i];
			//M1[12+i] = v[i];
			M1[12+i] = viewOriginCamera[i];
		}
		
		/*
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 4; j++) {
				M3[4*i+j] = r_locals.world_matrix[j*4+i];
			}
		}
		*/

		//Matrix4x4_Invert_Full(r_locals.world_matrix, M2);
		//Matrix4x4_Invert_Full(r_locals.proj_matrix, M3);

		/* @todo we don't really need to use GL_TEXTURE7 here */
		glActiveTexture(GL_TEXTURE7);
		glMatrixMode(GL_TEXTURE);
		//glPushMatrix();
		glLoadIdentity();
		glLoadMatrixf(bias);
		glMultMatrixf(projection);
		glMultMatrixf(modelView);
		glMultMatrixf(M1);

		//glMultMatrixf(M2);
		//glMultMatrixf(M3);
		//glPopMatrix();
		
		glActiveTexture(GL_TEXTURE0);
		glMatrixMode(GL_MODELVIEW);

	} else {
		/* Re-enable standard rendering */
		//glCullFace(GL_BACK);
		//glDisable(GL_CULL_FACE);
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

		/* restore camera as the view origin */
		VectorCopy(viewOriginCamera, refdef.viewOrigin);
		R_UseProgram(default_program);
		R_EnableShadowbuffer(qfalse);
	}

	R_CheckError();
}

void R_EnableUseShadowmap (qboolean enable)
{
	if (!r_shadowmapping->integer)
		return;

	if (r_state.use_shadowmap_enabled == enable)
		return;

	r_state.use_shadowmap_enabled = enable;


	if (r_state.lighting_enabled) {
		if (enable) {
			R_ProgramParameter1i("SHADOWMAP", 1);
			R_BindTextureForTexUnit(r_state.shadowmapBuffer->textures[0], &texunit_shadowmap);
			//R_BindTextureForTexUnit(r_state.shadowmapBuffer->depth, &texunit_shadowmap);
		} else {
			R_ProgramParameter1i("SHADOWMAP", 0);
		}
	}
}


/**
 * @sa R_Setup2D
 */
void R_Setup3D (void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(viddef.x * viddef.width / viddef.width);
	x2 = ceil((viddef.x + viddef.viewWidth) * viddef.width / viddef.width);
	y = floor(viddef.height - viddef.y * viddef.height / viddef.height);
	y2 = ceil(viddef.height - (viddef.y + viddef.viewHeight) * viddef.height / viddef.height);

	w = x2 - x;
	h = y - y2;

	glViewport(x, y2, w, h);
	R_CheckError();

	/* set up projection matrix */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	MYgluPerspective(4.0, MAX_WORLD_WIDTH);
	glGetFloatv(GL_PROJECTION_MATRIX, r_locals.proj_matrix);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90.0, 1.0, 0.0, 0.0);	/* put Z going up */
	glRotatef(90.0, 0.0, 0.0, 1.0);	/* put Z going up */
	glRotatef(-refdef.viewAngles[2], 1.0, 0.0, 0.0);
	glRotatef(-refdef.viewAngles[0], 0.0, 1.0, 0.0);
	glRotatef(-refdef.viewAngles[1], 0.0, 0.0, 1.0);
	glTranslatef(-refdef.viewOrigin[0], -refdef.viewOrigin[1], -refdef.viewOrigin[2]);

	/* retrieve the resulting matrix for other manipulations  */
	glGetFloatv(GL_MODELVIEW_MATRIX, r_locals.world_matrix);

	/* set vertex array pointer */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);

	glEnable(GL_DEPTH_TEST);

	/* set up framebuffers for postprocessing */
	R_EnableRenderbuffer(qtrue);

	R_CheckError();
}

extern const float SKYBOX_DEPTH;

/**
 * @sa R_Setup3D
 */
void R_Setup2D (void)
{
	/* set 2D virtual screen size */
	glViewport(0, 0, viddef.width, viddef.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	/* switch to orthographic (2 dimensional) projection
	 * don't draw anything before skybox */
	glOrtho(0, viddef.width, viddef.height, 0, 9999.0f, SKYBOX_DEPTH);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	/* bind default vertex array */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_Color(NULL);

	glEnable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);

	/* disable render-to-framebuffer */
	R_EnableRenderbuffer(qfalse);

	R_CheckError();
}

/* global ambient lighting */
static const vec4_t ambient = {
	0.0, 0.0, 0.0, 1.0
};

/* material reflection */
static const vec4_t material = {
	1.0, 1.0, 1.0, 1.0
};

void R_SetDefaultState (void)
{
	int i;

	glClearColor(0, 0, 0, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	/* setup vertex array pointers */
	glEnableClientState(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_EnableColorArray(qtrue);
	R_BindDefaultArray(GL_COLOR_ARRAY);
	R_EnableColorArray(qfalse);

	glEnableClientState(GL_NORMAL_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	/* reset gl error state */
	R_CheckError();

	/* setup texture units */
	for (i = 0; i < r_config.maxTextureCoords && i < MAX_GL_TEXUNITS; i++) {
		gltexunit_t *tex = &r_state.texunits[i];
		tex->texture = GL_TEXTURE0 + i;

		R_EnableTexture(tex, qtrue);

		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

		if (i > 0)  /* turn them off for now */
			R_EnableTexture(tex, qfalse);

		R_CheckError();
	}

	R_SelectTexture(&texunit_diffuse);
	/* alpha test parameters */
	glAlphaFunc(GL_GREATER, 0.01f);

	/* fog parameters */
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, FOG_START);
	glFogf(GL_FOG_END, FOG_END);

	/* polygon offset parameters */
	glPolygonOffset(1, 1);

	/* alpha blend parameters */
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* make sure no dynamic lights are active */
	R_ClearActiveLights();

	/* reset gl error state */
	R_CheckError();
}

void R_TexEnv (GLenum mode)
{
	if (mode == r_state.active_texunit->texenv)
		return;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
	r_state.active_texunit->texenv = mode;
}

const vec4_t color_white = {1, 1, 1, 1};

/**
 * @brief Change the color to given value
 * @param[in] rgba A pointer to a vec4_t with rgba color value
 * @note To reset the color let rgba be NULL
 */
void R_Color (const vec4_t rgba)
{
	const float *color;
	if (r_state.build_shadowmap_enabled)
		return;

	if (rgba)
		color = rgba;
	else
		color = color_white;

	glColor4fv(color);
	R_CheckError();
}
