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
// gl_mesh.c: triangle model functions

#include "gl_local.h"

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

static	vec4_t	s_lerped[MAX_VERTS];
float	normalArray[MAX_VERTS*3];
//static	vec3_t	lerped[MAX_VERTS];

vec3_t	shadevector;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

float	*shadedots = r_avertexnormal_dots[0];

void GL_LerpVerts( int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] )
{
	int i;

	for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4)
	{
		lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0];
		lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1];
		lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2];
	}
}

/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GL_DrawAliasFrameLerp (dmdl_t *paliashdr, float backlerp, int framenum, int oldframenum )
{
//	float 	l;
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t	*v, *ov, *verts;
	int		*order;
	int		count;
	float	frontlerp;
	vec3_t	move;
//	vec3_t	delta;
	vec3_t	frontv, backv;
	int		i;
	float	*lerp;
	float	*na;
//	float	*matrix;
	float	*oldNormal, *newNormal;

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
		+ framenum * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
		+ oldframenum * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

//	glTranslatef (frame->translate[0], frame->translate[1], frame->translate[2]);
//	glScalef (frame->scale[0], frame->scale[1], frame->scale[2]);

	frontlerp = 1.0 - backlerp;

#if 0
	// move should be the delta back to the previous frame * backlerp
	matrix = trafo[currententity - r_newrefdef.entities].matrix;
	VectorSubtract (currententity->oldorigin, currententity->origin, delta);

	for (i=0 ; i<3 ; i++)
	{
		move[i] = backlerp*(DotProduct(delta, (matrix+i*4)) + oldframe->translate[i]) + frontlerp*frame->translate[i];
	}
#endif

	for (i=0 ; i<3 ; i++)
	{
		move[i] = backlerp*(oldframe->translate[i]) + frontlerp*frame->translate[i];
	}

	for (i=0 ; i<3 ; i++)
	{
		frontv[i] = frontlerp*frame->scale[i];
		backv[i] = backlerp*oldframe->scale[i];
	}

	lerp = s_lerped[0];

	GL_LerpVerts( paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv );

	// setup vertex array
	qglEnableClientState( GL_VERTEX_ARRAY );
	qglVertexPointer( 3, GL_FLOAT, 16, s_lerped );	// padded for SIMD

	// setup normal array
	qglEnableClientState( GL_NORMAL_ARRAY );
	qglNormalPointer( GL_FLOAT, 0, normalArray );

	na = normalArray;
	if ( backlerp == 0.0 )
		for ( i = 0; i < paliashdr->num_xyz; v++, i++ )
		{
			// get current normals
			newNormal = r_avertexnormals[v->lightnormalindex];

			*na++ = newNormal[0];
			*na++ = newNormal[1];
			*na++ = newNormal[2];
		}
	else
		for ( i = 0; i < paliashdr->num_xyz; v++, ov++, i++ )
		{
			// normalize interpolated normals?
			// no: probably too much to compute
			oldNormal = r_avertexnormals[ov->lightnormalindex];
			newNormal = r_avertexnormals[ v->lightnormalindex];

			*na++ = backlerp*oldNormal[0] + frontlerp*newNormal[0];
			*na++ = backlerp*oldNormal[1] + frontlerp*newNormal[1];
			*na++ = backlerp*oldNormal[2] + frontlerp*newNormal[2];
		}

	if ( qglLockArraysEXT != 0 )
		qglLockArraysEXT( 0, paliashdr->num_xyz );

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break; // done
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		}
		else
		{
			qglBegin (GL_TRIANGLE_STRIP);
		}
		do
		{
			// texture coordinates come from the draw list
			qglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			order += 2;

			qglArrayElement( *order++ );

		} while (--count);

		qglEnd ();
	}

	if ( qglUnlockArraysEXT != 0 )
		qglUnlockArraysEXT();
}


#if 1
/*
=============
GL_DrawAliasShadow
=============
*/
//extern	vec3_t			lightspot;

void GL_DrawAliasShadow (dmdl_t *paliashdr, int posenum)
{
	dtrivertx_t	*verts;
	int		*order;
	vec3_t	point;
	float	height, lheight;
	int		count;
	daliasframe_t	*frame;

	lheight = currententity->origin[2];// - lightspot[2];

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
		+ currententity->as.frame * paliashdr->framesize);
	verts = frame->verts;

	height = 0;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break; // done
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		}
		else
			qglBegin (GL_TRIANGLE_STRIP);

		do
		{
			// normals and vertexes come from the frame list
/*
			point[0] = verts[order[2]].v[0] * frame->scale[0] + frame->translate[0];
			point[1] = verts[order[2]].v[1] * frame->scale[1] + frame->translate[1];
			point[2] = verts[order[2]].v[2] * frame->scale[2] + frame->translate[2];
*/

			memcpy( point, s_lerped[order[2]], sizeof( point )  );

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			qglVertex3fv (point);

			order += 3;

//			verts++;

		} while (--count);

		qglEnd ();
	}
}

#endif

/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel( entity_t *e )
{
	int i;
	vec3_t		mins, maxs;
	dmdl_t		*paliashdr;
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
// 	vec3_t		angles;
	vec4_t		bbox[8];

	if ( r_isometric->value )
		return false;

	paliashdr = (dmdl_t *)currentmodel->extradata;

	pframe = ( daliasframe_t * ) ( ( byte * ) paliashdr +
		                              paliashdr->ofs_frames +
									  e->as.frame * paliashdr->framesize);

	poldframe = ( daliasframe_t * ) ( ( byte * ) paliashdr +
		                              paliashdr->ofs_frames +
									  e->as.oldframe * paliashdr->framesize);

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i]*255;
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i]*255;

			oldmins[i]  = poldframe->translate[i];
			oldmaxs[i]  = oldmins[i] + poldframe->scale[i]*255;

			if ( thismins[i] < oldmins[i] )
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];
		}
	}

	/*
	** compute a full bounding box
	*/
	for ( i = 0; i < 8; i++ )
	{
		vec3_t   tmp;

		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy( tmp, bbox[i] );
		bbox[i][3] = 1.0;
	}

	/*
	** transform the bounding box
	*/
	for ( i = 0; i < 8; i++ )
	{
		vec4_t		tmp;

		Vector4Copy( bbox[i], tmp );
		GLVectorTransform( trafo[e - r_newrefdef.entities].matrix, tmp, bbox[i] );
	}

	{
		int p, f, aggregatemask = ~0;

		for ( p = 0; p < 8; p++ )
		{
			int mask = 0;

			for ( f = 0; f < 4; f++ )
			{
				float dp = DotProduct( frustum[f].normal, bbox[p] );

				if ( ( dp - frustum[f].dist ) < 0 )
				{
					mask |= ( 1 << f );
				}
			}

			aggregatemask &= mask;
		}

		if ( aggregatemask )
		{
			return true;
		}

		return false;
	}
}

/*
=================
R_EnableLights

=================
*/
void R_EnableLights( qboolean fixed, float *matrix, float *lightparam, float *lightambient )
{
	dlight_t	*light;
	vec3_t		delta;
	float		bright, sumBright, lsqr, max;
//	vec4_t		color;
	vec4_t		sumColor, trorigin, sumDelta;
	int			i, j, n;

//	if ( !r_newrefdef.num_lights )
//		return;

	if ( fixed )
	{
		// add the fixed light
		qglLightfv( GL_LIGHT0, GL_POSITION, lightparam );
		qglLightfv( GL_LIGHT0, GL_DIFFUSE, matrix );
		qglLightfv( GL_LIGHT0, GL_AMBIENT, lightambient );

		// enable the lighting
		qglEnable( GL_LIGHTING );
		qglEnable( GL_LIGHT0 );
		return;
	}

	VectorClear( sumDelta );
	sumDelta[3] = 1.0;
	GLVectorTransform( matrix, sumDelta, trorigin );

	if ( *lightparam != 0.0 )
	{
		VectorScale( r_newrefdef.sun->dir, 1.0, sumDelta );
		sumBright = 0.7;
		VectorScale( r_newrefdef.sun->color, sumBright, sumColor );
	} else {
		VectorClear( sumDelta );
		VectorClear( sumColor );
		sumBright = 0;
	}

	for ( j = 0; j < 2; j++ )
	{
		if ( j ) {
			light = r_newrefdef.ll;
			n = r_newrefdef.num_lights;
		} else {
			light = r_newrefdef.dlights;
			n = r_newrefdef.num_dlights;
		}

		for ( i = 0; i < n; i++, light++ )
		{
			VectorSubtract( light->origin, trorigin, delta );
			lsqr = VectorLengthSqr( delta ) + 1;
			bright = 8.0 * light->intensity / (lsqr + 64*64);
			sumBright += bright;
			VectorMA( sumColor, bright, light->color, sumColor );
			VectorScale( delta, 1.0 / sqrt(lsqr), delta );
			VectorMA( sumDelta, bright, delta, sumDelta );
//			ri.Con_Printf( PRINT_ALL, "%f %f\n", bright, VectorLength( delta ) );
		}
	}

	// normalize delta and color
	VectorNormalize( sumDelta );
	VectorMA( trorigin, 512, sumDelta, sumDelta );
	sumDelta[3] = 0.0;
	VectorScale( sumColor, sumBright, sumColor );
	sumColor[3] = 1.0;

	max = sumColor[0];
	if ( sumColor[1] > max ) max = sumColor[1];
	if ( sumColor[2] > max ) max = sumColor[2];
	if ( max > 2.0 ) VectorScale( sumColor, 2.0 / max, sumColor );

	// add the light
	qglLightfv( GL_LIGHT0, GL_POSITION, sumDelta );
	qglLightfv( GL_LIGHT0, GL_DIFFUSE, sumColor );
	qglLightfv( GL_LIGHT0, GL_AMBIENT, r_newrefdef.sun->ambient );

	// enable the lighting
	qglEnable( GL_LIGHTING );
	qglEnable( GL_LIGHT0 );
}


/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel (entity_t *e)
{
	qboolean	lightfixed;
// 	int			i;
	dmdl_t		*paliashdr;
// 	float		an;
	image_t		*skin;

	paliashdr = (dmdl_t *)currentmodel->extradata;

	// check animations
	if ( ( e->as.frame >= paliashdr->num_frames ) || ( e->as.frame < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such frame %d\n",
			currentmodel->name, e->as.frame);
		e->as.frame = 0;
	}
	if ( ( e->as.oldframe >= paliashdr->num_frames ) || ( e->as.oldframe < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n",
			currentmodel->name, e->as.oldframe);
		e->as.oldframe = 0;
	}

	if ( !r_lerpmodels->value )
		e->as.backlerp = 0;

	// check if model is out of fov
	if ( R_CullAliasModel( e ) )
		return;

	// select skin
	if (e->skin)
		skin = e->skin;	// custom player skin
	else
	{
		if (e->skinnum >= MAX_MD2SKINS)
			skin = currentmodel->skins[0];
		else
		{
			skin = currentmodel->skins[e->skinnum];
			if (!skin)
				skin = currentmodel->skins[0];
		}
	}
	if (!skin)
		skin = r_notexture;	// fallback...

	if ( skin->has_alpha && !(e->flags & RF_TRANSLUCENT) )
	{
		// it will be drawn in the next entity render pass
		// for the translucent entities
		e->flags |= RF_TRANSLUCENT;
		if ( !e->alpha ) e->alpha = 1.0;
		return;
	}

	//
	// locate the proper data
	//
	c_alias_polys += paliashdr->num_tris;

	//
	// set-up lighting
	//
	lightfixed = e->flags & RF_LIGHTFIXED ? true : false;
	if ( lightfixed )  R_EnableLights( lightfixed, e->lightcolor, e->lightparam, e->lightambient );
	else R_EnableLights( lightfixed, trafo[e - r_newrefdef.entities].matrix, e->lightparam, NULL );

	// IR goggles override color
	if ( r_newrefdef.rdflags & RDF_IRGOGGLES ) qglColor4f( 1.0, 0.0, 0.0, e->alpha );
	else qglColor4f( 1.0, 1.0, 1.0, e->alpha );

	//
	// draw all the triangles
	//
	if (e->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	qglPushMatrix ();

	qglMultMatrixf( trafo[e - r_newrefdef.entities].matrix );

	// draw it
	GL_Bind(skin->texnum);

	if ( !(e->flags & RF_NOSMOOTH ) ) qglShadeModel( GL_SMOOTH );

	if ( gl_combine )
	{
		GL_TexEnv( gl_combine );
		qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, intensity->value);
	}
	else
	{
		GL_TexEnv( GL_MODULATE );
	}

	if ( (e->flags & RF_TRANSLUCENT) )
	{
		qglEnable (GL_BLEND);
	}

	GL_DrawAliasFrameLerp (paliashdr, e->as.backlerp, e->as.frame, e->as.oldframe);

	GL_TexEnv( GL_REPLACE );
	qglDisable( GL_LIGHTING );

	if ( !(e->flags & RF_NOSMOOTH ) ) qglShadeModel( GL_FLAT );

	if ( ( e->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
		qglMatrixMode( GL_PROJECTION );
		qglPopMatrix();
		qglMatrixMode( GL_MODELVIEW );
		qglCullFace( GL_FRONT );
	}

	if ( (e->flags & RF_TRANSLUCENT) || (skin && skin->has_alpha) )
	{
		qglDisable (GL_BLEND);
	}

	if ( gl_shadows->value && (e->flags & RF_SHADOW) )
	{
		if ( !(e->flags & RF_TRANSLUCENT) ) qglDepthMask (0);
		qglEnable (GL_BLEND);

		qglColor4f( 1, 1, 1, 1 );
		GL_Bind( shadow->texnum );
		qglBegin( GL_QUADS );

		qglTexCoord2f( 0.0, 1.0); qglVertex3f(-18.0, 14.0, -28.5 );
		qglTexCoord2f( 1.0, 1.0); qglVertex3f( 10.0, 14.0, -28.5 );
		qglTexCoord2f( 1.0, 0.0); qglVertex3f( 10.0,-14.0, -28.5 );
		qglTexCoord2f( 0.0, 0.0); qglVertex3f(-18.0,-14.0, -28.5 );

		qglEnd();

		qglDisable (GL_BLEND);
		if ( !(e->flags & RF_TRANSLUCENT) ) qglDepthMask (1);
	}

	if ( gl_fog->value && r_newrefdef.fog ) qglDisable( GL_FOG );

	// draw the circles for team-members and allied troops
	if (e->flags & (RF_SELECTED | RF_ALLIED | RF_MEMBER))
	{
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_LINE_SMOOTH );
		qglEnable( GL_BLEND );

		if ( e->flags & RF_MEMBER )
		{
			if ( e->flags & RF_SELECTED )
				qglColor4f( 0, 1, 0, 1 );
			else
				qglColor4f( 0, 1, 0, 0.3 );
		} else if ( e->flags & RF_ALLIED )
		{
			if ( e->flags & RF_SELECTED )
				qglColor4f( 0, 0.5, 1, 1 );
			else
				qglColor4f( 0, 0.5, 1, 0.3 );
		} else
			qglColor4f( 0, 1, 0, 1 );

		qglBegin( GL_LINE_STRIP );

		// circle points
		qglVertex3f( 10.0,  0.0, -27.0 );
		qglVertex3f(  7.0, -7.0, -27.0 );
		qglVertex3f(  0.0,-10.0, -27.0 );
		qglVertex3f( -7.0, -7.0, -27.0 );
		qglVertex3f(-10.0,  0.0, -27.0 );
		qglVertex3f( -7.0,  7.0, -27.0 );
		qglVertex3f(  0.0, 10.0, -27.0 );
		qglVertex3f(  7.0,  7.0, -27.0 );
		qglVertex3f( 10.0,  0.0, -27.0 );

		qglEnd();

		qglDisable (GL_BLEND);
		qglDisable( GL_LINE_SMOOTH );
		qglEnable( GL_TEXTURE_2D );
	}

	qglPopMatrix ();

	if (e->flags & RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmax);

#if 0
	if (gl_shadows->value && !(e->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)))
	{
		qglPushMatrix ();
		R_RotateForEntity (e);
		qglDisable (GL_TEXTURE_2D);
		qglEnable (GL_BLEND);
		qglColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, e->frame );
		qglEnable (GL_TEXTURE_2D);
		qglDisable (GL_BLEND);
		qglPopMatrix ();
	}
#endif

	if ( gl_fog->value && r_newrefdef.fog ) qglEnable( GL_FOG );

	qglColor4f (1,1,1,1);
}

/*
=================
R_TransformModelDirect
=================
*/
void R_TransformModelDirect( modelInfo_t *mi )
{
	// translate and rotate
	qglTranslatef( mi->origin[0], mi->origin[1], mi->origin[2] );

	qglRotatef (mi->angles[0], 0, 0, 1);
	qglRotatef (mi->angles[1], 0, 1, 0);
	qglRotatef (mi->angles[2], 1, 0, 0);

	if ( mi->scale )
	{
		// scale by parameters
		qglScalef (mi->scale[0], mi->scale[1], mi->scale[2] );
		if ( mi->center ) qglTranslatef( -mi->center[0], -mi->center[1], -mi->center[2] );
	}
	else if ( mi->center )
	{
		// autoscale
		dmdl_t		*paliashdr;
		daliasframe_t *pframe;

		float	max, size;
		vec3_t	mins, maxs, center;
		int		i;

		// get model data
		paliashdr = (dmdl_t *)mi->model->extradata;
		pframe = (daliasframe_t *) ( (byte *)paliashdr + paliashdr->ofs_frames);

		// get center and scale
		for ( max = 1.0, i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i]*255;
			center[i] = -(mins[i] + maxs[i]) / 2;
			size = maxs[i] - mins[i];
			if ( size > max ) max = size;
		}
		size = (mi->center[0] < mi->center[1] ? mi->center[0] : mi->center[1]) / max;
		qglScalef (size, size, size );
		qglTranslatef( center[0], center[1], center[2] );
	}
}

/*
=================
R_DrawModelDirect
=================
*/
void R_DrawModelDirect( modelInfo_t *mi, modelInfo_t *pmi, char *tagname )
{
	int			i;
	dmdl_t		*paliashdr;
// 	float		an;
	image_t		*skin;
// 	vec4_t		pos, color;

	// register the model
	mi->model = R_RegisterModelShort( mi->name );

	// check if the model exists
	if ( !mi->model || mi->model->type != mod_alias )
		return;

	paliashdr = (dmdl_t *)mi->model->extradata;

	// check animations
	if ( ( mi->frame >= paliashdr->num_frames ) || ( mi->frame < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawModelDirect %s: no such frame %d\n",
			mi->model->name, mi->frame);
		mi->frame = 0;
	}
	if ( ( mi->oldframe >= paliashdr->num_frames ) || ( mi->oldframe < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawModelDirect %s: no such oldframe %d\n",
			mi->model->name, mi->oldframe);
		mi->oldframe = 0;
	}

	if ( !r_lerpmodels->value )
		mi->backlerp = 0;

	// select skin
	if ( mi->skin >= 0 && mi->skin < paliashdr->num_skins )
		skin = mi->model->skins[mi->skin];
	else
		skin = mi->model->skins[0];

	if (!skin)
		skin = r_notexture;	// fallback...

	//
	// locate the proper data
	//
	c_alias_polys += paliashdr->num_tris;

#if 0
	// add a light
	VectorSet( pos, 100, 100, -500 );
	pos[3] = 0.0;
	color[3] = 1.0;
	qglLightfv( GL_LIGHT0, GL_POSITION, pos );
	color[0] = color[1] = color[2] = 5.0;
	qglLightfv( GL_LIGHT0, GL_DIFFUSE, color );
	color[0] = color[1] = color[2] = 0.1;
	qglLightfv( GL_LIGHT0, GL_AMBIENT, color );

	// enable lighting
	qglEnable( GL_LIGHTING );
	qglEnable( GL_LIGHT0 );
#endif
	//
	// draw all the triangles
	//
	qglPushMatrix ();
//	qglLoadIdentity ();
	qglScalef( vid.rx, vid.ry, (vid.rx + vid.ry)/2 );

	if ( mi->color[3] ) qglColor4fv( mi->color );
	else qglColor4f( 1,1,1,1 );

	if ( pmi )
	{
		// register the parent model
		pmi->model = R_RegisterModelShort( pmi->name );

		// transform
		R_TransformModelDirect( pmi );

		// tag trafo
		if ( tagname && pmi->model && pmi->model->tagdata )
		{
			animState_t as;
			dtag_t	*taghdr;
			char	*name;
			float	*tag;
			float	interpolated[16];

			taghdr = (dtag_t *)pmi->model->tagdata;

			// find the right tag
			name = (char *)taghdr + taghdr->ofs_names;
			for ( i = 0; i < taghdr->num_tags; i++, name += MAX_TAGNAME )
				if ( !strcmp( name, tagname ) )
				{
					// found the tag (matrix)
					tag = (float *)((byte *)taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					// do interpolation
					as.frame = pmi->frame;
					as.oldframe = pmi->oldframe;
					as.backlerp = pmi->backlerp;
					R_InterpolateTransform( &as, taghdr->num_frames, tag, interpolated );

					// transform
					qglMultMatrixf( interpolated );
					break;
				}
		}
	}

	// transform
	R_TransformModelDirect( mi );

	// draw it
	GL_Bind(skin->texnum);

	qglShadeModel (GL_SMOOTH);
	qglEnable (GL_DEPTH_TEST);
	qglEnable (GL_CULL_FACE);

	if ( gl_combine )
	{
		GL_TexEnv( gl_combine );
		qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, intensity->value);
	}
	else
	{
		GL_TexEnv( GL_MODULATE );
	}

	if ( (mi->color[3] && mi->color[3] < 1.0f) || ( skin && skin->has_alpha ) )
	{
		qglEnable (GL_BLEND);
	}

	// draw the model
	GL_DrawAliasFrameLerp (paliashdr, mi->backlerp, mi->frame, mi->oldframe);

	GL_TexEnv( GL_MODULATE );
	qglShadeModel( GL_FLAT );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_DEPTH_TEST );
//	qglDisable( GL_LIGHTING );

	if ( (mi->color[3] && mi->color[3] < 1.0f) || ( skin && skin->has_alpha ) )
	{
		qglDisable (GL_BLEND);
	}

	qglPopMatrix ();

	qglColor4f (1,1,1,1);
}

/*
=================
R_DrawModelParticle
=================
*/
void R_DrawModelParticle( modelInfo_t *mi )
{
// 	int			i;
	dmdl_t		*paliashdr;
// 	float		an;
	image_t		*skin;

	// check if the model exists
	if ( !mi->model || mi->model->type != mod_alias )
		return;

	paliashdr = (dmdl_t *)mi->model->extradata;

	// check animations
	if ( ( mi->frame >= paliashdr->num_frames ) || ( mi->frame < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawModelParticle %s: no such frame %d\n",
			mi->model->name, mi->frame);
		mi->frame = 0;
	}
	if ( ( mi->oldframe >= paliashdr->num_frames ) || ( mi->oldframe < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawModelParticle %s: no such oldframe %d\n",
			mi->model->name, mi->oldframe);
		mi->oldframe = 0;
	}

	if ( !r_lerpmodels->value )
		mi->backlerp = 0;

	// select skin
	skin = mi->model->skins[0];
	if (!skin)
		skin = r_notexture;	// fallback...

	//
	// locate the proper data
	//

	c_alias_polys += paliashdr->num_tris;

	//
	// set-up lighting
	//

//	R_EnableLights( trafo[e - r_newrefdef.entities].matrix, e->sunfrac );
	if ( mi->color[3] ) qglColor4fv( mi->color );
	else qglColor4f( 1,1,1,1 );

	//
	// draw all the triangles
	//
	qglPushMatrix ();

	qglTranslatef( mi->origin[0], mi->origin[1], mi->origin[2] );
	qglRotatef (mi->angles[1],  0, 0, 1);
	qglRotatef (-mi->angles[0], 0, 1, 0);
	qglRotatef (-mi->angles[2], 1, 0, 0);

	// draw it
	GL_Bind(skin->texnum);

	qglShadeModel (GL_SMOOTH);
	qglEnable (GL_DEPTH_TEST);
	qglEnable (GL_CULL_FACE);

	if ( gl_combine )
	{
		GL_TexEnv( gl_combine );
		qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, intensity->value);
	}
	else
	{
		GL_TexEnv( GL_MODULATE );
	}

	if ( (mi->color[3] && mi->color[3] < 1.0f) || ( skin && skin->has_alpha ) )
	{
		qglEnable (GL_BLEND);
	}

	// draw the model
	GL_DrawAliasFrameLerp (paliashdr, mi->backlerp, mi->frame, mi->oldframe);

	GL_TexEnv( GL_REPLACE );
	qglShadeModel( GL_FLAT );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_DEPTH_TEST );

	if ( (mi->color[3] && mi->color[3] < 1.0f) || ( skin && skin->has_alpha ) )
	{
		qglDisable (GL_BLEND);
	}

	qglPopMatrix ();

	qglColor4f (1,1,1,1);
}
