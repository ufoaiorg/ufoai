/**
 * @file r_sphere.c
 * @brief Functions to generate and render spheres
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
#include "r_sphere.h"
#include "r_error.h"

static cvar_t *r_sphereDetails;

sphere_t r_globeEarth;
sphere_t r_globeMoon;
sphere_t r_globeEarthAtmosphere;

static inline float rhoSpiral (const int index, const float deltaRho,const float thetaAngle)
{
	const float rhoAngle = (index == 0) ? 0.0f : (float) ((index - 1) * deltaRho + thetaAngle * deltaRho / (2.0f * M_PI));
	return rhoAngle > M_PI ? M_PI : rhoAngle;
}

/**
 * @brief Initialize the globe chain arrays
 * @param[out] sphere The sphere you want to create
 * @param[in] tris The amount of tris the globe should have - the higher the number
 * the higher the details. tris*tris triangles are obtained.
 * @param[in] radius The radius of the sphere
 * @sa R_Draw3DGlobe
 */
void R_SphereGenerate (sphere_t *sphere, const int tris, const float radius)
{
	const float drho = M_PI / tris; /**< angle from the pole (z-axis) */
	/* must multiply pi by 2, rather than do integer division of tris by two,
	 * otherwise it goes wrong when tris is an odd number */
	const float dtheta = 2.0f * M_PI / tris; /**< angle around the equator, from y-axis */

	int i, j;

	int vertspos = 0;
	int texespos = 0;

	sphere->glslProgram = NULL;

	sphere->verts = (float*)Mem_PoolAlloc(sizeof(float) * ((tris + 1) * (tris + 2) * 6), vid_genericPool, 0);
	sphere->texes = (float*)Mem_PoolAlloc(sizeof(float) * ((tris + 1) * (tris + 2) * 4), vid_genericPool, 0);
	sphere->normals = (float*)Mem_PoolAlloc(sizeof(float) * ((tris + 1) * (tris + 2) * 6), vid_genericPool, 0);

	/* must be i <= tris, as one loop is wasted, because of the spiral */
	for (i = 0; i <= tris; i++) { /* loop through rho, from pole to pole */
		/* must be j <= tris, so it meets up again */
		for (j = 0; j <= tris ; j++) { /* loop through theta, around equator */
			const float theta = j * dtheta;
			const float stheta = (float) (-sin(theta));
			const float ctheta = (float) (cos(theta));

			/* second term in rho adds a spiral */
			const float rho = rhoSpiral(i, drho, theta);
			const float rhodrho = rhoSpiral(i + 1, drho, theta); /* rho plus drho, minding boundary conditions */
			const float srho = (float) (sin(rho));
			const float crho = (float) (cos(rho));
			const float srhodrho = (float) (sin(rhodrho));
			const float crhodrho = (float) (cos(rhodrho));

			const float st   = 1 - rho     / M_PI;
			const float stdt = 1 - rhodrho / M_PI;

			const float s = theta / (4.0f * M_PI);

			Vector2Set((&sphere->texes[texespos]), s, stdt);
			texespos += 2;

			VectorSet((&sphere->verts[vertspos]),
				stheta * srhodrho * radius,
				ctheta * srhodrho * radius,
				crhodrho * radius);
			VectorNormalize2((&sphere->verts[vertspos]), (&sphere->normals[vertspos]));
			vertspos += 3;

			Vector2Set((&sphere->texes[texespos]), s, st);
			texespos += 2;

			VectorSet((&sphere->verts[vertspos]),
				stheta * srho * radius,
				ctheta * srho * radius,
				crho * radius);
			VectorNormalize2((&sphere->verts[vertspos]), (&sphere->normals[vertspos]));
			vertspos += 3;
		}
	}
	sphere->num_tris = (tris + 1) * (tris + 2) * 2;
}

/**
 * @brief Creates the spheres we need for rendering the 3d globe
 * @note The moon sphere has less detail because it's smaller in the scene
 * @note The sizes are arbitrary, becasue we use orthographic projection.  The real
 * 		sizes are: lunarRadius = 0.273 * earthRadius, solarRadius = 110.0 * earthRadius
 * @sa R_Init
 */
void R_SphereInit (void)
{
	r_sphereDetails = Cvar_Get("r_sphereDetails", "1.0", CVAR_ARCHIVE, "Factor to increase or decrease the sphere tris");
	if (r_sphereDetails->integer <= 0)
		Cvar_SetValue("r_sphereDetails", 1.0);

	R_SphereGenerate(&r_globeEarth, 60 * r_sphereDetails->value, EARTH_RADIUS);
	R_SphereGenerate(&r_globeEarthAtmosphere, 60 * r_sphereDetails->value, EARTH_RADIUS * 1.03);
	/* the earth has more details than the moon */
	R_SphereGenerate(&r_globeMoon, 20 * r_sphereDetails->value, MOON_RADIUS);
}

static inline void R_SphereActivateTextureUnit (gltexunit_t *texunit, void *texCoordBuffer)
{
	R_SelectTexture(texunit);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, texCoordBuffer);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

static inline void R_SphereDeactivateTextureUnit (gltexunit_t *texunit)
{
	R_SelectTexture(texunit);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void R_SphereRenderTris (const sphere_t *sphere)
{
	glEnable(GL_CULL_FACE);
	glEnable(GL_NORMALIZE);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, sphere->num_tris);

	glDisable(GL_NORMALIZE);
	glDisable(GL_CULL_FACE);
}

/**
 * @param sphere The sphere to check
 * @return @c true if all needed data is loaded to use the geoscape glsl shaders, @c false otherwise
 */
static inline qboolean R_SphereCheckGLSL (const sphere_t *sphere)
{
	return sphere->glslProgram && qglUseProgram && r_programs->integer;
}

/**
 * @brief Draw the sphere
 * @param[in] sphere The sphere that should be rendered
 * @param[in] pos The position (translation) of the matrix
 * @param[in] rotate The rotation of the matrix
 * @param[in] scale The scale of the matrix
 * @param[in] lightPos Set this to NULL if you don't want to change the light position
 */
void R_SphereRender (const sphere_t *sphere, const vec3_t pos, const vec3_t rotate, const float scale, const vec4_t lightPos)
{

	/* go to a new matrix */
	glPushMatrix();

	glMatrixMode(GL_MODELVIEW);
	glTranslatef(pos[0], pos[1], pos[2]);

	/* flatten the sphere */
	glScalef(scale * viddef.rx, scale * viddef.ry, scale);
	R_CheckError();

	/* rotate the globe as given in ccs.angles */
	glRotatef(rotate[YAW], 1, 0, 0);
	glRotatef(rotate[ROLL], 0, 1, 0);
	glRotatef(rotate[PITCH], 0, 0, 1);

	if (lightPos && VectorNotEmpty(lightPos))
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

	R_CheckError();

	if (!sphere->overlay  && R_SphereCheckGLSL(sphere))
		R_SphereShadeGLSL(sphere); /* render globe with bump mapping, specularity, etc. */
	else
		R_SphereShade(sphere); /* otherwise, use basic OpenGL rendering */

	/* cleanup common to both GLSL and normal rendering */
	R_CheckError();

	/* restore the previous matrix */
	glPopMatrix();

	refdef.aliasCount += sphere->num_tris * sphere->num_tris;

	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
}

/**
 * @brief render sphere using standard OpenGL lighting
 */
void R_SphereShade (const sphere_t *sphere)
{
	if (sphere->overlay)
		R_BindTexture(sphere->overlay->texnum);
	else
		R_BindTexture(sphere->texture->texnum);

	if (sphere->overlayAlphaMask) {
		R_EnableTexture(&texunit_lightmap, qtrue);
		R_SelectTexture(&texunit_lightmap);
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, sphere->texes);
		R_BindLightmapTexture(sphere->overlayAlphaMask->texnum);
	}

	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, sphere->verts);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, sphere->texes);
	R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, sphere->normals);

	glEnableClientState(GL_NORMAL_ARRAY);

	R_SphereRenderTris(sphere);

	glDisableClientState(GL_NORMAL_ARRAY);

	if (sphere->overlayAlphaMask)
		R_EnableTexture(&texunit_lightmap, qfalse);
}

/**
 * @brief render sphere using GLSL (bump mapping, specularity, and season-blending)
 */
void R_SphereShadeGLSL (const sphere_t *sphere)
{
	if (Vector4NotEmpty(sphere->nightLightPos))
		glLightfv(GL_LIGHT1, GL_POSITION, sphere->nightLightPos);

	/* configure openGL to use our shader program */
	R_EnableLighting(sphere->glslProgram, qtrue);

	R_BindTexture(sphere->texture->texnum);
	if (sphere->blendTexture)
		R_BindTextureForTexUnit(sphere->blendTexture->texnum, &texunit_1);
	if (sphere->normalMap)
		R_BindTextureForTexUnit(sphere->normalMap->texnum, &texunit_2);

	if (r_lights->integer) {
		if (sphere->blendScale >= 0)
			R_ProgramParameter1f("BLENDSCALE", sphere->blendScale);
		if (r_postprocess->integer && sphere->glowScale >= 0)
			R_ProgramParameter1f("GLOWSCALE", sphere->glowScale);
	}

	/* set up pointers */
	R_SphereActivateTextureUnit(&texunit_1, sphere->texes);
	R_SphereActivateTextureUnit(&texunit_2, sphere->texes);

	R_SelectTexture(&texunit_diffuse);
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, sphere->verts);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, sphere->texes);
	R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, sphere->normals);

	R_SphereRenderTris(sphere);

	R_SphereDeactivateTextureUnit(&texunit_1);
	R_SphereDeactivateTextureUnit(&texunit_2);

	/* deactivate the shader program */
	R_EnableLighting(NULL, qfalse);
	R_SelectTexture(&texunit_diffuse);
}
