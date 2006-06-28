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
/* r_main.c */
#include "gl_local.h"
#include <ctype.h>

void R_Clear (void);

viddef_t	vid;

refimport_t	ri;

int gl_texture0, gl_texture1, gl_texture2, gl_texture3;
int gl_combine;

float		gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t  gl_state;

image_t		*r_notexture;		/* use for bad textures */
image_t		*r_particletexture;	/* little dot for particles */

entity_t	*currententity;
model_t		*currentmodel;

cplane_t	frustum[4];

int			r_framecount;		/* used for dlight push checking */

int			c_brush_polys, c_alias_polys;

float		v_blend[4];			/* final blending color */

void GL_Strings_f( void );

/* entity transform */
transform_t	trafo[MAX_ENTITIES];

/* */
/* view origin */
/* */
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

/* */
/* screen size info */
/* */
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_isometric;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;
cvar_t  *r_displayrefresh;
cvar_t  *r_anisotropic;
cvar_t  *r_ext_max_anisotropy;
cvar_t  *r_texture_lod; /* lod_bias */

cvar_t	*gl_nosubimage;
cvar_t	*gl_allow_software;

cvar_t	*gl_vertex_arrays;

cvar_t	*gl_particle_min_size;
cvar_t	*gl_particle_max_size;
cvar_t	*gl_particle_size;
cvar_t	*gl_particle_att_a;
cvar_t	*gl_particle_att_b;
cvar_t	*gl_particle_att_c;

cvar_t	*gl_ext_swapinterval;
cvar_t	*gl_ext_palettedtexture;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_combine;
cvar_t	*gl_ext_pointparameters;
cvar_t	*gl_ext_compiled_vertex_array;
cvar_t	*gl_ext_texture_compression;
cvar_t	*gl_ext_s3tc_compression;

cvar_t	*gl_log;
cvar_t	*gl_bitdepth;
cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_shadow_debug_volume;
cvar_t	*gl_shadow_debug_shade;
cvar_t	*r_ati_separate_stencil;
cvar_t	*r_stencil_two_side;

cvar_t	*gl_embossfilter;
cvar_t	*gl_mode;
cvar_t	*gl_dynamic;
cvar_t  *gl_monolightmap;
cvar_t	*gl_modulate;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
cvar_t	*gl_maxtexres;
cvar_t	*gl_showtris;
cvar_t	*gl_ztrick;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_flashblend;
cvar_t	*gl_playermip;
cvar_t  *gl_saturatelighting;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_texturealphamode;
cvar_t	*gl_texturesolidmode;
cvar_t	*gl_lockpvs;
cvar_t  *gl_wire;
cvar_t	*gl_fog;

cvar_t	*gl_3dlabs_broken;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*vid_ref;
cvar_t  *vid_grabmouse;

void R_CastShadow (void)
{
	int i;

	/* no shadows at all */
	if(!gl_shadows->value)
		return;

	for (i=0;i<r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];
		currentmodel = currententity->model;
		if (!currentmodel)
			continue;
		if (currentmodel->type!=mod_alias)
			continue;
		if(gl_shadows->value ==2)
			R_DrawShadowVolume (currententity);
		else if(gl_shadows->value ==1)
			R_DrawShadow (currententity);
	}
}

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	if (r_nocull->value || r_isometric->value)
		return qfalse;

	for (i=0 ; i<4 ; i++)
		if ( BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
			return qtrue;
	return qfalse;
}


void R_RotateForEntity (entity_t *e)
{
        qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

        qglRotatef (e->angles[1],  0, 0, 1);
        qglRotatef (-e->angles[0],  0, 1, 0);
        qglRotatef (-e->angles[2],  1, 0, 0);
}


/*
=============================================================

  SPRITE MODELS

=============================================================
*/


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	float alpha = 1.0F;
	vec3_t	point;
	dsprframe_t	*frame;
	float		*up, *right;
	dsprite_t		*psprite;

	/* don't even bother culling, because it's just a single */
	/* polygon without a surface cache */

	psprite = (dsprite_t *)currentmodel->extradata;

#if 0
	if (e->frame < 0 || e->frame >= psprite->numframes) {
		ri.Con_Printf (PRINT_ALL, "no such sprite frame %i\n", e->frame);
		e->frame = 0;
	}
#endif
	e->as.frame %= psprite->numframes;

	frame = &psprite->frames[e->as.frame];

#if 0
	if (psprite->type == SPR_ORIENTED) {	/* bullet marks on walls */
	vec3_t		v_forward, v_right, v_up;

	AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	} else
#endif
	{	/* normal sprite */
		up = vup;
		right = vright;
	}

	if ( e->flags & RF_TRANSLUCENT )
		alpha = e->alpha;

	if ( alpha != 1.0F )
		qglEnable( GL_BLEND );

	qglColor4f( 1, 1, 1, alpha );

        GL_Bind(currentmodel->skins[e->as.frame]->texnum);

	GL_TexEnv( GL_MODULATE );

	if ( alpha == 1.0 )
		qglEnable (GL_ALPHA_TEST);
	else
		qglDisable( GL_ALPHA_TEST );

	qglBegin (GL_QUADS);

	qglTexCoord2f (0, 1);
	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (0, 0);
	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 0);
	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 1);
	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);

	qglEnd ();

	qglDisable (GL_ALPHA_TEST);
	GL_TexEnv( GL_REPLACE );

	if ( alpha != 1.0F )
		qglDisable( GL_BLEND );

	qglColor4f( 1, 1, 1, 1 );
}

/*================================================================================== */

/*
=============
R_DrawNullModel
=============
*/

void R_DrawNullModel (void)
{
	vec3_t	shadelight;
	int		i;

/*	if ( currententity->flags & RF_FULLBRIGHT ) */
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
/*	else */
/*		R_LightPoint (currententity->origin, shadelight); */

	qglPushMatrix ();

	qglMultMatrixf( trafo[currententity - r_newrefdef.entities].matrix );

	qglDisable (GL_TEXTURE_2D);

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, -16);
	for (i=0 ; i<=4 ; i++) {
		qglColor3f (0.2 + 0.6*(i%2), 0.0, 0.2 + 0.6*(i%2));
		qglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0);
	}
	qglEnd ();

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, 16);
	for (i=4 ; i>=0 ; i--) {
		qglColor3f (0.2 + 0.6*(i%2), 0.0, 0.2 + 0.6*(i%2));
		qglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0);
	}
	qglEnd ();

	qglColor3f (1,1,1);
	qglPopMatrix ();
	qglEnable (GL_TEXTURE_2D);
}


/*
=============
R_InterpolateTransform
=============
*/
void R_InterpolateTransform( animState_t *as, int numframes, float *tag, float *interpolated )
{
	float	*current, *old;
	float	bl, fl;
	int		i;

	/* range check */
	if ( as->frame >= numframes || as->frame < 0 )
		as->frame = 0;
	if ( as->oldframe >= numframes || as->oldframe < 0 )
		as->oldframe = 0;

	/* calc relevant values */
	current = tag + as->frame*16;
	old = tag + as->oldframe*16;
	bl = as->backlerp;
	fl = 1.0 - as->backlerp;

	/* right on a frame? */
	if ( bl == 0.0 ) {
		memcpy( interpolated, current, sizeof(float)*16 );
		return;
	}
	if ( bl == 1.0 ) {
		memcpy( interpolated, old, sizeof(float)*16 );
		return;
	}

	/* interpolate */
	for ( i = 0; i < 16; i++ )
		interpolated[i] = fl * current[i] + bl * old[i];

	/* normalize */
	for ( i = 0; i < 3; i++ )
		VectorNormalize( interpolated + i*4 );
}


/*
=============
R_CalcTransform
=============
*/
float *R_CalcTransform( entity_t *e )
{
	vec3_t		angles;
	transform_t	*t;
	float		*mp;
	float		mt[16], mc[16];
	int			i;

	/* check if this entity is already transformed */
	t = &trafo[e - r_newrefdef.entities];

	if ( t->processing )
		ri.Sys_Error( ERR_DROP, "Ring in entity transformations!\n" );

	if ( t->done )
		return t->matrix;

	/* process this matrix */
	t->processing = qtrue;
	mp = NULL;

	/* do parent object trafos first */
	if ( e->tagent ) {
		/* parent trafo */
		mp = R_CalcTransform( e->tagent );

		/* tag trafo */
		if ( e->tagent->model && e->tagent->model->tagdata ) {
			dtag_t	*taghdr;
			char	*name;
			float	*tag;
			float	interpolated[16];

			taghdr = (dtag_t *)e->tagent->model->tagdata;

			/* find the right tag */
			name = (char *)taghdr + taghdr->ofs_names;
			for ( i = 0; i < taghdr->num_tags; i++, name += MAX_TAGNAME )
				if ( !strcmp( name, e->tagname ) ) {
					/* found the tag (matrix) */
					tag = (float *)((byte *)taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					/* do interpolation */
					R_InterpolateTransform( &e->tagent->as, taghdr->num_frames, tag, interpolated );

					/* transform */
					GLMatrixMultiply( mp, interpolated, mt );
					mp = mt;

					break;
				}
		}
	}

	/* fill in edge values */
	mc[3] = mc[7] = mc[11] = 0.0;
	mc[15] = 1.0;

	/* add rotation */
	VectorCopy( e->angles, angles );
/*	angles[YAW] = -angles[YAW]; */

	AngleVectors( angles, &mc[0], &mc[4], &mc[8] );

	/* add translation */
	mc[12] = e->origin[0];
	mc[13] = e->origin[1];
	mc[14] = e->origin[2];

	/* flip an axis */
	VectorScale( &mc[4], -1, &mc[4] );

	/* combine trafos */
	if ( mp )
		GLMatrixMultiply( mp, mc, t->matrix );
	else
		memcpy( t->matrix, mc, sizeof(float)*16 );

	/* we're done */
	t->done = qtrue;
	t->processing = qfalse;

	return t->matrix;
}


/*
=============
R_TransformEntitiesOnList
=============
*/
void R_TransformEntitiesOnList (void)
{
	int			i;

	if (!r_drawentities->value)
		return;

	/* clear flags */
	for (i=0 ; i<r_newrefdef.num_entities ; i++) {
		trafo[i].done = qfalse;
		trafo[i].processing = qfalse;
	}

	/* calculate all transformations */
	/* possibly recursive */
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
		R_CalcTransform( &r_newrefdef.entities[i] );
}


/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->value)
		return;

	/* draw non-transparent first */

	for (i=0 ; i<r_newrefdef.num_entities ; i++) {
		currententity = &r_newrefdef.entities[i];

		/* find out if and how an entity should be drawn */
		if ( currententity->flags & RF_TRANSLUCENT )
			continue;	/* solid */

		if ( currententity->flags & RF_BEAM )
			R_DrawBeam( currententity );
		else if ( currententity->flags & RF_BOX )
			R_DrawBox( currententity );
		else {
			currentmodel = currententity->model;
			if (!currentmodel) {
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type) {
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				ri.Sys_Error (ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}

	/* draw transparent entities */
	/* we could sort these if it ever becomes a problem... */
	qglDepthMask (0);		/* no z writes */
	for (i=0 ; i<r_newrefdef.num_entities ; i++) {
		currententity = &r_newrefdef.entities[i];
		if ( !(currententity->flags & RF_TRANSLUCENT) )
			continue;	/* solid */

		if ( currententity->flags & RF_BEAM )
			R_DrawBeam( currententity );
		else if ( currententity->flags & RF_BOX )
			R_DrawBox( currententity );
		else {
			currentmodel = currententity->model;

			if (!currentmodel) {
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type) {
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				ri.Sys_Error (ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}
	qglDepthMask (1);		/* back to writing */

}

/*
** GL_DrawParticles
**
*/
void GL_DrawParticles( int num_particles, const particle_t particles[], const unsigned colortable[768] )
{
	const particle_t *p;
	int				i;
	vec3_t			up, right;
	float			scale;
	byte			color[4];

	GL_Bind(r_particletexture->texnum);
	qglDepthMask( GL_FALSE );		/* no z buffering */
	qglEnable( GL_BLEND );
	GL_TexEnv( GL_MODULATE );
	qglBegin( GL_TRIANGLES );

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	for ( p = particles, i=0 ; i < num_particles ; i++,p++) {
		/* hack a scale up to keep particles from disapearing */
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] +
			    ( p->origin[1] - r_origin[1] ) * vpn[1] +
			    ( p->origin[2] - r_origin[2] ) * vpn[2];

		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;

		*(int *)color = colortable[p->color];
		color[3] = p->alpha*255;

		qglColor4ubv( color );

		qglTexCoord2f( 0.0625, 0.0625 );
		qglVertex3fv( p->origin );

		qglTexCoord2f( 1.0625, 0.0625 );
		qglVertex3f( p->origin[0] + up[0]*scale,
			         p->origin[1] + up[1]*scale,
					 p->origin[2] + up[2]*scale);

		qglTexCoord2f( 0.0625, 1.0625 );
		qglVertex3f( p->origin[0] + right[0]*scale,
			         p->origin[1] + right[1]*scale,
					 p->origin[2] + right[2]*scale);
	}

	qglEnd ();
	qglDisable( GL_BLEND );
	qglColor4f( 1,1,1,1 );
	qglDepthMask( 1 );		/* back to normal Z buffering */
	GL_TexEnv( GL_REPLACE );
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if ( gl_ext_pointparameters->value && qglPointParameterfEXT ) {
		int i;
		unsigned char color[4];
		const particle_t *p;

		qglDepthMask( GL_FALSE );
		qglEnable( GL_BLEND );
		qglDisable( GL_TEXTURE_2D );

		qglPointSize( gl_particle_size->value );

		qglBegin( GL_POINTS );
		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ ) {
			*(int *)color = d_8to24table[p->color];
			color[3] = p->alpha*255;

			qglColor4ubv( color );

			qglVertex3fv( p->origin );
		}
		qglEnd();

		qglDisable( GL_BLEND );
		qglColor4f( 1.0F, 1.0F, 1.0F, 1.0F );
		qglDepthMask( GL_TRUE );
		qglEnable( GL_TEXTURE_2D );

	} else
		GL_DrawParticles( r_newrefdef.num_particles, r_newrefdef.particles, d_8to24table );
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!gl_polyblend->value)
		return;
	if (!v_blend[3])
		return;

	qglDisable (GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

        qglLoadIdentity ();

	/* FIXME: get rid of these */
        qglRotatef (-90,  1, 0, 0);	    /* put Z going up */
        qglRotatef (90,  0, 0, 1);	    /* put Z going up */

	qglColor4fv (v_blend);

	qglBegin (GL_QUADS);

	qglVertex3f (10, 100, 100);
	qglVertex3f (10, -100, 100);
	qglVertex3f (10, -100, -100);
	qglVertex3f (10, 100, -100);
	qglEnd ();

	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_ALPHA_TEST);

	qglColor4f(1,1,1,1);
}

/*======================================================================= */

int SignbitsForPlane (cplane_t *out)
{
	int	bits, j;

	/* for fast box on planeside test */

	bits = 0;
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if ( r_isometric->value ) {
		/* 4 planes of a cube */
		VectorScale( vright, +1, frustum[0].normal );
		VectorScale( vright, -1, frustum[1].normal );
		VectorScale( vup, +1, frustum[2].normal );
		VectorScale( vup, -1, frustum[3].normal );

		for (i=0 ; i<4 ; i++) {
			frustum[i].type = PLANE_ANYZ;
			frustum[i].dist = -10 * r_newrefdef.fov_x;
			frustum[i].signbits = SignbitsForPlane (&frustum[i]);
		}
	} else {
		/* rotate VPN right by FOV_X/2 degrees */
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_newrefdef.fov_x / 2 ) );
		/* rotate VPN left by FOV_X/2 degrees */
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_newrefdef.fov_x / 2 );
		/* rotate VPN up by FOV_X/2 degrees */
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_newrefdef.fov_y / 2 );
		/* rotate VPN down by FOV_X/2 degrees */
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_newrefdef.fov_y / 2 ) );

		for (i=0 ; i<4 ; i++) {
			frustum[i].type = PLANE_ANYZ;
			frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
			frustum[i].signbits = SignbitsForPlane (&frustum[i]);
		}
	}
}

/*======================================================================= */

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int i;
/* 	mleaf_t	*leaf; */

	r_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy (r_newrefdef.vieworg, r_origin);

	AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);

	/* current viewcluster */
#if 0
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) ) {
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		/* check above and below so crossing solid water doesn't draw wrong */
		if (!leaf->contents) {	/* look down a bit */
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		} else {	/* look up a bit */
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}
#endif
	for (i=0 ; i<4 ; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) {
		qglEnable( GL_SCISSOR_TEST );
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		qglDisable( GL_SCISSOR_TEST );
	}
}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	xmin += -( 2 * gl_state.camera_separation ) / zNear;
	xmax += -( 2 * gl_state.camera_separation ) / zNear;

	if ( !r_isometric->value )
	        qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
	else
		qglOrtho( -10*fovy*aspect, 10*fovy*aspect, -10*fovy, 10*fovy, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
/*	float	yfov; */
	int		x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(r_newrefdef.x * vid.width / vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

/*	ri.Con_Printf( "%i %i", vid.width, vid.height ); */

	w = x2 - x;
	h = y - y2;

	qglViewport (x, y2, w, h);

	/* set up projection matrix */
        screenaspect = (float)r_newrefdef.width/r_newrefdef.height;
/*	yfov = 2*atan((float)r_newrefdef.height/r_newrefdef.width)*180/M_PI; */
	qglMatrixMode(GL_PROJECTION);
        qglLoadIdentity ();
        MYgluPerspective (r_newrefdef.fov_y,  screenaspect,  4,  2048);

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
        qglLoadIdentity ();

        qglRotatef (-90,  1, 0, 0);	    /* put Z going up */
        qglRotatef (90,  0, 0, 1);	    /* put Z going up */
        qglRotatef (-r_newrefdef.viewangles[2],  1, 0, 0);
        qglRotatef (-r_newrefdef.viewangles[0],  0, 1, 0);
        qglRotatef (-r_newrefdef.viewangles[1],  0, 0, 1);
        qglTranslatef (-r_newrefdef.vieworg[0],  -r_newrefdef.vieworg[1],  -r_newrefdef.vieworg[2]);

	qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	/* set drawing parms */
	if (gl_cull->value)
		qglEnable(GL_CULL_FACE);
	else
		qglDisable(GL_CULL_FACE);

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);

	if ( gl_fog->value && r_newrefdef.fog && gl_state.fog_coord ) {
		qglEnable( GL_FOG );
		qglFogi( GL_FOG_MODE, GL_LINEAR );
		qglFogf( GL_FOG_START, 0.1 * r_newrefdef.fog );
		qglFogf( GL_FOG_END, r_newrefdef.fog );
/*		qglFogDensity( GL_FOG_DENSITY, 0.01 * r_newrefdef.fog ); */
		qglFogfv( GL_FOG_COLOR, r_newrefdef.fogColor );
	}
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (gl_ztrick->value) {
		static int trickframe;

		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1) {
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
		} else {
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
		}
	} else {
		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);
}

void R_Flash( void )
{
/*         R_ShadowBlend(); */
	R_PolyBlend ();
}


/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView (refdef_t *fd)
{
	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

/*	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) ) */
/*		ri.Sys_Error (ERR_DROP, "R_RenderView: NULL worldmodel"); */

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

/*	R_PushDlights (); */

	if (gl_finish->value)
		qglFinish ();

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	if (gl_wire->value)
		qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);

	R_DrawLevelBrushes ();
	R_DrawTriangleOutlines ();

	R_TransformEntitiesOnList();
	R_DrawEntitiesOnList();

/* 	if (gl_shadows->value == 2) R_CastShadow(); */

	if (gl_wire->value)
		qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	R_DrawAlphaSurfaces ();

/* 	if (gl_shadows->value == 1) R_CastShadow(); */

	R_DrawParticles ();
	R_DrawPtls ();

	R_RenderDlights ();

	R_Flash();

	if (r_speeds->value) {
		ri.Con_Printf (PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
			c_brush_polys,
			c_alias_polys,
			c_visible_textures,
			c_visible_lightmaps);
	}
}


void	R_LeaveGL2D (void)
{
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();

	qglPopAttrib();
}

void	R_SetGL2D (void)
{
	/* set 2D virtual screen size */
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho  (0, vid.width, vid.height, 0, 9999, -9999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
	qglDisable (GL_FOG);
	qglEnable (GL_ALPHA_TEST);
	GL_TexEnv( GL_MODULATE );
	qglColor4f (1,1,1,1);
}

/*
====================
R_RenderFrame
====================
*/
void R_RenderFrame (refdef_t *fd)
{
	R_RenderView( fd );
	R_SetGL2D ();
}

static cmdList_t r_commands[] = {
	{"imagelist", GL_ImageList_f},
	{"fontcachelist", Font_ListCache_f},
	{"screenshot", GL_ScreenShot_f},
	{"modellist", Mod_Modellist_f},
	{"gl_strings", GL_Strings_f},

	{NULL, NULL}
};

void R_Register( void )
{
	cmdList_t* commands;

	r_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0);
	r_novis = ri.Cvar_Get ("r_novis", "0", 0);
	r_nocull = ri.Cvar_Get ("r_nocull", "0", 0);
	r_isometric = ri.Cvar_Get ("r_isometric", "0", 0);
	r_lerpmodels = ri.Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = ri.Cvar_Get ("r_speeds", "0", 0);
	r_displayrefresh = ri.Cvar_Get( "r_displayrefresh", "0", CVAR_ARCHIVE );
	r_anisotropic = ri.Cvar_Get( "r_anisotropic", "1", CVAR_ARCHIVE );
	r_ext_max_anisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "0", 0 );
	r_texture_lod = ri.Cvar_Get( "r_texture_lod", "0", CVAR_ARCHIVE );
/* 	r_arb_samples = ri.Cvar_Get ("r_arb_samples", "1", CVAR_ARCHIVE ); */

/*	r_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", 0); */

	gl_nosubimage = ri.Cvar_Get( "gl_nosubimage", "0", 0 );
	gl_allow_software = ri.Cvar_Get( "gl_allow_software", "0", 0 );

	gl_particle_min_size = ri.Cvar_Get( "gl_particle_min_size", "2", CVAR_ARCHIVE );
	gl_particle_max_size = ri.Cvar_Get( "gl_particle_max_size", "40", CVAR_ARCHIVE );
	gl_particle_size = ri.Cvar_Get( "gl_particle_size", "40", CVAR_ARCHIVE );
	gl_particle_att_a = ri.Cvar_Get( "gl_particle_att_a", "0.01", CVAR_ARCHIVE );
	gl_particle_att_b = ri.Cvar_Get( "gl_particle_att_b", "0.0", CVAR_ARCHIVE );
	gl_particle_att_c = ri.Cvar_Get( "gl_particle_att_c", "0.01", CVAR_ARCHIVE );

	gl_modulate = ri.Cvar_Get ("gl_modulate", "1", CVAR_ARCHIVE );
	gl_log = ri.Cvar_Get( "gl_log", "0", 0 );
	gl_bitdepth = ri.Cvar_Get( "gl_bitdepth", "0", CVAR_ARCHIVE );
	gl_mode = ri.Cvar_Get( "gl_mode", "3", CVAR_ARCHIVE );
	gl_lightmap = ri.Cvar_Get ("gl_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get ("gl_shadows", "1", CVAR_ARCHIVE );
	gl_shadow_debug_volume = ri.Cvar_Get( "r_shadow_debug_volume", "0", CVAR_ARCHIVE );
	gl_shadow_debug_shade = ri.Cvar_Get( "r_shadow_debug_shade", "0", CVAR_ARCHIVE );
	r_ati_separate_stencil = ri.Cvar_Get( "r_ati_separate_stencil", "1", CVAR_ARCHIVE );
	r_stencil_two_side = ri.Cvar_Get( "r_stencil_two_side", "1", CVAR_ARCHIVE );
	gl_embossfilter = ri.Cvar_Get("gl_embossfilter", "0", CVAR_ARCHIVE);
	gl_dynamic = ri.Cvar_Get ("gl_dynamic", "1", 0);
	gl_nobind = ri.Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = ri.Cvar_Get ("gl_round_down", "1", 0);
	gl_picmip = ri.Cvar_Get ("gl_picmip", "0", 0);
	gl_maxtexres = ri.Cvar_Get ("gl_maxtexres", "2048", CVAR_ARCHIVE);
	gl_showtris = ri.Cvar_Get ("gl_showtris", "0", 0);
	gl_ztrick = ri.Cvar_Get ("gl_ztrick", "0", 0);
	gl_finish = ri.Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = ri.Cvar_Get ("gl_clear", "1", 0);
	gl_cull = ri.Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = ri.Cvar_Get ("gl_polyblend", "1", 0);
	gl_flashblend = ri.Cvar_Get ("gl_flashblend", "0", 0);
	gl_playermip = ri.Cvar_Get ("gl_playermip", "0", 0);
	gl_monolightmap = ri.Cvar_Get( "gl_monolightmap", "0", 0 );
#if defined(_WIN32)
	gl_driver = ri.Cvar_Get( "gl_driver", "opengl32", CVAR_ARCHIVE );
#elif defined(MACOS_X)
	gl_driver = ri.Cvar_Get( "gl_driver", "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib", CVAR_ARCHIVE );
#else
	gl_driver = ri.Cvar_Get( "gl_driver", "libGL.so.1", CVAR_ARCHIVE );
#endif
	gl_texturemode = ri.Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	gl_texturealphamode = ri.Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE );
	gl_texturesolidmode = ri.Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE );
	gl_lockpvs = ri.Cvar_Get( "gl_lockpvs", "0", 0 );
	gl_wire = ri.Cvar_Get ("gl_wire", "0", 0);
	gl_fog = ri.Cvar_Get ("gl_fog", "1", CVAR_ARCHIVE );
	gl_vertex_arrays = ri.Cvar_Get( "gl_vertex_arrays", "0", CVAR_ARCHIVE );

	gl_ext_swapinterval = ri.Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE );
	gl_ext_palettedtexture = ri.Cvar_Get( "gl_ext_palettedtexture", "1", CVAR_ARCHIVE );
	gl_ext_multitexture = ri.Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE );
	gl_ext_combine = ri.Cvar_Get( "gl_ext_combine", "1", CVAR_ARCHIVE );
	gl_ext_pointparameters = ri.Cvar_Get( "gl_ext_pointparameters", "1", CVAR_ARCHIVE );
	gl_ext_compiled_vertex_array = ri.Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE );
	gl_ext_texture_compression = ri.Cvar_Get( "gl_ext_texture_compression", "0", CVAR_ARCHIVE );
	gl_ext_s3tc_compression = ri.Cvar_Get( "gl_ext_s3tc_compression", "1", CVAR_ARCHIVE );

	gl_drawbuffer = ri.Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = ri.Cvar_Get( "gl_swapinterval", "1", CVAR_ARCHIVE );

	gl_saturatelighting = ri.Cvar_Get( "gl_saturatelighting", "0", 0 );

	gl_3dlabs_broken = ri.Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE );

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = ri.Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );
#if defined(_WIN32)
	vid_ref = ri.Cvar_Get( "vid_ref", "gl", CVAR_ARCHIVE );
#elif defined(MACOS_X)
	/* FIXME: Don't know the macosx driver, yet */'
	vid_ref = ri.Cvar_Get( "vid_ref", "glx", CVAR_ARCHIVE );
#else
	vid_ref = ri.Cvar_Get( "vid_ref", "glx", CVAR_ARCHIVE );
#endif
	vid_grabmouse = ri.Cvar_Get( "vid_grabmouse", "1", CVAR_ARCHIVE );
	vid_grabmouse->modified = qfalse;

	for (commands = r_commands; commands->name; commands++)
		ri.Cmd_AddCommand(commands->name, commands->function);
}

/*
==================
R_SetMode
==================
*/
qboolean R_SetMode (void)
{
	rserr_t err;
	qboolean fullscreen;

/*	if ( vid_fullscreen->modified && !gl_config.allow_cds ) {
		ri.Con_Printf( PRINT_ALL, "R_SetMode() - CDS not allowed with this driver\n" );
		ri.Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->value );
		vid_fullscreen->modified = qfalse;
	}  */

	fullscreen = vid_fullscreen->value;

	vid_fullscreen->modified = qfalse;
	gl_mode->modified = qfalse;
	gl_ext_texture_compression->modified = qfalse;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, fullscreen ) ) == rserr_ok )
		gl_state.prev_mode = gl_mode->value;
	else {
		if ( err == rserr_invalid_fullscreen ) {
			ri.Cvar_SetValue( "vid_fullscreen", 0);
			vid_fullscreen->modified = qfalse;
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, qfalse ) ) == rserr_ok )
				return qtrue;
		} else if ( err == rserr_invalid_mode ) {
			ri.Cvar_SetValue( "gl_mode", gl_state.prev_mode );
			gl_mode->modified = qfalse;
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - invalid mode\n" );
		}

		/* try setting it back to something safe */
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, qfalse ) ) != rserr_ok ) {
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return qfalse;
		}
	}
	return qtrue;
}

/*
===============
R_Init
===============
*/
qboolean R_Init( HINSTANCE hinstance, WNDPROC wndproc )
{
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	int		err;
	int		j;
	extern float r_turbsin[256];
	int aniso_level, max_aniso;

	for ( j = 0; j < 256; j++ )
		r_turbsin[j] *= 0.5;

	ri.Con_Printf (PRINT_ALL, "ref_gl version: "REF_VERSION"\n");

	Draw_GetPalette ();

	R_Register();

	/* initialize our QGL dynamic bindings */
	if ( !QGL_Init( gl_driver->string ) ) {
		QGL_Shutdown();
		ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string );
		return qfalse;
	}

	/* initialize OS-specific parts of OpenGL */
	if ( !GLimp_Init( hinstance, wndproc ) ) {
		QGL_Shutdown();
		return qfalse;
	}

	/* set our "safe" modes */
	gl_state.prev_mode = 3;

	/* create the window and set up the context */
	if ( !R_SetMode () ) {
		QGL_Shutdown();
		ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - could not R_SetMode()\n" );
		return qfalse;
	}

	/* get our various GL strings */
	gl_config.vendor_string = (char *)qglGetString (GL_VENDOR);
	ri.Con_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string );
	gl_config.renderer_string = (char *)qglGetString (GL_RENDERER);
	ri.Con_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string );
	gl_config.version_string = (char *)qglGetString (GL_VERSION);
	ri.Con_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string );
	gl_config.extensions_string = (char *)qglGetString (GL_EXTENSIONS);
	ri.Con_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string );

	Q_strncpyz( renderer_buffer, gl_config.renderer_string, sizeof(renderer_buffer) );
	renderer_buffer[sizeof(renderer_buffer)-1] = 0;
	Q_strlwr( renderer_buffer );

	Q_strncpyz( vendor_buffer, gl_config.vendor_string, sizeof(vendor_buffer) );
	vendor_buffer[sizeof(vendor_buffer)-1] = 0;
	Q_strlwr( vendor_buffer );

	if ( strstr( renderer_buffer, "voodoo" ) ) {
		if ( !strstr( renderer_buffer, "rush" ) )
			gl_config.renderer = GL_RENDERER_VOODOO;
		else
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH;
	} else if ( strstr( vendor_buffer, "sgi" ) )
		gl_config.renderer = GL_RENDERER_SGI;
	else if ( strstr( renderer_buffer, "permedia" ) )
		gl_config.renderer = GL_RENDERER_PERMEDIA2;
	else if ( strstr( renderer_buffer, "glint" ) )
		gl_config.renderer = GL_RENDERER_GLINT_MX;
	else if ( strstr( renderer_buffer, "glzicd" ) )
		gl_config.renderer = GL_RENDERER_REALIZM;
	else if ( strstr( renderer_buffer, "gdi" ) )
		gl_config.renderer = GL_RENDERER_MCD;
	else if ( strstr( renderer_buffer, "pcx2" ) )
		gl_config.renderer = GL_RENDERER_PCX2;
	else if ( strstr( renderer_buffer, "verite" ) )
		gl_config.renderer = GL_RENDERER_RENDITION;
	else
		gl_config.renderer = GL_RENDERER_OTHER;

	if ( toupper( gl_monolightmap->string[1] ) != 'F' ) {
		if ( gl_config.renderer == GL_RENDERER_PERMEDIA2 ) {
			ri.Cvar_Set( "gl_monolightmap", "A" );
			ri.Con_Printf( PRINT_ALL, "...using gl_monolightmap 'a'\n" );
		}
		else if ( gl_config.renderer & GL_RENDERER_POWERVR )
			ri.Cvar_Set( "gl_monolightmap", "0" );
		else
			ri.Cvar_Set( "gl_monolightmap", "0" );
	}

	/* power vr can't have anything stay in the framebuffer, so */
	/* the screen needs to redraw the tiled background every frame */
	if ( gl_config.renderer & GL_RENDERER_POWERVR )
		ri.Cvar_Set( "scr_drawall", "1" );
	else
		ri.Cvar_Set( "scr_drawall", "0" );

#ifdef __linux__
	ri.Cvar_SetValue( "gl_finish", 1 );
#endif

	/* MCD has buffering issues */
	if ( gl_config.renderer == GL_RENDERER_MCD )
		ri.Cvar_SetValue( "gl_finish", 1 );

	if ( gl_config.renderer & GL_RENDERER_3DLABS ) {
		if ( gl_3dlabs_broken->value )
			gl_config.allow_cds = qfalse;
		else
			gl_config.allow_cds = qtrue;
	} else
		gl_config.allow_cds = qtrue;

	if ( gl_config.allow_cds )
		ri.Con_Printf( PRINT_ALL, "...allowing CDS\n" );
	else
		ri.Con_Printf( PRINT_ALL, "...disabling CDS\n" );

	/*
	** grab extensions
	*/
	if ( strstr( gl_config.extensions_string, "GL_EXT_compiled_vertex_array" ) ||
		 strstr( gl_config.extensions_string, "GL_SGI_compiled_vertex_array" ) ) {
		ri.Con_Printf( PRINT_ALL, "...enabling GL_EXT_compiled_vertex_array\n" );
		qglLockArraysEXT = ( void ( APIENTRY *)( int, int ) ) qwglGetProcAddress( "glLockArraysEXT" );
		qglUnlockArraysEXT = ( void ( APIENTRY *) ( void ) ) qwglGetProcAddress( "glUnlockArraysEXT" );
	} else
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );

#ifdef _WIN32
	if ( strstr( gl_config.extensions_string, "WGL_EXT_swap_control" ) ) {
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
		ri.Con_Printf( PRINT_ALL, "...enabling WGL_EXT_swap_control\n" );
	} else
		ri.Con_Printf( PRINT_ALL, "...WGL_EXT_swap_control not found\n" );
#endif

	if ( strstr( gl_config.extensions_string, "GL_EXT_point_parameters" ) ) {
		if ( gl_ext_pointparameters->value ) {
			qglPointParameterfEXT = ( void (APIENTRY *)( GLenum, GLfloat ) ) qwglGetProcAddress( "glPointParameterfEXT" );
			qglPointParameterfvEXT = ( void (APIENTRY *)( GLenum, const GLfloat * ) ) qwglGetProcAddress( "glPointParameterfvEXT" );
			ri.Con_Printf( PRINT_ALL, "...using GL_EXT_point_parameters\n" );
		} else
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_EXT_point_parameters\n" );
	} else
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_point_parameters not found\n" );

	if ( strstr( gl_config.extensions_string, "GL_ARB_multitexture" ) ) {
		if ( gl_ext_multitexture->value ) {
			ri.Con_Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
			qglMTexCoord2fSGIS = ( void ( APIENTRY *)( GLenum, GLfloat, GLfloat ) ) qwglGetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( void ( APIENTRY *) ( GLenum ) ) qwglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void ( APIENTRY *) ( GLenum ) ) qwglGetProcAddress( "glClientActiveTextureARB" );
			gl_texture0 = GL_TEXTURE0_ARB;
			gl_texture1 = GL_TEXTURE1_ARB;
			gl_texture2 = GL_TEXTURE2_ARB;
			gl_texture3 = GL_TEXTURE3_ARB;
		} else
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
	} else
		ri.Con_Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );

	if ( strstr( gl_config.extensions_string, "GL_EXT_texture_env_combine" ) ||
		strstr( gl_config.extensions_string, "GL_ARB_texture_env_combine" ) ) {
		if ( gl_ext_combine->value ) {
			ri.Con_Printf( PRINT_ALL, "...using GL_EXT_texture_env_combine\n" );
			gl_combine = GL_COMBINE_EXT;
		} else {
			ri.Con_Printf( PRINT_ALL, "...ignoring EXT_texture_env_combine\n" );
			gl_combine = 0;
		}
	} else {
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_texture_env_combine not found\n" );
		gl_combine = 0;
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_multitexture" ) ) {
		if ( qglActiveTextureARB )
			ri.Con_Printf( PRINT_ALL, "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n" );
		else if ( gl_ext_multitexture->value ) {
			ri.Con_Printf( PRINT_ALL, "...using GL_SGIS_multitexture\n" );
			qglMTexCoord2fSGIS = ( void ( APIENTRY *)( GLenum, GLfloat, GLfloat ) ) qwglGetProcAddress( "glMTexCoord2fSGIS" );
			qglSelectTextureSGIS = ( void ( APIENTRY *)( GLenum ) ) qwglGetProcAddress( "glSelectTextureSGIS" );
			gl_texture0 = GL_TEXTURE0_SGIS;
			gl_texture1 = GL_TEXTURE1_SGIS;
			gl_texture2 = GL_TEXTURE2_SGIS;
			gl_texture3 = GL_TEXTURE3_SGIS;
		} else
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_SGIS_multitexture\n" );
	} else
		ri.Con_Printf( PRINT_ALL, "...GL_SGIS_multitexture not found\n" );

	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_compression" ) ) {
		if ( gl_ext_texture_compression->value ) {
			ri.Con_Printf( PRINT_ALL, "...using GL_ARB_texture_compression\n" );
			if ( gl_ext_s3tc_compression->value && strstr( gl_config.extensions_string, "GL_EXT_texture_compression_s3tc" ) ) {
/*				ri.Con_Printf( PRINT_ALL, "   with s3tc compression\n" ); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			} else {
/*				ri.Con_Printf( PRINT_ALL, "   without s3tc compression\n" ); */
				gl_compressed_solid_format = GL_COMPRESSED_RGB_ARB;
				gl_compressed_alpha_format = GL_COMPRESSED_RGBA_ARB;
			}
		} else {
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_ARB_texture_compression\n" );
			gl_compressed_solid_format = 0;
			gl_compressed_alpha_format = 0;
		}
	} else {
		ri.Con_Printf( PRINT_ALL, "...GL_ARB_texture_compression not found\n" );
		gl_compressed_solid_format = 0;
		gl_compressed_alpha_format = 0;
	}

	/* anisotropy */
	gl_state.anisotropic = qfalse;

	qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&max_aniso);
	ri.Cvar_SetValue("r_ext_max_anisotropy", max_aniso);
	if (r_anisotropic->value > r_ext_max_anisotropy->value) {
		ri.Con_Printf(PRINT_ALL, "...max GL_EXT_texture_filter_anisotropic value is %i\n", max_aniso );
		ri.Cvar_SetValue("r_anisotropic", r_ext_max_anisotropy->value);
	}

	aniso_level = r_anisotropic->value;
	if ( strstr( gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic" ) ) {
		if(r_anisotropic->value == 0)
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n");
		else {
			ri.Con_Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic [%2i max] [%2i selected]\n", max_aniso, aniso_level );
			gl_state.anisotropic = qtrue;
		}
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n");
		ri.Cvar_Set("r_anisotropic", "0");
		ri.Cvar_Set("r_ext_max_anisotropy", "0");
	}

	if ( strstr( gl_config.extensions_string, "GL_EXT_texture_lod_bias" ) ) {
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_texture_lod_bias\n");
		gl_state.lod_bias=qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_texture_lod_bias not found\n");
		gl_state.lod_bias=qfalse;
	}

#if 0
	if ( strstr( gl_config.extensions_string, "GL_SGIS_generate_mipmap" ) ) {
		ri.Con_Printf(PRINT_ALL, "...using GL_SGIS_generate_mipmap\n");
		gl_state.sgis_mipmap=qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_SGIS_generate_mipmap not found\n");
		ri.Sys_Error (ERR_FATAL, "GL_SGIS_generate_mipmap not found!");
		gl_state.sgis_mipmap=qfalse;
	}
#endif

	if ( strstr( gl_config.extensions_string, "GL_EXT_stencil_wrap" ) ) {
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_stencil_wrap\n");
		gl_state.stencil_warp=qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_stencil_wrap not found\n");
		gl_state.stencil_warp=qfalse;
	}

	if ( strstr( gl_config.extensions_string, "GL_EXT_fog_coord" ) ) {
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_fog_coord\n");
		gl_state.fog_coord=qtrue;
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_fog_coord not found\n");
		gl_state.fog_coord=qfalse;
	}

	gl_state.arb_fragment_program = qfalse;
#ifdef SHADERS
	if ( strstr( gl_config.extensions_string, "GL_ARB_fragment_program" ) ) {
		ri.Con_Printf(PRINT_ALL, "...using GL_ARB_fragment_program\n");
		gl_state.arb_fragment_program = qtrue;

		qglProgramStringARB = (void*)qwglGetProcAddress("glProgramStringARB");
		qglBindProgramARB = (void*)qwglGetProcAddress("glBindProgramARB");
		qglDeleteProgramsARB = (void*)qwglGetProcAddress("glDeleteProgramsARB");
		qglGenProgramsARB = (void*)qwglGetProcAddress("glGenProgramsARB");
		qglProgramEnvParameter4dARB = (void*)qwglGetProcAddress("glProgramEnvParameter4dARB");
		qglProgramEnvParameter4dvARB = (void*)qwglGetProcAddress("glProgramEnvParameter4dvARB");
		qglProgramEnvParameter4fARB = (void*)qwglGetProcAddress("glProgramEnvParameter4fARB");
		qglProgramEnvParameter4fvARB = (void*)qwglGetProcAddress("glProgramEnvParameter4fvARB");
		qglProgramLocalParameter4dARB = (void*)qwglGetProcAddress("glProgramLocalParameter4dARB");
		qglProgramLocalParameter4dvARB = (void*)qwglGetProcAddress("glProgramLocalParameter4dvARB");
		qglProgramLocalParameter4fARB = (void*)qwglGetProcAddress("glProgramLocalParameter4fARB");
		qglProgramLocalParameter4fvARB = (void*)qwglGetProcAddress("glProgramLocalParameter4fvARB");
		qglGetProgramEnvParameterdvARB = (void*)qwglGetProcAddress("glGetProgramEnvParameterdvARB");
		qglGetProgramEnvParameterfvARB = (void*)qwglGetProcAddress("glGetProgramEnvParameterfvARB");
		qglGetProgramLocalParameterdvARB = (void*)qwglGetProcAddress("glGetProgramLocalParameterdvARB");
		qglGetProgramLocalParameterfvARB = (void*)qwglGetProcAddress("glGetProgramLocalParameterfvARB");
		qglGetProgramivARB = (void*)qwglGetProcAddress("glGetProgramivARB");
		qglGetProgramStringARB = (void*)qwglGetProcAddress("glGetProgramStringARB");
		qglIsProgramARB = (void*)qwglGetProcAddress("glIsProgramARB");

		GL_ShaderInit();

/*		water_shader = LoadProgram_ARB_FP("arb_water"); */
/*		water_shader = CompileWaterShader(); */
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_ARB_fragment_program not found\n");
		gl_state.arb_fragment_program = qfalse;
	}
#endif /* SHADERS */

	gl_state.ati_separate_stencil=qfalse;
	if ( strstr( gl_config.extensions_string, "GL_ATI_separate_stencil" ) ) {
		if (!r_ati_separate_stencil->value) {
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_ATI_separate_stencil\n");
			gl_state.ati_separate_stencil=qfalse;
		} else {
			ri.Con_Printf(PRINT_ALL, "...using GL_ATI_separate_stencil\n");
			gl_state.ati_separate_stencil=qtrue;
			qglStencilOpSeparateATI = ( void ( APIENTRY *)(GLenum, GLenum, GLenum, GLenum) ) qwglGetProcAddress("glStencilOpSeparateATI");
			qglStencilFuncSeparateATI = ( void ( APIENTRY *)(GLenum, GLenum, GLint, GLuint) ) qwglGetProcAddress("glStencilFuncSeparateATI");
		}
	} else {
		ri.Con_Printf(PRINT_ALL, "...GL_ATI_separate_stencil not found\n");
		gl_state.ati_separate_stencil=qfalse;
		ri.Cvar_Set("r_ati_separate_stencil", "0");
	}

	{
		int size;
		GLenum err;

		ri.Con_Printf( PRINT_ALL, "Max texture size supported\n" );
		qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
		if (( err = qglGetError() )) {
			ri.Con_Printf( PRINT_ALL, "...cannot detect !\n");
		} else {
			ri.Con_Printf( PRINT_ALL, "...detected %d\n", size);
			if (gl_maxtexres->value > size) {
				ri.Con_Printf( PRINT_ALL, "...downgrading from %.0f\n", gl_maxtexres->value);
				ri.Cvar_SetValue("gl_maxtexres", size);
			} else if (gl_maxtexres->value < size) {
				ri.Con_Printf( PRINT_ALL, "...but using %.0f as requested\n",
					       gl_maxtexres->value);
			}
		}
	}

	GL_SetDefaultState();

	GL_InitImages ();
	Mod_Init ();
	R_InitParticleTexture ();
	Draw_InitLocal ();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ri.Con_Printf (PRINT_ALL, "glGetError() = 0x%x\n", err);

	return qfalse;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{
	cmdList_t *commands;

	for (commands = r_commands; commands->name; commands++)
		ri.Cmd_RemoveCommand(commands->name);

	Mod_FreeAll ();

	GL_ShutdownImages ();
#ifdef SHADERS
	GL_ShutdownShaders ();
#endif
	Font_Shutdown();

	/*
	** shut down OS specific OpenGL stuff like contexts, etc.
	*/
	GLimp_Shutdown();

	/*
	** shutdown our QGL subsystem
	*/
	QGL_Shutdown();
}



/*
====================
R_BeginFrame
====================
*/
void R_BeginFrame( float camera_separation )
{

	gl_state.camera_separation = camera_separation;

	/* change modes if necessary */
	/* FIXME: only restart if CDS is required */
	if ( gl_mode->modified || vid_fullscreen->modified || gl_ext_texture_compression->modified ) {
		cvar_t	*ref;

		ref = ri.Cvar_Get ("vid_ref", "gl", 0);
		ref->modified = qtrue;
	}

	if ( gl_log->modified ) {
		GLimp_EnableLogging( gl_log->value );
		gl_log->modified = qfalse;
	}

	if ( r_anisotropic->modified ) {
		if (r_anisotropic->value > r_ext_max_anisotropy->value) {
			ri.Con_Printf(PRINT_ALL, "...max GL_EXT_texture_filter_anisotropic value is %i\n", r_ext_max_anisotropy->value );
			ri.Cvar_SetValue("r_anisotropic", r_ext_max_anisotropy->value);
		}
		r_anisotropic->modified = qfalse;
	}

	if ( gl_log->value )
		GLimp_LogNewFrame();

	if ( vid_gamma->modified ) {
		vid_gamma->modified = qfalse;
#ifndef _WIN32
		if ( gl_state.hwgamma )
			GLimp_SetGamma( NULL, NULL, NULL );
		else
#endif /* _WIN32 */
		/* update 3Dfx gamma -- it is expected that a user will do a vid_restart
		 after tweaking this value */
		if ( gl_config.renderer & ( GL_RENDERER_VOODOO ) ) {
			char envbuffer[1024];
			float g;

			g = 2.00 * ( 0.8 - ( vid_gamma->value - 0.5 ) ) + 1.0F;
			Com_sprintf( envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%f", g );
			Q_putenv( envbuffer );
			Com_sprintf( envbuffer, sizeof(envbuffer), "SST_GAMMA=%f", g );
			Q_putenv( envbuffer );
		}
	}

	GLimp_BeginFrame( camera_separation );

	/* go into 2D mode */
	R_SetGL2D();

	/* draw buffer stuff */
	if ( gl_drawbuffer->modified ) {
		gl_drawbuffer->modified = qfalse;

		if ( gl_state.camera_separation == 0 || !gl_state.stereo_enabled ) {
			if ( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	/* texturemode stuff */
	/* Realtime set level of anisotropy filtering and change texture lod bias */
	if ( gl_texturemode->modified ) {
		GL_TextureMode( gl_texturemode->string );
		gl_texturemode->modified = qfalse;
	}

	if ( gl_texturealphamode->modified ) {
		GL_TextureAlphaMode( gl_texturealphamode->string );
		gl_texturealphamode->modified = qfalse;
	}

	if ( gl_texturesolidmode->modified ) {
		GL_TextureSolidMode( gl_texturesolidmode->string );
		gl_texturesolidmode->modified = qfalse;
	}

	/* swapinterval stuff */
	GL_UpdateSwapInterval();

	/* clear screen if desired */
	R_Clear ();
}

/*
=============
R_SetPalette
=============
*/
unsigned r_rawpalette[256];

void R_SetPalette ( const unsigned char *palette)
{
	int		i;

	byte *rp = ( byte * ) r_rawpalette;

	if ( palette ) {
		for ( i = 0; i < 256; i++ ) {
			rp[i*4+0] = palette[i*3+0];
			rp[i*4+1] = palette[i*3+1];
			rp[i*4+2] = palette[i*3+2];
			rp[i*4+3] = 0xff;
		}
	} else {
		for ( i = 0; i < 256; i++ ) {
			rp[i*4+0] = LittleLong(d_8to24table[i]) & 0xff;
			rp[i*4+1] = LittleLong(( d_8to24table[i]) >> 8 ) & 0xff;
			rp[i*4+2] = LittleLong(( d_8to24table[i]) >> 16 ) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}

	qglClear (GL_COLOR_BUFFER_BIT);
}

/*
==================
RB_TakeVideoFrameCmd
==================
*/
void R_TakeVideoFrame( int w, int h, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg  )
{
	int	frameSize;
	int	i;

	qglReadPixels( 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, captureBuffer );

	if( motionJpeg ) {
		frameSize = SaveJPGToBuffer( encodeBuffer, 95, w, h, captureBuffer );
	} else {
		frameSize = w * h * 4;

		/* Vertically flip the image */
		for( i = 0; i < h; i++ )
			memcpy( &encodeBuffer[ i * ( w * 4 ) ], &captureBuffer[ ( h - i - 1 ) * ( w * 4 ) ], w * 4 );
	}

	ri.CL_WriteAVIVideoFrame( encodeBuffer, frameSize );
}


/*=================================================================== */


void	R_BeginRegistration (char *tiles, char *pos);
struct model_s	*R_RegisterModelShort (char *name);
struct image_s	*R_RegisterSkin (char *name);
void	R_EndRegistration (void);

void	R_RenderFrame (refdef_t *fd);

struct image_s	*Draw_FindPic (char *name);

void	Draw_Char (int x, int y, int c);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_FadeScreen (void);

void	LoadTGA( char *name, byte **pic, int *width, int *height );

/*
====================
GetRefAPI

====================
*/
refexport_t GetRefAPI (refimport_t rimp )
{
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModelShort;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.EndRegistration = R_EndRegistration;

	re.RenderFrame = R_RenderFrame;

	re.DrawModelDirect = R_DrawModelDirect;
	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawNormPic = Draw_NormPic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.FontDrawString = Font_DrawString;
	re.FontLength = Font_Length;
	re.FontRegister = Font_Register;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawColor = Draw_Color;
	re.DrawFadeScreen = Draw_FadeScreen;
	re.DrawDayAndNight = Draw_DayAndNight;
	re.DrawLineStrip = Draw_LineStrip;
	re.Draw3DGlobe = Draw_3DGlobe;
	re.Draw3DMapMarkers = Draw_3DMapMarkers;
	re.Draw3DMapLine = Draw_3DMapLine;

	re.AnimAppend = Anim_Append;
	re.AnimChange = Anim_Change;
	re.AnimRun = Anim_Run;
	re.AnimGetName = Anim_GetName;

	re.LoadTGA = LoadTGA;

	re.DrawStretchRaw = Draw_StretchRaw;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.BeginFrame = R_BeginFrame;
	re.EndFrame = GLimp_EndFrame;

	re.AppActivate = GLimp_AppActivate;
	re.TakeVideoFrame = R_TakeVideoFrame;
	Swap_Init ();

	return re;
}


#ifndef REF_HARD_LINKED
/* this is only here so the functions in q_shared.c and q_shwin.c can link */
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	ri.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	ri.Con_Printf (PRINT_ALL, "%s", text);
}

#endif
