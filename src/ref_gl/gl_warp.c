/**
 * @file gl_warp.c
 * @brief water polygons
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

extern model_t *loadmodel;

static msurface_t *warpface;

#define	SUBDIVIDE_SIZE	64
/*#define	SUBDIVIDE_SIZE	1024 */

/**
 * @brief
 */
void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int i, j;
	float *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
		for (j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

/**
 * @brief
 */
void SubdividePolygon(int numverts, float *verts)
{
	int i, j, k;
	vec3_t mins, maxs;
	float m;
	float *v;
	vec3_t front[64], back[64];
	int f, b;
	float dist[64];
	float frac;
	glpoly_t *poly;
	float s, t;
	vec3_t total;
	float total_s, total_t;

	if (numverts > 60)
		ri.Sys_Error(ERR_DROP, "numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = SUBDIVIDE_SIZE * floor(m / SUBDIVIDE_SIZE + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		/* cut it */
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		/* wrap cases */
		dist[j] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy(v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy(v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				/* clip point */
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	/* add a point in the center to help keep warp valid */
	poly = Hunk_Alloc(sizeof(glpoly_t) + ((numverts - 4) + 2) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts + 2;
	VectorClear(total);
	total_s = 0;
	total_t = 0;
	for (i = 0; i < numverts; i++, verts += 3) {
		VectorCopy(verts, poly->verts[i + 1]);
		s = DotProduct(verts, warpface->texinfo->vecs[0]);
		t = DotProduct(verts, warpface->texinfo->vecs[1]);

		total_s += s;
		total_t += t;
		VectorAdd(total, verts, total);

		poly->verts[i + 1][3] = s;
		poly->verts[i + 1][4] = t;
	}

	VectorScale(total, (1.0 / numverts), poly->verts[0]);
	poly->verts[0][3] = total_s / numverts;
	poly->verts[0][4] = total_t / numverts;

	/* copy first vertex to last */
	memcpy(poly->verts[i + 1], poly->verts[1], sizeof(poly->verts[0]));
}

/**
 * @brief Breaks a polygon up along axial 64 unit boundaries so that turbulent
 * can be done reasonably.
 */
void GL_SubdivideSurface(msurface_t * fa)
{
	vec3_t verts[64];
	int numverts;
	int i;
	int lindex;
	float *vec;

	warpface = fa;

	/* convert edges back to a normal polygon */
	numverts = 0;
	for (i = 0; i < fa->numedges; i++) {
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon(numverts, verts[0]);
}



/* speed up sin calculations - Ed */
float r_turbsin[] = {
#include "warpsin.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))

/**
 * @brief Does a water warp on the pre-fragmented glpoly_t chain
 */
void EmitWaterPolys(msurface_t * fa)
{
	glpoly_t *p, *bp;
	float *v;
	int i;
	float s, t, os, ot;
	float scroll;
	float rdt = r_newrefdef.time;

	if (fa->texinfo->flags & SURF_FLOWING)
		scroll = -64 * ((r_newrefdef.time * 0.5) - (int) (r_newrefdef.time * 0.5));
	else
		scroll = 0;
	for (bp = fa->polys; bp; bp = bp->next) {
		p = bp;

		qglBegin(GL_TRIANGLE_FAN);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			os = v[3];
			ot = v[4];

#if !id386
			s = os + r_turbsin[(int) ((ot * 0.125 + r_newrefdef.time) * TURBSCALE) & 255];
#else
			s = os + r_turbsin[Q_ftol(((ot * 0.125 + rdt) * TURBSCALE)) & 255];
#endif
			s += scroll;
			s *= (1.0 / 64);

#if !id386
			t = ot + r_turbsin[(int) ((os * 0.125 + rdt) * TURBSCALE) & 255];
#else
			t = ot + r_turbsin[Q_ftol(((os * 0.125 + rdt) * TURBSCALE)) & 255];
#endif
			t *= (1.0 / 64);

			qglTexCoord2f(s, t);
			qglVertex3fv(v);
		}
		qglEnd();
	}
}
