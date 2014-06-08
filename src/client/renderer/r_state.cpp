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

#include "r_local.h"
#include "r_program.h"
#include "r_error.h"

/* useful for particles, pics, etc.. */
const vec2_t default_texcoords[4] = {
	{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}
};

/* Hack to keep track of active rendering program (have to reinit binings&parameters once it changed)*/
static r_program_t* lastProgram = nullptr;

/**
 * @brief Returns false if the texunit is not supported
 */
bool R_SelectTexture (gltexunit_t* texunit)
{
	if (texunit == r_state.active_texunit)
		return true;

	/* not supported */
	if (texunit->texture >= r_config.maxTextureCoords + GL_TEXTURE0)
		return false;

	r_state.active_texunit = texunit;

	qglActiveTexture(texunit->texture);
	qglClientActiveTexture(texunit->texture);
	return true;
}

static void R_BindTexture_ (int texnum)
{
	if (texnum == r_state.active_texunit->texnum)
		return;

	assert(texnum > 0);

	r_state.active_texunit->texnum = texnum;

	glBindTexture(GL_TEXTURE_2D, texnum);
	R_CheckError();
}

void R_BindTextureDebug (int texnum, const char* file, int line, const char* function)
{
	if (texnum <= 0) {
		Com_Printf("Bad texnum (%d) in: %s (%d): %s\n", texnum, file, line, function);
	}
	R_BindTexture_(texnum);
}

void R_BindTextureForTexUnit (GLuint texnum, gltexunit_t* texunit)
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

void R_UseMaterial (const material_t* material)
{
	static float last_b, last_p, last_s, last_h;
	float b;

	if (r_state.active_material == material)
		return;

	if (!r_state.active_normalmap)
		return;

	if (material)
		r_state.active_material = material;
	else
		r_state.active_material = &defaultMaterial;

	b = r_state.active_material->bump * r_bumpmap->value;
	if (b != last_b)
		R_ProgramParameter1f("BUMP", b);
	last_b = b;

	if (r_state.active_program != r_state.world_program || r_programs->integer > 1) {
		const float p = r_state.active_material->parallax * r_parallax->value;
		if (p != last_p)
			R_ProgramParameter1f("PARALLAX", p);
		last_p = p;

		const float h = r_state.active_material->hardness * r_hardness->value;
		if (h != last_h)
			R_ProgramParameter1f("HARDNESS", h);
		last_h = h;

		const float s = r_state.active_material->specular * r_specular->value;
		if (s != last_s)
			R_ProgramParameter1f("SPECULAR", s);
		last_s = s;
	} else {
		last_p = -1;
		last_h = -1;
		last_s = -1;
	}
}

void R_BindArray (GLenum target, GLenum type, const void* array)
{
	const int v = static_cast<int>(target);
	switch (v) {
	case GL_VERTEX_ARRAY:
		glVertexPointer(COMPONENTS_VERTEX_ARRAY3D, type, 0, array);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		glTexCoordPointer(COMPONENTS_TEXCOORD_ARRAY, type, 0, array);
		break;
	case GL_COLOR_ARRAY:
		glColorPointer(COMPONENTS_COLOR_ARRAY, type, 0, array);
		break;
	case GL_NORMAL_ARRAY:
		glNormalPointer(type, 0, array);
		break;
	case GL_TANGENT_ARRAY:
		R_AttributePointer("TANGENTS", COMPONENTS_TANGENT_ARRAY, array);
		break;
	case GL_NEXT_VERTEX_ARRAY:
		R_AttributePointer("NEXT_FRAME_VERTS", COMPONENTS_VERTEX_ARRAY3D, array);
		break;
	case GL_NEXT_NORMAL_ARRAY:
		R_AttributePointer("NEXT_FRAME_NORMALS", COMPONENTS_NORMAL_ARRAY, array);
		break;
	case GL_NEXT_TANGENT_ARRAY:
		R_AttributePointer("NEXT_FRAME_TANGENTS", COMPONENTS_TANGENT_ARRAY, array);
		break;
	}
}

/**
 * @brief Binds the appropriate shared vertex array to the specified target.
 */
void R_BindDefaultArray (GLenum target)
{
	const int v = static_cast<int>(target);
	switch (v) {
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

	if (target == GL_ELEMENT_ARRAY_BUFFER) {
		qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
		return; /* No need to call gl***Poiner -- this binding is special and automates that */
	} else {
		qglBindBuffer(GL_ARRAY_BUFFER, id);
	}

	if (type && id)  /* assign the array pointer as well */
		R_BindArray(target, type, nullptr);
}

void R_BlendFunc (GLenum src, GLenum dest)
{
	if (r_state.blend_src == src && r_state.blend_dest == dest)
		return;

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	glBlendFunc(src, dest);
}

void R_EnableMultisample (bool enable)
{
	if (r_multisample->integer == 0 && enable)
		return;

	if (r_state.multisample_enabled == enable)
		return;

	r_state.multisample_enabled = enable;

	if (enable) {
		glEnable(GL_MULTISAMPLE);
	} else {
		glDisable(GL_MULTISAMPLE);
	}
	R_CheckError();
}

void R_EnableBlend (bool enable)
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

void R_EnableAlphaTest (bool enable)
{
	if (r_state.alpha_test_enabled == enable)
		return;

	r_state.alpha_test_enabled = enable;

	if (enable)
		glEnable(GL_ALPHA_TEST);
	else
		glDisable(GL_ALPHA_TEST);
}

void R_EnableStencilTest (bool enable)
{
	if (r_state.stencil_test_enabled == enable)
		return;

	r_state.stencil_test_enabled = enable;

	if (enable)
		glEnable(GL_STENCIL_TEST);
	else
		glDisable(GL_STENCIL_TEST);
}

void R_EnableTexture (gltexunit_t* texunit, bool enable)
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
			if (r_lightmap->integer)
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

void R_EnableColorArray (bool enable)
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
bool R_EnableLighting (r_program_t* program, bool enable)
{
	if (!r_programs->integer)
		return r_state.lighting_enabled;

	if (enable && (!program || !program->id))
		return r_state.lighting_enabled;

	if (r_state.lighting_enabled == enable && r_state.active_program == program)
		return r_state.lighting_enabled;

	r_state.lighting_enabled = enable;

	if (enable) {  /* toggle state */
		R_UseProgram(program);

		glEnableClientState(GL_NORMAL_ARRAY);
	} else {
		glDisableClientState(GL_NORMAL_ARRAY);

		R_UseProgram(nullptr);
	}

	return r_state.lighting_enabled;
}

void R_SetupSpotLight (int index, const light_t* light)
{
	const vec4_t blackColor = {0.0, 0.0, 0.0, 1.0};
	vec4_t position;

	if (!r_programs->integer || !r_dynamic_lights->integer)
		return;

	if (index < 0 || index >= r_dynamic_lights->integer)
		return;

	index += GL_LIGHT0;

	glEnable(index);
	glLightf(index, GL_CONSTANT_ATTENUATION, MIN_GL_CONSTANT_ATTENUATION);
	glLightf(index, GL_LINEAR_ATTENUATION, 0);
	glLightf(index, GL_QUADRATIC_ATTENUATION, 16.0 / (light->radius * light->radius));

	VectorCopy(light->origin, position);
	position[3] = 1.0; /* spot light */

	glLightfv(index, GL_POSITION, position);
	glLightfv(index, GL_AMBIENT, blackColor);
	glLightfv(index, GL_DIFFUSE, light->color);
	glLightfv(index, GL_SPECULAR, blackColor);
}

void R_DisableSpotLight (int index)
{
	const vec4_t blackColor = {0.0, 0.0, 0.0, 1.0};

	if (!r_programs->integer || !r_dynamic_lights)
		return;

	if (index < 0 || index >= MAX_GL_LIGHTS - 1)
		return;

	index += GL_LIGHT0;

	glDisable(index);
	glLightf(index, GL_CONSTANT_ATTENUATION, MIN_GL_CONSTANT_ATTENUATION);
	glLightf(index, GL_LINEAR_ATTENUATION, 0.0);
	glLightf(index, GL_QUADRATIC_ATTENUATION, 0.0);
	glLightfv(index, GL_AMBIENT, blackColor);
	glLightfv(index, GL_DIFFUSE, blackColor);
	glLightfv(index, GL_SPECULAR, blackColor);
}

/**
 * @brief Enables animation using keyframe interpolation on the GPU
 * @param mesh The mesh to animate
 * @param backlerp The temporal proximity to the previous keyframe (in the range 0.0 to 1.0)
 * @param enable Whether to turn animation on or off
 */
void R_EnableAnimation (const mAliasMesh_t* mesh, float backlerp, bool enable)
{
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
void R_EnableBumpmap (const image_t* normalmap)
{
	if (!r_state.lighting_enabled)
		return;

	if (!r_bumpmap->value)
		return;

	if (r_state.active_normalmap == normalmap)
		return;

	if (!normalmap) {
		/* disable bump mapping */
		R_ProgramParameter1i("BUMPMAP", 0);

		r_state.active_normalmap = normalmap;
		return;
	}

	if (!r_state.active_normalmap) {
		/* enable bump mapping */
		R_ProgramParameter1i("BUMPMAP", 1);
		/* default material to use if no material gets loaded */
		R_UseMaterial(&defaultMaterial);
	}

	R_BindNormalmapTexture(normalmap->texnum);

	r_state.active_normalmap = normalmap;
}

void R_EnableWarp (r_program_t* program, bool enable)
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
		R_UseProgram(nullptr);
	}

	R_SelectTexture(&texunit_diffuse);
}

void R_EnableBlur (r_program_t* program, bool enable, r_framebuffer_t* source, r_framebuffer_t* dest, int dir)
{
	if (!r_programs->integer)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_postprocess->integer || r_state.blur_enabled == enable)
		return;

	r_state.blur_enabled = enable;

	R_SelectTexture(&texunit_lightmap);

	if (enable) {
		float userdata[] = { static_cast<float>(source->width), static_cast<float>(dir) };
		R_UseFramebuffer(dest);
		program->userdata = userdata;
		R_UseProgram(program);
	} else {
		R_UseFramebuffer(fbo_screen);
		R_UseProgram(nullptr);
	}

	R_SelectTexture(&texunit_diffuse);
}

void R_EnableShell (bool enable)
{
	if (enable == r_state.shell_enabled)
		return;

	r_state.shell_enabled = enable;

	if (enable) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0, 1.0);

		R_EnableDrawAsGlow(true);
		R_EnableBlend(true);
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

		if (r_state.lighting_enabled)
			R_ProgramParameter1f("OFFSET", refdef.time / 3.0);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		R_EnableBlend(false);
		R_EnableDrawAsGlow(false);

		glPolygonOffset(0.0, 0.0);
		glDisable(GL_POLYGON_OFFSET_FILL);

		if (r_state.lighting_enabled)
			R_ProgramParameter1f("OFFSET", 0.0);
	}
}

#define FOG_START	300.0
#define FOG_END		2500.0

vec2_t fogRange = {FOG_START, FOG_END};

void R_EnableFog (bool enable)
{
	if (!r_fog->integer || r_state.fog_enabled == enable)
		return;

	r_state.fog_enabled = false;

	/* This is ugly. Shaders could be enabled or disabled between this and rendering call, so we have to setup both FFP and GLSL */
	if (enable) {
		if (((refdef.weather & WEATHER_FOG) && r_fog->integer) || r_fog->integer == 2) {
			r_state.fog_enabled = true;

			glFogfv(GL_FOG_COLOR, refdef.fogColor);
			glFogf(GL_FOG_DENSITY, refdef.fogColor[3]);
			glEnable(GL_FOG);

			if (r_programs->integer && (r_state.active_program == r_state.world_program || r_state.active_program == r_state.model_program || r_state.active_program == r_state.warp_program)) {
				R_ProgramParameter3fv("FOGCOLOR", refdef.fogColor);
				R_ProgramParameter1f("FOGDENSITY", refdef.fogColor[3]);
				R_ProgramParameter2fv("FOGRANGE", fogRange);
			}
		}
	} else {
		glFogf(GL_FOG_DENSITY, 0.0);
		glDisable(GL_FOG);
		if (r_programs->integer && (r_state.active_program == r_state.world_program || r_state.active_program == r_state.model_program || r_state.active_program == r_state.warp_program))
			R_ProgramParameter1f("FOGDENSITY", 0.0f);
	}
}

static void R_UpdateGlowBufferBinding (void)
{
#ifndef GL_VERSION_ES_CM_1_0
	static GLenum glowRenderTarget = GL_COLOR_ATTACHMENT1_EXT; // Not supported in GLES

	if (r_state.active_program) {
		/* got an active shader */
		if (r_state.draw_glow_enabled) {
			/* switch the draw buffer to the glow buffer */
			R_BindColorAttachments(1, &glowRenderTarget);
			R_ProgramParameter1f("GLOWSCALE", 0.0); /* Plumbing to avoid state leak */
		} else {
			/* switch back to the draw buffer we are currently rendering into ... */
			R_DrawBuffers(2);
			/* ... and enable glow, if there is a glow map */
			if (r_state.glowmap_enabled) {
				R_ProgramParameter1f("GLOWSCALE", 1.0);
			} else {
				R_ProgramParameter1f("GLOWSCALE", 0.0);
			}
		}
	} else {
		/* no shader */
		if (r_state.draw_glow_enabled) {
			/* switch the draw buffer to the glow buffer */
			R_BindColorAttachments(1, &glowRenderTarget);
		} else {
			if (!r_state.glowmap_enabled) {
				/* Awkward case. Postprocessing is requested, but fragment
				 * shader is disabled, and we are supposed to draw non-glowing object.
				 * So we just draw this object into color buffer only and hope that it's not
				 * supposed to overlay any glowing objects.
				 */
				R_DrawBuffers(1);
			} else {
				/* FIXME: Had to draw glowmapped object,  but don't know how to do it
				 * when rendering through FFP.
				 * So -- just copy color into glow buffer.
				 * (R_DrawAsGlow may hit this)
				 */
				R_DrawBuffers(2);
				/*Con_Print("GLSL glow with no program!\n");*/
			}
		}
	}
#endif
}

void R_EnableGlowMap (const image_t* image)
{
	if (!r_programs->integer)
		return;

	if (image)
		R_BindTextureForTexUnit(image->texnum, &texunit_glowmap);

	/** @todo Is the following condition correct or not? Either fix it or write the comment why it should be done that way */
	if (!image && r_state.glowmap_enabled == !!image && r_state.active_program == lastProgram)
		return;

	r_state.glowmap_enabled = !!image;

	/* Shouldn't render glow without GLSL, so enable simple program for it */
	if (image) {
		if (!r_state.active_program)
			R_UseProgram(r_state.simple_glow_program);
	} else {
		if (r_state.active_program == r_state.simple_glow_program)
			R_UseProgram(nullptr);
	}

	lastProgram = r_state.active_program;

	R_UpdateGlowBufferBinding();
}

void R_EnableDrawAsGlow (bool enable)
{
	if (!r_programs->integer)
		return;

	if (r_state.draw_glow_enabled == enable && r_state.active_program == lastProgram)
		return;

	r_state.draw_glow_enabled = enable;

	lastProgram = r_state.active_program;

	R_UpdateGlowBufferBinding();
}

void R_EnableSpecularMap (const image_t* image, bool enable)
{
	if (!r_state.dynamic_lighting_enabled)
		return;

	if (r_programs->integer < 2)
		return;

	if (enable && image != nullptr) {
		R_BindTextureForTexUnit(image->texnum, &texunit_specularmap);
		R_ProgramParameter1i("SPECULARMAP", 1);
		r_state.specularmap_enabled = enable;
	} else {
		R_ProgramParameter1i("SPECULARMAP", 0);
		r_state.specularmap_enabled = false;
	}
}

void R_EnableRoughnessMap (const image_t* image, bool enable)
{
	if (!r_state.dynamic_lighting_enabled)
		return;

	if (enable && image != nullptr) {
		R_BindTextureForTexUnit(image->texnum, &texunit_roughnessmap);
		R_ProgramParameter1i("ROUGHMAP", 1);
		r_state.roughnessmap_enabled = enable;
	} else {
		R_ProgramParameter1i("ROUGHMAP", 0);
		r_state.roughnessmap_enabled = false;
	}
}

/**
 * @sa R_Setup3D
 */
static void MYgluPerspective (GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax, yaspect = (float) viddef.context.height / viddef.context.width;

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
 * @sa R_Setup2D
 */
void R_Setup3D (void)
{
	/* only for the battlescape rendering */
	if ((refdef.rendererFlags & RDF_NOWORLDMODEL) == 0) {
		int x, x2, y2, y, w, h;

		/* set up viewport */
		x = floorf(viddef.x * viddef.context.width / viddef.context.width);
		x2 = ceilf((viddef.x + viddef.viewWidth) * viddef.context.width / viddef.context.width);
		y = floorf(viddef.context.height - viddef.y * viddef.context.height / viddef.context.height);
		y2 = ceilf(viddef.context.height - (viddef.y + viddef.viewHeight) * viddef.context.height / viddef.context.height);

		w = x2 - x;
		h = y - y2;

		glViewport(x, y2, w, h);
		R_CheckError();
	}

	/* set up projection matrix */
	glMatrixMode(GL_PROJECTION);

	glLoadIdentity();
	if ((refdef.rendererFlags & RDF_NOWORLDMODEL) != 0) {
		/* center image into the viewport */
		float x = viddef.x + (viddef.viewWidth - VID_NORM_WIDTH) / 2.0 - (viddef.virtualWidth - VID_NORM_WIDTH) / 2.0;
		float y = viddef.y + (viddef.viewHeight - VID_NORM_HEIGHT) / 2.0 - (viddef.virtualHeight - VID_NORM_HEIGHT) / 2.0;
		/* @todo magic coef, i dont know where it come from */
		x *=  2.0f / (float) viddef.virtualWidth;
		y *=  2.0f / (float) viddef.virtualHeight;
		glTranslatef(x, -y, 0.0f);
	}
	MYgluPerspective(4.0, MAX_WORLD_WIDTH);

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
	R_EnableRenderbuffer(true);

	R_CheckError();
}

/**
 * @sa R_Setup3D
 */
void R_Setup2D (void)
{
	/* only for the battlescape rendering */
	if ((refdef.rendererFlags & RDF_NOWORLDMODEL) == 0) {
		/* set 2D virtual screen size */
		glViewport(0, 0, viddef.context.width, viddef.context.height);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	/* switch to orthographic (2 dimensional) projection
	 * don't draw anything before skybox */
	glOrtho(0, viddef.context.width, viddef.context.height, 0, 9999.0f, SKYBOX_DEPTH);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	/* bind default vertex array */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_Color(nullptr);

	glEnable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);

	glDisable(GL_LIGHTING);

	/* disable render-to-framebuffer */
	R_EnableRenderbuffer(false);

	R_CheckError();
}

void R_SetDefaultState (void)
{
	r_state.shell_enabled = false;
	r_state.blend_enabled = false;
	r_state.color_array_enabled = false;
	r_state.alpha_test_enabled = false;
	r_state.stencil_test_enabled = false;
	r_state.lighting_enabled = false;
	r_state.warp_enabled = false;
	r_state.fog_enabled = false;
	r_state.blur_enabled = false;
	r_state.glowmap_enabled = false;
	r_state.draw_glow_enabled = false;
	r_state.dynamic_lighting_enabled = false;
	r_state.specularmap_enabled = false;
	r_state.roughnessmap_enabled = false;
	r_state.animation_enabled = false;
	r_state.renderbuffer_enabled = false;
	r_state.active_material = nullptr;
	r_state.blend_src = 0;
	r_state.blend_dest = 0;
	r_state.active_texunit = nullptr;

	glClearColor(0, 0, 0, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	R_ReallocateStateArrays(GL_ARRAY_LENGTH_CHUNK);
	R_ReallocateTexunitArray(&texunit_0, GL_ARRAY_LENGTH_CHUNK);
	R_ReallocateTexunitArray(&texunit_1, GL_ARRAY_LENGTH_CHUNK);
	R_ReallocateTexunitArray(&texunit_2, GL_ARRAY_LENGTH_CHUNK);
	R_ReallocateTexunitArray(&texunit_3, GL_ARRAY_LENGTH_CHUNK);
	R_ReallocateTexunitArray(&texunit_4, GL_ARRAY_LENGTH_CHUNK);

	/* setup vertex array pointers */
	glEnableClientState(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_EnableColorArray(true);
	R_BindDefaultArray(GL_COLOR_ARRAY);
	R_EnableColorArray(false);

	glEnableClientState(GL_NORMAL_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	R_EnableAlphaTest(false);

	/* reset gl error state */
	R_CheckError();

	/* setup texture units */
	for (int i = 0; i < r_config.maxTextureCoords && i < MAX_GL_TEXUNITS; i++) {
		gltexunit_t* tex = &r_state.texunits[i];
		tex->texture = GL_TEXTURE0 + i;
		tex->enabled = false;

		R_EnableTexture(tex, true);

		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
		R_TexEnv(GL_MODULATE);
		R_ReallocateTexunitArray(tex, GL_ARRAY_LENGTH_CHUNK);

		if (i > 0)  /* turn them off for now */
			R_EnableTexture(tex, false);

		R_CheckError();
	}

	R_SelectTexture(&texunit_diffuse);
	/* alpha test parameters */
	glAlphaFunc(GL_GREATER, 0.01f);

	/* fog parameters */
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, FOG_START);
	glFogf(GL_FOG_END, FOG_END);

	/* stencil test parameters */
	glStencilFunc(GL_GEQUAL, 1, 0xff);
	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

	/* polygon offset parameters */
	glPolygonOffset(1, 1);

	/* alpha blend parameters */
	R_EnableBlend(true);
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* remove leftover lights */
	R_ClearStaticLights();

	/* reset gl error state */
	R_CheckError();
}

void R_TexEnv (GLenum mode)
{
	if (mode == r_state.active_texunit->texenv)
		return;

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
	r_state.active_texunit->texenv = mode;
}

/**
 * @brief Sets special texture environment mode to override texture color; don't forget to call R_TexOverride(nullptr) to reset after drawing; intended for UI only, will conflict with lightmaps
 */
void R_TexOverride (vec4_t rgba)
{
	R_SelectTexture(&texunit_lightmap); /* abuse lightmap texture unit for color manipulation */

	if (!rgba) {
		R_TexEnv(GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
		R_SelectTexture(&texunit_diffuse);
		return;
	}

	glEnable(GL_TEXTURE_2D);

	R_TexEnv(GL_COMBINE); /* enable color combiner */
	/* setup texture combiner to blend between actual color of previous texture stage and the constant rgb color given as the parameter to this function
	 * amount of blending is defined by alpha channel of constant color
	 */
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT); /* set constant color as blending target*/
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS); /* set incoming color as blending source */
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT); /* set constant color alpha as blending factor */
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE); /* set blending mode to interpolation from src1 to src0 */
	/* copy alpha from incoming color, it could be used later for framebuffer operations */
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, rgba);

	R_BindTexture(r_dummyTexture->texnum);

	R_SelectTexture(&texunit_diffuse);
}

const vec4_t color_white = {1, 1, 1, 1};

/**
 * @brief Change the color to given value
 * @param[in] rgba A pointer to a vec4_t with rgba color value
 * @note To reset the color let rgba be nullptr
 */
void R_Color (const vec4_t rgba)
{
	const float* color;
	if (rgba)
		color = rgba;
	else
		color = color_white;

	glColor4f(color[0], color[1], color[2], color[3]);
	R_CheckError();
}

/**
 * @brief Reallocate arrays of GL primitives if needed
 * @param size The new array size
 * @note Also resets all non-default GL array bindings
 * @sa R_ReallocateTexunitArray
*/
void R_ReallocateStateArrays (int size)
{
	if (size <= r_state.array_size)
		return;
	r_state.vertex_array_3d = (GLfloat*) Mem_SafeReAlloc(r_state.vertex_array_3d, size * COMPONENTS_VERTEX_ARRAY3D * sizeof(GLfloat));
	r_state.vertex_array_2d = (GLshort*) Mem_SafeReAlloc(r_state.vertex_array_2d, size * COMPONENTS_VERTEX_ARRAY2D * sizeof(GLshort));
	r_state.color_array = (GLfloat*) Mem_SafeReAlloc(r_state.color_array, size * COMPONENTS_COLOR_ARRAY * sizeof(GLfloat));
	r_state.index_array = (GLint*) Mem_SafeReAlloc(r_state.index_array, size * COMPONENTS_INDEX_ARRAY * sizeof(GLint));
	r_state.normal_array = (GLfloat*) Mem_SafeReAlloc(r_state.normal_array, size * COMPONENTS_NORMAL_ARRAY * sizeof(GLfloat));
	r_state.tangent_array = (GLfloat*) Mem_SafeReAlloc(r_state.tangent_array, size * COMPONENTS_TANGENT_ARRAY * sizeof(GLfloat));
	r_state.next_vertex_array_3d = (GLfloat*) Mem_SafeReAlloc(r_state.next_vertex_array_3d, size * COMPONENTS_VERTEX_ARRAY3D * sizeof(GLfloat));
	r_state.next_normal_array = (GLfloat*) Mem_SafeReAlloc(r_state.next_normal_array, size * COMPONENTS_NORMAL_ARRAY * sizeof(GLfloat));
	r_state.next_tangent_array = (GLfloat*) Mem_SafeReAlloc(r_state.next_tangent_array, size * COMPONENTS_TANGENT_ARRAY * sizeof(GLfloat));
	r_state.array_size = size;
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	R_BindDefaultArray(GL_TANGENT_ARRAY);
	R_BindDefaultArray(GL_NEXT_VERTEX_ARRAY);
	R_BindDefaultArray(GL_NEXT_NORMAL_ARRAY);
	R_BindDefaultArray(GL_NEXT_TANGENT_ARRAY);
}

/**
 * @brief Reallocate texcoord array of the specified texunit, if needed
 * @param texunit Pointer to texunit (TODO: remove this comment as obvious and redundant)
 * @param size The new array size
 * @note Also resets active texunit
 * @sa R_ReallocateStateArrays
*/
void R_ReallocateTexunitArray (gltexunit_t* texunit, int size)
{
	if (size <= texunit->array_size)
		return;
	texunit->texcoord_array = (GLfloat*) Mem_SafeReAlloc(texunit->texcoord_array, size * COMPONENTS_TEXCOORD_ARRAY * sizeof(GLfloat));
	texunit->array_size = size;
	if (!r_state.active_texunit)
		r_state.active_texunit = texunit;
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
}
