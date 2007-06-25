/**
 * @file gl_mesh.c
 * @brief triangle model functions
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

#include "gl_local.h"

/**
 * @brief
 * @sa R_DrawAliasMD2Model
 * @sa R_DrawAliasMD3Model
 * @param[in] lightambient May not be null for fixed lightning
 */
extern void R_EnableLights (qboolean fixed, float *matrix, float *lightparam, float *lightambient)
{
	dlight_t *light;
	vec3_t delta;
	float bright, sumBright, lsqr, max;
	vec4_t sumColor, trorigin, sumDelta;
	int i, j, n;

	if (fixed) {
		assert(lightambient);
		/* add the fixed light */
		qglLightfv(GL_LIGHT0, GL_POSITION, lightparam);
		qglLightfv(GL_LIGHT0, GL_DIFFUSE, matrix);
		qglLightfv(GL_LIGHT0, GL_AMBIENT, lightambient);

		/* enable the lighting */
		qglEnable(GL_LIGHTING);
		qglEnable(GL_LIGHT0);
		return;
	}

	VectorClear(sumColor);
	VectorClear(sumDelta);
	sumDelta[3] = 1.0;
	GLVectorTransform(matrix, sumDelta, trorigin);

	if (*lightparam != 0.0) {
		VectorScale(r_newrefdef.sun->dir, 1.0, sumDelta);
		sumBright = 0.7;
		VectorScale(r_newrefdef.sun->color, sumBright, sumColor);
	} else {
		VectorClear(sumDelta);
		sumBright = 0;
	}

	for (j = 0; j < 2; j++) {
		if (j) {
			light = r_newrefdef.ll;
			n = r_newrefdef.num_lights;
		} else {
			light = r_newrefdef.dlights;
			n = r_newrefdef.num_dlights;
		}

		for (i = 0; i < n; i++, light++) {
			VectorSubtract(light->origin, trorigin, delta);
			lsqr = VectorLengthSqr(delta) + 1;
			bright = 8.0 * light->intensity / (lsqr + 64 * 64);
			sumBright += bright;
			VectorMA(sumColor, bright, light->color, sumColor);
			VectorScale(delta, 1.0 / sqrt(lsqr), delta);
			VectorMA(sumDelta, bright, delta, sumDelta);
/*			ri.Con_Printf(PRINT_ALL, "%f %f\n", bright, VectorLength(delta)); */
		}
	}

	/* normalize delta and color */
	VectorNormalize(sumDelta);
	VectorMA(trorigin, 512, sumDelta, sumDelta);
	sumDelta[3] = 0.0;
	VectorScale(sumColor, sumBright, sumColor);
	sumColor[3] = 1.0;

	max = sumColor[0];
	if (sumColor[1] > max)
		max = sumColor[1];
	if (sumColor[2] > max)
		max = sumColor[2];
	if (max > 2.0)
		VectorScale(sumColor, 2.0 / max, sumColor);

	/* add the light */
	qglLightfv(GL_LIGHT0, GL_POSITION, sumDelta);
	qglLightfv(GL_LIGHT0, GL_DIFFUSE, sumColor);
	qglLightfv(GL_LIGHT0, GL_AMBIENT, r_newrefdef.sun->ambient);

	/* enable the lighting */
	qglEnable(GL_LIGHTING);
	qglEnable(GL_LIGHT0);
}

