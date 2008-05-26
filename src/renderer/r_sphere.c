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

/** @brief This var is needed - because a Sys_Error may call R_SphereShutdown
 * before the precompiled lists are even generated */
static qboolean sphereInit = qfalse;

static cvar_t *r_sphereDetails;

sphere_t r_globeEarth;
sphere_t r_globeMoon;

/**
 * @brief Initialize the globe chain arrays
 * @param[out] sphere The sphere you want to create
 * @param[in] tris The amount of tris the globe should have - the higher the number
 * the higher the details
 * @param[in] radius The radius of the sphere
 * @sa R_Draw3DGlobe
 */
void R_SphereGenerate (sphere_t *sphere, const int tris, const float radius)
{
	const float drho = M_PI / tris;
	const float dtheta = M_PI / (tris / 2);

	const float ds = 1.0f / (tris * 2);
	const float dt = 1.0f / tris;

	float t = 1.0f;
	float s = 0.0f;

	int i, j;

	int vertspos = 0;
	int texespos = 0;

	sphere->verts = (float*)Mem_PoolAlloc(sizeof(float) * ((tris + 1) * (tris + 1) * 6), vid_modelPool, 0);
	sphere->texes = (float*)Mem_PoolAlloc(sizeof(float) * ((tris + 1) * (tris + 1) * 4), vid_modelPool, 0);
	sphere->normals = (float*)Mem_PoolAlloc(sizeof(float) * ((tris + 1) * (tris + 1) * 6), vid_modelPool, 0);

	for (i = 0; i < tris; i++) {
		float rho = (float) i * drho;
		float srho = (float) (sin(rho));
		float crho = (float) (cos(rho));
		float srhodrho = (float) (sin(rho + drho));
		float crhodrho = (float) (cos(rho + drho));

		s = 0.0f;

		for (j = 0; j <= tris; j++) {
			float theta = (j == tris) ? 0.0f : j * dtheta;
			float stheta = (float) (-sin(theta));
			float ctheta = (float) (cos(theta));

			Vector2Set((&sphere->texes[texespos]), s, (t - dt));
			texespos += 2;

			VectorSet((&sphere->verts[vertspos]),
				stheta * srhodrho * radius,
				ctheta * srhodrho * radius,
				crhodrho * radius);
			VectorNormalize2((&sphere->verts[vertspos]), (&sphere->normals[vertspos]));
			vertspos += 3;

			Vector2Set((&sphere->texes[texespos]), s, t);
			texespos += 2;

			VectorSet((&sphere->verts[vertspos]),
				stheta * srho * radius,
				ctheta * srho * radius,
				crho * radius);
			VectorNormalize2((&sphere->verts[vertspos]), (&sphere->normals[vertspos]));
			vertspos += 3;

			s += ds;
		}

		t -= dt;
	}

	vertspos = 0;
	texespos = 0;

	/* build the sphere display list */
	sphere->list = qglGenLists(1);
	R_CheckError();

	qglNewList(sphere->list, GL_COMPILE);
	R_CheckError();

	qglEnable(GL_NORMALIZE);

	for (i = 0; i < tris; i++) {
		qglBegin(GL_TRIANGLE_STRIP);

		for (j = 0; j <= tris; j++) {
			qglNormal3fv(&sphere->normals[vertspos]);
			qglTexCoord2fv(&sphere->texes[texespos]);
			qglVertex3fv(&sphere->verts[vertspos]);

			texespos += 2;
			vertspos += 3;

			qglNormal3fv(&sphere->normals[vertspos]);
			qglTexCoord2fv(&sphere->texes[texespos]);
			qglVertex3fv(&sphere->verts[vertspos]);

			texespos += 2;
			vertspos += 3;
		}

		qglEnd();
		R_CheckError();
	}

	qglDisable(GL_NORMALIZE);

	sphere->num_tris = tris * tris;

	qglEndList();
}

/**
 * @brief Draw the sphere
 * @param[in] sphere The sphere that should be rendered
 * @param[in] pos The position (translation) of the matrix
 * @param[in] rotate The rotation of the matrix
 * @param[in] scale The scale of the matrix
 * @param[in] lightPos Set this to NULL if you don't want to change the light position
 */
void R_SphereRender (const sphere_t *sphere, const vec3_t pos, const vec3_t rotate, const float scale, const vec3_t lightPos)
{
	/* go to a new matrix */
	qglPushMatrix();

	qglTranslatef(pos[0], pos[1], pos[2]);

	/* flatten the sphere */
	qglScalef(scale * viddef.rx, scale * viddef.ry, scale);
	R_CheckError();

	/* rotate the globe as given in ccs.angles */
	qglRotatef(rotate[YAW], 1, 0, 0);
	qglRotatef(rotate[ROLL], 0, 1, 0);
	qglRotatef(rotate[PITCH], 0, 0, 1);

	if (lightPos)
		qglLightfv(GL_LIGHT0, GL_POSITION, lightPos);

	R_CheckError();

	/* solid globe texture */
	if (sphere->overlay)
		R_BindTexture(sphere->overlay->texnum);
	else
		R_BindTexture(sphere->texture->texnum);

	qglEnable(GL_CULL_FACE);
	qglCallList(sphere->list);
	qglDisable(GL_CULL_FACE);

	R_CheckError();

	/* restore the previous matrix */
	qglPopMatrix();

	refdef.alias_count += sphere->num_tris;
}

/**
 * @brief Creates the spheres we need for rendering the 3d globe
 * @note The moon sphere has less details because it's a lot smaller in the real scene
 * @sa R_Init
 */
void R_SphereInit (void)
{
	r_sphereDetails = Cvar_Get("r_sphereDetails", "1.0", CVAR_ARCHIVE, "Factor to increase or decrease the sphere tris");
	if (r_sphereDetails->integer <= 0)
		Cvar_SetValue("r_sphereDetails", 1.0);

	R_SphereGenerate(&r_globeEarth, 60 * r_sphereDetails->value, EARTH_RADIUS);
	/* the earth has more details than the moon */
	R_SphereGenerate(&r_globeMoon, 20 * r_sphereDetails->value, MOON_RADIUS);

	sphereInit = qtrue;
}

/**
 * @brief Frees the precompiled display lists
 * @sa R_Shutdown
 */
void R_SphereShutdown (void)
{
	if (sphereInit) {
		qglDeleteLists(r_globeEarth.list, 1);
		qglDeleteLists(r_globeMoon.list, 1);
	}
}
