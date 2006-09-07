/**
 * @file gl_drawmd3.c
 * @brief MD3 Model code
 */

/*
All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.

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
#include "gl_md3.h"

static float	shadelight_md3[3];

static dlight_t model_dlights_md3[MAX_MODEL_DLIGHTS];
static int model_dlights_num_md3;

/**
 * @brief
 */
static void light_md3_model(vec3_t normal, vec3_t color)
{
	int	i;
	vec3_t	colo[8];
	float s1,s2,s3,angle; /* , dif; */


	VectorClear(color);
	for (i=0; i<model_dlights_num_md3; i++) {
		s1 = model_dlights_md3[i].direction[0]*normal[0] + model_dlights_md3[i].direction[1]*normal[1] + model_dlights_md3[i].direction[2]*normal[2];
		s2 = sqrt(model_dlights_md3[i].direction[0]*model_dlights_md3[i].direction[0] + model_dlights_md3[i].direction[1]*model_dlights_md3[i].direction[1] + model_dlights_md3[i].direction[2]*model_dlights_md3[i].direction[2]);
		s3 = sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);

		angle = s1/(s2*s3);
		colo[i][0] = model_dlights_md3[i].color[0]*(angle*100);
		colo[i][1] = model_dlights_md3[i].color[1]*(angle*100);
		colo[i][2] = model_dlights_md3[i].color[2]*(angle*100);
		colo[i][0] /= 100;
		colo[i][1] /= 100;
		colo[i][2] /= 100;
		color[0] += colo[i][0];
		color[1] += colo[i][1];
		color[2] += colo[i][2];
	}
	color[0] /= model_dlights_num_md3;
	color[1] /= model_dlights_num_md3;
	color[2] /= model_dlights_num_md3;
	if (color[0] < 0 || color[1] < 0 || color[2] < 0)
		color[0] = color[1] = color[2] = 0;
}

/**
 * @brief
 */
static void GL_DrawAliasMD3FrameLerp (maliasmodel_t *paliashdr, maliasmesh_t mesh, float backlerp)
{
	int i,j;
	/*int k;*/
	maliasframe_t	*frame, *oldframe;
	vec3_t	move, delta, vectors[3];
	maliasvertex_t	*v, *ov;
	vec3_t	tempVertexArray[4096];
	vec3_t	tempNormalsArray[4096];
	vec3_t	color1,color2,color3;
	float	alpha;
	/*
	vec3_t	coss, forward1, forward2, norm;
	float	diff1, diff2, diff3, diff;
	vec3_t	lightpoint;
	*/
	float	frontlerp;
	vec3_t	tempangle;

	color1[0] = color1[1] = color1[2] = 0;
	color2[0] = color2[1] = color2[2] = 0;
	color3[0] = color3[1] = color3[2] = 0;

	frontlerp = 1.0 - backlerp;

	if (currententity->flags & RF_TRANSLUCENT)
		alpha = currententity->alpha;
	else
		alpha = 1.0;

	/* if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) */
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )
		qglDisable( GL_TEXTURE_2D );

	frame = paliashdr->frames + currententity->frame;
	oldframe = paliashdr->frames + currententity->oldframe;

	VectorSubtract ( currententity->oldorigin, currententity->origin, delta );
	VectorCopy(currententity->angles, tempangle);
	tempangle[YAW] = tempangle[YAW] - 90;
	AngleVectors (tempangle, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct (delta, vectors[0]);	/* forward */
	move[1] = -DotProduct (delta, vectors[1]);	/* left */
	move[2] = DotProduct (delta, vectors[2]);	/* up */

	VectorAdd (move, oldframe->translate, move);

	for (i=0 ; i<3 ; i++) {
		move[i] = backlerp*move[i] + frontlerp*frame->translate[i];
	}

	v = mesh.vertexes + currententity->frame*mesh.num_verts;
	ov = mesh.vertexes + currententity->oldframe*mesh.num_verts;
	for ( i = 0; i < mesh.num_verts; i++, v++, ov++ ) {
		VectorSet ( tempNormalsArray[i],
				v->normal[0] + (ov->normal[0] - v->normal[0])*backlerp,
				v->normal[1] + (ov->normal[1] - v->normal[1])*backlerp,
				v->normal[2] + (ov->normal[2] - v->normal[2])*backlerp );
		VectorSet ( tempVertexArray[i],
				move[0] + ov->point[0]*backlerp + v->point[0]*frontlerp,
				move[1] + ov->point[1]*backlerp + v->point[1]*frontlerp,
				move[2] + ov->point[2]*backlerp + v->point[2]*frontlerp);
	}
	qglBegin (GL_TRIANGLES);

	for(j=0; j < mesh.num_tris; j++) {
		/* use color instead of shadelight_md3... maybe */
		/* TODO
        if (gl_shading->value == 2) {
			light_md3_model(tempNormalsArray[mesh.indexes[3*j+0]], color1);
			light_md3_model(tempNormalsArray[mesh.indexes[3*j+1]], color2);
			light_md3_model(tempNormalsArray[mesh.indexes[3*j+2]], color3);
		}
		*/
		qglColor4f (shadelight_md3[0], shadelight_md3[1], shadelight_md3[2], alpha);
		qglTexCoord2f (mesh.stcoords[mesh.indexes[3*j+0]].st[0], mesh.stcoords[mesh.indexes[3*j+0]].st[1]);
		qglVertex3fv(tempVertexArray[mesh.indexes[3*j+0]]);

		qglColor4f (shadelight_md3[0], shadelight_md3[1], shadelight_md3[2], alpha);
		qglTexCoord2f (mesh.stcoords[mesh.indexes[3*j+1]].st[0], mesh.stcoords[mesh.indexes[3*j+1]].st[1]);
		qglVertex3fv(tempVertexArray[mesh.indexes[3*j+1]]);

		qglColor4f (shadelight_md3[0], shadelight_md3[1], shadelight_md3[2], alpha);
		qglTexCoord2f (mesh.stcoords[mesh.indexes[3*j+2]].st[0], mesh.stcoords[mesh.indexes[3*j+2]].st[1]);
		qglVertex3fv(tempVertexArray[mesh.indexes[3*j+2]]);
	}
	qglEnd ();

	/* if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) */
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )
		qglEnable( GL_TEXTURE_2D );
}

/*void R_LightPointDynamics (vec3_t p, vec3_t color, dlight_t *list, int *amount, int max);*/

/**
 * @brief
 */
void R_DrawAliasMD3Model (entity_t *e)
{
	maliasmodel_t	*paliashdr;
	image_t		*skin;
	int	i;

	paliashdr = (maliasmodel_t *)currentmodel->extradata;

	/* if ( e->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) ) */
	if ( e->flags & ( RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE ) ) {
		VectorClear (shadelight_md3);
		/*
		if (e->flags & RF_SHELL_HALF_DAM)
		{
				shadelight_md3[0] = 0.56;
				shadelight_md3[1] = 0.59;
				shadelight_md3[2] = 0.45;
		}
		if ( e->flags & RF_SHELL_DOUBLE )
		{
			shadelight_md3[0] = 0.9;
			shadelight_md3[1] = 0.7;
		}
		*/
		if ( e->flags & RF_SHELL_RED )
			shadelight_md3[0] = 1.0;
		if ( e->flags & RF_SHELL_GREEN )
			shadelight_md3[1] = 1.0;
		if ( e->flags & RF_SHELL_BLUE )
			shadelight_md3[2] = 1.0;
	} else if ( e->flags & RF_FULLBRIGHT ) {
		for (i=0 ; i<3 ; i++)
			shadelight_md3[i] = 1.0;
	} else {
		/* TODO
        if (gl_shading->value < 2)
			R_LightPoint (e->origin, shadelight_md3);
/ *		else
			R_LightPointDynamics (e->origin, shadelight_md3, model_dlights_md3, &model_dlights_num_md3, 8);
*/
		if (gl_monolightmap->string[0] != '0') {
			float s = shadelight_md3[0];

			if ( s < shadelight_md3[1] )
				s = shadelight_md3[1];
			if ( s < shadelight_md3[2] )
				s = shadelight_md3[2];

			shadelight_md3[0] = s;
			shadelight_md3[1] = s;
			shadelight_md3[2] = s;
		}
	}
	if (e->flags & RF_MINLIGHT) {
		for (i=0 ; i<3 ; i++)
			if (shadelight_md3[i] > 0.1)
				break;
		if (i == 3) {
			shadelight_md3[0] = 0.1;
			shadelight_md3[1] = 0.1;
			shadelight_md3[2] = 0.1;
		}
	}

	if ( e->flags & RF_GLOW ) {
		/* bonus items will pulse with time */
		float	scale;
		float	min;

		scale = 0.1 * sin(r_newrefdef.time*7);
		for (i=0 ; i<3 ; i++)
		{
			min = shadelight_md3[i] * 0.8;
			shadelight_md3[i] += scale;
			if (shadelight_md3[i] < min)
				shadelight_md3[i] = min;
		}
	}

	if (e->flags & RF_DEPTHHACK) /* hack the depth range to prevent view model from poking into walls */
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	/*
	if ( ( e->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
		extern void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar );

		qglMatrixMode( GL_PROJECTION );
		qglPushMatrix();
		qglLoadIdentity();
		qglScalef( -1, 1, 1 );
	    MYgluPerspective( r_newrefdef.fov_y, ( float ) r_newrefdef.width / r_newrefdef.height,  4,  4096);
		qglMatrixMode( GL_MODELVIEW );

		qglCullFace( GL_BACK );
	}

	if ( e->flags & RF_WEAPONMODEL )
	{
		if ( r_lefthand->value == 2 )
			return;
	}
	*/

	for(i=0; i < paliashdr->num_meshes; i++) {
		c_alias_polys += paliashdr->meshes[i].num_tris;
	}

	qglPushMatrix ();
	e->angles[PITCH] = -e->angles[PITCH];	/* sigh. */
	e->angles[YAW] = e->angles[YAW] - 90;
	R_RotateForEntity (e);
	e->angles[PITCH] = -e->angles[PITCH];	/* sigh. */
	e->angles[YAW] = e->angles[YAW] + 90;

	qglShadeModel (GL_SMOOTH);

	GL_TexEnv( GL_MODULATE );
	if (e->flags & RF_TRANSLUCENT) {
		qglEnable (GL_BLEND);
	}

	if ((e->frame >= paliashdr->num_frames) || (e->frame < 0)) {
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasMD3Model %s: no such frame %d\n", currentmodel->name, e->frame);
		e->frame = 0;
		e->oldframe = 0;
	}

	if ((e->oldframe >= paliashdr->num_frames) || (e->oldframe < 0)) {
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n",
			currentmodel->name, e->oldframe);
		e->frame = 0;
		e->oldframe = 0;
	}

	if (!r_lerpmodels->value)
		e->backlerp = 0;

	for (i=0; i < paliashdr->num_meshes; i++) {
		skin = currentmodel->skins[e->skinnum];
		if (!skin)
			skin = r_notexture;
		GL_Bind(skin->texnum);
		GL_DrawAliasMD3FrameLerp (paliashdr, paliashdr->meshes[i], e->backlerp);
	}
	/*
	if ( ( e->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
		qglMatrixMode( GL_PROJECTION );
		qglPopMatrix();
		qglMatrixMode( GL_MODELVIEW );
		qglCullFace( GL_FRONT );
	}
	*/
	if ( e->flags & RF_TRANSLUCENT ) {
		qglDisable (GL_BLEND);
	}

	if (e->flags & RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmax);

	GL_TexEnv( GL_REPLACE );
	qglShadeModel (GL_FLAT);

	qglPopMatrix ();
	qglColor4f (1,1,1,1);
}
