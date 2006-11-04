// cl_campaign.c -- single player campaign control

#include "client.h"

// public vars

mission_t	missions[MAX_MISSIONS];
int			numMissions = 0;
int			selMission;

campaign_t	campaigns[MAX_CAMPAIGNS];
int			numCampaigns = 0;

byte		actMis[MAX_ACTMISSIONS];
int			numActMis;

int			doneMis[MAX_MISFIELDS];

campaign_t	*curCampaign;
equipDef_t	cEquip;
equipDef_t	mEquip;

equipDef_t	cMarket;
int			credits;

int			cReward;

// ===========================================================

/*
======================
CL_MapClick
======================
*/
void CL_MapClick( int x, int y )
{
	mission_t	*ms;
	int		i;

	for ( i = 0; i < numActMis; i++ )
	{
		ms = &missions[actMis[i]];
		if ( x >= ms->pos[0]-16 && x <= ms->pos[0]+16 && y >= ms->pos[1]-16 && y <= ms->pos[1]+16 )
		{
			selMission = actMis[i];
			break;
		}
	}
}


/*
======================
CL_GetActiveMissions
======================
*/
void CL_GetActiveMissions( void )
{
	qboolean	ok;
	int		i, j, mis;


	if ( !curCampaign )
		return;

	selMission = -1;
	numActMis = 0;
	for ( i = 0; i < curCampaign->num; i++ )
	{
		mis = curCampaign->mission[i];
		if ( doneMis[mis>>5] & 1<<(mis%32) ) continue;

		ok = true;
		for ( j = 0; j < 4 && curCampaign->required[i][j] != NONE; j++ )
			if ( ~doneMis[ curCampaign->required[i][j]>>5 ] & 1<<(curCampaign->required[i][j]%32) )
			{
				ok = false;
				break;
			}
		if ( ok ) actMis[numActMis++] = mis;
	}

	if ( !numActMis && !curCampaign->finished )
	{
		// campaign finished
		curCampaign->finished = true;
		Cbuf_AddText( "mn_push enddemo\n" );
	}
}


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
		Com_Printf( "CL_GameNew: Campaign \"%s\" doesn't exist.\n", name );
		return;
	}

	// setup team
	for ( i = 0; i < curCampaign->soldiers; i++ )
		CL_GenerateCharacter( curCampaign->team );

	// credits
	credits = curCampaign->credits;

	// equipment
	for ( i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++ )
		if ( !strcmp( curCampaign->equipment, ed->name ) )
			break;
	if ( i != csi.numEDs ) 
		cEquip = *ed;
	else
		memset( &cEquip, 0, sizeof( equipDef_t ) );
	
	// market
	for ( i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++ )
		if ( !strcmp( curCampaign->market, ed->name ) )
			break;
	if ( i != csi.numEDs ) 
		cMarket = *ed;
	else
		memset( &cEquip, 0, sizeof( equipDef_t ) );

	// reset missions
	for ( i = 0; i < MAX_MISFIELDS; i++ ) 
		doneMis[i] = 0;
	CL_GetActiveMissions();

	MN_PopMenu( true );
	MN_PushMenu( "map" );
}


/*
======================
CL_GameSave
======================
*/
#define MAX_GAMESAVESIZE	8192
#define MAX_COMMENTLENGTH	32

void CL_GameSave( char *filename, char *comment )
{
	sizebuf_t	sb;
	byte	buf[MAX_GAMESAVESIZE];
	FILE	*f;
	int		res;
	int		i;

	if ( !curCampaign )
	{
		Com_Printf( "No campaign active.\n" );
		return;
	}

	f = fopen( va( "%s/save/%s.sav", FS_Gamedir(), filename ), "wb" );
	if ( !f ) 
	{
		Com_Printf( "Couldn't write file.\n" );
		return;
	}

	// create data
	SZ_Init( &sb, buf, MAX_GAMESAVESIZE );

	// store comment
	MSG_WriteString( &sb, comment );

	// store campaign name
	MSG_WriteString( &sb, curCampaign->name );

	// store done missions
	for ( i = 0; i < MAX_MISFIELDS; i++ )
		MSG_WriteLong( &sb, doneMis[i] );

	// store credits
	MSG_WriteShort( &sb, credits );

	// store equipment
	for ( i = 0; i < csi.numODs; i++ )
		MSG_WriteByte( &sb, cEquip.num[i] );

	// store market
	for ( i = 0; i < csi.numODs; i++ )
		MSG_WriteByte( &sb, cMarket.num[i] );

	// store team
	CL_SendTeamInfo( &sb, wholeTeam, numWholeTeam );

	// store assignement
	MSG_WriteShort( &sb, teamMask );
	MSG_WriteShort( &sb, numOnTeam );

	// write data
	res = fwrite( buf, 1, sb.cursize, f );
	fclose( f );
	
	if ( res == sb.cursize )
	{
		Cvar_Set( "mn_lastsave", filename );
		Com_Printf( "Campaign '%s' saved.\n", filename );
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
	character_t	*chr;
	sizebuf_t	sb;
	byte	buf[MAX_GAMESAVESIZE];
	char	*name;
	FILE	*f;
	int		i, p;

	// open file
	f = fopen( va( "%s/save/%s.sav", FS_Gamedir(), filename ), "rb" );
	if ( !f ) 
	{
		Com_Printf( "Couldn't open file '%s'.\n", filename );
		return;
	}

	// read data
	SZ_Init( &sb, buf, MAX_GAMESAVESIZE );
	sb.cursize = fread( buf, 1, MAX_GAMESAVESIZE, f );
	fclose( f );

	// reset data
	CL_ResetCharacters();

	// read comment
	MSG_ReadString( &sb );

	// read campaign name
	name = MSG_ReadString( &sb );

	for ( i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++ )
		if ( !strcmp( name, curCampaign->name ) )
			break;

	if ( i == numCampaigns )
	{
		Com_Printf( "CL_GameLoad: Campaign \"%s\" doesn't exist.\n", name );
		return;
	}

	// read done missions
	for ( i = 0; i < MAX_MISFIELDS; i++ )
		doneMis[i] = MSG_ReadLong( &sb );

	// read credits
	credits = MSG_ReadShort( &sb );

	// read equipment
	for ( i = 0; i < csi.numODs; i++ )
		cEquip.num[i] = MSG_ReadByte( &sb );

	// read market
	for ( i = 0; i < csi.numODs; i++ )
		cMarket.num[i] = MSG_ReadByte( &sb );

	// read whole team list
	MSG_ReadByte( &sb );
	numWholeTeam = MSG_ReadByte( &sb );
	for ( i = 0, chr = wholeTeam; i < numWholeTeam; chr++, i++ )
		CL_LoadTeamMember( &sb, chr );

	// get assignement
	teamMask = MSG_ReadShort( &sb );
	numOnTeam = MSG_ReadShort( &sb );

	for ( i = 0, p = 0; i < numWholeTeam; i++ )
		if ( teamMask & (1 << i) )
			curTeam[p++] = wholeTeam[i];

	for ( ; p < MAX_ACTIVETEAM; p++ )
	{
		Cvar_ForceSet( va( "mn_name%i", p ), "" );
		Cbuf_AddText( va( "equipdisable%i\n", p ) );
	}

	Com_Printf( "Campaign '%s' loaded.\n", filename );
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

	CL_GetActiveMissions();
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
	char	comment[MAX_COMMENTLENGTH];
	FILE	*f;
	int		i;

	for ( i = 0; i < 8; i++ )
	{
		// open file
		f = fopen( va( "%s/save/slot%i.sav", FS_Gamedir(), i ), "rb" );
		if ( !f ) 
		{
			Cvar_Set( va( "mn_slot%i", i ), "" );
			continue;
		}

		// read data
		fread( comment, 1, MAX_COMMENTLENGTH, f );
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

	CL_GetActiveMissions();
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

	if ( !curCampaign || selMission == -1 )
		return;

	if ( numOnTeam == 0 )
		return;

	// start the map
	mis = &missions[selMission];
	Cvar_SetValue( "ai_numaliens", mis->aliens );
	Cvar_SetValue( "ai_numcivilians", mis->civilians );
	Cvar_Set( "ai_equipment", mis->alienEquipment );
	Cvar_Set( "music", mis->music );

	// check inventory
	mEquip = cEquip;
	CL_CheckCampaignInventory( &mEquip );
	mEquip = cEquip;

	// prepare
	deathMask = 0;
	MN_PopMenu( true );
	Cvar_Set( "mn_main", "singleplayermission" );
	Cbuf_AddText( va( "map %s\n", mis->map ) );
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
		Com_Printf( "Usage: buy_select <num>\n" );
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
		Com_Printf( "Usage: buy_type <category>\n" );
		return;
	}
	num = atoi( Cmd_Argv( 1 ) );

	Cvar_Set( "mn_credits", va( "%i $", credits ) );

	// get item list
	for ( i = 0, j = 0, od = csi.ods; i < csi.numODs; i++, od++ )
		if ( od->buytype == num && (cEquip.num[i] || cMarket.num[i]) )
		{
			sprintf( str, "mn_item%i", j );		Cvar_Set( str, od->name );
			sprintf( str, "mn_storage%i", j );	Cvar_SetValue( str, cEquip.num[i] );
			sprintf( str, "mn_supply%i", j );	Cvar_SetValue( str, cMarket.num[i] );
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
		menuText = NULL;
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
		Com_Printf( "Usage: buy <num>\n" );
		return;
	}

	num = atoi( Cmd_Argv( 1 ) );
	if ( num > buyListLength )
		return;

	item = buyList[num];
	Cbuf_AddText( va( "buyselect%i\n", num ) );
	CL_ItemDescription( item );

	if ( credits >= csi.ods[item].price && cMarket.num[item] )
	{
		Cvar_SetValue( va( "mn_storage%i", num ), ++cEquip.num[item] );
		Cvar_SetValue( va( "mn_supply%i", num ),  --cMarket.num[item] );
		credits -= csi.ods[item].price;
		Cvar_Set( "mn_credits", va( "%i $", credits ) );
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
		Com_Printf( "Usage: buy <num>\n" );
		return;
	}

	num = atoi( Cmd_Argv( 1 ) );
	if ( num > buyListLength )
		return;

	item = buyList[num];
	Cbuf_AddText( va( "buyselect%i\n", num ) );
	CL_ItemDescription( item );

	if ( cEquip.num[item] )
	{
		Cvar_SetValue( va( "mn_storage%i", num ), --cEquip.num[item] );
		Cvar_SetValue( va( "mn_supply%i", num ),  ++cMarket.num[item] );
		credits += csi.ods[item].price;
		Cvar_Set( "mn_credits", va( "%i $", credits ) );
	}
}


// ===========================================================

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
	credits += cReward;
	cEquip = mEquip;

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
			teamInv[j].inv = NULL;
		}
		else i++;
	}

	// add recruits
	if ( won && missions[selMission].recruits )
		for ( i = 0; i < missions[selMission].recruits; i++ )
			CL_GenerateCharacter( curCampaign->team );

	// activate new missions
	if ( won ) doneMis[selMission>>5] |= 1<<(selMission%32);

	selMission = -1;
	CL_GetActiveMissions();
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
	Cmd_AddCommand( "buy_type", CL_BuyType );
	Cmd_AddCommand( "buy_select", CL_BuySelectCmd );
	Cmd_AddCommand( "mn_buy", CL_Buy );
	Cmd_AddCommand( "mn_sell", CL_Sell );
}


// ===========================================================

#define	MISSIONOFS(x)	(int)&(((mission_t *)0)->x)

value_t mission_vals[] =
{
	{ "text",		V_STRING,		0 },
	{ "map",		V_STRING,		MISSIONOFS( map ) },
	{ "music",		V_STRING,		MISSIONOFS( music ) },
	{ "pos",		V_POS,			MISSIONOFS( pos ) },
	{ "aliens",		V_INT,			MISSIONOFS( aliens ) },
	{ "civilians",	V_INT,			MISSIONOFS( civilians ) },
	{ "alienequip",	V_STRING,		MISSIONOFS( alienEquipment ) },
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
	char		*errhead = "CL_ParseMission: unexptected end of file (mission ";
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
		Com_Printf( "Com_ParseMission: mission def \"%s\" with same name found, second ignored\n", name );
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
		Com_Printf( "Com_ParseMission: mission def \"%s\" without body ignored\n", name );
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
						mtp = strchr( mtp, ';' );
						if ( mtp ) *mtp = '\n';
					} while ( mtp );
					mtp = ms->text + strlen( token ) + 1;
				}
				break;
			}

		if ( !vp->string )
			Com_Printf( "Com_ParseMission: unknown token \"%s\" ignored (mission %s)\n", token, name );

	} while ( *text );
}


// ===========================================================

#define	CAMPAIGNOFS(x)	(int)&(((campaign_t *)0)->x)

value_t campaign_vals[] =
{
	{ "team",		V_STRING,	CAMPAIGNOFS( team ) },
	{ "soldiers",	V_INT,		CAMPAIGNOFS( soldiers ) },
	{ "equipment",	V_STRING,	CAMPAIGNOFS( equipment ) },
	{ "market",		V_STRING,	CAMPAIGNOFS( market ) },
	{ "credits",	V_INT,		CAMPAIGNOFS( credits ) },
	{ NULL, 0, 0 },
};

/*
======================
CL_ParseCampaign
======================
*/
void CL_ParseCampaign( char *name, char **text )
{
	char		*errhead = "CL_ParseCampaign: unexptected end of file (campaign ";
	campaign_t	*cp;
	mission_t	*ms;
	value_t		*vp;
	char		*token, *reqp;
	char		req[256];
	int			i, j, num;

	// search for campaigns with same name
	for ( i = 0; i < numCampaigns; i++ )
		if ( !strcmp( name, campaigns[i].name ) )
			break;

	if ( i < numCampaigns )
	{
		Com_Printf( "Com_ParseCampaign: campaign def \"%s\" with same name found, second ignored\n", name );
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
		Com_Printf( "Com_ParseCampaign: campaign def \"%s\" without body ignored\n", name );
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
		if ( vp->string )
			continue;

		// check for mission names
		for ( i = 0, ms = missions; i < numMissions; i++, ms++ )
			if ( !strcmp( token, ms->name ) )
			{
				// add mission
				cp->mission[cp->num] = i;

				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;
				strncpy( req, token, 255 );
				req[strlen(req)] = 0;
				reqp = req;

				// add required missions
				num = 0;
				do {
					token = COM_Parse( &reqp );
					if ( !reqp ) break;

					for ( j = 0; j < numMissions; j++ )
						if ( !strcmp( token, missions[j].name ) )
						{
							cp->required[cp->num][num++] = j;
							break;
						}

					if ( j == numMissions )
						Com_Printf( "Com_ParseCampaign: unknown required mission \"%s\" ignored (campaign %s)\n", token, name );

				} while ( reqp && num < MAX_REQMISSIONS );

				if ( num < MAX_REQMISSIONS ) 
					cp->required[cp->num][num] = NONE;
				cp->num++;
				break;
			}

		if ( i == numMissions )
		{
			Com_Printf( "Com_ParseCampaign: unknown mission \"%s\" ignored (campaign %s)\n", token, name );
			COM_EParse( text, errhead, name );
		}
	} while ( *text );
}
