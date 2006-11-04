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
// cmodel.c -- model loading

#include "qcommon.h"

typedef struct
{
	cplane_t	*plane;
	vec3_t		mins, maxs;
	int			children[2];		// negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t	*plane;
	mapsurface_t	*surface;
} cbrushside_t;

typedef struct
{
	int			contents;
	int			cluster;
	int			area;
	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;
} cleaf_t;

typedef struct
{
	int			contents;
	int			numsides;
	int			firstbrushside;
	int			checkcount;		// to avoid repeated testings
} cbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;			// if two areas have equal floodnums, they are connected
	int		floodvalid;
} carea_t;

int			checkcount;

char		map_name[MAX_QPATH];

int			numbrushsides;
cbrushside_t map_brushsides[MAX_MAP_BRUSHSIDES];

int			numtexinfo;
mapsurface_t	map_surfaces[MAX_MAP_TEXINFO];

int			numplanes;
cplane_t	map_planes[MAX_MAP_PLANES+6];		// extra for box hull

int			numnodes;
cnode_t		map_nodes[MAX_MAP_NODES+6];		// extra for box hull

int			numleafs = 1;	// allow leaf funcs to be called without a map
cleaf_t		map_leafs[MAX_MAP_LEAFS];
int			emptyleaf, solidleaf;

int			numleafbrushes;
unsigned short	map_leafbrushes[MAX_MAP_LEAFBRUSHES];

int			numcmodels;
cmodel_t	map_cmodels[MAX_MAP_MODELS];

int			numbrushes;
cbrush_t	map_brushes[MAX_MAP_BRUSHES];

byte		map_route[8][256][256];
byte		map_fall[256][256];
byte		map_step[256][256];

byte		sh_low;
byte		sh_big;

int			map_maxlevel;
sun_t		map_sun;
dlight_t	map_lights[MAX_MAP_LIGHTS];
int			map_numlights;

vec3_t		map_min, map_max;

int			numentitychars;
char		map_entitystring[MAX_MAP_ENTSTRING];

int			numareas = 1;
carea_t		map_areas[MAX_MAP_AREAS];

int			numareaportals;
dareaportal_t map_areaportals[MAX_MAP_AREAPORTALS];

int			numclusters = 1;

mapsurface_t	nullsurface;

int			floodvalid;

qboolean	portalopen[MAX_MAP_AREAPORTALS];


cvar_t		*map_noareas;

void	CM_InitBoxHull (void);
void	FloodAreaConnections (void);


int		c_pointcontents;
int		c_traces, c_brush_traces;


// =====================================================================

#define	ON_EPSILON	0.1

typedef struct tnode_s
{
	int		type;
	vec3_t	normal;
	float	dist;
	int		children[2];
	int		pad;
} tnode_t;

tnode_t		*tnodes, *tnode_p;

int		cur_level;

int		numtheads;
int		thead[256];

vec3_t	tr_end;

typedef struct chead_s
{
	int		cnode;
	int		level;
} chead_t;

int		numcheads;
chead_t	chead[MAX_MAP_NODES];

/*
===============================================================================

					MAP LOADING

===============================================================================
*/

byte	*cmod_base;

/*
=================
CMod_LoadSubmodels
=================
*/
void CMod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error (ERR_DROP, "Map has too many models");

	numcmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out = &map_cmodels[i];

		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		out->headnode = LittleLong (in->headnode);
	}
}


/*
=================
CMod_LoadSurfaces
=================
*/
void CMod_LoadSurfaces (lump_t *l)
{
	texinfo_t	*in;
	mapsurface_t	*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no surfaces");
	if (count > MAX_MAP_TEXINFO)
		Com_Error (ERR_DROP, "Map has too many surfaces");

	numtexinfo = count;
	out = map_surfaces;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		strncpy (out->c.name, in->texture, sizeof(out->c.name)-1);
		strncpy (out->rname, in->texture, sizeof(out->rname)-1);
		out->c.flags = LittleLong (in->flags);
		out->c.value = LittleLong (in->value);
	}
}


/*
=================
CMod_LoadNodes

=================
*/
void CMod_LoadNodes (lump_t *l)
{
	dnode_t		*in;
	int			child;
	cnode_t		*out;
	int			i, j, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Com_Error (ERR_DROP, "Map has too many nodes");

	out = map_nodes;

	numnodes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		if ( in->planenum == -1 ) 
			out->plane = NULL;
		else
			out->plane = map_planes + LittleLong(in->planenum);

		VectorCopy( in->mins, out->mins );
		VectorCopy( in->maxs, out->maxs );

		for (j=0 ; j<2 ; j++)
		{
			child = LittleLong (in->children[j]);
			out->children[j] = child;
		}
	}

}

/*
=================
CMod_LoadBrushes

=================
*/
void CMod_LoadBrushes (lump_t *l)
{
	dbrush_t	*in;
	cbrush_t	*out;
	int			i, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_BRUSHES)
		Com_Error (ERR_DROP, "Map has too many brushes");

	out = map_brushes;

	numbrushes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
	}

}

/*
=================
CMod_LoadLeafs
=================
*/
void CMod_LoadLeafs (lump_t *l)
{
	int			i;
	cleaf_t		*out;
	dleaf_t 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");
	// need to save space for box planes
	if (count > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_leafs;	
	numleafs = count;
	numclusters = 0;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->contents = LittleLong (in->contents);
		out->cluster = LittleShort (in->cluster);
		out->area = LittleShort (in->area);
		out->firstleafbrush = LittleShort (in->firstleafbrush);
		out->numleafbrushes = LittleShort (in->numleafbrushes);

		if (out->cluster >= numclusters)
			numclusters = out->cluster + 1;
	}

	if (map_leafs[0].contents != CONTENTS_SOLID)
		Com_Error (ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");
	solidleaf = 0;
	emptyleaf = -1;
	for (i=1 ; i<numleafs ; i++)
	{
		if (!map_leafs[i].contents)
		{
			emptyleaf = i;
			break;
		}
	}
	if (emptyleaf == -1)
		Com_Error (ERR_DROP, "Map does not have an empty leaf");
}

/*
=================
CMod_LoadPlanes
=================
*/
void CMod_LoadPlanes (lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	// need to save space for box planes
	if (count > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_planes;	
	numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
CMod_LoadLeafBrushes
=================
*/
void CMod_LoadLeafBrushes (lump_t *l)
{
	int			i;
	unsigned short	*out;
	unsigned short 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	// need to save space for box planes
	if (count > MAX_MAP_LEAFBRUSHES)
		Com_Error (ERR_DROP, "Map has too many leafbrushes");

	out = map_leafbrushes;
	numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = LittleShort (*in);
}

/*
=================
CMod_LoadBrushSides
=================
*/
void CMod_LoadBrushSides (lump_t *l)
{
	int			i, j;
	cbrushside_t	*out;
	dbrushside_t 	*in;
	int			count;
	int			num;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	// need to save space for box planes
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_brushsides;	
	numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		num = LittleShort (in->planenum);
		out->plane = &map_planes[num];
		j = LittleShort (in->texinfo);
		if (j >= numtexinfo)
			Com_Error (ERR_DROP, "Bad brushside texinfo");
		out->surface = &map_surfaces[j];
	}
}

/*
=================
CMod_LoadAreas
=================
*/
void CMod_LoadAreas (lump_t *l)
{
	int			i;
	carea_t		*out;
	darea_t 	*in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error (ERR_DROP, "Map has too many areas");

	out = map_areas;
	numareas = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->numareaportals = LittleLong (in->numareaportals);
		out->firstareaportal = LittleLong (in->firstareaportal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}

/*
=================
CMod_LoadAreaPortals
=================
*/
void CMod_LoadAreaPortals (lump_t *l)
{
	int			i;
	dareaportal_t		*out;
	dareaportal_t 	*in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error (ERR_DROP, "Map has too many areas");

	out = map_areaportals;
	numareaportals = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->portalnum = LittleLong (in->portalnum);
		out->otherarea = LittleLong (in->otherarea);
	}
}


/*
===============
Cmod_DeCompressRouting

source will be set to the end of the compressed data block!
===============
*/
int Cmod_DeCompressRouting( byte **source, byte *dataStart )
{
	int	i, c;
	byte	*data_p;
	byte	*src;

	data_p = dataStart;
	src = *source;

	while ( *src )
	{
		if ( *src & 0x80 )
		{
			// repetitions
			c = *src++ & ~0x80;
			for ( i = 0; i < c+1; i++ ) *data_p++ = *src;
			src++;
		}
		else
		{
			// identities
			c = *src++;
			for ( i = 0; i < c; i++ ) *data_p++ = *src++;
		}
	}

	src++;
	*source = src;

	return data_p - dataStart;
}

/*
=================
CMod_GetMapSize
=================
*/
void CMod_GetMapSize( void )
{
	vec3_t	offset = {100, 100, 100};
	pos3_t	min, max;
	int x, y;

	VectorSet( min, 255, 255, 0 );
	VectorSet( max, 0, 0, 0);

	// get border
	for ( y = 0; y < 256; y++ )
		for ( x = 0; x < 256; x++ )
			if ( map_fall[y][x] )
			{
				if ( x < min[0] ) min[0] = x;
				if ( y < min[1] ) min[1] = y;
				if ( x > max[0] ) max[0] = x;
				if ( y > max[1] ) max[1] = y;
			}

	// convert to vectors
	PosToVec( min, map_min );
	PosToVec( max, map_max );

	// tiny offset
	VectorAdd( map_min, offset, map_min );
	VectorSubtract( map_max, offset, map_max );
}

/*
=================
CMod_LoadRouting
=================
*/
void CMod_LoadRouting (lump_t *l)
{
	byte	*source;
	int		length;

	if ( !l->filelen )
		Com_Error (ERR_DROP, "Map has NO routing lump");

	source = cmod_base + l->fileofs;
	sh_low = *source++;
	sh_big = *source++;
	length =  Cmod_DeCompressRouting( &source, &(map_route[0][0][0]) );
	length += Cmod_DeCompressRouting( &source, &(map_fall[0][0]) );
	length += Cmod_DeCompressRouting( &source, &(map_step[0][0]) );


	if ( length != 256*256*10 )
	{
		Com_Error (ERR_DROP, "Map has BAD routing lump");
	}

//	old code without compression
//	memcpy( map_route, source, 256*256*8 );
//	memcpy( map_fall, source + 256*256*8, 256*256 );
//	memcpy( map_step, source + 256*256*9, 256*256 );

//	calculate border
	CMod_GetMapSize();
}


/*
=================
CM_GetLightList
=================
*/
int CM_GetLightList( dlight_t **list )
{
	*list = &(map_lights[0]);
	return map_numlights;
}


/*
=================
CMod_ParseEntitystring
=================
*/
void CMod_ParseEntitystring( char *es )
{
	char		*strstart;
	char		*com_token;
	char		keyname[256];

	char	classname[64];
	char	ref[MAX_QPATH];
	float	light;
	vec3_t	color, ambient, origin, angles;
	vec2_t	wait;
	int		spawnflags;
	int		maxlevel;

	map_maxlevel = 8;
	map_numlights = 0;
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
		ref[0] = 0;
		maxlevel = 8;

		// parse the opening brace	
		com_token = COM_Parse (&es);
		if (!es)
			break;
		if (com_token[0] != '{')
			Com_Error (ERR_DROP, "ED_LoadFromFile: found %s when expecting {",com_token);

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
				Com_Error (ERR_DROP, "ED_ParseEntity: EOF without closing brace");

			strncpy (keyname, com_token, sizeof(keyname)-1);
			
			// parse value	
			com_token = COM_Parse (&es);
			if (!es)
				Com_Error (ERR_DROP, "ED_ParseEntity: EOF without closing brace");

			if (com_token[0] == '}')
				Com_Error (ERR_DROP, "ED_ParseEntity: closing brace without data");

			// filter interesting keys
			if ( !strcmp( keyname, "classname" ) )
				strncpy( classname, com_token, 64 );

			if ( !strcmp( keyname, "model" ) || !strcmp( keyname, "particle" ) )
				strncpy( ref, com_token, MAX_QPATH );

			if ( !strcmp( keyname, "_color" ) )
				sscanf( com_token, "%f %f %f", &(color[0]), &(color[1]), &(color[2]) );

			if ( !strcmp( keyname, "origin" ) )
				sscanf( com_token, "%f %f %f", &(origin[0]), &(origin[1]), &(origin[2]) );

			if ( !strcmp( keyname, "ambient" ) )
				sscanf( com_token, "%f %f %f", &(ambient[0]), &(ambient[1]), &(ambient[2]) );

			if ( !strcmp( keyname, "angles" ) )
				sscanf( com_token, "%f %f %f", &(angles[0]), &(angles[1]), &(angles[2]) );

			if ( !strcmp( keyname, "wait" ) )
				sscanf( com_token, "%f %f", &(wait[0]), &(wait[1]) );

			if ( !strcmp( keyname, "light" ) )
				light = atof( com_token );

			if ( !strcmp( keyname, "spawnflags" ) )
				spawnflags = atoi( com_token );

			if ( !strcmp( keyname, "maxlevel" ) )
				maxlevel = atoi( com_token );
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
				Com_Error( ERR_DROP, "Too many lights...\n" );

			newlight = &(map_lights[map_numlights++]);
			VectorCopy( origin, newlight->origin );
			VectorNormalize2( color, newlight->color );
			newlight->intensity = light;
		}

		if ( !strcmp( classname, "misc_model" ) )
		{
			if ( !ref[0] )
			{
				Com_Printf( "misc_model without \"model\" specified\n" );
				continue;
			}

			// add it
			CL_AddLocalModel( ref, origin, angles, (spawnflags & 0xFF) );
		}

		if ( !strcmp( classname, "misc_particle" ) )
		{
			CL_AddMapParticle( ref, origin, wait, strstart );
		}
	}
	
//	Com_Printf( "lights: %i\n", map_numlights );
}


/*
=================
CMod_LoadEntityString
=================
*/
void CMod_LoadEntityString (lump_t *l)
{
	numentitychars = l->filelen;
	if (l->filelen > MAX_MAP_ENTSTRING)
		Com_Error (ERR_DROP, "Map has too large entity lump");

	memcpy (map_entitystring, cmod_base + l->fileofs, l->filelen);

	CMod_ParseEntitystring( map_entitystring );
}



/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
cmodel_t *CM_LoadMap (char *name, qboolean clientload, unsigned *checksum)
{
	unsigned		*buf;
	int				i;
	dheader_t		header;
	int				length;
	static unsigned	last_checksum;

	map_noareas = Cvar_Get ("map_noareas", "0", 0);

	if (  !strcmp (map_name, name) && (clientload || !Cvar_VariableValue ("flushmap")) )
	{
		*checksum = last_checksum;
		if (!clientload)
		{
			memset (portalopen, 0, sizeof(portalopen));
			FloodAreaConnections ();
		}
		return &map_cmodels[0];		// still have the right version
	}

	// free old stuff
	numplanes = 0;
	numnodes = 0;
	numleafs = 0;
	numcmodels = 0;
	numentitychars = 0;
	map_entitystring[0] = 0;
	map_name[0] = 0;

	if (!name || !name[0])
	{
		numleafs = 1;
		numclusters = 1;
		numareas = 1;
		*checksum = 0;
		return &map_cmodels[0];			// cinematic servers won't have anything at all
	}

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buf);
	if (!buf)
		Com_Error (ERR_DROP, "Couldn't load %s", name);

	last_checksum = LittleLong (Com_BlockChecksum (buf, length));
	*checksum = last_checksum;

	header = *(dheader_t *)buf;
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)&header)[i] = LittleLong ( ((int *)&header)[i]);

	if (header.version != BSPVERSION)
		Com_Error (ERR_DROP, "CMod_LoadBrushModel: %s has wrong version number (%i should be %i)"
		, name, header.version, BSPVERSION);

	cmod_base = (byte *)buf;

	// load into heap
	CMod_LoadSurfaces (&header.lumps[LUMP_TEXINFO]);
	CMod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes (&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadPlanes (&header.lumps[LUMP_PLANES]);
	CMod_LoadBrushes (&header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides (&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
	CMod_LoadNodes (&header.lumps[LUMP_NODES]);
	CMod_LoadAreas (&header.lumps[LUMP_AREAS]);
	CMod_LoadAreaPortals (&header.lumps[LUMP_AREAPORTALS]);
	CMod_LoadRouting (&header.lumps[LUMP_ROUTING]);
	CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);

	FS_FreeFile (buf);

	CM_InitBoxHull ();
	CM_MakeTnodes ();

	memset (portalopen, 0, sizeof(portalopen));
	FloodAreaConnections ();

	strcpy (map_name, name);

	return &map_cmodels[0];
}

/*
==================
CM_InlineModel
==================
*/
cmodel_t	*CM_InlineModel (char *name)
{
	int		num;

	if (!name || name[0] != '*')
		Com_Error (ERR_DROP, "CM_InlineModel: bad name");
	num = atoi (name+1);
	if (num < 1 || num >= numcmodels)
		Com_Error (ERR_DROP, "CM_InlineModel: bad number");

	return &map_cmodels[num];
}

int		CM_NumClusters (void)
{
	return numclusters;
}

int		CM_NumInlineModels (void)
{
	return numcmodels;
}

char	*CM_EntityString (void)
{
	return map_entitystring;
}

int		CM_LeafContents (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafContents: bad number");
	return map_leafs[leafnum].contents;
}

int		CM_LeafCluster (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafCluster: bad number");
	return map_leafs[leafnum].cluster;
}

int		CM_LeafArea (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafArea: bad number");
	return map_leafs[leafnum].area;
}

//=======================================================================


cplane_t	*box_planes;
int			box_headnode;
cbrush_t	*box_brush;
cleaf_t		*box_leaf;

/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cnode_t		*c;
	cplane_t	*p;
	cbrushside_t	*s;

	box_headnode = numnodes;
	box_planes = &map_planes[numplanes];
	if (numnodes+6 > MAX_MAP_NODES
		|| numbrushes+1 > MAX_MAP_BRUSHES
		|| numleafbrushes+1 > MAX_MAP_LEAFBRUSHES
		|| numbrushsides+6 > MAX_MAP_BRUSHSIDES
		|| numplanes+12 > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Not enough room for box tree");

	box_brush = &map_brushes[numbrushes];
	box_brush->numsides = 6;
	box_brush->firstbrushside = numbrushsides;
	box_brush->contents = CONTENTS_MONSTER;

	box_leaf = &map_leafs[numleafs];
	box_leaf->contents = CONTENTS_MONSTER;
	box_leaf->firstleafbrush = numleafbrushes;
	box_leaf->numleafbrushes = 1;

	map_leafbrushes[numleafbrushes] = numbrushes;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		// brush sides
		s = &map_brushsides[numbrushsides+i];
		s->plane = 	map_planes + (numplanes+i*2+side);
		s->surface = &nullsurface;

		// nodes
		c = &map_nodes[box_headnode+i];
		c->plane = map_planes + (numplanes+i*2);
		c->children[side] = -1 - emptyleaf;
		if (i != 5)
			c->children[side^1] = box_headnode+i + 1;
		else
			c->children[side^1] = -1 - numleafs;

		// planes
		p = &box_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;
	}	
}


/*
===================
CM_HeadnodeForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
int	CM_HeadnodeForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	return box_headnode;
}


/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnum_r (vec3_t p, int num)
{
	float		d;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = map_nodes + num;
		plane = node->plane;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++;		// optimize counter

	return -1 - num;
}

int CM_PointLeafnum (vec3_t p)
{
	if (!numplanes)
		return 0;		// sound may call this without map loaded
	return CM_PointLeafnum_r (p, 0);
}



/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
int		leaf_count, leaf_maxcount;
int		*leaf_list;
float	*leaf_mins, *leaf_maxs;
int		leaf_topnode;

void CM_BoxLeafnums_r (int nodenum)
{
	cplane_t	*plane;
	cnode_t		*node;
	int		s;

	while (1)
	{
		if (nodenum < 0)
		{
			if (leaf_count >= leaf_maxcount)
			{
//				Com_Printf ("CM_BoxLeafnums_r: overflow\n");
				return;
			}
			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}
	
		node = &map_nodes[nodenum];
		plane = node->plane;
//		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane);
		s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);
		if (s == 1)
			nodenum = node->children[0];
		else if (s == 2)
			nodenum = node->children[1];
		else
		{	// go down both
			if (leaf_topnode == -1)
				leaf_topnode = nodenum;
			CM_BoxLeafnums_r (node->children[0]);
			nodenum = node->children[1];
		}

	}
}

int	CM_BoxLeafnums_headnode (vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r (headnode);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}

int	CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode)
{
	return CM_BoxLeafnums_headnode (mins, maxs, list,
		listsize, map_cmodels[0].headnode, topnode);
}



/*
==================
CM_PointContents

==================
*/
int CM_PointContents (vec3_t p, int headnode)
{
	int		l;

	if (!numnodes)	// map not loaded
		return 0;

	l = CM_PointLeafnum_r (p, headnode);

	return map_leafs[l].contents;
}

/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int	CM_TransformedPointContents (vec3_t p, int headnode, vec3_t origin, vec3_t angles)
{
	vec3_t		p_l;
	vec3_t		temp;
	vec3_t		forward, right, up;
	int			l;

	// subtract origin offset
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (headnode != box_headnode && 
	(angles[0] || angles[1] || angles[2]) )
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	l = CM_PointLeafnum_r (p_l, headnode);

	return map_leafs[l].contents;
}


/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

vec3_t	trace_start, trace_end;
vec3_t	trace_mins, trace_maxs;
vec3_t	trace_extents;

trace_t	trace_trace;
int		trace_contents;
qboolean	trace_ispoint;		// optimized case

/*
================
CM_ClipBoxToBrush
================
*/
void CM_ClipBoxToBrush (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
					  trace_t *trace, cbrush_t *brush)
{
	int			i, j;
	cplane_t	*plane, *clipplane;
	float		dist;
	float		enterfrac, leavefrac;
	vec3_t		ofs;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
	cbrushside_t	*side, *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if (!brush->numsides)
		return;

	c_brush_traces++;

	getout = false;
	startout = false;
	leadside = NULL;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		// FIXME: special case for axial

		if (!trace_ispoint)
		{	// general box case

			// push the plane out apropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
		}
		else
		{	// special point case
			dist = plane->dist;
		}

		d1 = DotProduct (p1, plane->normal) - dist;
		d2 = DotProduct (p2, plane->normal) - dist;

		if (d2 > 0)
			getout = true;	// endpoint is not in solid
		if (d1 > 0)
			startout = true;

		// if completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		// crosses face
		if (d1 > d2)
		{	// enter
			f = (d1-DIST_EPSILON) / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		}
		else
		{	// leave
			f = (d1+DIST_EPSILON) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		if (!getout)
			trace->allsolid = true;
		return;
	}
	if (enterfrac < leavefrac)
	{
		if (enterfrac > -1 && enterfrac < trace->fraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = &(leadside->surface->c);
			trace->contents = brush->contents;
		}
	}
}

/*
================
CM_TestBoxInBrush
================
*/
void CM_TestBoxInBrush (vec3_t mins, vec3_t maxs, vec3_t p1,
					  trace_t *trace, cbrush_t *brush)
{
	int			i, j;
	cplane_t	*plane;
	float		dist;
	vec3_t		ofs;
	float		d1;
	cbrushside_t	*side;

	if (!brush->numsides)
		return;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		// FIXME: special case for axial

		// general box case

		// push the plane out apropriately for mins/maxs

		// FIXME: use signbits into 8 way lookup for each mins/maxs
		for (j=0 ; j<3 ; j++)
		{
			if (plane->normal[j] < 0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}
		dist = DotProduct (ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct (p1, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0)
			return;

	}

	// inside this brush
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->contents = brush->contents;
}


/*
================
CM_TraceToLeaf
================
*/
void CM_TraceToLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;
	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];
		b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;
		CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

}


/*
================
CM_TestInLeaf
================
*/
void CM_TestInLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;
	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];
		b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;
		CM_TestBoxInBrush (trace_mins, trace_maxs, trace_start, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

}


/*
==================
CM_RecursiveHullCheck

==================
*/
void CM_RecursiveHullCheck (int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	cnode_t		*node;
	cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

	if (trace_trace.fraction <= p1f)
		return;		// already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceToLeaf (-1-num);
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = map_nodes + num;
	plane = node->plane;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if (trace_ispoint)
			offset = 0;
		else
			offset = fabs(trace_extents[0]*plane->normal[0]) +
				fabs(trace_extents[1]*plane->normal[1]) +
				fabs(trace_extents[2]*plane->normal[2]);
	}


#if 0
CM_RecursiveHullCheck (node->children[0], p1f, p2f, p1, p2);
CM_RecursiveHullCheck (node->children[1], p1f, p2f, p1, p2);
return;
#endif

	// see which sides we need to consider
	if (t1 >= offset && t2 >= offset)
	{
		CM_RecursiveHullCheck (node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (t1 < -offset && t2 < -offset)
	{
		CM_RecursiveHullCheck (node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2)
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset + DIST_EPSILON)*idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;
		
	midf = p1f + (p2f - p1f)*frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);


	// go past the node
	if (frac2 < 0)
		frac2 = 0;
	if (frac2 > 1)
		frac2 = 1;
		
	midf = p1f + (p2f - p1f)*frac2;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac2*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (node->children[side^1], midf, p2f, mid, p2);
}



//======================================================================

/*
==================
CM_BoxTrace
==================
*/
trace_t		CM_BoxTrace (vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  int headnode, int brushmask)
{
	int		i;

	checkcount++;		// for multi-check avoidance

	c_traces++;			// for statistics, may be zeroed

	// fill in a default trace
	memset (&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.surface = &(nullsurface.c);

	if (!numnodes)	// map not loaded
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy (start, trace_start);
	VectorCopy (end, trace_end);
	VectorCopy (mins, trace_mins);
	VectorCopy (maxs, trace_maxs);

	//
	// check for position test special case
	//
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		int		leafs[1024];
		int		i, numleafs;
		vec3_t	c1, c2;
		int		topnode;

		VectorAdd (start, mins, c1);
		VectorAdd (start, maxs, c2);
		for (i=0 ; i<3 ; i++)
		{
			c1[i] -= 1;
			c2[i] += 1;
		}

		numleafs = CM_BoxLeafnums_headnode (c1, c2, leafs, 1024, headnode, &topnode);
		for (i=0 ; i<numleafs ; i++)
		{
			CM_TestInLeaf (leafs[i]);
			if (trace_trace.allsolid)
				break;
		}
		VectorCopy (start, trace_trace.endpos);
		return trace_trace;
	}

	//
	// check for point special case
	//
	if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0
		&& maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0)
	{
		trace_ispoint = true;
		VectorClear (trace_extents);
	}
	else
	{
		trace_ispoint = false;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	//
	// general sweeping through world
	//
	CM_RecursiveHullCheck (headnode, 0, 1, start, end);

	if (trace_trace.fraction == 1)
	{
		VectorCopy (end, trace_trace.endpos);
	}
	else
	{
		for (i=0 ; i<3 ; i++)
			trace_trace.endpos[i] = start[i] + trace_trace.fraction * (end[i] - start[i]);
	}
	return trace_trace;
}


/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
#ifdef _WIN32
#pragma optimize( "", off )
#endif


trace_t		CM_TransformedBoxTrace (vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  int headnode, int brushmask,
						  vec3_t origin, vec3_t angles)
{
	trace_t		trace;
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	qboolean	rotated;

	// subtract origin offset
	VectorSubtract (start, origin, start_l);
	VectorSubtract (end, origin, end_l);

	// rotate start and end into the models frame of reference
	if (headnode != box_headnode && 
	(angles[0] || angles[1] || angles[2]) )
		rotated = true;
	else
		rotated = false;

	if (rotated)
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = -DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		VectorCopy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = -DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}

	// sweep the box through the model
	trace = CM_BoxTrace (start_l, end_l, mins, maxs, headnode, brushmask);

	if (rotated && trace.fraction != 1.0)
	{
		// FIXME: figure out how to do this with existing angles
		VectorNegate (angles, a);
		AngleVectors (a, forward, right, up);

		VectorCopy (trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct (temp, forward);
		trace.plane.normal[1] = -DotProduct (temp, right);
		trace.plane.normal[2] = DotProduct (temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}

#ifdef _WIN32
#pragma optimize( "", on )
#endif



/*
==================
CM_CompleteBoxTrace

Handles all 255 level specific submodels too
==================
*/
trace_t		CM_CompleteBoxTrace (vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs, 
						  int levelmask, int brushmask)
{
	trace_t	newtr, tr;
	int		i;


	tr.fraction = 2.0f;
	for ( i = 0; i < numcheads; i++ )
	{
		if ( chead[i].level && levelmask && !(chead[i].level & levelmask) ) 
			continue;

		newtr = CM_BoxTrace( start, end, mins, maxs, chead[i].cnode, brushmask );

		// memorize the trace with the minimal fraction
		if ( newtr.fraction == 0.0 ) 
			return newtr;
		if ( newtr.fraction < tr.fraction ) 
			tr = newtr;
	}
	return tr;
}



// =====================================================================


/*
==============
MakeTnode

Converts the disk node structure into the efficient tracing structure
==============
*/
void MakeTnode (int nodenum)
{
	tnode_t			*t;
	cplane_t		*plane;
	int				i;
	cnode_t 		*node;
	
	t = tnode_p++;

	node = map_nodes + nodenum;
	plane = node->plane;

	t->type = plane->type;
	VectorCopy (plane->normal, t->normal);
	t->dist = plane->dist;
	
	for (i=0 ; i<2 ; i++)
	{
		if (node->children[i] < 0)
			if ( map_leafs[-node->children[i] - 1].contents & (CONTENTS_SOLID) )
				t->children[i] = 1 | (1<<31);
			else 
				t->children[i] = (1<<31);
		else
		{
			t->children[i] = tnode_p - tnodes;
			MakeTnode (node->children[i]);
		}
	}
			
}


/*
=============
BuildTnode_r
=============
*/
void BuildTnode_r( int node )
{
	if ( !map_nodes[node].plane )
	{
		cnode_t *n;
		tnode_t *t;
		vec3_t	c0maxs, c1mins;
		int		i;

		n = &map_nodes[node];

		// alloc new node
		t = tnode_p++;

		if ( n->children[0] < 0 || n->children[1] < 0 )
			Com_Error( ERR_DROP, "Unexpected leaf" );

		VectorCopy( map_nodes[n->children[0]].maxs, c0maxs );
		VectorCopy( map_nodes[n->children[1]].mins, c1mins );

	//	printf( "(%i %i : %i %i) (%i %i : %i %i)\n", 
	//		(int)dnodes[n->children[0]].mins[0], (int)dnodes[n->children[0]].mins[1], (int)dnodes[n->children[0]].maxs[0], (int)dnodes[n->children[0]].maxs[1],
	//		(int)dnodes[n->children[1]].mins[0], (int)dnodes[n->children[1]].mins[1], (int)dnodes[n->children[1]].maxs[0], (int)dnodes[n->children[1]].maxs[1] );

		for ( i = 0; i < 2; i++ )
			if ( c0maxs[i] <= c1mins[i] )
			{
				// create a separation plane
				t->type = i;
				t->normal[0] = i;
				t->normal[1] = i^1;
				t->normal[2] = 0;
				t->dist = (c0maxs[i] + c1mins[i]) / 2;

				t->children[1] = tnode_p - tnodes;
				BuildTnode_r( n->children[0] );
				t->children[0] = tnode_p - tnodes;
				BuildTnode_r( n->children[1] );			
				return;
			}

		// can't construct such a separation plane
		t->type = PLANE_NONE;

		for ( i = 0; i < 2; i++ ) 
		{
			t->children[i] = tnode_p - tnodes;
			BuildTnode_r( n->children[i] );
		}


	}
	else
	{
		chead[numcheads].cnode = node;
		chead[numcheads].level = cur_level;
		numcheads++;

		MakeTnode( node );
	}
}


/*
=============
CM_MakeTnodes

Loads the node structure out of a .bsp file to be used for light occlusion
=============
*/
void CM_MakeTnodes ( void )
{
	int		i;

	// 32 byte align the structs
	tnodes = malloc( (numnodes+1) * sizeof(tnode_t));
	tnodes = (tnode_t *)(((int)tnodes + 31)&~31);
	tnode_p = tnodes;

	numtheads = 0;
	numcheads = 0;

	for ( i = 0; i < 256; i++ )
	{
		if ( map_cmodels[i].headnode == -1 )
			continue;

		thead[numtheads] = tnode_p - tnodes;
		numtheads++;

		cur_level = i;

		BuildTnode_r( map_cmodels[i].headnode );
//		MakeTnode (map_models[i].headnode);
	}
}


//==========================================================


int TestLine_r (int node, vec3_t start, vec3_t stop)
{
	tnode_t	*tnode;
	float	front, back;
	vec3_t	mid;
	float	frac;
	int		side;
	int		r;

	// leaf node
	if (node & (1<<31)) 
		return node & ~(1<<31);

	tnode = &tnodes[node];
	switch (tnode->type)
	{
	case PLANE_X:
		front = start[0] - tnode->dist;
		back = stop[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = start[1] - tnode->dist;
		back = stop[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = start[2] - tnode->dist;
		back = stop[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestLine_r (tnode->children[0], start, stop);
		if (r)
			return r;
		return TestLine_r (tnode->children[1], start, stop);
		break;
	default:
		front = (start[0]*tnode->normal[0] + start[1]*tnode->normal[1] + start[2]*tnode->normal[2]) - tnode->dist;
		back = (stop[0]*tnode->normal[0] + stop[1]*tnode->normal[1] + stop[2]*tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TestLine_r (tnode->children[0], start, stop);
	
	if (front < ON_EPSILON && back < ON_EPSILON)
		return TestLine_r (tnode->children[1], start, stop);

	side = front < 0;
	
	frac = front / (front-back);

	mid[0] = start[0] + (stop[0] - start[0])*frac;
	mid[1] = start[1] + (stop[1] - start[1])*frac;
	mid[2] = start[2] + (stop[2] - start[2])*frac;

	r = TestLine_r (tnode->children[side], start, mid);
	if (r)
		return r;
	return TestLine_r (tnode->children[!side], mid, stop);
}


int TestLineDist_r (int node, vec3_t start, vec3_t stop)
{
	tnode_t	*tnode;
	float	front, back;
	vec3_t	mid;
	float	frac;
	int		side;
	int		r;

	if (node & (1<<31))
	{
		r = node & ~(1<<31);
		if ( r ) VectorCopy( start, tr_end );
		return r;	// leaf node
	}

	tnode = &tnodes[node];
	switch (tnode->type)
	{
	case PLANE_X:
		front = start[0] - tnode->dist;
		back = stop[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = start[1] - tnode->dist;
		back = stop[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = start[2] - tnode->dist;
		back = stop[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestLineDist_r (tnode->children[0], start, stop);
		if ( r ) VectorCopy( tr_end, mid );
		side = TestLineDist_r (tnode->children[1], start, stop);
		if ( side && r ) 
			if ( VectorNearer( mid, tr_end, start ) )
			{
				VectorCopy( mid, tr_end );
				return r;
			}
			else return side;

		if ( r )
		{
			VectorCopy( mid, tr_end );
			return r;
		}

		return side;
				
		break;
	default:
		front = (start[0]*tnode->normal[0] + start[1]*tnode->normal[1] + start[2]*tnode->normal[2]) - tnode->dist;
		back = (stop[0]*tnode->normal[0] + stop[1]*tnode->normal[1] + stop[2]*tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TestLineDist_r (tnode->children[0], start, stop);
	
	if (front < ON_EPSILON && back < ON_EPSILON)
		return TestLineDist_r (tnode->children[1], start, stop);

	side = front < 0;
	
	frac = front / (front-back);

	mid[0] = start[0] + (stop[0] - start[0])*frac;
	mid[1] = start[1] + (stop[1] - start[1])*frac;
	mid[2] = start[2] + (stop[2] - start[2])*frac;

	r = TestLineDist_r (tnode->children[side], start, mid);
	if (r)
		return r;
	return TestLineDist_r (tnode->children[!side], mid, stop);
}


int CM_TestLine (vec3_t start, vec3_t stop)
{
	int		i;

	for ( i = 0; i < numtheads; i++ )
	{
		// inside cullbox -> do trace
		if ( TestLine_r( thead[i], start, stop ) )
			return 1;

	}
	return 0;
}

int CM_TestLineDM (vec3_t start, vec3_t stop, vec3_t end)
{
	int		i;

	VectorCopy( stop, end );

	for ( i = 0; i < numtheads; i++ )
	{
		if ( TestLineDist_r( thead[i], start, stop ) )
			if ( VectorNearer( tr_end, end, start ) )
				VectorCopy( tr_end, end );
	}

	if ( VectorCompare( end, stop ) ) return 0;
	else return 1;
}

// =====================================================================


/*
===============================================================================

AREAPORTALS

===============================================================================
*/

void FloodArea_r (carea_t *area, int floodnum)
{
	int		i;
	dareaportal_t	*p;

	if (area->floodvalid == floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Com_Error (ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	p = &map_areaportals[area->firstareaportal];
	for (i=0 ; i<area->numareaportals ; i++, p++)
	{
		if (portalopen[p->portalnum])
			FloodArea_r (&map_areas[p->otherarea], floodnum);
	}
}

/*
====================
FloodAreaConnections


====================
*/
void	FloodAreaConnections (void)
{
	int		i;
	carea_t	*area;
	int		floodnum;

	// all current floods are now invalid
	floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for (i=1 ; i<numareas ; i++)
	{
		area = &map_areas[i];
		if (area->floodvalid == floodvalid)
			continue;		// already flooded into
		floodnum++;
		FloodArea_r (area, floodnum);
	}

}

void	CM_SetAreaPortalState (int portalnum, qboolean open)
{
	if (portalnum > numareaportals)
		Com_Error (ERR_DROP, "areaportal > numareaportals");

	portalopen[portalnum] = open;
	FloodAreaConnections ();
}

qboolean	CM_AreasConnected (int area1, int area2)
{
	if (map_noareas->value)
		return true;

	if (area1 > numareas || area2 > numareas)
		Com_Error (ERR_DROP, "area > numareas");

	if (map_areas[area1].floodnum == map_areas[area2].floodnum)
		return true;
	return false;
}


/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits (byte *buffer, int area)
{
	int		i;
	int		floodnum;
	int		bytes;

	bytes = (numareas+7)>>3;

	if (map_noareas->value)
	{	// for debugging, send everything
		memset (buffer, 255, bytes);
	}
	else
	{
		memset (buffer, 0, bytes);

		floodnum = map_areas[area].floodnum;
		for (i=0 ; i<numareas ; i++)
		{
			if (map_areas[i].floodnum == floodnum || !area)
				buffer[i>>3] |= 1<<(i&7);
		}
	}

	return bytes;
}


/*
===================
CM_WritePortalState

Writes the portal state to a savegame file
===================
*/
void	CM_WritePortalState (FILE *f)
{
	fwrite (portalopen, sizeof(portalopen), 1, f);
}

/*
===================
CM_ReadPortalState

Reads the portal state from a savegame file
and recalculates the area connections
===================
*/
void	CM_ReadPortalState (FILE *f)
{
	FS_Read (portalopen, sizeof(portalopen), f);
	FloodAreaConnections ();
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
qboolean CM_HeadnodeVisible (int nodenum, byte *visbits)
{
	int		leafnum;
	int		cluster;
	cnode_t	*node;

	if (nodenum < 0)
	{
		leafnum = -1-nodenum;
		cluster = map_leafs[leafnum].cluster;
		if (cluster == -1)
			return false;
		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return true;
		return false;
	}

	node = &map_nodes[nodenum];
	if (CM_HeadnodeVisible(node->children[0], visbits))
		return true;
	return CM_HeadnodeVisible(node->children[1], visbits);
}



//====================================================================================

/*
==========================================================

  GRID ORIENTED MOVEMENT AND SCANNING
  
==========================================================
*/

#define QUANT		4

byte area[WIDTH][WIDTH][HEIGHT];
byte areaStored[WIDTH][WIDTH][HEIGHT];

byte tf, stf;

byte	**fblist;
int		fblength;


/*
============
Grid_MoveMark
============
*/
void Grid_MoveMark( int x, int y, int z, int dx, int dy, int h, int l )
{
	int		nx, ny, sh, i;
	byte	**p;

	// range check
	if ( dx > 0 && x >= WIDTH-1 ) return;
	if ( dy > 0 && y >= WIDTH-1 ) return;
	if ( dx < 0 && x <= 0 ) return;
	if ( dy < 0 && y <= 0 ) return;

	// connection checks
	if ( dx > 0 && !(map_route[z][y][x] & 0x10) ) return;
	if ( dx < 0 && !(map_route[z][y][x] & 0x20) ) return;
	if ( dy > 0 && !(map_route[z][y][x] & 0x40) ) return;
	if ( dy < 0 && !(map_route[z][y][x] & 0x80) ) return;

	nx = x+dx;
	ny = y+dy;

	// height checks
	sh = (map_step[y][x] & (1<<z)) ? sh_big : sh_low;
	z += (h + sh) / 0x10;
	while ( map_fall[ny][nx] & (1<<z) ) z--;

	// is it better than ever?
	if ( (area[nx][ny][z] & 0x3F) < l ) 
		return;

	// test for forbidden areas
	for ( i = 0, p = fblist; i < fblength; i++, p++ )
		if ( nx == (*p)[0] && ny == (*p)[1] && z == (*p)[2] )
			return;

	// test for diagonal move
	if ( area[nx][ny][z] == (l | stf) )
	{
		// suppose it's possible
		area[nx][ny][z] = (l-1) | stf;
		if ( nx > 0       && ny > 0       && area[nx-1][ny-1][z] == l-4 && area[nx-1][ny][z] == l-2 && area[nx][ny-1][z] == l-2 ) return;
		if ( nx < WIDTH-1 && ny > 0       && area[nx+1][ny-1][z] == l-4 && area[nx+1][ny][z] == l-2 && area[nx][ny-1][z] == l-2 ) return;
		if ( nx > 0       && ny < WIDTH-1 && area[nx-1][ny+1][z] == l-4 && area[nx-1][ny][z] == l-2 && area[nx][ny+1][z] == l-2 ) return;
		if ( nx < WIDTH-1 && ny < WIDTH-1 && area[nx+1][ny+1][z] == l-4 && area[nx+1][ny][z] == l-2 && area[nx][ny+1][z] == l-2 ) return;
		// isn't possible... fall back
	} 

	// standard move
	area[nx][ny][z] = l | stf;
}



/*
============
Grid_MoveMarkRoute
============
*/
void Grid_MoveMarkRoute( int xl, int yl, int xh, int yh )
{
	int x, y, z, h;
	int l;

	for ( z = 0; z < HEIGHT; z++ )
		for ( y = yl; y < yh; y++ )
			for ( x = xl; x < xh; x++ )
				if ( (area[x][y][z] & (0x40|0x80)) == tf ) 
				{
					// reset test flags
					l = area[x][y][z] &= 0x3F;
					h = map_route[z][y][x] & 0xF;

					// check for end
					if ( l+2 >= MAX_MOVELENGTH ) continue;

					// test the next connections
					Grid_MoveMark( x, y, z, +1, 0, h, l+2 );
					Grid_MoveMark( x, y, z, -1, 0, h, l+2 );
					Grid_MoveMark( x, y, z, 0, +1, h, l+2 );
					Grid_MoveMark( x, y, z, 0, -1, h, l+2 );
				}
}


/*
============
Grid_MoveCalc
============
*/
void Grid_MoveCalc( pos3_t from, int distance, byte **fb_list, int fb_length )
{
	int xl, xh, yl, yh;
	int i;

	// reset move data
	memset( area, 0x3F, WIDTH*WIDTH*HEIGHT );
	fblist = fb_list;
	fblength = fb_length;

	// set test flags
	tf = 0x40; stf = tf ^ (0x40|0x80);

	// set starting point
	area[from[0]][from[1]][from[2]] = tf;

	xl = xh = from[0];
	yl = yh = from[1];
	
	if ( distance > MAX_ROUTE ) distance = MAX_ROUTE;

	for ( i = 0; i < distance; i++ )
	{
		// go on checking
		if ( xl > 0 ) xl--;
		if ( yl > 0 ) yl--;
		if ( xh < WIDTH-1 ) xh++;
		if ( yh < WIDTH-1 ) yh++;

		Grid_MoveMarkRoute( xl, yl, xh, yh );

		// swap the test flag
		stf = tf;
		tf ^= (0x40|0x80);
	}
}


/*
============
Grid_MoveStore
============
*/
void Grid_MoveStore( void )
{
	memcpy( areaStored, area, WIDTH*WIDTH*HEIGHT );
}


/*
============
Grid_MoveLength

returns:
0x3F if the move isn't possible
length of move otherwise
============
*/
int Grid_MoveLength( pos3_t to, qboolean stored )
{
	if ( !stored ) return (area[to[0]][to[1]][to[2]] & 0x3F);
	else return (areaStored[to[0]][to[1]][to[2]] & 0x3F);
}


/*
============
Grid_MoveCheck
============
*/
int Grid_MoveCheck( pos3_t pos, int sz, int l )
{
	int x, y, sh;
	int	dv, dx, dy;
	int z;

	for ( dv = 0; dv < 8; dv++ )
	{
		dx = dvecs[dv][0];
		dy = dvecs[dv][1];

		// range check
		if ( dx > 0 && pos[0] >= WIDTH-1 ) continue;
		if ( dy > 0 && pos[1] >= WIDTH-1 ) continue;
		if ( dx < 0 && pos[0] <= 0 ) continue;
		if ( dy < 0 && pos[1] <= 0 ) continue;

		// distance table check
		x = pos[0]+dx;
		y = pos[1]+dy;
		z = sz;

		if ( area[x][y][z] != (l-2) - (dv>3) ) continue;

		// connection checks
		if ( dx < 0 && !(map_route[z][y][x] & 0x10) ) continue;
		if ( dx > 0 && !(map_route[z][y][x] & 0x20) ) continue;
		if ( dy < 0 && !(map_route[z][y][x] & 0x40) ) continue;
		if ( dy > 0 && !(map_route[z][y][x] & 0x80) ) continue;

		// height checks
		sh = (map_step[y][x] & (1<<z)) ? sh_big : sh_low;
		z += ((map_route[z][y][x] & 0xF) + sh) / 0x10;
		while ( map_fall[pos[1]][pos[0]] & (1<<z) ) z--;

		//	Com_Printf( "pos: (%i %i %i) (x,y,z): (%i %i %i)\n", pos[0], pos[1], pos[2], x, y, z );

		if ( pos[2] == z ) break; // found it!

		// not found... try next dv
	}

	return dv;
}



/*
============
Grid_MoveNext (Last?)
============
*/
int Grid_MoveNext( pos3_t from )
{
	int		l, x, y, z, dv;

	l = (area[from[0]][from[1]][from[2]] & 0x3F);

	// finished
	if ( !l ) return 0xFF;

	// initialize tests
	x = from[0]; y = from[1];

	// do tests
	for ( z=0; z<HEIGHT; z++) 
	{
		// suppose it's possible at that height
		dv = Grid_MoveCheck( from, z, l );
		if ( dv < 8 ) return dv | (z<<3);
	}

	// shouldn't happen
	Com_Printf( "failed...\n" );
	return 0xFF;
}


/*
============
Grid_Height
============
*/
int Grid_Height( pos3_t pos )
{
	return (map_route[pos[2]][pos[1]][pos[0]] & 0x0F) * QUANT;
}


/*
============
Grid_PosToVec
============
*/
void Grid_PosToVec( pos3_t pos, vec3_t vec )
{
	PosToVec( pos, vec );
	vec[2] += Grid_Height( pos );
}
