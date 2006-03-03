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
// cl_view.c -- player rendering positioning

#include "client.h"

cvar_t		*cursor;
cvar_t		*cl_testparticles;
cvar_t		*cl_testentities;
cvar_t		*cl_testlights;
cvar_t		*cl_testblend;

cvar_t		*cl_stats;


int			r_numdlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numentities;
entity_t	r_entities[MAX_ENTITIES];

int			r_numparticles;
particle_t	r_particles[MAX_PARTICLES];

lightstyle_t	r_lightstyles[MAX_LIGHTSTYLES];

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;

int			map_maxlevel;
sun_t		map_sun;
dlight_t	map_lights[MAX_MAP_LIGHTS];
int			map_numlights;

float		map_fog;
vec4_t		map_fogColor;

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene (void)
{
	r_numdlights = 0;
	r_numentities = 0;
	r_numparticles = 0;
}


/*
=====================
V_GetEntity

=====================
*/
entity_t *V_GetEntity( void )
{
	return r_entities + r_numentities;
}


/*
=====================
V_AddEntity

=====================
*/
void V_AddEntity (entity_t *ent)
{
	if (r_numentities >= MAX_ENTITIES)
		return;

	r_entities[r_numentities++] = *ent;
}


/*
=====================
V_AddParticle

=====================
*/
void V_AddParticle (vec3_t org, int color, float alpha)
{
	particle_t	*p;

	if (r_numparticles >= MAX_PARTICLES)
		return;
	p = &r_particles[r_numparticles++];
	VectorCopy (org, p->origin);
	p->color = color;
	p->alpha = alpha;
}

/*
=====================
V_AddLight

=====================
*/
void V_AddLight (vec3_t org, float intensity, float r, float g, float b)
{
	dlight_t	*dl;

	if (r_numdlights >= MAX_DLIGHTS)
		return;
	dl = &r_dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->intensity = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


/*
=====================
V_AddLightStyle

=====================
*/
void V_AddLightStyle (int style, float r, float g, float b)
{
	lightstyle_t	*ls;

	if (style < 0 || style > MAX_LIGHTSTYLES)
		Com_Error (ERR_DROP, _("Bad light style %i"), style);
	ls = &r_lightstyles[style];

	ls->white = r+g+b;
	ls->rgb[0] = r;
	ls->rgb[1] = g;
	ls->rgb[2] = b;
}

/*
================
V_TestParticles

If cl_testparticles is set, create 4096 particles in the view
================
*/
void V_TestParticles (void)
{
	particle_t	*p;
	int			i, j;
	float		d, r, u;

	r_numparticles = MAX_PARTICLES;
	for (i=0 ; i<r_numparticles ; i++)
	{
		d = i*0.25;
		r = 4*((i&7)-3.5);
		u = 4*(((i>>3)&7)-3.5);
		p = &r_particles[i];

		for (j=0 ; j<3 ; j++)
			p->origin[j] = cl.refdef.vieworg[j] + cl.cam.axis[j][0]*d +
			cl.cam.axis[j][1]*r + cl.cam.axis[j][2]*u;

		p->color = 8;
		p->alpha = cl_testparticles->value;
	}
}

/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
void V_TestEntities (void)
{
	int			i, j;
	float		f, r;
	entity_t	*ent;

	r_numentities = 32;
	memset (r_entities, 0, sizeof(r_entities));

	for (i=0 ; i<r_numentities ; i++)
	{
		ent = &r_entities[i];

		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for (j=0 ; j<3 ; j++)
			ent->origin[j] = cl.refdef.vieworg[j] + cl.cam.axis[j][0]*f +
			cl.cam.axis[j][1]*r;

		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights (void)
{
	int			i, j;
	float		f, r;
	dlight_t	*dl;

	r_numdlights = 32;
	memset (r_dlights, 0, sizeof(r_dlights));

	for (i=0 ; i<r_numdlights ; i++)
	{
		dl = &r_dlights[i];

		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for (j=0 ; j<3 ; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.cam.axis[j][0]*f +
			cl.cam.axis[j][1]*r;
		dl->color[0] = ((i%6)+1) & 1;
		dl->color[1] = (((i%6)+1) & 2)>>1;
		dl->color[2] = (((i%6)+1) & 4)>>2;
		dl->intensity = 200;
	}
}

//===================================================================

/*
=================
CL_ParseEntitystring
=================
*/
void CL_ParseEntitystring( char *es )
{
	char		*strstart;
	char		*com_token;
	char		keyname[256];

	char	classname[64];
	char	model[MAX_VAR];
	char	particle[MAX_VAR];
	float	light;
	vec3_t	color, ambient, origin, angles, lightangles;
	vec2_t	wait;
	int		spawnflags;
	int		maxlevel;
	int		entnum;
	int		nosmooth;
	int		skin;

	VectorSet( map_fogColor, 0.5f, 0.5f, 0.5f );
	map_fogColor[3] = 1.0f;
	map_maxlevel = 8;
	map_numlights = 0;
	map_fog = 0.0;
	entnum = 0;
	numLMs = 0;
	numMPs = 0;

	// parse ents
	while (1)
	{
		// initialize
		VectorCopy( vec3_origin, ambient );
		VectorCopy( vec3_origin, color );
		VectorCopy( vec3_origin, origin );
		VectorCopy( vec3_origin, angles );
		wait[0] = wait[1] = 0;
		spawnflags = 0;
		light = 0;
		model[0] = 0;
		particle[0] = 0;
		nosmooth = 0;
		skin = 0;
		maxlevel = 8;

		// parse the opening brace	
		com_token = COM_Parse (&es);
		if (!es)
			break;
		if (com_token[0] != '{')
			Com_Error (ERR_DROP, _("CL_ParseEntitystring: found %s when expecting {"), com_token);

		// memorize the start
		strstart = es;

		// go through all the dictionary pairs
		while (1)
		{	
			// parse key
			com_token = COM_Parse (&es);
			if (com_token[0] == '}')
				break;
			if (!es)
				Com_Error (ERR_DROP, _("CL_ParseEntitystring: EOF without closing brace"));

			strncpy (keyname, com_token, sizeof(keyname)-1);
			
			// parse value	
			com_token = COM_Parse (&es);
			if (!es)
				Com_Error (ERR_DROP, _("CL_ParseEntitystring: EOF without closing brace"));

			if (com_token[0] == '}')
				Com_Error (ERR_DROP, _("CL_ParseEntitystring: closing brace without data"));

			// filter interesting keys
			if ( !strcmp( keyname, "classname" ) )
				strncpy( classname, com_token, 64 );

			if ( !strcmp( keyname, "model" ) )
				strncpy( model, com_token, MAX_VAR );

			if ( !strcmp( keyname, "particle" ) )
				strncpy( particle, com_token, MAX_VAR );

			if ( !strcmp( keyname, "_color" ) || !strcmp( keyname, "lightcolor" ) )
				sscanf( com_token, "%f %f %f", &(color[0]), &(color[1]), &(color[2]) );

			if ( !strcmp( keyname, "origin" ) )
				sscanf( com_token, "%f %f %f", &(origin[0]), &(origin[1]), &(origin[2]) );

			if ( !strcmp( keyname, "ambient" ) || !strcmp( keyname, "lightambient" ) )
				sscanf( com_token, "%f %f %f", &(ambient[0]), &(ambient[1]), &(ambient[2]) );

			if ( !strcmp( keyname, "angles" ) )
				sscanf( com_token, "%f %f %f", &(angles[0]), &(angles[1]), &(angles[2]) );

			if ( !strcmp( keyname, "wait" ) )
				sscanf( com_token, "%f %f", &(wait[0]), &(wait[1]) );

			if ( !strcmp( keyname, "light" ) )
				light = atof( com_token );

			if ( !strcmp( keyname, "lightangles" ) )
				sscanf( com_token, "%f %f %f", &(lightangles[0]), &(lightangles[1]), &(lightangles[2]) );

			if ( !strcmp( keyname, "spawnflags" ) )
				spawnflags = atoi( com_token );

			if ( !strcmp( keyname, "maxlevel" ) )
				maxlevel = atoi( com_token );

			if ( !strcmp( keyname, "fog" ) )
				map_fog = atof( com_token );

			if ( !strcmp( keyname, "fogcolor" ) )
				sscanf( com_token, "%f %f %f", &(map_fogColor[0]), &(map_fogColor[1]), &(map_fogColor[2]) );

			if ( !strcmp( keyname, "nosmooth" ) )
				nosmooth = atoi( com_token );

			if ( !strcmp( keyname, "skin" ) )
				skin = atoi( com_token );
		}

		// analyze values
		if ( !strcmp( classname, "worldspawn" ) )
		{
			// init sun
			angles[YAW] *= 3.14159/180;
			angles[PITCH] *= 3.14159/180;
			map_sun.dir[0] = cos( angles[YAW] ) * sin( angles[PITCH] );
			map_sun.dir[1] = sin( angles[YAW] ) * sin( angles[PITCH] );
			map_sun.dir[2] = cos( angles[PITCH] );

			VectorNormalize( color );
			VectorScale( color, light/100, map_sun.color );
			map_sun.color[3] = 1.0;

			// init ambient
			VectorScale( ambient, 1.4, map_sun.ambient );
			map_sun.ambient[3] = 1.0;

			// maximum level
			map_maxlevel = maxlevel;
		}

		if ( !strcmp( classname, "light" ) && light )
		{
			dlight_t	*newlight;
			// add light to list

			if ( map_numlights >= MAX_MAP_LIGHTS )
				Com_Error( ERR_DROP, _("Too many lights...\n") );

			newlight = &(map_lights[map_numlights++]);
			VectorCopy( origin, newlight->origin );
			VectorNormalize2( color, newlight->color );
			newlight->intensity = light;
		}

		if ( !strcmp( classname, "misc_model" ) )
		{
			lm_t *lm;

			if ( !model[0] )
			{
				Com_Printf( _("misc_model without \"model\" specified\n") );
				continue;
			}

			// add it
			lm = CL_AddLocalModel( model, particle, origin, angles, entnum, (spawnflags & 0xFF) );

			// get light parameters
			if ( lm )
			{
				if ( light != 0.0 ) 
				{
					lm->flags |= LMF_LIGHTFIXED;
					VectorCopy( color, lm->lightcolor );
					VectorCopy( ambient, lm->lightambient );
					lm->lightcolor[3] = light;
					lm->lightambient[3] = 1.0;

					lightangles[YAW] *= 3.14159/180;
					lightangles[PITCH] *= 3.14159/180;
					lm->lightorigin[0] = cos( lightangles[YAW] ) * sin( lightangles[PITCH] );
					lm->lightorigin[1] = sin( lightangles[YAW] ) * sin( lightangles[PITCH] );
					lm->lightorigin[2] = cos( lightangles[PITCH] );
					lm->lightorigin[3] = 0.0;
				}
				if ( nosmooth ) lm->flags |= LMF_NOSMOOTH;
				lm->skin = skin;
			}
		}

		if ( !strcmp( classname, "misc_particle" ) )
		{
			CL_AddMapParticle( particle, origin, wait, strstart );
		}

		if ( !strcmp( classname, "func_breakable" ) )
		{
			angles[0] = angles[1] = angles[2] = 0.0;
			CL_AddLocalModel( model, particle, origin, angles, entnum, (spawnflags & 0xFF) );
		}

		entnum++;
	}
}


/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh (void)
{
	le_t		*le;
	int			i;
	char		name[MAX_QPATH];

	if (!cl.configstrings[CS_TILES][0])
		return;		// no map loaded

	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);

	// register models, pics, and skins
	Com_Printf ( _("Map: %s\r"), cl.configstrings[CS_NAME]); 
	SCR_UpdateScreen ();
	re.BeginRegistration( cl.configstrings[CS_TILES], cl.configstrings[CS_POSITIONS] );
	CL_ParseEntitystring( map_entitystring );
	Com_Printf ("                                     \r");

	// precache status bar pics
	Com_Printf (_("pics\r") ); 
	SCR_UpdateScreen ();
	SCR_TouchPics ();
	Com_Printf ("                                     \r");

	CL_RegisterLocalModels ();
	CL_ParticleRegisterArt ();

	num_cl_weaponmodels = 1;
	strcpy(cl_weaponmodels[0], "weapon.md2");

	for (i=1 ; i<MAX_MODELS && cl.configstrings[CS_MODELS+i][0] ; i++)
	{
		strcpy (name, cl.configstrings[CS_MODELS+i]);
		name[37] = 0;	// never go beyond one line
		Com_Printf( name );
		if (name[0] != '*')
			Com_Printf ("%s\r", name); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		if (name[0] == '#')
		{
			// special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				strncpy(cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS+i]+1,
					sizeof(cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
		} 
		else
		{
			cl.model_draw[i] = re.RegisterModel (cl.configstrings[CS_MODELS+i]);
			if (name[0] == '*')
				cl.model_clip[i] = CM_InlineModel (cl.configstrings[CS_MODELS+i]);
			else
				cl.model_clip[i] = NULL;
		}
		if (name[0] != '*')
			Com_Printf ("                                     \r");
	}

	// update le model references
	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
		if ( le->inuse )
		{
			if ( le->modelnum1 ) le->model1 = cl.model_draw[le->modelnum1];
			if ( le->modelnum2 ) le->model2 = cl.model_draw[le->modelnum2];
		}

	// load weapons stuff
	for ( i = 0; i < csi.numODs; i++ )
		cl.model_weapons[i] = re.RegisterModel( csi.ods[i].model );

	// images
	Com_Printf (_("images\r"), i); 
	SCR_UpdateScreen ();
	for (i=1 ; i<MAX_IMAGES && cl.configstrings[CS_IMAGES+i][0] ; i++)
	{
		cl.image_precache[i] = re.RegisterPic (cl.configstrings[CS_IMAGES+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	
	Com_Printf ("                                     \r");
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		Com_Printf (_("client %i\r"), i); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
//		CL_ParseClientinfo (i);
		Com_Printf ("                                     \r");
	}

	// set sky textures and speed
/*	Com_Printf ("sky\r", i); 
	SCR_UpdateScreen ();
	rotate = atof (cl.configstrings[CS_SKYROTATE]);
	sscanf (cl.configstrings[CS_SKYAXIS], "%f %f %f", 
		&axis[0], &axis[1], &axis[2]);
	re.SetSky (cl.configstrings[CS_SKY], rotate, axis);
	Com_Printf ("                                     \r");*/

	// the renderer can now free unneeded stuff
	re.EndRegistration ();

	// clear any lines of console text
	Con_ClearNotify ();
	SCR_EndLoadingPlaque ();	

	SCR_UpdateScreen ();
	cl.refresh_prepped = true;
	cl.force_refdef = true;	// make sure we have a valid refdef

	// start the cd track
	CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
}

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float	a;
	float	x;

	if (fov_x < 1 || fov_x > 179)
		Com_Error (ERR_DROP, _("Bad fov: %f"), fov_x);

	x = width/tan(fov_x/360*M_PI);

	a = atan (height/x);

	a = a*360/M_PI;

	return a;
}



/*
===================
CL_CalcRefdef
===================
*/
void CL_CalcRefdef ()
{
	float	zoom;

	VectorCopy( cl.cam.camorg, cl.refdef.vieworg );
	VectorCopy( cl.cam.angles, cl.refdef.viewangles );

	VectorSet( cl.refdef.blend, 0.0, 0.0, 0.0 );

	// set field of view
	if ( cl.cam.zoom < MIN_ZOOM ) zoom = MIN_ZOOM;
	else if ( cl.cam.zoom > MAX_ZOOM ) zoom = MAX_ZOOM;
	else zoom = cl.cam.zoom;
	cl.refdef.fov_x = FOV / zoom;

	// set dependant variables
	cl.refdef.x = scr_vrect.x;
	cl.refdef.y = scr_vrect.y;
	cl.refdef.width = scr_vrect.width;
	cl.refdef.height = scr_vrect.height;
	cl.refdef.fov_y = CalcFov (cl.refdef.fov_x, scr_vrect.width, scr_vrect.height);
	cl.refdef.time = cl.time*0.001;
	cl.refdef.areabits = 0;
}


/*
==================
V_RenderView

==================
*/
void V_RenderView( float stereo_separation )
{
	extern int entitycmpfnc( const entity_t *, const entity_t * );

	if (cls.state != ca_active && cls.state != ca_sequence) 
		return;

	if (!scr_vrect.width || !scr_vrect.height)
		return;

	if (!cl.refresh_prepped)
		return;			// still loading

	if (cl_timedemo->value)
	{
		if (!cl.timedemo_start)
			cl.timedemo_start = Sys_Milliseconds ();
		cl.timedemo_frames++;
	}


	V_ClearScene ();
//	CL_RunLightStyles ();

	if (cls.state == ca_sequence)
	{
		CL_SequenceRender();
		cl.refdef.rdflags |= RDF_NOWORLDMODEL;
	} 
	else 
	{
		LM_AddToScene ();
		LE_AddToScene ();
		CL_AddTargeting ();
		CL_AddParticles ();
		CL_AddLightStyles ();
	}

	CL_CalcRefdef ();

	// offset vieworg appropriately if we're doing stereo separation
	if ( stereo_separation != 0 )
	{
		vec3_t tmp;

		VectorScale( cl.cam.axis[1], stereo_separation, tmp );
		VectorAdd( cl.refdef.vieworg, tmp, cl.refdef.vieworg );
	}

	// setup refdef
	cl.refdef.worldlevel = cl_worldlevel->value;
	cl.refdef.num_entities = r_numentities;
	cl.refdef.entities = r_entities;
	cl.refdef.num_particles = r_numparticles;
	cl.refdef.particles = r_particles;
	cl.refdef.num_dlights = r_numdlights;
	cl.refdef.dlights = r_dlights;
	cl.refdef.lightstyles = r_lightstyles;
	cl.refdef.fog = map_fog;
	cl.refdef.fogColor = map_fogColor;

	cl.refdef.num_ptls = numPtls;
	cl.refdef.ptls = ptl;
	cl.refdef.ptl_art = ptlArt;

	cl.refdef.sun = &map_sun;
	if ( cls.state == ca_sequence )
		cl.refdef.num_lights = 0;
	else
	{
		cl.refdef.ll = map_lights;
		cl.refdef.num_lights = map_numlights;
	}

	// render the frame
	re.RenderFrame (&cl.refdef);
	if (cl_stats->value)
		Com_Printf (_("ent:%i  lt:%i  part:%i\n"), r_numentities, r_numdlights, r_numparticles);
	if ( log_stats->value && ( log_stats_file != 0 ) )
		fprintf( log_stats_file, "%i,%i,%i,",r_numentities, r_numdlights, r_numparticles);


	SCR_AddDirtyPoint (scr_vrect.x, scr_vrect.y);
	SCR_AddDirtyPoint (scr_vrect.x+scr_vrect.width-1,
		scr_vrect.y+scr_vrect.height-1);

	if ( cls.state == ca_sequence )
		CL_Sequence2D();
}


/*
=============
V_CenterView
=============
*/
void V_CenterView( pos3_t pos )
{
	vec3_t	vec;

	PosToVec( pos, vec );
	VectorCopy( vec, cl.cam.reforg );
	Cvar_SetValue( "cl_worldlevel", pos[2] );
}

	
/*
=============
V_Viewpos_f
=============
*/
void V_Viewpos_f (void)
{
	Com_Printf ("(%i %i %i) : %i\n", (int)cl.refdef.vieworg[0],
		(int)cl.refdef.vieworg[1], (int)cl.refdef.vieworg[2], 
		(int)cl.refdef.viewangles[YAW]);
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("viewpos", V_Viewpos_f);

	cursor = Cvar_Get ("cursor", "1", CVAR_ARCHIVE);

	cl_testblend = Cvar_Get ("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get ("cl_testlights", "0", 0);

	cl_stats = Cvar_Get ("cl_stats", "0", 0);
}
