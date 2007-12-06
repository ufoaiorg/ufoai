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


static void R_MaterialStageVertexColor (const materialStage_t *stage, const vec3_t v, vec4_t color)
{
	float a;

	VectorSet(color, 1.0, 1.0, 1.0);

	/* resolve alpha for vert based on z axis height */
	if (v[2] < stage->terrain.floor)
		a = 0.0;
	else if (v[2] > stage->terrain.ceil)
		a = 1.0;
	else
		a = (v[2] - stage->terrain.floor) / stage->terrain.height;

	color[3] = a;
}

#define UPDATE_THRESHOLD 0.02
void R_UpdateMaterial (material_t *m)
{
	materialStage_t *s;

	if (refdef.time - m->time < UPDATE_THRESHOLD)
		return;

	m->time = refdef.time;

	for (s = m->stages; s; s = s->next) {
		if (s->flags & STAGE_ROTATE){
			s->rotate.dhz = refdef.time * s->rotate.hz * 6.28;
			s->rotate.dsin = sin(s->rotate.dhz);
			s->rotate.dcos = cos(s->rotate.dhz);
		}
	}
}

static void R_StageTexcoord (const materialStage_t *stage, vec3_t v, vec2_t st)
{
	if (stage->flags & STAGE_ROTATE) {
		float s0, t0;
		s0 = st[0] - 0.5;
		t0 = st[1] + 0.5;
		st[0] = stage->rotate.dcos * s0 - stage->rotate.dsin * t0 + 0.5;
		st[1] = stage->rotate.dsin * s0 + stage->rotate.dcos * t0 - 0.5;
	} else {
		st[0] = v[3];
		st[1] = v[4];
	}
}

void R_DrawMaterialSurface (mBspSurface_t *surf, materialStage_t *stage)
{
	int i, nv;
	float *v;
	vec4_t color;
	vec2_t st;

	nv = surf->polys->numverts;
	R_Bind(stage->texture->texnum);
	qglBegin(GL_POLYGON);
	for (i = 0, v = surf->polys->verts[0]; i < nv; i++, v += VERTEXSIZE) {
		if (stage->flags & STAGE_TERRAIN) {
			R_MaterialStageVertexColor(stage, v, color);
			qglColor4fv(color);
		}
		R_StageTexcoord(stage, v, st);
		qglTexCoord2f(st[0], st[1]);
		qglVertex3fv(v);
	}
	qglEnd();
	R_CheckError();
}

