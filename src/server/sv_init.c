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

#include "server.h"

server_static_t	svs;				// persistant server info
server_t		sv;					// local server

/*
================
SV_FindIndex

================
*/
int SV_FindIndex (char *name, int start, int max, qboolean create)
{
	int		i;

	if (!name || !name[0])
		return 0;

	for (i=1 ; i<max && sv.configstrings[start+i][0] ; i++)
		if (!strcmp(sv.configstrings[start+i], name))
			return i;

	if (!create)
		return 0;

	if (i == max)
		Com_Error (ERR_DROP, "*Index: overflow");

	strncpy (sv.configstrings[start+i], name, sizeof(sv.configstrings[i]));

	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_WriteChar (&sv.multicast, svc_configstring);
		MSG_WriteShort (&sv.multicast, start+i);
		MSG_WriteString (&sv.multicast, name);
		SV_Multicast ( ~0 );
	}

	return i;
}


int SV_ModelIndex (char *name)
{
	return SV_FindIndex (name, CS_MODELS, MAX_MODELS, true);
}

int SV_SoundIndex (char *name)
{
	return SV_FindIndex (name, CS_SOUNDS, MAX_SOUNDS, true);
}

int SV_ImageIndex (char *name)
{
	return SV_FindIndex (name, CS_IMAGES, MAX_IMAGES, true);
}


/*
================
MAP ASSEMBLY
================
*/
#define MAX_MAPASSEMBLIES 32
#define MAX_TILETYPES 64
#define MAX_TILESIZE 10
#define MAX_TILEALTS 8
#define MAX_FIXEDTILES 32
#define MAX_REGIONSIZE 32

#define MAX_ASSEMBLYRETRIES 8000
#define MAX_REGIONRETRIES 1

typedef struct mTile_s
{
	char	name[MAX_VAR];
	byte	spec[MAX_TILESIZE][MAX_TILESIZE][MAX_TILEALTS];
	int		w, h;
} mTile_t;

typedef struct mAssembly_s
{
	char	name[MAX_VAR];
	byte	min[MAX_TILETYPES];
	byte	max[MAX_TILETYPES];
	byte	fT[MAX_FIXEDTILES];
	byte	fX[MAX_FIXEDTILES];
	byte	fY[MAX_FIXEDTILES];
	int		numFixed;
	int		w, h;
} mAssembly_t;

typedef struct mPlaced_s
{
	mTile_t	*tile;
	int		x, y;
} mPlaced_t;

mTile_t		mTile[MAX_TILETYPES];
mAssembly_t	mAssembly[MAX_MAPASSEMBLIES];

int numTiles;
int numAssemblies;

mPlaced_t	mPlaced[MAX_MAPTILES];
int numPlaced;

short prList[32*32];
short trList[MAX_TILETYPES];

mAssembly_t *mAsm;
int mapSize;
int mapX, mapY, mapW, mapH;

/*
================
RandomList
================
*/
void RandomList( int n, short *list )
{
	short i, r, t;

	for ( i = 0; i < n; i++ ) list[i] = i;

	for ( i = 0; i < n; i++ )
	{
		r = i + (n-i)*frand();
		t = list[r];
		list[r] = list[i];
		list[i] = t;
	}
}


/*
================
SV_ParseMapTile
================
*/
#define SOLID 255

byte tileChar( char chr )
{
	if ( chr == '+' ) return SOLID;
	else if ( chr >= '0' && chr <= '9' ) return chr - '0';
	else if ( chr >= 'a' && chr <= 'z' ) return chr - ('a' + 10);
	else if ( chr >= 'A' && chr <= 'Z' ) return chr - ('A' + 'z'-'a' + 11);
	return 0;
}

void SV_ParseMapTile( char *filename, char **text )
{
	char *errhead = "SV_ParseMapTile: Unexpected end of file (";
	char *token, *chr;
	mTile_t *t;
	int x, y, i;

	// get tile name
	token = COM_EParse( text, errhead, filename );
	if ( !*text ) return;
	if ( numTiles >= MAX_TILETYPES )
	{
		Com_Printf( "SV_ParseMapTile: Too many map tile types (%s)\n", filename );
		return;
	}
	t = &mTile[numTiles++];
	memset( t, 0, sizeof( mTile_t ) );
	strncpy( t->name, token, MAX_VAR );

	// start parsing the block
	token = COM_EParse( text, errhead, filename );
	if ( !*text ) return;
	if ( *token != '{' )
	{
		Com_Printf( "SV_ParseMapTile: Expected '{' for tile '%s' (%s)\n", filename );
		return;
	}

	// get width and height
	token = COM_EParse( text, errhead, filename );
	if ( !*text ) return;
	t->w = atoi( token );

	token = COM_EParse( text, errhead, filename );
	if ( !*text ) return;
	t->h = atoi( token );

	if ( t->w > MAX_TILESIZE || t->h > MAX_TILESIZE )
	{
		Com_Printf( "SV_ParseMapTile: Bad tile size [%i %i] (%s) (max. [%i %i])\n", t->w, t->h, filename, MAX_TILESIZE, MAX_TILESIZE );
		*text = strchr( *text, '}' );
		return;
	}

	// get tile specs
	for ( y = t->h-1; y >= 0; y-- )
		for ( x = 0; x < t->w; x++ )
		{
			token = COM_EParse( text, errhead, filename );
			if ( !*text ) return;
			chr = (char *)&t->spec[y][x][0];
			for ( i = 0; token[i] && i < MAX_TILEALTS; i++, chr++ )
				*chr = tileChar( token[i] );
		}

	// get connections
	*text = strchr( *text, '}' ) + 1;
}


/*
================
SV_ParseAssembly
================
*/
void SV_ParseAssembly( char *filename, char **text )
{
	char *errhead = "SV_ParseAssembly: Unexpected end of file (";
	char *token;
	mAssembly_t *a;
	int i, x, y;

	// get assembly name
	token = COM_EParse( text, errhead, filename );
	if ( !*text ) return;
	if ( numAssemblies >= MAX_MAPASSEMBLIES )
	{
		Com_Printf( "SV_ParseAssembly: Too many map assemblies (%s)\n", filename );
		return;
	}

	// init
	a = &mAssembly[numAssemblies++];
	memset( a, 0, sizeof( mAssembly_t ) );
	strncpy( a->name, token, MAX_VAR );
	a->w = 8; a->h = 8;

	do {
		// get tile name
		token = COM_EParse( text, errhead, filename );
		if ( !text || *token == '}' ) break;

		if ( !strcmp( token, "size" ) )
		{
			// get map size
			token = COM_EParse( text, errhead, filename );
			if ( !text ) break;

			sscanf( token, "%i %i", &a->w, &a->h );
			continue;
		}

		if ( !strcmp( token, "fix" ) )
		{
			// get tile
			token = COM_EParse( text, errhead, filename );
			if ( !text ) break;

			for ( i = 0; i < numTiles; i++ )
				if ( !strcmp( token, mTile[i].name ) )
				{
					if ( a->numFixed >= MAX_FIXEDTILES )
					{
						Com_Printf( "SV_ParseAssembly: Too many fixed tiles\n" );
						break;
					}
					// get coordinates
					token = COM_EParse( text, errhead, filename );
					if ( !text ) break;

					sscanf( token, "%i %i", &x, &y );
					a->fX[a->numFixed] = x;
					a->fY[a->numFixed] = y;
					a->fT[a->numFixed] = i;
					a->numFixed++;
				}
			continue;
		}

		for ( i = 0; i < numTiles; i++ )
			if ( !strcmp( token, mTile[i].name ) )
			{
				// get min and max tile number
				token = COM_EParse( text, errhead, filename );
				if ( !text ) break;

				sscanf( token, "%i %i", &x, &y );
				a->min[i] = x;
				a->max[i] = y;
				break;
			}
	}
	while ( text );
}


/*
================
SV_AddTile
================
*/
void SV_AddTile( byte map[32][32][MAX_TILEALTS], mTile_t *tile, int x, int y, int *toFill )
{
	qboolean bad;
	int tx, ty;
	int a, b, c;

	// add the new tile
	for ( ty = 0; ty < tile->h; ty++ )
		for ( tx = 0; tx < tile->w; tx++ )
		{
			if ( tile->spec[ty][tx][0] == SOLID || !map[y+ty][x+tx][0] )
			{
				// copy the solid info
				if ( tile->spec[ty][tx][0] == SOLID && toFill ) *toFill -= 1;
				for ( a = 0; a < MAX_TILEALTS; a++ )
					map[y+ty][x+tx][a] = tile->spec[ty][tx][a];
			}
			else if ( tile->spec[ty][tx][0] && map[y+ty][x+tx][0] != SOLID )
			{
				// calc remaining connection options
				for ( a = 0; map[y+ty][x+tx][a] && a < MAX_TILEALTS; a++ )
				{
					bad = true;
					for ( b = 0; bad && tile->spec[ty][tx][b] && b < MAX_TILEALTS; b++ )
						if ( tile->spec[ty][tx][b] == map[y+ty][x+tx][a] )
							bad = false;

					if ( bad )
					{
						// not an option anymore
						for ( c = a+1; c < MAX_TILEALTS; c++ )
							map[y+ty][x+tx][c-1] = map[y+ty][x+tx][c];
						map[y+ty][x+tx][MAX_TILEALTS-1] = 0;
						a--;
					}
				}
			}
		}

	// add the tile
	if ( numPlaced >= MAX_MAPTILES )
		Com_Error( ERR_DROP, "SV_AddTile: Too many map tiles\n" );

	mPlaced[numPlaced].tile = tile;
	mPlaced[numPlaced].x = x;
	mPlaced[numPlaced].y = y;
	numPlaced++;
}


/*
================
SV_FitTile
================
*/
qboolean SV_FitTile( byte map[32][32][MAX_TILEALTS], mTile_t *tile, int x, int y, qboolean force )
{
	qboolean touch;
	int tx, ty;
	int a, b;
	byte *spec;
	byte *m;

	// check for map border
	if ( x + tile->w > mapX + mapW + 2 || y + tile->h > mapY + mapH + 2 )
		return false;

	// require touching tiles
	if ( x == mapX ) touch = true;
	else touch = false;

	// test for fit
	spec = &tile->spec[0][0][0];
	m = &map[y][x][0];
	for ( ty = 0; ty < tile->h; ty++ )
	{
		for ( tx = 0; tx < tile->w; tx++, spec += MAX_TILEALTS, m += MAX_TILEALTS )
		{
			if ( *spec == SOLID && *m == SOLID )
			{
				// already something there
				return false;
			}
			else if ( *spec && *m )
			{
				// check connection, avoid contradictory connections
				if ( *m == SOLID ) touch = true;

				for ( a = 0; spec[a] && a < MAX_TILEALTS; a++ )
					for ( b = 0; m[b] && b < MAX_TILEALTS; b++ )
						if ( spec[a] == m[b] )
							goto fine;

				// not jumped => impossible
				return false;
			}
			fine:;
		}
		spec += MAX_TILEALTS * (MAX_TILESIZE - tile->w);
		m += MAX_TILEALTS * (32 - tile->w);
	}

	// it fits, check for touch
	if ( touch || !force ) return true;
	else return false;
}


/*
================
SV_AddRegion
================
*/
qboolean SV_AddRegion( byte map[32][32][MAX_TILEALTS], byte *num )
{
	mTile_t *tile;
	int i, j, x, y;
	int toFill, oldToFill, oldNumPlaced;
	int pos, lastPos;
//	byte oldMap[32][32][MAX_TILEALTS];
//	byte oldNum[MAX_TILETYPES];
//	int tries;

	// store old values
	oldNumPlaced = numPlaced;
//	memcpy( &oldMap[0][0][0], map, sizeof( map ) );
//	memcpy( &oldNum[0], num, sizeof( num ) );

	// count tiles to fill in
	oldToFill = 0;
	for ( y = mapY+1; y < mapY+mapH+1; y++ )
		for ( x = mapX+1; x < mapX+mapW+1; x++ )
			if ( map[y][x][0] != SOLID ) oldToFill++;

//	for ( tries = 0; tries < MAX_REGIONRETRIES; tries++ )
	{
		// restore old values
		toFill = oldToFill;
		numPlaced = oldNumPlaced;
//		memcpy( &map[0][0][0], oldMap, sizeof( map ) );
//		memcpy( &num[0], oldNum, sizeof( num ) );

		RandomList( mapSize, prList );
		lastPos = mapSize-1;
		pos = 0;

		// finishing condition
		while ( toFill > 0 )
		{
			// refresh random lists
			RandomList( numTiles, trList );

			while ( pos != lastPos )
			{
				x = prList[pos] % mapW + mapX;
				y = prList[pos] / mapW + mapY;

				for ( i = 0; i < numTiles; i++ )
				{
					j = trList[i];
					if ( num[j] >= mAsm->max[j] )
						continue;
					tile = &mTile[j];

					// add the tile, if it fits
					if ( SV_FitTile( map, tile, x, y, true ) )
					{
						// mark as used and add the tile
						num[j]++;
						SV_AddTile( map, tile, x, y, &toFill );
						lastPos = pos;
						break;
					}
				}
				pos++;
				if ( pos >= mapSize ) pos = 0;

				if ( i < numTiles ) break;
			}
			if ( pos == lastPos ) break;
		}
		// check for success
		if ( toFill <= 0 ) return true;
	}

	// too many retries
	return false;
}


/*
================
SV_AddMandatoryParts
================
*/
qboolean SV_AddMandatoryParts( byte map[32][32][MAX_TILEALTS], byte *num )
{
	mTile_t *tile;
	int i, j, n, x, y;

	for ( i = 0, tile = mTile; i < numTiles; i++, tile++ )
	{
		for ( j = 0; j < mAsm->min[i]; j++ )
		{
			RandomList( mapSize, prList );
			for ( n = 0; n < mapSize; n++ )
			{
				x = prList[n] % mapW;
				y = prList[n] / mapH;
				if ( SV_FitTile( map, tile, x, y, false ) )
				{
					// add tile
					SV_AddTile( map, tile, x, y, NULL );
					break;
				}
			}
			if ( n >= mapSize ) return false;
		}
		num[i] = mAsm->min[i];
	}
	// success
	return true;
}


/*
================
SV_AssembleMap
================
*/
void SV_AssembleMap( char *name, char *assembly, char **map, char **pos )
{
	mPlaced_t	*pl;
	byte curMap[32][32][MAX_TILEALTS];
	byte curNum[MAX_TILETYPES];
	char asmMap[MAX_TOKEN_CHARS*MAX_TILESTRINGS];
	char asmPos[MAX_TOKEN_CHARS*MAX_TILESTRINGS];
	char filename[MAX_QPATH];
	char basePath[MAX_QPATH];
	char *buf;
	char *text, *token;
	int i, tries;
	int regNumX, regNumY;
	float regFracX, regFracY;
	qboolean ok;

	// load the map info
	sprintf( filename, "maps/%s.ump", name );
	FS_LoadFile( filename, (void **)&buf );
	if ( !buf )
		Com_Error( ERR_DROP, "SV_AssembleMap: Map assembly info '%s' not found\n", filename );

	// parse it
	text = buf;
	numTiles = 0;
	numAssemblies = 0;
	basePath[0] = 0;
	do {
		token = COM_Parse( &text );
		if ( !text ) break;

		if ( !strcmp( token, "base" ) )
		{
			token = COM_Parse( &text );
			strncpy( basePath, token, MAX_QPATH );
		}
		else if ( !strcmp( token, "tile" ) ) SV_ParseMapTile( filename, &text );
		else if ( !strcmp( token, "assembly" ) ) SV_ParseAssembly( filename, &text );
		else if ( !strcmp( token, "{" ) )
		{
			// ignore unknown block
			text = strchr( text, '}' ) + 1;
			if ( !text ) break;
		}
		else Com_Printf( "SV_AssembleMap: Unknown token '%s' (%s)\n", token, filename );
	} while ( text );

	// free the file
	FS_FreeFile( buf );

	// check for parsed tiles and assemblies
	if ( !numTiles )
		Com_Error( ERR_DROP, "No map tiles defined (%s)!\n", filename );
	if ( !numAssemblies )
		Com_Error( ERR_DROP, "No map assemblies defined (%s)!\n", filename );

	// get assembly
	if ( assembly && assembly[0] )
	{
		for ( i = 0, mAsm = mAssembly; i < numAssemblies; i++, mAsm++ )
			if ( !strcmp( assembly, mAsm->name ) )
				break;
		if ( i >= numAssemblies )
		{
			Com_Printf( "SV_AssembleMap: Map assembly '%s' not found\n", assembly );
			mAsm = NULL;
		}
	}
	else mAsm = NULL;

	// use random assembly, if no valid one has been specified
	if ( !mAsm ) mAsm = &mAssembly[(int)(frand()*numAssemblies)];

	// calculate regions
	regNumX = mAsm->w / MAX_REGIONSIZE + 1;
	regNumY = mAsm->h / MAX_REGIONSIZE + 1;
	regFracX = (float)mAsm->w / regNumX;
	regFracY = (float)mAsm->h / regNumY;

	for ( tries = 0; tries < MAX_ASSEMBLYRETRIES; tries++ )
	{
		int x, y;

		// assemble the map
		numPlaced = 0;
		memset( &curMap[0][0][0], 0, sizeof( curMap ) );
		memset( &curNum[0], 0, sizeof( curNum ) );

		// place fixed parts
		for ( i = 0; i < mAsm->numFixed; i++ )
			SV_AddTile( curMap, &mTile[mAsm->fT[i]], mAsm->fX[i], mAsm->fY[i], NULL );

		// place mandatory parts
		mapX = 0;
		mapY = 0;
		mapW = mAsm->w;
		mapH = mAsm->h;
		mapSize = mAsm->w * mAsm->h;
		if ( !SV_AddMandatoryParts( curMap, curNum ) ) continue;

		// start region assembly
		ok = true;
		for ( y = 0; y < regNumY && ok; y++ )
			for ( x = 0; x < regNumX && ok; x++ )
			{
				mapX = x * regFracX;
				mapY = y * regFracY;
				mapW = (int)((x+1) * regFracX + 0.1) - (int)(x * regFracX);
				mapH = (int)((y+1) * regFracY + 0.1) - (int)(y * regFracY);
				mapSize = mapW * mapH;
				if ( !SV_AddRegion( curMap, curNum ) ) ok = false;
			}

		// break if everything seems to be ok
		if ( ok ) break;
	}

	if ( tries >= MAX_ASSEMBLYRETRIES )
		Com_Error( ERR_DROP, "SV_AssembleMap: Failed to assemble map (%s)\n", filename );

	// prepare map and pos strings
	if ( basePath[0] )
	{
		asmMap[0] = '-';
		strncpy( &asmMap[1], basePath, MAX_QPATH );
		*map = asmMap;
	}
	else
	{
		asmMap[0] = 0;
		*map = asmMap+1;
	}
	asmPos[0] = 0;
	*pos = asmPos+1;

	// generate the strings
	for ( i = 0, pl = mPlaced; i < numPlaced; i++, pl++ )
	{
		strncat( asmMap, va( " %s", pl->tile->name ), MAX_TOKEN_CHARS*MAX_TILESTRINGS );
		strncat( asmPos, va( " %i %i", (pl->x - mAsm->w/2)*8, (pl->y - mAsm->h/2)*8 ), MAX_TOKEN_CHARS*MAX_TILESTRINGS );
	}

//	Com_Printf( "tiles: %s\n", *map );
//	Com_Printf( "pos: %s\n", *pos );
	Com_Printf( "tiles: %i tries: %i\n", numPlaced, tries+1 );
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
void SV_SpawnServer (char *server, char *param, server_state_t serverstate, qboolean attractloop, qboolean loadgame)
{
	int			i;
	unsigned	checksum = 0;

	if (attractloop)
		Cvar_Set ("paused", "0");

	Com_Printf ("------- Server Initialization -------\n");

	Com_DPrintf ("SpawnServer: %s\n",server);
	if (sv.demofile)
		fclose (sv.demofile);

	svs.spawncount++;		// any partially connected client will be
							// restarted
	sv.state = ss_dead;
	Com_SetServerState (sv.state);

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));
	svs.realtime = 0;
	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	// save name for levels that don't set message
	strcpy (sv.configstrings[CS_NAME], server);

	SZ_Init (&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

	strcpy (sv.name, server);

	// leave slots at start for clients only
	for (i=0 ; i<sv_maxclients->value ; i++)
	{
		// needs to reconnect
		if (svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	sv.time = 1000;

	strcpy (sv.name, server);
	strcpy (sv.configstrings[CS_NAME], server);

	if ( serverstate == ss_game )
	{
		char *map, *pos;

		// assemble and load the map
		if ( server[0] == '+' ) SV_AssembleMap( server+1, param, &map, &pos );
		else
		{
			map = server;
			pos = param;
		}

		strncpy( sv.configstrings[CS_TILES], map, MAX_TOKEN_CHARS*MAX_TILESTRINGS );
		if ( pos ) strncpy( sv.configstrings[CS_POSITIONS], pos, MAX_TOKEN_CHARS*MAX_TILESTRINGS );
		else sv.configstrings[CS_POSITIONS][0] = 0;

		CM_LoadMap( map, pos );
	}
	else
	{
		// fix this! what here?
	}

	Com_sprintf (sv.configstrings[CS_MAPCHECKSUM],sizeof(sv.configstrings[CS_MAPCHECKSUM]),
		"%i", checksum);

	//
	// clear physics interaction links
	//
	SV_ClearWorld ();

	// fix this!
	for ( i=1; i <= CM_NumInlineModels(); i++)
		sv.models[i] = CM_InlineModel( va( "*%i", i ) );

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	Com_SetServerState (sv.state);

	// load and spawn all other entities
	ge->SpawnEntities ( sv.name, CM_EntityString() );

	// all precaches are complete
	sv.state = serverstate;
	Com_SetServerState (sv.state);

	// set serverinfo variable
	Cvar_FullSet ("mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	Com_Printf ("-------------------------------------\n");
}

/*
==============
SV_InitGame

A brand new game has been started
==============
*/
void SV_InitGame (void)
{
//	edict_t	*ent;
	char	idmaster[32];

	if (svs.initialized)
	{
		// cause any connected clients to reconnect
		SV_Shutdown ("Server restarted\n", true);
	}
	else
	{
		// make sure the client is down
		CL_Drop ();
		SCR_BeginLoadingPlaque ();
	}

	// get any latched variable changes (maxclients, etc)
	Cvar_GetLatchedVars ();

	svs.initialized = true;

//	Cvar_FullSet ("maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);

	svs.spawncount = rand();
	svs.clients = Z_Malloc (sizeof(client_t)*sv_maxclients->value);
	svs.num_client_entities = sv_maxclients->value*UPDATE_BACKUP*64;

	// init network stuff
	NET_Config ( (sv_maxclients->value > 1) );

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999;		// send immediately
	Com_sprintf(idmaster, sizeof(idmaster), "192.246.40.37:%i", PORT_MASTER);
	NET_StringToAdr (idmaster, &master_adr[0]);

	// init game
	SV_InitGameProgs ();
}


/*
======================
SV_Map

  the full syntax is:

  map [*]<map>$<startspot>+<nextserver>

command from the console or progs.
Map can also be a.cin, .pcx, or .dm2 file
Nextserver is used to allow a cinematic to play, then proceed to
another level:

	map tram.cin+jail_e3
======================
*/
void SV_Map (qboolean attractloop, char *levelstring, qboolean loadgame)
{
	char	level[MAX_QPATH];
	char	*ch;
	int		l;

	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	if (sv.state == ss_dead && !sv.loadgame)
		SV_InitGame ();	// the game is just starting

	strcpy (level, levelstring);

	// if there is a + in the map, set nextserver to the remainder
	ch = strstr(level, "+");
	if (ch)
	{
		*ch = 0;
			Cvar_Set ("nextserver", va("gamemap \"%s\"", ch+1));
	}
	else
		Cvar_Set ("nextserver", "");

	//ZOID special hack for end game screen in coop mode
	if (Cvar_VariableValue ("coop") && !Q_stricmp(level, "victory.pcx"))
		Cvar_Set ("nextserver", "gamemap \"*base1\"");

	// skip the end-of-unit flag if necessary
	if (level[0] == '*')
		strcpy (level, level+1);

	l = strlen(level);
	if (l > 4 && !strcmp (level+l-4, ".cin") )
	{
		SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, NULL, ss_cinematic, attractloop, loadgame);
	}
	else if (l > 4 && !strcmp (level+l-4, ".dm2") )
	{
		SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, NULL, ss_demo, attractloop, loadgame);
	}
	else if (l > 4 && !strcmp (level+l-4, ".pcx") )
	{
		SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, NULL, ss_pic, attractloop, loadgame);
	}
	else
	{
		SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, NULL, ss_game, attractloop, loadgame);
		Cbuf_CopyToDefer ();
	}

	SV_BroadcastCommand ("reconnect\n");
}
