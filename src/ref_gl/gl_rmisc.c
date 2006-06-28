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
/* r_misc.c */

#include "gl_local.h"

/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

byte	gridtexture[8][8] =
{
	{1,1,1,1,1,1,1,1},
	{1,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,1},
	{1,1,1,1,1,1,1,1},
};

void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8][4];

	/* particle texture */
	for (x=0 ; x<8 ; x++) {
		for (y=0 ; y<8 ; y++) {
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}
	r_particletexture = GL_LoadPic ("***particle***", (byte *)data, 8, 8, it_sprite, 32);

	/* also use this for bad textures, but without alpha */
	for (x=0 ; x<8 ; x++) {
		for (y=0 ; y<8 ; y++) {
			data[y][x][0] = gridtexture[x][y]*255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}
	r_notexture = GL_LoadPic ("***r_notexture***", (byte *)data, 8, 8, it_wall, 32);
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
==================
GL_ScreenShot_f
==================
*/
void GL_ScreenShot_f (void)
{
	byte		*buffer;
	int			i, c, temp;
	FILE		*f;
	char		checkname[MAX_OSPATH];

	for (i=0 ; i<=99 ; i++) {
		Com_sprintf (checkname, MAX_QPATH, "%s/scrnshot/ufo%i%i.tga", ri.FS_Gamedir(), i/10, i%10);
		f = fopen (checkname, "rb");
		/* file doesn't exist */
		if (!f)
			break;
		fclose (f);
	}
	if (i==100) {
		ri.Con_Printf (PRINT_ALL, "GL_ScreenShot_f: Couldn't create a file\n");
		return;
	}

	buffer = malloc(vid.width*vid.height*3 + 18);
	memset (buffer, 0, 18);
	/* uncompressed type */
	buffer[2] = 2;
	buffer[12] = vid.width&255;
	buffer[13] = vid.width>>8;
	buffer[14] = vid.height&255;
	buffer[15] = vid.height>>8;
	/* pixel size */
	buffer[16] = 24;

	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 );

	/* swap rgb to bgr */
	c = 18+vid.width*vid.height*3;
	for (i=18 ; i<c ; i+=3) {
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
	if ( ! ri.FS_WriteFile(buffer, c, checkname) )
		ri.Con_Printf(PRINT_ALL, "Could not write screenshot file to %s\n", checkname);
	free (buffer);
}

/*
** GL_Strings_f
*/
void GL_Strings_f( void )
{
	ri.Con_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string );
	ri.Con_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string );
	ri.Con_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string );
	ri.Con_Printf (PRINT_ALL, "MODE: %.0f, %d x %d FULLSCREEN: %.0f\n", gl_mode->value, vid.width, vid.height, vid_fullscreen->value );
	ri.Con_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string );
	ri.Con_Printf (PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", gl_config.maxTextureSize );
}

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.1);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);

	qglColor4f (1,1,1,1);
	qglClearColor( 0, 0, 0, 0 );

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel (GL_FLAT);

	GL_TextureMode( gl_texturemode->string );
	GL_TextureAlphaMode( gl_texturealphamode->string );
	GL_TextureSolidMode( gl_texturesolidmode->string );

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TexEnv( GL_REPLACE );

	if ( qglPointParameterfEXT ) {
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		qglEnable( GL_POINT_SMOOTH );
		qglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value );
		qglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value );
		qglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, attenuations );
	}

	GL_UpdateSwapInterval();

	/* doesn't really belong here... but works fine */
	vid.rx = (float)vid.width / VID_NORM_WIDTH;
	vid.ry = (float)vid.height / VID_NORM_HEIGHT;
}

void GL_UpdateSwapInterval( void )
{
	if ( gl_swapinterval->modified ) {
		gl_swapinterval->modified = qfalse;

#ifdef _WIN32
		if ( !gl_state.stereo_enabled ) {
			if ( qwglSwapIntervalEXT )
				qwglSwapIntervalEXT( gl_swapinterval->value );
		}
#endif
	}
}


/*
==============================================================================

						SOME DRAWING

==============================================================================
*/


/*
** R_DrawBeam
*/
void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->as.frame / 2, perpvec );

	for ( i = 0; i < 6; i++ ) {
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglDepthMask( GL_FALSE );

	r = ( d_8to24table[e->skinnum & 0xFF] ) & 0xFF;
	g = ( d_8to24table[e->skinnum & 0xFF] >> 8 ) & 0xFF;
	b = ( d_8to24table[e->skinnum & 0xFF] >> 16 ) & 0xFF;

	r *= 1/255.0F;
	g *= 1/255.0F;
	b *= 1/255.0F;

	qglColor4f( r, g, b, e->alpha );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ ) {
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglDepthMask( GL_TRUE );
}

/*=============
R_DrawBox

Draws the field marker
entity is specified in cl_actor.c CL_AddTargeting
=============*/
void R_DrawBox( entity_t *e )
{
	vec3_t	upper, lower;
	float	dx, dy;

/*	if ( R_CullBox( e->origin, e->oldorigin ) ) */
/*		return; */

	qglDepthMask( GL_FALSE );
	qglEnable( GL_BLEND );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_TEXTURE_2D );
	if ( !gl_wire->value )
		qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	qglEnable( GL_LINE_SMOOTH );

	qglColor4f( e->angles[0], e->angles[1], e->angles[2], e->alpha );

	VectorCopy( e->origin, lower );
	VectorCopy( e->origin, upper );
	upper[2] = e->oldorigin[2];

	dx = e->oldorigin[0] - e->origin[0];
	dy = e->oldorigin[1] - e->origin[1];

	qglBegin( GL_QUAD_STRIP );

	qglVertex3fv( lower );
	qglVertex3fv( upper );
	lower[0] += dx;
	upper[0] += dx;
	qglVertex3fv( lower );
	qglVertex3fv( upper );
	lower[1] += dy;
	upper[1] += dy;
	qglVertex3fv( lower );
	qglVertex3fv( upper );
	lower[0] -= dx;
	upper[0] -= dx;
	qglVertex3fv( lower );
	qglVertex3fv( upper );
	lower[1] -= dy;
	upper[1] -= dy;
	qglVertex3fv( lower );
	qglVertex3fv( upper );

	qglEnd();

	qglDisable( GL_LINE_SMOOTH );
	if ( !gl_wire->value )
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglEnable( GL_TEXTURE_2D );
	qglEnable( GL_CULL_FACE );
	qglDisable( GL_BLEND );
	qglDepthMask( GL_TRUE );
}
