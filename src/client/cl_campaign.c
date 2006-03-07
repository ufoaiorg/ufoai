// cl_campaign.c -- single player campaign control

#include "client.h"

// public vars
mission_t	missions[MAX_MISSIONS];
int		numMissions;
actMis_t	*selMis;

campaign_t	campaigns[MAX_CAMPAIGNS];
int		numCampaigns = 0;

stageSet_t	stageSets[MAX_STAGESETS];
stage_t		stages[MAX_STAGES];
int		numStageSets = 0;
int		numStages = 0;

campaign_t	*curCampaign;
ccs_t		ccs;
base_t		*baseCurrent;

int		mapAction;
int		gameTimeScale;

byte		*maskPic;
int		maskWidth, maskHeight;


/*
============================================================================

Boolean expression parser

============================================================================
*/

enum {
	BEPERR_NONE,
	BEPERR_KLAMMER,
	BEPERR_NOEND,
	BEPERR_NOTFOUND
} BEPerror;

char varName[MAX_VAR];

qboolean (*varFunc)( char *var );

qboolean CheckOR( char **s );
qboolean CheckAND( char **s );

static void SkipWhiteSpaces( char **s ) {
	while ( **s == ' ' ) (*s)++;
}

static void NextChar( char **s ) {
	(*s)++;
	// skip white-spaces too
	SkipWhiteSpaces( s );
}

static char *GetSwitchName( char **s ) {
	int	pos = 0;

	while ( **s > 32 && **s != '^' && **s != '|' && **s != '&' && **s != '!' && **s != '(' && **s != ')' ) {
		varName[ pos++ ] = **s;
		(*s)++;
	}
	varName[ pos ] = 0;

	return varName;
}

qboolean CheckOR( char **s ) {
	qboolean result = false;
	int		goon = 0;

	SkipWhiteSpaces( s );
	do {
		if ( goon == 2 ) result ^= CheckAND( s );
		else result |= CheckAND( s );

		if ( **s == '|' ) {
			goon = 1;
			NextChar( s );
		} else if ( **s == '^' ) {
			goon = 2;
			NextChar( s );
		} else {
			goon = 0;
		}
	} while ( goon && !BEPerror );

	return result;
}

qboolean CheckAND( char **s ) {
	qboolean result = true;
	qboolean negate = false;
	qboolean goon = false;
	int value;

	do {
		while ( **s == '!' ) {
			negate ^= true;
			NextChar( s );
		}
		if ( **s == '(' ) {
			NextChar( s );
			result &= CheckOR( s ) ^ negate;
			if ( **s != ')' ) BEPerror = BEPERR_KLAMMER;
			NextChar( s );
		} else {
			// get the variable state
			value = varFunc( GetSwitchName( s ) );
			if ( value == -1 ) BEPerror = BEPERR_NOTFOUND;
			else result &= value ^ negate;
			SkipWhiteSpaces( s );
		}

		if ( **s == '&' ) {
			goon = true;
			NextChar( s );
		} else {
			goon = false;
		}
		negate = false;
	} while ( goon && !BEPerror );

	return result;
}

qboolean CheckBEP( char *expr, qboolean (*varFuncParam)( char *var ) ) {
	qboolean result;
	char	*str;

	BEPerror = BEPERR_NONE;
	varFunc = varFuncParam;
	str = expr;
	result = CheckOR( &str );

	// check for no end error
	if ( *str && !BEPerror ) BEPerror = BEPERR_NOEND;

	switch ( BEPerror ) {
	case BEPERR_NONE:
		// do nothing
		return result;
	case BEPERR_KLAMMER:
		Com_Printf( _("')' expected in BEP (%s).\n"), expr );
		return true;
	case BEPERR_NOEND:
		Com_Printf( _("Unexpected end of condition in BEP (%s).\n"), expr );
		return result;
	case BEPERR_NOTFOUND:
		Com_Printf( _("Variable '%s' not found in BEP (%s).\n"), varName, expr );
		return false;
	default:
		// shouldn't happen
		Com_Printf( _("Unknown CheckBEP error in BEP (%s).\n"), expr );
		return true;
	}
}


// ===========================================================


/*
=================
CL_MapIsNight
=================
*/
qboolean CL_MapIsNight( vec2_t pos )
{
	float p, q, a, root, x;

	p = (float)ccs.date.sec/(3600*24);
	q = (ccs.date.day + p) * 2*M_PI/365.25 - M_PI;
	p = (0.5 + pos[0]/360 - p)*2*M_PI - q;
	a = sin(pos[1]*M_PI/180);
	root = sqrt(1-a*a);
	x = sin(p)*root*sin(q) - (a*SIN_ALPHA + cos(p)*root*COS_ALPHA)*cos(q);
	return (x > 0);
}


/*
======================
Date_LatherThan
======================
*/
qboolean Date_LaterThan( date_t now, date_t compare )
{
	if ( now.day > compare.day ) return true;
	if ( now.day < compare.day ) return false;
	if ( now.sec > compare.sec ) return true;
	return false;
}


/*
======================
Date_Add
======================
*/
date_t Date_Add( date_t a, date_t b )
{
	a.sec += b.sec;
	a.day += (a.sec/(3600*24)) + b.day;
	a.sec %= 3600*24;
	return a;
}


/*
======================
Date_Random
======================
*/
date_t Date_Random( date_t frame )
{
	frame.sec = (frame.day*3600*24 + frame.sec) * frand();
	frame.day = frame.sec / (3600*24);
	frame.sec = frame.sec % (3600*24);
	return frame;
}


// ===========================================================


/*
======================
CL_MapMaskFind
======================
*/
qboolean CL_MapMaskFind( byte *color, vec2_t polar )
{
	byte *c;
	int res, i, num;

	// check color
	if ( !color[0] && !color[1] && !color[2] )
		return false;

	// find possible positions
	res = maskWidth*maskHeight;
	num = 0;
	for ( i = 0, c = maskPic; i < res; i++, c += 4 )
		if ( c[0] == color[0] && c[1] == color[1] && c[2] == color[2] )
			num++;

	// nothing found?
	if ( !num ) return false;

	// get position
	num *= frand();
	for ( i = 0, c = maskPic; i < num; c += 4 )
		if ( c[0] == color[0] && c[1] == color[1] && c[2] == color[2] )
			i++;

	// transform to polar coords
	res = (c - maskPic) / 4;
	polar[0] = 180 - 360 * ((float)(res % maskWidth) + 0.5) / maskWidth;
	polar[1] = 90 - 180 * ((float)(res / maskWidth) + 0.5) / maskHeight;
	return true;
}


// ===========================================================

/*
======================
CL_NewAircraft
======================
*/
void CL_NewAircraft ( base_t* base )
{
	aircraft_t	*air;

	assert(base);
	air = &ccs.air[ccs.numAir];
	ccs.numAir++;
	memset( air, 0, sizeof(aircraft_t) );
	air->status = AIR_HOME;
	air->homebase = base;
	air->speed = 80.0f;
}

// check for water
// blue value is 64
#define MapIsWater(color) (color[0] == 0 && color[1] == 0 && color[2] == 64)
#define MapIsArctic(color) (color[0] == 128 && color[1] == 255 && color[2] == 255)
#define MapIsDesert(color) (color[0] == 255 && color[1] == 128 && color[2] == 0)
// others:
// red 255, 0, 0
// yellow 255, 255, 0
// green 128, 255, 0
// violet 128, 0, 128
// blue (not water) 128, 128, 255
// blue (not water, too) 0, 0, 255

/*
======================
CL_NewBase
======================
*/
void CL_NewBase( vec2_t pos )
{
	int x, y;
	byte *color;

	// get coordinates
	x = (180 - pos[0]) / 360 * maskWidth;
	y = (90 - pos[1]) / 180 * maskHeight;
	if ( x < 0 ) x = 0;
	if ( y < 0 ) y = 0;

	color = maskPic + 4 * (x + y * maskWidth);

	if ( MapIsWater(color) )
	{
		MN_Popup( _("Notice"), _("Could not set up your base at this location") );
		return;
	} else if ( MapIsDesert(color) ){
		baseCurrent->mapChar='d';
	} else if ( MapIsArctic(color) ){
		baseCurrent->mapChar='a';
	} else {
		baseCurrent->mapChar='g';
	}

	// build base
	baseCurrent->pos[0] = pos[0];
	baseCurrent->pos[1] = pos[1];

	ccs.numBases++;

	// set up the base with buildings that have the autobuild flag set
	B_SetUpBase();
	// set up the aircraft
	CL_NewAircraft( baseCurrent );
}


// ===========================================================


/*
======================
CL_StageSetDone
======================
*/
stage_t *testStage;

qboolean CL_StageSetDone( char *name )
{
	setState_t *set;
	int i;

	for ( i = 0, set = &ccs.set[testStage->first]; i < testStage->num; i++, set++ )
		if ( !strcmp( name, set->def->name ) )
		{
			if ( set->done >= set->def->quota ) return true;
			else return false;
		}

	// didn't find set
	return false;
}


/*
======================
CL_CampaignActivateStageSets
======================
*/
void CL_CampaignActivateStageSets( stage_t *stage )
{
	setState_t *set;
	int i;

	testStage = stage;
	for ( i = 0, set = &ccs.set[stage->first]; i < stage->num; i++, set++ )
		if ( !set->active && !set->done && !set->num )
		{
			// check needed sets
			if ( set->def->needed[0] && !CheckBEP( set->def->needed, CL_StageSetDone ) )
				continue;

			// activate it
			set->active = true;
			set->start = Date_Add( ccs.date, set->def->delay );
			set->event = Date_Add( set->start, Date_Random( set->def->frame ) );
		}
}


/*
======================
CL_CampaignActivateStage
======================
*/
stageState_t *CL_CampaignActivateStage( char *name )
{
	stage_t	*stage;
	stageState_t *state;
	int i, j;

	for ( i = 0, stage = stages; i < numStages; i++, stage++ )
	{
		if ( !strcmp( stage->name, name ) )
		{
			// add it to the list
			state = &ccs.stage[i];
			state->active = true;
			state->def = stage;
			state->start = ccs.date;

			// add stage sets
			for ( j = stage->first; j < stage->first + stage->num; j++ )
			{
				memset( &ccs.set[j], 0, sizeof(setState_t) );
				ccs.set[j].stage = &stage[j];
				ccs.set[j].def = &stageSets[j];
			}

			// activate stage sets
			CL_CampaignActivateStageSets( stage );
			return state;
		}
	}

	Com_Printf( _("CL_CampaignActivateStage: stage '%s' not found.\n"), name );
	return NULL;
}


/*
======================
CL_CampaignEndStage
======================
*/
void CL_CampaignEndStage( char *name )
{
	stageState_t *state;
	int i;

	for ( i = 0, state = ccs.stage; i < numStages; i++, state++ )
		if ( !strcmp( state->def->name, name ) )
		{
			state->active = false;
			return;
		}

	Com_Printf( _("CL_CampaignActivateStage: stage '%s' not found.\n"), name );
}


/*
======================
CL_CampaignAddMission
======================
*/
void CL_CampaignAddMission( setState_t *set )
{
	actMis_t *mis;

	// add mission
	if ( ccs.numMissions >= MAX_ACTMISSIONS )
	{
		Com_Printf( _("Too many active missions!\n") );
		return;
	}
	mis = &ccs.mission[ccs.numMissions++];
	memset( mis, 0, sizeof(actMis_t) );

	// set relevant info
	mis->def = &missions[ set->def->missions[(int)(set->def->numMissions*frand())] ];
	mis->cause = set;
	if ( set->def->expire.day )
		mis->expire = Date_Add( ccs.date, set->def->expire );

	// get default position first, then try to find a corresponding mask color
	mis->realPos[0] = mis->def->pos[0];
	mis->realPos[1] = mis->def->pos[1];
	CL_MapMaskFind( mis->def->mask, mis->realPos );

	// prepare next event (if any)
	set->num++;
	if ( set->def->number && set->num >= set->def->number ) set->active = false;
	else set->event = Date_Add( ccs.date, Date_Random( set->def->frame ) );

	// stop time
	CL_GameTimeStop();
}


/*
======================
CL_CampaignRemoveMission
======================
*/
void CL_CampaignRemoveMission( actMis_t *mis )
{
	int i, num;

	num = mis - ccs.mission;
	if ( num >= ccs.numMissions )
	{
		Com_Printf( _("CL_CampaignRemoveMission: Can't remove mission.\n") );
		return;
	}

	ccs.numMissions--;
	for ( i = num; i < ccs.numMissions; i++ )
		ccs.mission[i] = ccs.mission[i+1];

	if ( selMis == mis ) selMis = NULL;
	else if ( selMis > mis ) selMis--;
}


/*
======================
CL_CampaignExecute
======================
*/
void CL_CampaignExecute( setState_t *set )
{
	// handle stages, execute commands
	if ( *set->def->nextstage )
		CL_CampaignActivateStage( set->def->nextstage );

	if ( *set->def->endstage )
		CL_CampaignEndStage( set->def->endstage );

	if ( *set->def->cmds )
		Cbuf_AddText( set->def->cmds );

	// activate new sets in old stage
	CL_CampaignActivateStageSets( set->stage );
}


char text[MAX_MENUTEXTLEN];
/*
======================
CL_CampaignCheckEvents
======================
*/
void CL_CampaignCheckEvents( void )
{
	stageState_t *stage;
	setState_t   *set;
	actMis_t     *mis;
	int i, j;

	// check campaign events
	for ( i = 0, stage = ccs.stage; i < numStages; i++, stage++ )
		if ( stage->active )
			for ( j = 0, set = &ccs.set[stage->def->first]; j < stage->def->num; j++, set++ )
				if ( set->active && set->event.day && Date_LaterThan( ccs.date, set->event ) )
				{
					if ( set->def->numMissions )
						CL_CampaignAddMission( set );
					else
						CL_CampaignExecute( set );
				}

	// let missions expire
	for ( i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++ )
		if ( mis->expire.day && Date_LaterThan( ccs.date, mis->expire ) )
		{
			// ok, waiting and not doing a mission will costs money
			int lose = mis->def->civilians * mis->def->cr_civilian;
			CL_UpdateCredits( ccs.credits - lose );
			Com_sprintf (text, sizeof(text), _("The mission expired and %i civilians died\\You've lost %i $"), mis->def->civilians, lose );
			MN_Popup( _("Notice"), text );
			CL_CampaignRemoveMission( mis );
		}
}


/*
======================
CL_CampaignRunAircraft
======================
*/
void CL_CampaignRunAircraft( int dt )
{
	aircraft_t *air;
	float dist, frac;
	int i, p;

	for ( i = 0, air = ccs.air; i < ccs.numAir; i++, air++ )
		if ( air->status > AIR_IDLE )
		{
			// calc distance
			air->time += dt;
			dist = air->speed * air->time / 3600;

			// check for end point
			if ( dist >= air->route.dist * (air->route.n-1) )
			{
				float *end;
				end = air->route.p[air->route.n-1];
				air->pos[0] = end[0];
				air->pos[1] = end[1];

				// TODO: add events here
				// near a base => AIR_HOME
				// near a mission => AIR_INTERCEPT
				// ...
				air->status = AIR_IDLE;
				continue;
			}

			// calc new position
			frac = dist / air->route.dist;
			p = (int)frac;
			frac -= p;
			air->point = p;

			air->pos[0] = (1-frac) * air->route.p[p][0] + frac * air->route.p[p+1][0];
			air->pos[1] = (1-frac) * air->route.p[p][1] + frac * air->route.p[p+1][1];
		}
}


/*
=================
CL_DateConvert
=================
*/
char *monthNames[12] =
{
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

int monthLength[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

void CL_DateConvert( date_t *date, int *day, int *month )
{
	int i, d;

	// get day
	d = date->day % 365;
	for ( i = 0; d >= monthLength[i]; i++ )
		d -= monthLength[i];

	// prepare return values
	*day = d+1;
	*month = i;
}


/*
======================
CL_CampaignRun
======================
*/
void CL_CampaignRun( void )
{
	// advance time
	ccs.timer += cls.frametime * gameTimeScale;
	if ( ccs.timer >= 1.0 )
	{
		// calculate new date
		int dt, day, month;
		dt = floor( ccs.timer );
		ccs.date.sec += dt;
		ccs.timer -= dt;
		while ( ccs.date.sec > 3600*24 )
		{
			ccs.date.sec -= 3600*24;
			ccs.date.day++;
			CL_UpdateBaseData();
		}

		// check for campaign events
		CL_CampaignRunAircraft( dt );
		CL_CampaignCheckEvents();

		// set time cvars
		CL_DateConvert( &ccs.date, &day, &month );
		Cvar_Set( "mn_mapdate", va( "%i %s %i", ccs.date.day/365, _(monthNames[month]), day ) );
		Cvar_Set( "mn_mapmin", va( "%i%i", (ccs.date.sec%3600)/60/10, (ccs.date.sec%3600)/60%10 ) );
		Cvar_Set( "mn_maphour", va( "%i:", ccs.date.sec/3600 ) );
	}
}


// ===========================================================

typedef struct gameLapse_s
{
	char name[16];
	int scale;
} gameLapse_t;

#define NUM_TIMELAPSE 6

gameLapse_t lapse[NUM_TIMELAPSE] =
{
	{ "5 sec",  5 },
	{ "5 mins", 5*60 },
	{ "1 hour", 60*60 },
	{ "12 hour",  12*3600 },
	{ "1 day",  24*3600 },
	{ "5 days", 5*24*3600 }
};

int gameLapse;

/*
======================
CL_GameTimeStop
======================
*/
void CL_GameTimeStop( void )
{
	gameLapse = 0;
	Cvar_Set( "mn_timelapse", _(lapse[gameLapse].name) );
	gameTimeScale = lapse[gameLapse].scale;
}


/*
======================
CL_GameTimeSlow
======================
*/
void CL_GameTimeSlow( void )
{
	//first we have to set up a home base
	if ( ! ccs.numBases )
		CL_GameTimeStop();
	else
	{
		if ( gameLapse > 0 ) gameLapse--;
		Cvar_Set( "mn_timelapse", _(lapse[gameLapse].name) );
		gameTimeScale = lapse[gameLapse].scale;
	}
}

/*
======================
CL_UpdateCredits
======================
*/
void CL_UpdateCredits ( int credits )
{
	// credits
	ccs.credits = credits;
	Cvar_Set( "mn_credits", va( "%i $", ccs.credits ) );
}

/*
======================
CL_GameTimeFast
======================
*/
void CL_GameTimeFast( void )
{
	//first we have to set up a home base
	if ( ! ccs.numBases )
		CL_GameTimeStop();
	else
	{
		if ( gameLapse < NUM_TIMELAPSE-1 ) gameLapse++;
		Cvar_Set( "mn_timelapse", _(lapse[gameLapse].name) );
		gameTimeScale = lapse[gameLapse].scale;
	}
}

// ===========================================================

/*
======================
CL_GameNew
======================
*/
void CL_GameNew( void )
{
	equipDef_t	*ed;
	char	*name;
	int		i;

	CL_ResetCharacters();
	Cvar_Set( "mn_main", "singleplayer" );
	Cvar_Set( "mn_active", "map" );

	// get campaign
	name = Cvar_VariableString( "campaign" );
	for ( i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++ )
		if ( !strcmp( name, curCampaign->name ) )
			break;

	if ( i == numCampaigns )
	{
		Com_Printf( _("CL_GameNew: Campaign \"%s\" doesn't exist.\n"), name );
		return;
	}

	// reset, set time
	selMis = NULL;
	memset( &ccs, 0, sizeof( ccs_t ) );
	ccs.date = curCampaign->date;

	// set map view
	ccs.center[0] = 0.5;
	ccs.center[1] = 0.5;
	ccs.zoom = 1.0;

	// setup team
	for ( i = 0; i < curCampaign->soldiers; i++ )
		CL_GenerateCharacter( curCampaign->team );

	CL_UpdateCredits( curCampaign->credits );

	// equipment
	for ( i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++ )
		if ( !strcmp( curCampaign->equipment, ed->name ) )
			break;
	if ( i != csi.numEDs )
		ccs.eCampaign = *ed;

	// market
	for ( i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++ )
		if ( !strcmp( curCampaign->market, ed->name ) )
			break;
	if ( i != csi.numEDs )
		ccs.eMarket = *ed;

	// stage setup
	CL_CampaignActivateStage( curCampaign->firststage );

	// base setup
	ccs.numBases = 0;
	MN_NewBases();

	// aircraft setup
	ccs.numAir = 0;

	MN_PopMenu( true );
	MN_PushMenu( "map" );

	// create a base as first step
	Cbuf_AddText( "mn_select_base -1" );
	Cbuf_Execute();

	CL_GameTimeStop();
}


/*
======================
CL_GameSave
======================
*/
#define MAX_GAMESAVESIZE	8192
#define MAX_COMMENTLENGTH	32

#ifndef SAVE_FILE_VERSION
#define SAVE_FILE_VERSION 2
#endif

void CL_GameSave( char *filename, char *comment )
{
	stageState_t	*state;
	actMis_t		*mis;
	sizebuf_t		sb;
	byte	buf[MAX_GAMESAVESIZE];
	FILE	*f;
	int		res;
	int		i, j;

	if ( !curCampaign )
	{
		Com_Printf( _("No campaign active.\n") );
		return;
	}

	f = fopen( va( "%s/save/%s.sav", FS_Gamedir(), filename ), "wb" );
	if ( !f )
	{
		Com_Printf( _("Couldn't write file.\n") );
		return;
	}

	// create data
	SZ_Init( &sb, buf, MAX_GAMESAVESIZE );

	// write prefix and version
	MSG_WriteByte( &sb, 0 );
	MSG_WriteLong( &sb, SAVE_FILE_VERSION );

	// store comment
	MSG_WriteString( &sb, comment );

	// store campaign name
	MSG_WriteString( &sb, curCampaign->name );

	// store date
	MSG_WriteLong( &sb, ccs.date.day );
	MSG_WriteLong( &sb, ccs.date.sec );

	// store map view
	MSG_WriteFloat( &sb, ccs.center[0] );
	MSG_WriteFloat( &sb, ccs.center[1] );
	MSG_WriteFloat( &sb, ccs.zoom );

	// store bases
	MN_SaveBases( &sb );

	// store credits
	MSG_WriteShort( &sb, ccs.credits );

	// store equipment
	for ( i = 0; i < MAX_OBJDEFS; i++ )
	{
		MSG_WriteLong( &sb, ccs.eCampaign.num[i] );
		MSG_WriteByte( &sb, ccs.eCampaign.num_loose[i] );
	}

	// store market
	for ( i = 0; i < MAX_OBJDEFS; i++ )
		MSG_WriteLong( &sb, ccs.eMarket.num[i] );

	// store team
	CL_SendTeamInfo( &sb, wholeTeam, numWholeTeam );

	// store assignement
	MSG_WriteLong( &sb, teamMask );
	MSG_WriteByte( &sb, numOnTeam );

	// store campaign data
	for ( i = 0, state = ccs.stage; i < numStages; i++, state++ )
		if ( state->active )
		{
			// write head
			setState_t *set;
			MSG_WriteString( &sb, state->def->name );
			MSG_WriteLong( &sb, state->start.day );
			MSG_WriteLong( &sb, state->start.sec );
			MSG_WriteByte( &sb, state->def->num );

			// write all sets
			for ( j = 0, set = &ccs.set[state->def->first]; j < state->def->num; j++, set++ )
			{
				MSG_WriteString( &sb, set->def->name );
				MSG_WriteByte( &sb, set->active );
				MSG_WriteShort( &sb, set->num );
				MSG_WriteShort( &sb, set->done );
				MSG_WriteLong( &sb, set->start.day );
				MSG_WriteLong( &sb, set->start.sec );
				MSG_WriteLong( &sb, set->event.day );
				MSG_WriteLong( &sb, set->event.sec );
			}
		}
	// terminate list
	MSG_WriteByte( &sb, 0 );

	// store active missions
	MSG_WriteByte( &sb, ccs.numMissions );
	for ( i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++ )
	{
		MSG_WriteString( &sb, mis->def->name );
		MSG_WriteString( &sb, mis->cause->def->name );
		MSG_WriteFloat( &sb, mis->realPos[0] );
		MSG_WriteFloat( &sb, mis->realPos[1] );
		MSG_WriteLong( &sb, mis->expire.day );
		MSG_WriteLong( &sb, mis->expire.sec );
	}

	// write data
	res = fwrite( buf, 1, sb.cursize, f );
	fclose( f );

	if ( res == sb.cursize )
	{
		Cvar_Set( "mn_lastsave", filename );
		Com_Printf( _("Campaign '%s' saved.\n"), filename );
	}
}


/*
======================
CL_GameSaveCmd
======================
*/
void CL_GameSaveCmd( void )
{
	char	comment[MAX_COMMENTLENGTH];
	char	*arg;

	// get argument
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: game_save <filename> <comment>\n" );
		return;
	}

	// get comment
	if ( Cmd_Argc() > 2 )
	{
		arg = Cmd_Argv( 2 );
		if ( arg[0] == '*' ) strncpy( comment, Cvar_VariableString( arg+1 ), MAX_COMMENTLENGTH );
		else strncpy( comment, arg, MAX_COMMENTLENGTH );
	}
	else comment[0] = 0;

	// save the game
	CL_GameSave( Cmd_Argv( 1 ), comment );
}


/*
======================
CL_GameLoad
======================
*/
void CL_GameLoad( char *filename )
{
	actMis_t     *mis;
	stageState_t *state;
	setState_t   *set;
	setState_t   dummy;
	sizebuf_t	sb;
	byte	buf[MAX_GAMESAVESIZE];
	char	*name;
	FILE	*f;
	int		version;
	int		i, j, num;

	// open file
	f = fopen( va( "%s/save/%s.sav", FS_Gamedir(), filename ), "rb" );
	if ( !f )
	{
		Com_Printf( _("Couldn't open file '%s'.\n"), filename );
		return;
	}

	// read data
	SZ_Init( &sb, buf, MAX_GAMESAVESIZE );
	sb.cursize = fread( buf, 1, MAX_GAMESAVESIZE, f );
	fclose( f );

	// Check if save file is versioned
	if (MSG_ReadByte( &sb ) == 0)
	{
		version = MSG_ReadLong( &sb );
		Com_Printf("Savefile version %d detected\n", version );
	}
	else
	{
		// no - reset position and take version as 0
		MSG_BeginReading ( &sb );
		version = 0;
	}

	// check current version
	if ( version > SAVE_FILE_VERSION )
	{
		Com_Printf( "File '%s' is a more recent version (%d) than is supported.\n", filename, version );
		return;
	}
	else if ( version < SAVE_FILE_VERSION )
	{
		Com_Printf( "Savefileformat has changed ('%s' is version %d) - you may experience problems.\n", filename, version );
	}

	// read comment
	MSG_ReadString( &sb );

	// read campaign name
	name = MSG_ReadString( &sb );

	for ( i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++ )
		if ( !strcmp( name, curCampaign->name ) )
			break;

	if ( i == numCampaigns )
	{
		Com_Printf( _("CL_GameLoad: Campaign \"%s\" doesn't exist.\n"), name );
		return;
	}

	// reset
	selMis = NULL;
	memset( &ccs, 0, sizeof( ccs_t ) );

	// read date
	ccs.date.day = MSG_ReadLong( &sb );
	ccs.date.sec = MSG_ReadLong( &sb );

	// read map view
	ccs.center[0] = MSG_ReadFloat( &sb );
	ccs.center[1] = MSG_ReadFloat( &sb );
	ccs.zoom = MSG_ReadFloat( &sb );

	// load bases
	MN_LoadBases( &sb, version );

	// read credits
	ccs.credits = MSG_ReadShort( &sb );

	// read equipment
	for ( i = 0; i < MAX_OBJDEFS; i++ )
	{
		if (version == 0)
		{
			ccs.eCampaign.num[i] = MSG_ReadByte( &sb );
			ccs.eCampaign.num_loose[i] = 0;
		}
		else if (version == 1)
		{
			ccs.eCampaign.num[i] = MSG_ReadLong( &sb );
			ccs.eCampaign.num_loose[i] = MSG_ReadByte( &sb );
		}
	}

	// read market
	for ( i = 0; i < MAX_OBJDEFS; i++ )
	{
		if (version == 0)
			ccs.eMarket.num[i] = MSG_ReadByte( &sb );
		else if (version == 1)
			ccs.eMarket.num[i] = MSG_ReadLong( &sb );
	}

	// reset data
	CL_ResetCharacters();

	// read whole team list
	CL_LoadTeam( &sb );

	// read campaign data
	name = MSG_ReadString( &sb );
	while ( *name )
	{
		state = CL_CampaignActivateStage( name );
		if ( !state )
		{
			Com_Printf( _("Unable to load campaign '%s', unknown stage '%'\n"), filename, name );
			curCampaign = NULL;
			Cbuf_AddText( "mn_pop\n" );
			return;
		}

		state->start.day = MSG_ReadLong( &sb );
		state->start.sec = MSG_ReadLong( &sb );
		num = MSG_ReadByte( &sb );
		for ( i = 0; i < num; i++ )
		{
			name = MSG_ReadString( &sb );
			for ( j = 0, set = &ccs.set[state->def->first]; j < state->def->num; j++, set++ )
				if ( !strcmp( name, set->def->name ) )
					break;
			// write on dummy set, if it's unknown
			if ( j >= state->def->num )
			{
				Com_Printf( _("Warning: Set '%s' not found\n"), name );
				set = &dummy;
			}

			set->active = MSG_ReadByte( &sb );
			set->num = MSG_ReadShort( &sb );
			set->done = MSG_ReadShort( &sb );
			set->start.day = MSG_ReadLong( &sb );
			set->start.sec = MSG_ReadLong( &sb );
			set->event.day = MSG_ReadLong( &sb );
			set->event.sec = MSG_ReadLong( &sb );
		}

		// read next stage name
		name = MSG_ReadString( &sb );
	}

	// store active missions
	ccs.numMissions = MSG_ReadByte( &sb );
	for ( i = 0, mis = ccs.mission; i < ccs.numMissions; i++, mis++ )
	{
		// get mission definition
		name = MSG_ReadString( &sb );
		for ( j = 0; j < numMissions; j++ )
			if ( !strcmp( name, missions[j].name ) )
			{
				mis->def = &missions[j];
				break;
			}
		if ( j >= numMissions )
			Com_Printf( _("Warning: Mission '%s' not found\n"), name );

		// get mission definition
		name = MSG_ReadString( &sb );
		for ( j = 0; j < numStageSets; j++ )
			if ( !strcmp( name, stageSets[j].name ) )
			{
				mis->cause = &ccs.set[j];
				break;
			}
		if ( j >= numStageSets )
			Com_Printf( _("Warning: Stage set '%s' not found\n"), name );

		// read position and time
		mis->realPos[0] = MSG_ReadFloat( &sb );
		mis->realPos[1] = MSG_ReadFloat( &sb );
		mis->expire.day = MSG_ReadLong( &sb );
		mis->expire.sec = MSG_ReadLong( &sb );

		// ignore incomplete info
		if ( !mis->def || !mis->cause )
		{
			memset( mis, 0, sizeof(actMis_t) );
			mis--; i--; ccs.numMissions--;
		}
	}

	Com_Printf( _("Campaign '%s' loaded.\n"), filename );
	CL_GameTimeStop();
}


/*
======================
CL_GameLoadCmd
======================
*/
void CL_GameLoadCmd( void )
{
	// get argument
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: game_load <filename>\n" );
		return;
	}

	// load and go to map
	CL_GameLoad( Cmd_Argv( 1 ) );

	Cvar_Set( "mn_main", "singleplayer" );
	Cvar_Set( "mn_active", "map" );
	Cbuf_AddText( "disconnect\n" );

	MN_PopMenu( true );
	MN_PushMenu( "map" );
}


/*
======================
CL_GameCommentsCmd
======================
*/
void CL_GameCommentsCmd( void )
{
	char	comment[MAX_VAR];
	FILE	*f;
	int		i;
	int		first_char;

	for ( i = 0; i < 8; i++ )
	{
		// open file
		f = fopen( va( "%s/save/slot%i.sav", FS_Gamedir(), i ), "rb" );
		if ( !f )
		{
			Cvar_Set( va( "mn_slot%i", i ), "" );
			continue;
		}

		// check if it's versioned
		first_char = fgetc( f );
		if ( first_char == 0 )
		{
			// skip the version number
			fread( comment, sizeof(int), 1, f );
			// read the comment
			fread( comment, 1, MAX_VAR, f );
		}
		else
		{
			// not versioned - first_char is the first character of the comment
			comment[0] = first_char;
			fread( comment + 1, 1, MAX_VAR - 1, f );
		}
		Cvar_Set( va( "mn_slot%i", i ), comment );
		fclose( f );
	}
}


/*
======================
CL_GameExit
======================
*/
void CL_GameExit( void )
{
	Cbuf_AddText( "disconnect\n" );
	curCampaign = NULL;
	Cvar_Set( "mn_main", "main" );
	Cvar_Set( "mn_active", "" );
}


/*
======================
CL_GameContinue
======================
*/
void CL_GameContinue( void )
{
	if ( cls.state == ca_active )
	{
		MN_PopMenu( false );
		return;
	}

	if ( !curCampaign )
	{
		// try to load the current campaign
		CL_GameLoad( mn_lastsave->string );
		if ( !curCampaign ) return;
	}

	Cvar_Set( "mn_main", "singleplayer" );
	Cvar_Set( "mn_active", "map" );
	Cbuf_AddText( "disconnect\n" );

	MN_PopMenu( true );
	MN_PushMenu( "map" );
}


/*
======================
CL_GameGo
======================
*/
void CL_GameGo( void )
{
	mission_t	*mis;
	char	expanded[MAX_QPATH];
	char	timeChar;

	if ( !curCampaign || !selMis )
		return;

	if ( numOnTeam == 0 )
	{
		MN_Popup( _("Note"), _("Enter your base and assemble a team") );
		return;
	}

	// start the map
	mis = selMis->def;
	Cvar_SetValue( "ai_numaliens", mis->aliens );
	Cvar_SetValue( "ai_numcivilians", mis->civilians );
	Cvar_Set( "ai_alien", mis->alienTeam );
	Cvar_Set( "ai_civilian", mis->civTeam );
	Cvar_Set( "ai_equipment", mis->alienEquipment );
	Cvar_Set( "music", mis->music );
	// TODO is this correct for a multiplayer game too?  CL_GameGo returns
	// if curCampaign is null...
	Cvar_Set( "equip", "campaign_player" );

	// check inventory
	ccs.eMission = ccs.eCampaign;
	CL_CheckInventory( &ccs.eMission );

	// prepare
	deathMask = 0;
	MN_PopMenu( true );
	Cvar_Set( "mn_main", "singleplayermission" );

	// get appropriate map
	if ( CL_MapIsNight( mis->pos ) ) timeChar = 'n';
	else timeChar = 'd';

	// TODO: Draw the base attack on worldmap
	//       use mis->pos for this, too
	if ( mis->map[0] == '.' && B_GetCount() > 0 )
	{
		Cbuf_AddText( va("base_assemble_rand") );
		return;
	}

	if ( mis->map[0] == '+' ) Com_sprintf (expanded, sizeof(expanded), "maps/%s%c.ump", mis->map+1, timeChar );
	else Com_sprintf (expanded, sizeof(expanded), "maps/%s%c.bsp", mis->map, timeChar );

	if (FS_LoadFile (expanded, NULL) != -1)
		Cbuf_AddText( va( "map %s%c %s\n", mis->map, timeChar, mis->param ) );
	else
		Cbuf_AddText( va( "map %s %s\n", mis->map, mis->param ) );
}


/*
======================
CL_GameAbort
======================
*/
void CL_GameAbort( void )
{
	// aborting means letting the aliens win
	Cbuf_AddText( va( "sv win %i\n", TEAM_ALIEN ) );
}


// ===========================================================

#define MAX_BUYLIST		32

byte	buyList[MAX_BUYLIST];
int		buyListLength;

/*
======================
CL_BuySelectCmd
======================
*/
void CL_BuySelectCmd( void )
{
	int num;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("Usage: buy_select <num>\n") );
		return;
	}

	num = atoi( Cmd_Argv( 1 ) );
	if ( num > buyListLength )
		return;

	Cbuf_AddText( va( "buyselect%i\n", num ) );
	CL_ItemDescription( buyList[num] );
}


/*
======================
CL_BuyType
======================
*/
void CL_BuyType( void )
{
	objDef_t *od;
	int		i, j, num;
	char	str[MAX_VAR];

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("Usage: buy_type <category>\n") );
		return;
	}
	num = atoi( Cmd_Argv( 1 ) );

	CL_UpdateCredits( ccs.credits );

	// get item list
	for ( i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++ )
		if ( od->buytype == num && (ccs.eCampaign.num[i] || ccs.eMarket.num[i]) )
		{
			sprintf( str, "mn_item%i", j );		Cvar_Set( str, od->name );
			sprintf( str, "mn_storage%i", j );	Cvar_SetValue( str, ccs.eCampaign.num[i] );
			sprintf( str, "mn_supply%i", j );	Cvar_SetValue( str, ccs.eMarket.num[i] );
			sprintf( str, "mn_price%i", j );	Cvar_Set( str, va( "%i $", od->price ) );
			buyList[j] = i;
			j++;
		}

	buyListLength = j;

	for ( ; j < 28; j++ )
	{
		Cvar_Set( va( "mn_item%i", j ), "" );
		Cvar_Set( va( "mn_storage%i", j ), "" );
		Cvar_Set( va( "mn_supply%i", j ), "" );
		Cvar_Set( va( "mn_price%i", j ), "" );
	}

	// select first item
	if ( buyListLength )
	{
		Cbuf_AddText( "buyselect0\n" );
		CL_ItemDescription( buyList[0] );
	} else {
		// reset description
		Cvar_Set( "mn_itemname", "" );
		Cvar_Set( "mn_item", "" );
		Cvar_Set( "mn_weapon", "" );
		Cvar_Set( "mn_ammo", "" );
		menuText[TEXT_STANDARD] = NULL;
	}
}


/*
======================
CL_Buy
======================
*/
void CL_Buy( void )
{
	int num, item;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("Usage: buy <num>\n") );
		return;
	}

	num = atoi( Cmd_Argv( 1 ) );
	if ( num > buyListLength )
		return;

	item = buyList[num];
	Cbuf_AddText( va( "buyselect%i\n", num ) );
	CL_ItemDescription( item );

	if ( ccs.credits >= csi.ods[item].price && ccs.eMarket.num[item] )
	{
		Cvar_SetValue( va( "mn_storage%i", num ), ++ccs.eCampaign.num[item] );
		Cvar_SetValue( va( "mn_supply%i", num ),  --ccs.eMarket.num[item] );
		CL_UpdateCredits( ccs.credits-csi.ods[item].price );
	}
}


/*
======================
CL_Sell
======================
*/
void CL_Sell( void )
{
	int num, item;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( _("Usage: sell <num>\n") );
		return;
	}

	num = atoi( Cmd_Argv( 1 ) );
	if ( num > buyListLength )
		return;

	item = buyList[num];
	Cbuf_AddText( va( "buyselect%i\n", num ) );
	CL_ItemDescription( item );

	if ( ccs.eCampaign.num[item] )
	{
		Cvar_SetValue( va( "mn_storage%i", num ), --ccs.eCampaign.num[item] );
		Cvar_SetValue( va( "mn_supply%i", num ),  ++ccs.eMarket.num[item] );
		CL_UpdateCredits( ccs.credits+csi.ods[item].price );
	}
}


// ===========================================================

void CL_CollectItemAmmo( invList_t *weapon , int left_hand )
{
	if (weapon->item.t == NONE ||
			(left_hand && csi.ods[weapon->item.t].twohanded))
		return;
	ccs.eMission.num[weapon->item.t]++;
	if ( !csi.ods[weapon->item.t].reload || weapon->item.m == NONE )
		return;
	ccs.eMission.num_loose[weapon->item.m] += weapon->item.a;
	if (ccs.eMission.num_loose[weapon->item.m] >= csi.ods[weapon->item.t].ammo)
	{
		ccs.eMission.num_loose[weapon->item.m] -= csi.ods[weapon->item.t].ammo;
		ccs.eMission.num[weapon->item.m]++;
	}
}

void CL_CollectItems( int won )
{
	int i;
	le_t *le;
	invList_t *item;
	int container;

	for ( i = 0, le = LEs; i < numLEs; i++, le++ )
	{
		// Winner collects everything on the floor, and everything carried
		// by surviving actors.  Loser only gets what their living team
		// members carry.
		if ( !le->inuse )
			;
		else if ( le->type == ET_ITEM && won )
		{
			for ( item = FLOOR(le); item; item = item->next )
				CL_CollectItemAmmo( item, 0 );
		}
		else if ( le->type == ET_ACTOR && !(le->state & STATE_DEAD) && won )
		{
			for ( container = 0; container < csi.numIDs; container++ )
				for ( item = le->i.c[container]; item; item = item->next )
					CL_CollectItemAmmo( item, (container == csi.idLeft) );
		}
	}
}

/*
======================
CL_UpdateCharacterStats

FIXME: See TODO and FIXME included
======================
*/
void CL_UpdateCharacterStats ( int won )
{
	le_t *le;
	character_t* chr;
	int i, j;

	for ( i = 0; i < numWholeTeam; i++ )
	{
		le = cl.teamList[i];

		// check if the soldier still lives
		// and give him skills
		if ( le && !(le->state & STATE_DEAD) )
		{
			// TODO: Is the array of character_t the same
			//      as the array of le_t??
			chr = &wholeTeam[i];
			assert( chr );

			// FIXME:
			for ( j = 0; i < SKILL_NUM_TYPES; j++ )
				chr->skills[j]++;
		}
	}
}

/*
======================
CL_GameResultsCmd
======================
*/
void CL_GameResultsCmd( void )
{
	int won;
	int i, j;
	int tempMask;

	// multiplayer?
	if ( !curCampaign )
		return;

	// check for replay
	if ( (int)Cvar_VariableValue( "game_tryagain" ) )
	{
		CL_GameGo();
		return;
	}

	// check for win
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: game_results <won>\n" );
		return;
	}
	won = atoi( Cmd_Argv( 1 ) );

	// give reward, change equipment
	CL_UpdateCredits( ccs.credits+ccs.reward );

	// remove the dead (and their item preference)
	for ( i = 0; i < numWholeTeam; )
	{
		if ( deathMask & (1<<i) )
		{
			deathMask >>= 1;
			tempMask = teamMask >> 1;
			teamMask = (teamMask & ((1<<i)-1)) | (tempMask & ~((1<<i)-1));
			numWholeTeam--;
			numOnTeam--;
			Com_DestroyInventory( &teamInv[i] );
			for ( j = i; j < numWholeTeam; j++ )
			{
				teamInv[j] = teamInv[j+1];
				wholeTeam[j] = wholeTeam[j+1];
				wholeTeam[j].inv = &teamInv[j];
			}
			memset( &teamInv[j], 0, sizeof(inventory_t) );
		}
		else i++;
	}

	// add recruits
	if ( won && selMis->def->recruits )
		for ( i = 0; i < selMis->def->recruits; i++ )
			CL_GenerateCharacter( curCampaign->team );

	// campaign effects
	selMis->cause->done++;
	if ( selMis->cause->done >= selMis->cause->def->quota )
		CL_CampaignExecute( selMis->cause );

	// remove mission from list
	CL_CampaignRemoveMission( selMis );
}


/*
======================
CL_MapActionReset
======================
*/
void CL_MapActionReset( void )
{
	// don't allow a reset when no base is set up
	if ( ccs.numBases )
		mapAction = MA_NONE;
}


/*
======================
CL_ResetCampaign
======================
*/
void CL_ResetCampaign( void )
{
	Cmd_AddCommand( "game_new", CL_GameNew );
	Cmd_AddCommand( "game_continue", CL_GameContinue );
	Cmd_AddCommand( "game_exit", CL_GameExit );
	Cmd_AddCommand( "game_save", CL_GameSaveCmd );
	Cmd_AddCommand( "game_load", CL_GameLoadCmd );
	Cmd_AddCommand( "game_comments", CL_GameCommentsCmd );
	Cmd_AddCommand( "game_go", CL_GameGo );
	Cmd_AddCommand( "game_abort", CL_GameAbort );
	Cmd_AddCommand( "game_results", CL_GameResultsCmd );
	Cmd_AddCommand( "game_timestop", CL_GameTimeStop );
	Cmd_AddCommand( "game_timeslow", CL_GameTimeSlow );
	Cmd_AddCommand( "game_timefast", CL_GameTimeFast );
	Cmd_AddCommand( "buy_type", CL_BuyType );
	Cmd_AddCommand( "buy_select", CL_BuySelectCmd );
	Cmd_AddCommand( "mn_buy", CL_Buy );
	Cmd_AddCommand( "mn_sell", CL_Sell );
	Cmd_AddCommand( "mn_mapaction_reset", CL_MapActionReset );

	re.LoadTGA( "pics/menu/map_mask.tga", &maskPic, &maskWidth, &maskHeight );
	if ( maskPic ) Com_Printf( _("Map mask loaded.\n") );
	else Com_Printf( _("Couldn't load map mask (pics/menu/map_mask.tga)\n") );
}


// ===========================================================

#define	MISSIONOFS(x)	(int)&(((mission_t *)0)->x)

value_t mission_vals[] =
{
	{ "text",		V_STRING,		0 },
	{ "map",		V_STRING,		MISSIONOFS( map ) },
	{ "param",		V_STRING,		MISSIONOFS( param ) },
	{ "music",		V_STRING,		MISSIONOFS( music ) },
	{ "pos",		V_POS,			MISSIONOFS( pos ) },
	{ "mask",		V_RGBA,			MISSIONOFS( mask ) },
	{ "aliens",		V_INT,			MISSIONOFS( aliens ) },
	{ "alienteam",	V_STRING,		MISSIONOFS( alienTeam ) },
	{ "alienequip",	V_STRING,		MISSIONOFS( alienEquipment ) },
	{ "civilians",	V_INT,			MISSIONOFS( civilians ) },
	{ "civteam",	V_STRING,		MISSIONOFS( civTeam ) },
	{ "recruits",	V_INT,			MISSIONOFS( recruits ) },
	{ "$win",		V_INT,			MISSIONOFS( cr_win ) },
	{ "$alien",		V_INT,			MISSIONOFS( cr_alien ) },
	{ "$civilian",	V_INT,			MISSIONOFS( cr_civilian ) },
	{ NULL, 0, 0 },
};

#define		MAX_MISSIONTEXTS	MAX_MISSIONS*128
char		missionTexts[MAX_MISSIONTEXTS];
char		*mtp = missionTexts;

/*
======================
CL_ParseMission
======================
*/
void CL_ParseMission( char *name, char **text )
{
	char		*errhead = _("CL_ParseMission: unexptected end of file (mission ");
	mission_t	*ms;
	value_t		*vp;
	char		*token;
	int			i;

	// search for missions with same name
	for ( i = 0; i < numMissions; i++ )
		if ( !strcmp( name, missions[i].name ) )
			break;

	if ( i < numMissions )
	{
		Com_Printf( _("Com_ParseMission: mission def \"%s\" with same name found, second ignored\n"), name );
		return;
	}

	// initialize the menu
	ms = &missions[numMissions++];
	memset( ms, 0, sizeof(mission_t) );

	strncpy( ms->name, name, MAX_VAR );

	// get it's body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("Com_ParseMission: mission def \"%s\" without body ignored\n"), name );
		numMissions--;
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( vp = mission_vals; vp->string; vp++ )
			if ( !strcmp( token, vp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				if ( vp->ofs )
					Com_ParseValue( ms, token, vp->type, vp->ofs );
				else
				{
					strcpy( mtp, token );
					ms->text = mtp;
					do {
						mtp = strchr( mtp, '\\' );
						if ( mtp ) *mtp = '\n';
					} while ( mtp );
					mtp = ms->text + strlen( token ) + 1;
				}
				break;
			}

		if ( !vp->string )
			Com_Printf( _("Com_ParseMission: unknown token \"%s\" ignored (mission %s)\n"), token, name );

	} while ( *text );
}


// ===========================================================


#define	STAGESETOFS(x)	(int)&(((stageSet_t *)0)->x)

value_t stageset_vals[] =
{
	{ "needed",		V_STRING,	STAGESETOFS( needed ) },
	{ "delay",		V_DATE,		STAGESETOFS( delay ) },
	{ "frame",		V_DATE,		STAGESETOFS( frame ) },
	{ "expire",		V_DATE,		STAGESETOFS( expire ) },
	{ "number",		V_INT,		STAGESETOFS( number ) },
	{ "quota",		V_INT,		STAGESETOFS( quota ) },
	{ "nextstage",	V_STRING,	STAGESETOFS( nextstage ) },
	{ "endstage",	V_STRING,	STAGESETOFS( endstage ) },
	{ "commands",	V_STRING,	STAGESETOFS( cmds ) },
	{ NULL, 0, 0 },
};

/*
======================
CL_ParseStageSet
======================
*/
void CL_ParseStageSet( char *name, char **text )
{
	char		*errhead = _("CL_ParseStageSet: unexptected end of file (stageset ");
	stageSet_t	*sp;
	value_t		*vp;
	char		missionstr[256];
	char		*token, *misp;
	int			j;

	// initialize the stage
	sp = &stageSets[numStageSets++];
	memset( sp, 0, sizeof(stageSet_t) );
	strncpy( sp->name, name, MAX_VAR );

	// get it's body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("Com_ParseStageSets: stageset def \"%s\" without body ignored\n"), name );
		numStageSets--;
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// check for some standard values
		for ( vp = stageset_vals; vp->string; vp++ )
			if ( !strcmp( token, vp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				Com_ParseValue( sp, token, vp->type, vp->ofs );
				break;
			}
		if ( vp->string )
			continue;

		// get mission set
		if ( !strcmp( token, "missions" ) )
		{
			token = COM_EParse( text, errhead, name );
			if ( !*text ) return;
			strncpy( missionstr, token, 255 );
			missionstr[strlen(missionstr)] = 0;
			misp = missionstr;

			// add mission options
			sp->numMissions = 0;
			do {
				token = COM_Parse( &misp );
				if ( !misp ) break;

				for ( j = 0; j < numMissions; j++ )
					if ( !strcmp( token, missions[j].name ) )
					{
						sp->missions[sp->numMissions++] = j;
						break;
					}

				if ( j == numMissions )
					Com_Printf( _("Com_ParseStageSet: unknown mission \"%s\" ignored (stageset %s)\n"), token, name );
			}
			while ( misp && sp->numMissions < MAX_SETMISSIONS );
			continue;
		}

		Com_Printf( _("Com_ParseStageSet: unknown token \"%s\" ignored (stageset %s)\n"), token, name );
	} while ( *text );
}


/*
======================
CL_ParseStage
======================
*/
void CL_ParseStage( char *name, char **text )
{
	char		*errhead = _("CL_ParseStage: unexptected end of file (stage ");
	stage_t		*sp;
	char		*token;
	int			i;

	// search for campaigns with same name
	for ( i = 0; i < numStages; i++ )
		if ( !strcmp( name, stages[i].name ) )
			break;

	if ( i < numStages )
	{
		Com_Printf( _("Com_ParseStage: stage def \"%s\" with same name found, second ignored\n"), name );
		return;
	}

	// get it's body
	token = COM_Parse( text );
	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("Com_ParseStages: stage def \"%s\" without body ignored\n"), name );
		return;
	}

	// initialize the stage
	sp = &stages[numStages++];
	memset( sp, 0, sizeof(stage_t) );
	strncpy( sp->name, name, MAX_VAR );
	sp->first = numStageSets;

	Com_Printf( _("stage: %s\n"), name );

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		if ( !strcmp( token, "set" ) )
		{
			token = COM_EParse( text, errhead, name );
			CL_ParseStageSet( token, text );
		}
		else Com_Printf( _("Com_ParseStage: unknown token \"%s\" ignored (stage %s)\n"), token, name );
	} while ( *text );

	sp->num = numStageSets - sp->first;
}


// ===========================================================

#define	CAMPAIGNOFS(x)	(int)&(((campaign_t *)0)->x)

value_t campaign_vals[] =
{
	{ "team",		V_STRING,	CAMPAIGNOFS( team ) },
	{ "soldiers",	V_INT,		CAMPAIGNOFS( soldiers ) },
	{ "equipment",	V_STRING,	CAMPAIGNOFS( equipment ) },
	{ "market",		V_STRING,	CAMPAIGNOFS( market ) },
	{ "firststage",	V_STRING,	CAMPAIGNOFS( firststage ) },
	{ "credits",	V_INT,		CAMPAIGNOFS( credits ) },
	{ "date",		V_DATE,		CAMPAIGNOFS( date ) },
	{ NULL, 0, 0 },
};

/*
======================
CL_ParseCampaign
======================
*/
void CL_ParseCampaign( char *name, char **text )
{
	char		*errhead = _("CL_ParseCampaign: unexptected end of file (campaign ");
	campaign_t	*cp;
	value_t		*vp;
	char		*token;
	int			i;

	// search for campaigns with same name
	for ( i = 0; i < numCampaigns; i++ )
		if ( !strcmp( name, campaigns[i].name ) )
			break;

	if ( i < numCampaigns )
	{
		Com_Printf( _("Com_ParseCampaign: campaign def \"%s\" with same name found, second ignored\n"), name );
		return;
	}

	// initialize the menu
	cp = &campaigns[numCampaigns++];
	memset( cp, 0, sizeof(campaign_t) );

	strncpy( cp->name, name, MAX_VAR );

	// get it's body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("Com_ParseCampaign: campaign def \"%s\" without body ignored\n"), name );
		numCampaigns--;
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// check for some standard values
		for ( vp = campaign_vals; vp->string; vp++ )
			if ( !strcmp( token, vp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				Com_ParseValue( cp, token, vp->type, vp->ofs );
				break;
			}

		if ( !vp->string )
		{
			Com_Printf( _("Com_ParseCampaign: unknown token \"%s\" ignored (campaign %s)\n"), token, name );
			COM_EParse( text, errhead, name );
		}
	} while ( *text );
}
