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
		qglTexCoord2f (mesh.stcoords[mesh.indexes[3*j+0]].st[0], mesh.stcoords[mesh.indexes[3*j+0]].st[1]);
		qglVertex3fv(tempVertexArray[mesh.indexes[3*j+0]]);

		qglTexCoord2f (mesh.stcoords[mesh.indexes[3*j+1]].st[0], mesh.stcoords[mesh.indexes[3*j+1]].st[1]);
		qglVertex3fv(tempVertexArray[mesh.indexes[3*j+1]]);

		qglTexCoord2f (mesh.stcoords[mesh.indexes[3*j+2]].st[0], mesh.stcoords[mesh.indexes[3*j+2]].st[1]);
		qglVertex3fv(tempVertexArray[mesh.indexes[3*j+2]]);
	}
	qglEnd ();

	/* if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) */
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )
		qglEnable( GL_TEXTURE_2D );
}

/**
 * @brief
 */
void R_DrawAliasMD3Model (entity_t *e)
{
	maliasmodel_t	*paliashdr;
	image_t		*skin;
	int	i;
	qboolean lightfixed;

	assert (currentmodel->type == mod_alias_md3);

	paliashdr = (maliasmodel_t *)currentmodel->extradata;

	/* set-up lighting */
	lightfixed = e->flags & RF_LIGHTFIXED ? qtrue : qfalse;
	if (lightfixed)
		R_EnableLights(lightfixed, e->lightcolor, e->lightparam, e->lightambient);
	else
		R_EnableLights(lightfixed, trafo[e - r_newrefdef.entities].matrix, e->lightparam, NULL);

	if (e->flags & RF_DEPTHHACK) /* hack the depth range to prevent view model from poking into walls */
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

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
