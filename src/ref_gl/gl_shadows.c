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
/* shadows.c: shadow functions */

#include "gl_local.h"

vec4_t s_lerped[MAX_VERTS];
vec3_t shadevector;
float shadelight[3];

extern bool_t nolight;
int c_shadow_volumes = 0;
int worldlight = 0;

/*
=============================================
  Dynamic Planar Shadows
  Original Code By Psychospaz
  Modificated By Vortex and Kirk Barnes
=============================================
*/
void vectoangles(vec3_t value1, vec3_t angles)
{
	float forward;
	float yaw, pitch;

	if (value1[1] == 0 && value1[0] == 0) {
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	} else {
		/* PMM - fixed to correct for pitch of 0 */
		if (value1[0])
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		if (yaw < 0)
			yaw += 360;

		forward = sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
		pitch = (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

bool_t nolight;
void R_ShadowLight(vec3_t pos, vec3_t lightAdd)
{
	int i;
	dlight_t *dl;
	float shadowdist;
	float add;
	vec3_t dist;
	vec3_t angle;

	nolight = false;
	VectorClear(lightAdd);		/* clear planar shadow vector */

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	/* add dynamic light shadow angles */
	dl = r_newrefdef.dlights;
	for (i = 0; i < r_newrefdef.num_dlights; i++, dl++) {
/*		if (dl->spotlight) // spotlights */
/*			continue; */

		VectorSubtract(dl->origin, pos, dist);
		add = sqrt(dl->intensity - VectorLength(dist));
		VectorNormalize(dist);
		if (add > 0) {
			VectorScale(dist, add, dist);
			VectorAdd(lightAdd, dist, lightAdd);
		}
	}

	shadowdist = VectorNormalize(lightAdd);
	if (shadowdist > 1)
		shadowdist = 1;
	if (shadowdist <= 0) {
		/* old style static shadow */
		/* scaled nolight shadow */
		nolight = true;
		return;
	} else {
		/* shadow from dynamic lights */
		vectoangles(lightAdd, angle);
		angle[YAW] -= currententity->angles[YAW];
	}
	AngleVectors(angle, dist, NULL, NULL);
	VectorScale(dist, shadowdist, lightAdd);
}

/*
=============
GL_DrawAliasShadow
Modifications by MrG, Carbon14, Kirk Barnes
=============
*/

extern cvar_t *r_shading;
dlight_t model_dlights[MAX_MODEL_DLIGHTS];
int model_dlights_num;

void GL_DrawAliasShadow(entity_t * e, dmdl_t * paliashdr, int posenum)
{
	dtrivertx_t *verts;
	int *order;
	vec3_t point;
	float height, lheight, alpha;
	int count;

	daliasframe_t *frame;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	/*calculate model lighting and setup shadow transparenty */
/*	if(r_shading->value) */
/*		R_LightPointDynamics (currententity->origin, shadelight, model_dlights, &model_dlights_num, 3); */
/*	else */
	R_LightPoint(currententity->origin, shadelight);

	alpha = 1 - (shadelight[0] + shadelight[1] + shadelight[2]);

	if (alpha <= 0.3)
		alpha = 0.3;

	lheight = currententity->origin[2];

	frame = (daliasframe_t *) ((uint8_t*) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);

	verts = frame->verts;
	height = 0;
	order = (int *) ((uint8_t*) paliashdr + paliashdr->ofs_glcmds);
	height = -lheight;

	if (nolight)
		qglScalef(1.1, 1.1, 1);	/* scale  nolight shadow by Kirk Barnes */

	qglColor4f(0, 0, 0, alpha);
	qglPolygonOffset(-2.0, 1.0);	/* shadow on the floor c14 */
	qglEnable(GL_POLYGON_OFFSET_FILL);

	qglEnable(GL_STENCIL_TEST);	/* stencil buffered shadow by MrG */
	qglStencilFunc(GL_GREATER, 2, 2);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	while (1) {
		/* get the vertex count and primitive type */
		count = *order++;
		if (!count)
			break;				/* done */
		if (count < 0) {
			count = -count;
			qglBegin(GL_TRIANGLE_FAN);
		} else
			qglBegin(GL_TRIANGLE_STRIP);

		do {
			/* normals and vertexes come from the frame list */
			memcpy(point, s_lerped[order[2]], sizeof(point));
			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
			qglVertex3fv(point);
			order += 3;
		} while (--count);
		qglEnd();
	}

	qglDisable(GL_STENCIL_TEST);
	qglDisable(GL_POLYGON_OFFSET_FILL);
	qglColor4f(1, 1, 1, 1);
}

/*
===============
SHADOW VOLUMES
===============
*/
void BuildShadowVolume(dmdl_t * hdr, vec3_t light, float projectdistance)
{
	dtriangle_t *ot, *tris;
	int i, j;
	bool_t trianglefacinglight[MAX_TRIANGLES];
	vec3_t v0, v1, v2, v3;

	daliasframe_t *frame;
	dtrivertx_t *verts;

	frame = (daliasframe_t *) ((uint8_t*) hdr + hdr->ofs_frames + currententity->as.frame * hdr->framesize);
	verts = frame->verts;

	ot = tris = (dtriangle_t *) ((unsigned char *) hdr + hdr->ofs_tris);

	for (i = 0; i < hdr->num_tris; i++, tris++) {
		for (j = 0; j < 3; j++) {
			v0[j] = s_lerped[tris->index_xyz[0]][j];
			v1[j] = s_lerped[tris->index_xyz[1]][j];
			v2[j] = s_lerped[tris->index_xyz[2]][j];
		}

		trianglefacinglight[i] = (light[0] - v0[0]) * ((v0[1] - v1[1]) * (v2[2] - v1[2]) - (v0[2] - v1[2]) * (v2[1] - v1[1]))
			+ (light[1] - v0[1]) * ((v0[2] - v1[2]) * (v2[0] - v1[0]) - (v0[0] - v1[0]) * (v2[2] - v1[2]))
			+ (light[2] - v0[2]) * ((v0[0] - v1[0]) * (v2[1] - v1[1]) - (v0[1] - v1[1]) * (v2[0] - v1[0])) > 0;
	}

	qglBegin(GL_QUADS);
	for (i = 0, tris = ot; i < hdr->num_tris; i++, tris++) {
		if (!trianglefacinglight[i])
			continue;

		if (!trianglefacinglight[currentmodel->edge_tri[i][0]]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[1]][j];
				v1[j] = s_lerped[tris->index_xyz[0]][j];
				v2[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
				v3[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			}
			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			qglVertex3fv(v3);
		}

		if (!trianglefacinglight[currentmodel->edge_tri[i][1]]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[2]][j];
				v1[j] = s_lerped[tris->index_xyz[1]][j];
				v2[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
				v3[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			}
			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			qglVertex3fv(v3);
		}
		if (!trianglefacinglight[currentmodel->edge_tri[i][2]]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[0]][j];
				v1[j] = s_lerped[tris->index_xyz[2]][j];
				v2[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
				v3[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			}
			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			qglVertex3fv(v3);
		}
	}
	qglEnd();

	/* cap the volume */
	qglBegin(GL_TRIANGLES);
	for (i = 0, tris = ot; i < hdr->num_tris; i++, tris++) {
		if (trianglefacinglight[i]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[0]][j];
				v1[j] = s_lerped[tris->index_xyz[1]][j];
				v2[j] = s_lerped[tris->index_xyz[2]][j];
			}
			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			continue;
		}
		for (j = 0; j < 3; j++) {
			v0[j] = s_lerped[tris->index_xyz[0]][j];
			v1[j] = s_lerped[tris->index_xyz[1]][j];
			v2[j] = s_lerped[tris->index_xyz[2]][j];
			v0[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			v1[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
			v2[j] = v2[j] + ((v2[j] - light[j]) * projectdistance);
		}
		qglVertex3fv(v0);
		qglVertex3fv(v1);
		qglVertex3fv(v2);
	}
	qglEnd();
}

void GL_RenderVolumes(dmdl_t * paliashdr, vec3_t lightdir, int projdist)
{
	int incr = gl_state.stencil_wrap ? GL_INCR_WRAP_EXT : GL_INCR;
	int decr = gl_state.stencil_wrap ? GL_DECR_WRAP_EXT : GL_DECR;

	if (!gl_state.ati_separate_stencil && !gl_state.stencil_two_side) {
		qglCullFace(GL_BACK);
		qglStencilOp(GL_KEEP, incr, GL_KEEP);
		BuildShadowVolume(paliashdr, lightdir, projdist);
		qglCullFace(GL_FRONT);
		qglStencilOp(GL_KEEP, decr, GL_KEEP);
		BuildShadowVolume(paliashdr, lightdir, projdist);
	}

	if (gl_state.ati_separate_stencil) {	/* ati separate stensil support for r300+ by Kirk Barnes */
		qglDisable(GL_CULL_FACE);
		qglStencilOpSeparateATI(GL_BACK, GL_KEEP, incr, GL_KEEP);
		qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, decr, GL_KEEP);
		BuildShadowVolume(paliashdr, lightdir, projdist);
	}

	if (gl_state.stencil_two_side) {	/* two side stensil support for nv30+ by Kirk Barnes */
		qglDisable(GL_CULL_FACE);
		qglActiveStencilFaceEXT(GL_BACK);
		qglStencilOp(GL_KEEP, incr, GL_KEEP);
		qglActiveStencilFaceEXT(GL_FRONT);
		qglStencilOp(GL_KEEP, decr, GL_KEEP);
		BuildShadowVolume(paliashdr, lightdir, projdist);
	}
}

void GL_DrawAliasShadowVolume(dmdl_t * paliashdr, int posenumm)
{
	int *order, i, o, dist;
	vec3_t light, temp;
	dtriangle_t *t, *tris;

	daliasframe_t *frame, *oldframe;
	dtrivertx_t *ov, *verts;
	dlight_t *l;
	float cost, sint, is, it;
	int projected_distance;

	l = r_newrefdef.dlights;

	if (gl_shadows->value != 2)
		return;
	if (r_newrefdef.vieworg[2] < (currententity->origin[2]))
		return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;
	if (currentmodel->noshadow)
		return;

	t = tris = (dtriangle_t *) ((unsigned char *) paliashdr + paliashdr->ofs_tris);

	frame = (daliasframe_t *) ((uint8_t*) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);
	verts = frame->verts;

	oldframe = (daliasframe_t *) ((uint8_t*) paliashdr + paliashdr->ofs_frames + currententity->as.oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *) ((uint8_t*) paliashdr + paliashdr->ofs_glcmds);

	cost = cos(-currententity->angles[1] / 180 * M_PI), sint = sin(-currententity->angles[1] / 180 * M_PI);

/*	if (!ri.IsVisible(r_newrefdef.vieworg, currententity->origin)) return; */

	if (gl_shadow_debug_volume->value)
		qglColor3f(1, 0, 0);
	else
		qglColorMask(0, 0, 0, 0);

	if (gl_state.stencil_two_side)
		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

	qglEnable(GL_STENCIL_TEST);

/*	if (clamp) qglEnable(GL_DEPTH_CLAMP_NV); */

	qglDepthMask(false);
	qglStencilMask((GLuint) ~ 0);
	qglDepthFunc(GL_LESS);

	if (gl_state.ati_separate_stencil)
		qglStencilFuncSeparateATI(GL_ALWAYS, GL_ALWAYS, 0, (GLuint) ~ 0);

	qglStencilFunc(GL_ALWAYS, 0, (GLuint) ~ 0);

	for (i = 0; i < r_newrefdef.num_dlights; i++, l++) {
		if ((l->origin[0] == currententity->origin[0]) && (l->origin[1] == currententity->origin[1]) && (l->origin[2] == currententity->origin[2]))
			continue;
		VectorSubtract(currententity->origin, l->origin, temp);
		dist = sqrt(DotProduct(temp, temp));

		if (dist > 200)
			continue;
/*		if (!ri.IsVisible(currententity->origin, l->origin)) continue; */

		/* lights origin in relation to the entity */
		for (o = 0; o < 3; o++)
			light[o] = -currententity->origin[o] + l->origin[o];

		is = light[0], it = light[1];
		light[0] = (cost * (is - 0) + sint * (0 - it) + 0);
		light[1] = (cost * (it - 0) + sint * (is - 0) + 0);
		light[2] += 8;

		c_shadow_volumes++;
		worldlight++;
	}

	if (!worldlight) {
		/*old style static shadow vector */
		VectorSet(light, 130, 0, 200);
		is = light[0], it = light[1];
		light[0] = (cost * (is - 0) + sint * (0 - it) + 0);
		light[1] = (cost * (it - 0) + sint * (is - 0) + 0);
		light[2] += 8;
		c_shadow_volumes++;
		projected_distance = 1;
	} else
		projected_distance = 25;

	GL_RenderVolumes(paliashdr, light, projected_distance);

	if (gl_state.stencil_two_side)
		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);

	qglDisable(GL_STENCIL_TEST);

/* 	if (clamp) qglDisable(GL_DEPTH_CLAMP_NV); */

	if (gl_shadow_debug_volume->value)
		qglColor3f(1, 1, 1);
	else
		qglColorMask(1, 1, 1, 1);

	qglDepthMask(true);
	qglDepthFunc(GL_LEQUAL);
}

void R_DrawShadow(entity_t * e)
{
	dmdl_t *paliashdr;

	daliasframe_t *frame, *oldframe;
	dtrivertx_t *v, *ov, *verts;
	int *order;
	float frontlerp;
	vec3_t move, delta, vectors[3];
	vec3_t frontv, backv;
	int i;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (currententity->flags & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE))
		return;

	paliashdr = (dmdl_t *) currentmodel->extradata;

	frame = (daliasframe_t *) ((uint8_t*) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *) ((uint8_t*) paliashdr + paliashdr->ofs_frames + currententity->as.oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *) ((uint8_t*) paliashdr + paliashdr->ofs_glcmds);

	frontlerp = 1.0 - currententity->as.backlerp;

	/* move should be the delta back to the previous frame * backlerp */
	VectorSubtract(currententity->oldorigin, currententity->origin, delta);
	AngleVectors(currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]);	/* forward */
	move[1] = -DotProduct(delta, vectors[1]);	/* left */
	move[2] = DotProduct(delta, vectors[2]);	/* up */

	VectorAdd(move, oldframe->translate, move);

	for (i = 0; i < 3; i++) {
		move[i] = currententity->as.backlerp * move[i] + frontlerp * frame->translate[i];
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = currententity->as.backlerp * oldframe->scale[i];
	}

/*	GL_LerpVerts( paliashdr->num_xyz, v, ov, verts, s_lerped[0], move, frontv, backv,0); */

	/*|RF_NOSHADOW */
	if (gl_shadows->value == 1 && !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL))) {
		qglPushMatrix();
		{
			vec3_t end;

			qglDisable(GL_TEXTURE_2D);
			qglEnable(GL_BLEND);
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);
			qglRotatef(e->angles[1], 0, 0, 1);
			VectorSet(end, currententity->origin[0], currententity->origin[1], currententity->origin[2] - 2048);
			RecursiveLightPoint(rTiles[0]->nodes, currententity->origin, end);
			R_ShadowLight(currententity->origin, shadevector);
			GL_DrawAliasShadow(e, paliashdr, currententity->as.frame);
		}
		qglEnable(GL_TEXTURE_2D);
		qglDisable(GL_BLEND);
		qglPopMatrix();
	}
}

void R_DrawShadowVolume(entity_t * e)
{
	dmdl_t *paliashdr;

	daliasframe_t *frame, *oldframe;
	dtrivertx_t *v, *ov, *verts;
	float frontlerp;
	vec3_t move, delta, vectors[3];
	vec3_t frontv, backv;
	int i;
	int *order;

	if (gl_shadows->value != 2)
		return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	paliashdr = (dmdl_t *) currentmodel->extradata;

	frame = (daliasframe_t *) ((uint8_t*) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *) ((uint8_t*) paliashdr + paliashdr->ofs_frames + currententity->as.oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *) ((uint8_t*) paliashdr + paliashdr->ofs_glcmds);

	frontlerp = 1.0 - currententity->as.backlerp;

	/* move should be the delta back to the previous frame * backlerp */
	VectorSubtract(currententity->oldorigin, currententity->origin, delta);
	AngleVectors(currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]);	/* forward */
	move[1] = -DotProduct(delta, vectors[1]);	/* left */
	move[2] = DotProduct(delta, vectors[2]);	/* up */

	VectorAdd(move, oldframe->translate, move);

	for (i = 0; i < 3; i++) {
		move[i] = currententity->as.backlerp * move[i] + frontlerp * frame->translate[i];
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = currententity->as.backlerp * oldframe->scale[i];
	}

/*	GL_LerpVerts( paliashdr->num_xyz, v, ov, verts, s_lerped[0], move, frontv, backv,0); */

/*	|RF_NOSHADOW|RF_NOSHADOW2 */
	if (gl_shadows->value == 2 && !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL))) {
		qglPushMatrix();
		{
			qglDisable(GL_TEXTURE_2D);
			qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);
			qglRotatef(e->angles[1], 0, 0, 1);
			GL_DrawAliasShadowVolume(paliashdr, currententity->as.frame);
		}
		qglEnable(GL_TEXTURE_2D);
		qglPopMatrix();
	}
}

/*
==============
R_ShadowBlend
==============
*/
void R_ShadowBlend(void)
{
	if (gl_shadows->value != 2)
		return;

	qglLoadIdentity();

	/* FIXME: get rid of these */
	qglRotatef(-90, 1, 0, 0);	/* put Z going up */
	qglRotatef(90, 0, 0, 1);	/* put Z going up */

	if (gl_shadow_debug_shade->value)
		qglColor3f(0, 0, 1);
	else
		qglColor4f(0, 0, 0, 0.3);

	GLSTATE_DISABLE_ALPHATEST GLSTATE_ENABLE_BLEND qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglDisable(GL_DEPTH_TEST);

	qglDepthMask(false);

	qglEnable(GL_STENCIL_TEST);

	if (gl_state.ati_separate_stencil)
		qglStencilFuncSeparateATI(GL_NOTEQUAL, GL_NOTEQUAL, 0, (GLuint) ~ 0);

	qglStencilFunc(GL_NOTEQUAL, 0, (GLuint) ~ 0);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglDisable(GL_TEXTURE_2D);
	qglBegin(GL_QUADS);
	qglVertex3f(10, 100, 100);
	qglVertex3f(10, -100, 100);
	qglVertex3f(10, -100, -100);
	qglVertex3f(10, 100, -100);
	qglEnd();

	GLSTATE_DISABLE_BLEND qglEnable(GL_TEXTURE_2D);
	GLSTATE_ENABLE_ALPHATEST qglDisable(GL_STENCIL_TEST);
	qglDepthMask(true);

	if (gl_shadow_debug_shade->value)
		qglColor3f(1, 1, 1);
	else
		qglColor4f(1, 1, 1, 1);
}
