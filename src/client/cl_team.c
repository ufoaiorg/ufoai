// cl_team.c -- team management, name generation and parsing


#include "client.h"

#define MAX_NAMECATS	64
#define MAX_TEAMDEFS	128
#define MAX_INFOSTRING	65536

typedef enum 
{
	NAME_NEUTRAL,
	NAME_FEMALE,
	NAME_MALE,

	NAME_LAST,
	NAME_FEMALE_LAST,
	NAME_MALE_LAST,

	NAME_NUM_TYPES
};

#define LASTNAME	3

typedef enum 
{
	MODEL_BODY,
	MODEL_HEAD,
	MODEL_SKIN,

	MODEL_NUM_TYPES
};

typedef struct nameCategory_s
{
	char title[MAX_VAR];
	char *names[NAME_NUM_TYPES];
	int  numNames[NAME_NUM_TYPES];
	char *models[NAME_LAST];
	int  numModels[NAME_LAST];
} nameCategory_t;

typedef struct teamDef_s
{
	char title[MAX_VAR];
	char *cats;
	int  num;
} teamDef_t;

nameCategory_t	nameCat[MAX_NAMECATS];
teamDef_t		teamDef[MAX_TEAMDEFS];
char	infoStr[MAX_INFOSTRING];
char	*infoPos;
int		numNameCats = 0;
int		numTeamDefs = 0;

char *name_strings[NAME_NUM_TYPES] =
{
	"neutral",
	"female",
	"male",
	"lastname",
	"female_lastname",
	"male_lastname"
};

int		hiredMask;
int		teamMask;
int		deathMask;

int		numHired;
int		numOnTeam;
int		numWholeTeam;

inventory_t	equipment;
inventory_t teamInv[MAX_WHOLETEAM];

character_t wholeTeam[MAX_WHOLETEAM];
character_t curTeam[MAX_ACTIVETEAM];

int		nextUCN;

/*
======================
CL_GiveName
======================
*/
char returnName[MAX_VAR];
char *CL_GiveName( int gender, char *category )
{
	nameCategory_t	*nc;
	char	*pos;
	int		i, j, name;

	// search the name
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !strcmp( category, nc->title ) )
		{
			// found category
			if ( !nc->numNames[gender] ) return NULL;
			name = nc->numNames[gender] * frand();

			// skip names
			pos = nc->names[gender];
			for ( j = 0; j < name; j++ ) 
				pos += strlen( pos ) + 1;

			// store the name
			strncpy( returnName, pos, MAX_VAR );
			return returnName;
		}
	
	// nothing found
	return NULL;
}

/*
======================
CL_GiveModel
======================
*/
char returnModel[MAX_VAR];
char *CL_GiveModel( int type, int gender, char *category )
{
	nameCategory_t	*nc;
	char	*path, *model;
	int		i, j, num;

	// search the name
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !strcmp( category, nc->title ) )
		{
			// found category
			if ( !nc->numModels[gender] ) return NULL;
			num = (int)(nc->numModels[gender] * frand()) * 4;

			// skip models
			path = nc->models[gender];
			for ( j = 0; j < num; j++ ) 
				path += strlen( path ) + 1;

			// get the desired info
			model = path + strlen( path ) + 1;
			if ( type == MODEL_HEAD || type == MODEL_SKIN ) 
				model += strlen( model ) + 1;

			if ( type == MODEL_SKIN )
			{
				model += strlen( model ) + 1;
				strncpy( returnModel, model, MAX_VAR );
				return returnModel;
			} else {
				strncpy( returnModel, va( "%s/%s", path, model ), MAX_VAR );
				return returnModel;
			}
		}
	
	// nothing found
	return NULL;
}

/*
======================
CL_GiveNameCmd
======================
*/
void CL_GiveNameCmd( void )
{
	char	*name;
	int		i, j, num;

	if ( Cmd_Argc() < 3 )
	{
		Com_Printf( "Usage: givename <gender> <category> [num]\n" );
		return;
	}

	// get gender
	for ( i = 0; i < NAME_LAST; i++ )
		if ( !strcmp( Cmd_Argv(1), name_strings[i] ) )
			break;

	if ( i == NAME_LAST )
	{
		Com_Printf( "'%s' isn't a gender! (male and female are)\n", Cmd_Argv(1) );
		return;
	}

	if ( Cmd_Argc() > 3 ) 
	{
		num = atoi( Cmd_Argv(3) );
		if ( num < 1 ) num = 1;
		if ( num > 20 ) num = 20;
	} else num = 1;

	for ( j = 0; j < num; j++ )
	{
		// get first name
		name = CL_GiveName( i, Cmd_Argv(2) );
		if ( !name )
		{
			Com_Printf( "No first name in category '%s'\n", Cmd_Argv(2) );
			return;
		}
		Com_Printf( name );

		// get last name
		name = CL_GiveName( i + LASTNAME, Cmd_Argv(2) );
		if ( !name )
		{
			Com_Printf( "\nNo last name in category '%s'\n", Cmd_Argv(2) );
			return;
		}

		// print out name
		Com_Printf( " %s\n", name );
	}
}


/*
======================
CL_GenerateCharacter
======================
*/
void CL_GenerateCharacter( char *team )
{
	teamDef_t	*td;
	character_t *chr;
	char	name[MAX_VAR], *str;
	int		i, category, gender;

	// check for too many characters
	if ( numWholeTeam >= (int)cl_numnames->value )
		return;

	// get team
	for ( i = 0; i < numTeamDefs; i++ )
		if ( !strcmp( team, teamDef[i].title ) )
			break;
	if ( i == numTeamDefs ) td = &teamDef[0];
	else td = &teamDef[i];

	// reset character
	chr = &wholeTeam[numWholeTeam];
	memset( chr, 0, sizeof( character_t ) );

	// link inventory
	chr->inv = &teamInv[numWholeTeam];
	Com_DestroyInventory( chr->inv );

	// get ucn
	chr->ucn = nextUCN++;

	// get attributes
	chr->strength = frand()*MAX_STARTSKILL;
	chr->dexterity = frand()*MAX_STARTSKILL;
	chr->swiftness = frand()*MAX_STARTSKILL;
	chr->intelligence = frand()*MAX_STARTSKILL;
	chr->courage = frand()*MAX_STARTSKILL;

	numWholeTeam++;
	while ( numWholeTeam ) {
		// select a type
		gender = frand()*NAME_LAST;
		category = (int)td->cats[(int)(frand()*td->num)];

		// get name
		str = CL_GiveName( gender, nameCat[category].title );
		if ( !str ) continue;
		strncpy( name, str, MAX_VAR );
		strcat( name, " " );

		str = CL_GiveName( gender + LASTNAME, nameCat[category].title );
		if ( !str ) continue;
		strncat( name, str, MAX_VAR - strlen( name ) );

		strcpy( chr->name, name );
		Cvar_ForceSet( va( "mn_name%i", numWholeTeam-1 ), name );

		// get model
		str = CL_GiveModel( MODEL_BODY, gender, nameCat[category].title );
		if ( !str ) continue;
		strncpy( chr->body, str, MAX_VAR );

		str = CL_GiveModel( MODEL_HEAD, gender, nameCat[category].title );
		if ( !str ) continue;
		strncpy( chr->head, str, MAX_VAR );

		str = CL_GiveModel( MODEL_SKIN, gender, nameCat[category].title );
		if ( !str ) continue;
		chr->skin = atoi( str );

		break;
	}
}


/*
======================
CL_ResetCharacters
======================
*/
void CL_ResetCharacters( void )
{
	int		i;

	// reset inventory data
	for ( i = 0; i < MAX_WHOLETEAM; i++ )
	{
		Com_DestroyInventory( &teamInv[i] );
		teamInv[i].floor = &equipment;
		wholeTeam[i].inv = &teamInv[i];
	}

	// reset hire info
	Cvar_ForceSet( "cl_selected", "0" );
	numWholeTeam = 0;
	numOnTeam = 0;
	teamMask = 0;
}


/*
======================
CL_GenerateNamesCmd
======================
*/
void CL_GenerateNamesCmd( void )
{
	CL_ResetCharacters();
	Cbuf_AddText( "disconnect\ngame_exit\n" );

	while ( numWholeTeam < cl_numnames->value )
		CL_GenerateCharacter( Cvar_VariableString( "team" ) );
}


/*
======================
CL_ChangeNameCmd
======================
*/
void CL_ChangeNameCmd( void )
{
	int sel;

	sel = cl_selected->value;
	if ( sel >= 0 && sel < numWholeTeam )
		strcpy( wholeTeam[sel].name, Cvar_VariableString( "mn_name" ) );
}


/*
======================
CL_GetModelInTeam
======================
*/
int CL_GetModelInTeam( char *team, char *body, char *head )
{
	teamDef_t	*td;
	char		*str;
	int		i, gender, category;

	for ( i = 0; i < numTeamDefs; i++ )
		if ( !strcmp( team, teamDef[i].title ) )
			break;
	if ( i == numTeamDefs ) td = &teamDef[0];
	else td = &teamDef[i];

	// get the models
	while ( team )
	{
		gender = frand()*NAME_LAST;
		category = (int)td->cats[(int)(frand()*td->num)];

		str = CL_GiveModel( MODEL_BODY, gender, nameCat[category].title );
		if ( !str ) continue;
		strncpy( body, str, MAX_VAR );

		str = CL_GiveModel( MODEL_HEAD, gender, nameCat[category].title );
		if ( !str ) continue;
		strncpy( head, str, MAX_VAR );

		str = CL_GiveModel( MODEL_SKIN, gender, nameCat[category].title );
		if ( !str ) continue;

		return atoi( str );
	}
	return 0;
}


/*
======================
CL_ItemDescription
======================
*/
#define MAX_ITEMTEXT	256
char	itemText[MAX_ITEMTEXT];

void CL_ItemDescription( int item )
{
	objDef_t	*od;

	// select item
	od = &csi.ods[item];
	Cvar_Set( "mn_itemname", od->name );

	// set models
	if ( od->link == NONE )
	{
		Cvar_Set( "mn_item", od->kurz );
		Cvar_Set( "mn_weapon", "" );
		Cvar_Set( "mn_ammo", "" );
	} else {
		Cvar_Set( "mn_weapon", od->kurz );
		Cvar_Set( "mn_ammo", csi.ods[od->link].kurz );
		Cvar_Set( "mn_item", "" );
	}

	// set description text
	if ( od->link != NONE )
	{
		sprintf( itemText, "Primary:\t%s\nSecondary:\t%s\nDamage:\t%i / %i\nTime Units:\t %i / %i\nRange:\t%1.1f / %1.1f\nSpread:\t%1.1f / %1.1f\nAmmo:\t%i\n", 
			od->fd[0].name, od->fd[1].name,
			(int)(od->fd[0].damage[0] * od->fd[0].shots + od->fd[0].spldmg[0]), (int)(od->fd[1].damage[0] * od->fd[1].shots + od->fd[0].spldmg[0]),
			(od->fd[0].time), (od->fd[1].time),
			(od->fd[0].range / 32.0), (od->fd[1].range / 32.0),
			(od->fd[0].spread[0] + od->fd[0].spread[1])/2, (od->fd[1].spread[0] + od->fd[1].spread[1])/2, 
			(int)(od->ammo) );
		menuText = itemText;
	} 
	else menuText = NULL;
}


/*
======================
CL_AddWeaponAmmo
======================
*/
int CL_AddWeaponAmmo( equipDef_t *ed, int type )
{
	if ( !ed->num[type] ) 
		return 0;

	ed->num[type]--;
	if ( csi.ods[type].link != NONE && ed->num[csi.ods[type].link] )
	{
		ed->num[csi.ods[type].link]--;
		return csi.ods[type].ammo;
	}
	return 0;
}


/*
======================
CL_CheckCampaignInventory
======================
*/
void CL_CheckCampaignInventory( equipDef_t *equip )
{
	character_t	*cp;
	invChain_t	*ic, *next;
	int p;
	
	for ( p = 0, cp = curTeam; p < numOnTeam; p++, cp++ )
	{
		if ( cp->inv->right.t != NONE )
		{
			if ( equip->num[cp->inv->right.t] ) cp->inv->right.a = CL_AddWeaponAmmo( equip, cp->inv->right.t );
			else cp->inv->right.t = NONE;
		}
		if ( cp->inv->left.t != NONE )
		{
			if ( equip->num[cp->inv->left.t] ) cp->inv->left.a = CL_AddWeaponAmmo( equip, cp->inv->left.t );
			else cp->inv->left.t = NONE;
		}
		for ( ic = cp->inv->inv; ic; ic = next )
		{
			next = ic->next;
			if ( equip->num[ic->item.t] ) ic->item.a = CL_AddWeaponAmmo( equip, ic->item.t );
			else Com_RemoveFromInventory( cp->inv, ic->container, ic->x, ic->y );
		}
	}
}


/*
======================
CL_GenerateEquipmentCmd
======================
*/
void CL_GenerateEquipmentCmd( void )
{
	equipDef_t	*ed;
	equipDef_t	unused;
	char	*name;
	int		i, p;
	int		x, y;

	if ( !numHired )
	{
		MN_PopMenu( false );
		return;
	}

	// store hired names
	Cvar_ForceSet( "cl_selected", "0" );
	numOnTeam = numHired;
	teamMask = hiredMask;
	for ( i = 0, p = 0; i < numWholeTeam; i++ )
		if ( hiredMask & (1 << i) )
		{
			curTeam[p] = wholeTeam[i];
			Cvar_ForceSet( va( "mn_name%i", p ), curTeam[p].name );
			p++;
		}

	for ( ; p < MAX_ACTIVETEAM; p++ )
	{
		Cvar_ForceSet( va( "mn_name%i", p ), "" );
		Cbuf_AddText( va( "equipdisable%i\n", p ) );
	}

	// reset equipment
	menuInventory = &teamInv[0];
	selActor = NULL;

	Com_DestroyInventory( &equipment );
	equipment.floor = &equipment;

	// reset description
	Cvar_Set( "mn_itemname", "" );
	Cvar_Set( "mn_item", "" );
	Cvar_Set( "mn_weapon", "" );
	Cvar_Set( "mn_ammo", "" );
	menuText = NULL;

	if ( !curCampaign )
	{
	// search equipment definition
		name = Cvar_VariableString( "equip" );
		for ( i = 0, ed = csi.eds; i < csi.numEDs; i++, ed++ )
			if ( !strcmp( name, ed->name ) )
				break;
		if ( i == csi.numEDs ) 
		{
			Com_Printf( "Equipment '%s' not found!\n", name );
			return;
		}

		// place equipment
		unused = *ed;
		for ( i = 0; i < csi.numODs; i++ )
			while ( unused.num[i] )
			{
				Com_FindSpace( &equipment, i, csi.idEquip, &x, &y );
				if ( x >= 32 && y >= 16 )
					return;
				Com_AddToInventory( &equipment, i, CL_AddWeaponAmmo( &unused, i ), csi.idEquip, x, y );
			}
	}
	else
	{
		// check soldiers inventory
		unused = cEquip;
		CL_CheckCampaignInventory( &unused );

		// place rest of campaign inventory
		for ( i = 0; i < csi.numODs; i++ )
			while ( unused.num[i] )
			{
				Com_FindSpace( &equipment, i, csi.idEquip, &x, &y );
				if ( x >= 32 && y >= 16 )
					return;
				Com_AddToInventory( &equipment, i, CL_AddWeaponAmmo( &unused, i ), csi.idEquip, x, y );
			}
	}
}


/*
======================
CL_SelectCmd
======================
*/
void CL_SelectCmd( void )
{
	char *command;
	int num;

	// check syntax
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: team_select <num>\n" );
		return;
	}
	num = atoi( Cmd_Argv(1) );

	// change highlights
	command = Cmd_Argv(0);
	*strchr( command, '_' ) = 0;

	if ( !strcmp( command, "equip" ) )
	{
		if ( num >= numOnTeam ) return;
		menuInventory = curTeam[num].inv;
	}
	else if ( !strcmp( command, "team" ) )
	{
		if ( num >= numWholeTeam ) return;
	}
	else if ( !strcmp( command, "hud" ) )
	{
		CL_ActorSelectList( num );
		return;
	}
	
	// console commands
	Cbuf_AddText( va( "%sdeselect%i\n", command, (int)cl_selected->value ) );
	Cbuf_AddText( va( "%sselect%i\n", command, num ) );
	Cvar_ForceSet( "cl_selected", va( "%i", num ) );

	// set info cvars
	if ( !strcmp( command, "team" ) ) CL_CharacterCvars( &wholeTeam[num] );
	else CL_CharacterCvars( &curTeam[num] );	
}


/*
======================
CL_MarkTeamCmd
======================
*/
void CL_MarkTeamCmd( void )
{
	int i;

	hiredMask = teamMask;
	numHired = numOnTeam;
	Cvar_Set( "mn_hired", va( "%i of %i\n", numHired, MAX_ACTIVETEAM ) );

	for ( i = 0; i < numWholeTeam; i++ )
	{
		Cvar_ForceSet( va( "mn_name%i", i ), wholeTeam[i].name );
		if ( hiredMask & (1 << i) )
			Cbuf_AddText( va( "listadd%i\n", i ) );
	}

	for ( i = numWholeTeam; i < (int)cl_numnames->value; i++ )
	{
		Cbuf_AddText( va( "listdisable%i\n", i ) );
		Cvar_ForceSet( va( "mn_name%i", i ), "" );
	}
}


/*
======================
CL_HireActorCmd
======================
*/
void CL_HireActorCmd( void )
{
	int num;

	// check syntax
	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: hire <num>\n" );
		return;
	}
	num = atoi( Cmd_Argv(1) );

	if ( num >= numWholeTeam )
		return;

	if ( hiredMask & (1 << num) )
	{
		// unhire
		Cbuf_AddText( va( "listdel%i\n", num ) );
		hiredMask &= ~(1 << num);
		numHired--;
	} 
	else if ( numHired < MAX_ACTIVETEAM )
	{
		// hire
		Cbuf_AddText( va( "listadd%i\n", num ) );
		hiredMask |= (1 << num);
		numHired++;
	}

	// select the desired one anyways
	Cvar_Set( "mn_hired", va( "%i of %i\n", numHired, MAX_ACTIVETEAM ) );
	Cbuf_AddText( va( "team_select %i\n", num ) );
}


/*
======================
CL_MessageMenuCmd
======================
*/
char nameBackup[MAX_VAR];
char cvarName[MAX_VAR];

void CL_MessageMenuCmd( void )
{
	char *msg;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: msgmenu <msg>\n" );
		return;
	}

	msg = Cmd_Argv( 1 );
	if ( msg[0] == '?'  )
	{
		// start
		Cbuf_AddText( "messagemenu\n" );
		strncpy( cvarName, msg+1, MAX_VAR );
		strncpy( nameBackup, Cvar_VariableString( cvarName ), MAX_VAR );
		strncpy( msg_buffer, nameBackup, MAX_VAR );
		msg_bufferlen = strlen( nameBackup );
	} 
	else if ( msg[0] == '!' )
	{
		// cancel
		Cvar_ForceSet( cvarName, nameBackup );
		Cvar_ForceSet( va( "%s%i", cvarName, (int)cl_selected->value ), nameBackup );
		Cbuf_AddText( va( "%s_changed\n", cvarName ) );
	}
	else if ( msg[0] == ':' )
	{
		// end
		Cvar_ForceSet( cvarName, msg+1 );
		Cvar_ForceSet( va( "%s%i", cvarName, (int)cl_selected->value ), msg+1 );
		Cbuf_AddText( va( "%s_changed\n", cvarName ) );
	}
	else
	{
		// continue
		Cvar_ForceSet( cvarName, msg );
		Cvar_ForceSet( va( "%s%i", cvarName, (int)cl_selected->value ), msg );
	}
}


/*
======================
CL_SaveTeamCmd
======================
*/
#define MAX_TEAMDATASIZE	2048

void CL_SaveTeamCmd( void )
{
	sizebuf_t	sb;
	byte	buf[MAX_TEAMDATASIZE];
	char	*filename;
	FILE	*f;
	int		res;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: saveteam <filename>\n" );
		return;
	}

	// open file
	filename = Cmd_Argv( 1 );
	f = fopen( va( "%s/save/%s.mpt", FS_Gamedir(), filename ), "wb" );
	if ( !f ) 
	{
		Com_Printf( "Couldn't write file.\n" );
		return;
	}

	// create data
	SZ_Init( &sb, buf, MAX_TEAMDATASIZE );
	CL_SendTeamInfo( &sb, curTeam, numOnTeam );

	// write data
	res = fwrite( buf, 1, sb.cursize, f );
	fclose( f );
	
	if ( res == sb.cursize )
		Com_Printf( "Team '%s' saved.\n", filename );
}


/*
======================
CL_LoadTeamMember
======================
*/
void CL_LoadTeamMember( sizebuf_t *sb, character_t *chr )
{
	int		item, ammo, container, x, y;

	// unique character number
	chr->ucn = MSG_ReadShort( sb );
	if ( chr->ucn >= nextUCN ) nextUCN = chr->ucn + 1;

	// name and model
	strcpy( chr->name, MSG_ReadString( sb ) );
	strcpy( chr->body, MSG_ReadString( sb ) );
	strcpy( chr->head, MSG_ReadString( sb ) );
	chr->skin = MSG_ReadByte( sb );

	// attributes
	chr->strength = MSG_ReadByte( sb );
	chr->dexterity = MSG_ReadByte( sb );
	chr->swiftness = MSG_ReadByte( sb );
	chr->intelligence = MSG_ReadByte( sb );
	chr->courage = MSG_ReadByte( sb );

	// inventory
	Com_DestroyInventory( chr->inv );
	item = MSG_ReadByte( sb );
	while ( item != NONE )
	{
		// read info
		ammo = MSG_ReadByte( sb );
		container = MSG_ReadByte( sb );
		x = MSG_ReadByte( sb );
		y = MSG_ReadByte( sb );

		// check info and add item if ok
		Com_AddToInventory( chr->inv, item, ammo, container, x, y );

		// get next item
		item = MSG_ReadByte( sb );
	}
}


/*
======================
CL_LoadTeamCmd
======================
*/
void CL_LoadTeamCmd( void )
{
	character_t	*chr;
	sizebuf_t	sb;
	byte	buf[MAX_TEAMDATASIZE];
	char	*filename;
	FILE	*f;
	int		i;

	if ( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: loadteam <filename>\n" );
		return;
	}

	// open file
	filename = Cmd_Argv( 1 );
	f = fopen( va( "%s/save/%s.mpt", FS_Gamedir(), filename ), "rb" );
	if ( !f ) 
	{
		Com_Printf( "Couldn't open file '%s'.\n", filename );
		return;
	}

	// read data
	SZ_Init( &sb, buf, MAX_TEAMDATASIZE );
	sb.cursize = fread( buf, 1, MAX_TEAMDATASIZE, f );
	fclose( f );

	// reset data
	CL_ResetCharacters();

	// read header
	MSG_ReadByte( &sb );
	numOnTeam = MSG_ReadByte( &sb );
	for ( i = 0, chr = curTeam; i < numOnTeam; chr++, i++ )
	{
		chr->inv = &teamInv[i];
		CL_LoadTeamMember( &sb, chr );
	}

	Com_Printf( "Team '%s' loaded.\n", filename );
}


/*
======================
CL_ResetTeams
======================
*/
void CL_ResetTeams( void )
{
//	int i;

	Cmd_AddCommand( "givename", CL_GiveNameCmd );
	Cmd_AddCommand( "gennames", CL_GenerateNamesCmd );
	Cmd_AddCommand( "genequip", CL_GenerateEquipmentCmd );
	Cmd_AddCommand( "team_mark", CL_MarkTeamCmd );
	Cmd_AddCommand( "team_hire", CL_HireActorCmd );
	Cmd_AddCommand( "team_select", CL_SelectCmd );
	Cmd_AddCommand( "team_changename", CL_ChangeNameCmd );
	Cmd_AddCommand( "equip_select", CL_SelectCmd );
	Cmd_AddCommand( "hud_select", CL_SelectCmd );
	Cmd_AddCommand( "saveteam", CL_SaveTeamCmd );
	Cmd_AddCommand( "loadteam", CL_LoadTeamCmd );
	Cmd_AddCommand( "msgmenu", CL_MessageMenuCmd );

/*	Cvar_Get( "mn_name", "", CVAR_NOSET );
	Cvar_Get( "mn_body", "", CVAR_NOSET );
	Cvar_Get( "mn_head", "", CVAR_NOSET );
	Cvar_Get( "mn_skin", "", CVAR_NOSET );
	for ( i = 0; i < MAX_WHOLETEAM; i++ )
		Cvar_Get( va( "mn_name%i", i ), "", CVAR_NOSET );
*/
	equipment.inv = NULL;
	numNameCats = 0;
	nextUCN = 0;
	infoPos = infoStr;
}

/*
======================
CL_SendTeamInfo
======================
*/
void CL_SendItem( sizebuf_t *buf, item_t item, int container, int x, int y )
{
	MSG_WriteByte( buf, item.t );
	MSG_WriteByte( buf, item.a );
	MSG_WriteByte( buf, container );
	MSG_WriteByte( buf, x );
	MSG_WriteByte( buf, y );
}

void CL_SendTeamInfo( sizebuf_t *buf, character_t *team, int num )
{
	character_t	*chr;
	invChain_t	*ic;
	int i;

	// header
	MSG_WriteByte( buf, clc_teaminfo );
	MSG_WriteByte( buf, num );

	for ( i = 0, chr = team; i < num; chr++, i++ )
	{
		// unique character number
		MSG_WriteShort( buf, chr->ucn );

		// name
		MSG_WriteString( buf, chr->name );

		// model
		MSG_WriteString( buf, chr->body );
		MSG_WriteString( buf, chr->head );
		MSG_WriteByte( buf, chr->skin );
		
		// attributes
		MSG_WriteByte( buf, chr->strength );
		MSG_WriteByte( buf, chr->dexterity );
		MSG_WriteByte( buf, chr->swiftness );
		MSG_WriteByte( buf, chr->intelligence );
		MSG_WriteByte( buf, chr->courage );

		// equipment
		if ( chr->inv->right.t != NONE )
			CL_SendItem( buf, chr->inv->right, csi.idRight, 0, 0 );
		if ( chr->inv->left.t != NONE )
			CL_SendItem( buf, chr->inv->left, csi.idLeft, 0, 0 );
		for ( ic = chr->inv->inv; ic; ic = ic->next )
			CL_SendItem( buf, ic->item, ic->container, ic->x, ic->y );

		// terminate list
		MSG_WriteByte( buf, NONE );
	}
}


/*
======================
CL_ParseResults
======================
*/
#define	MAX_RESULTTEXT	512
char	resultText[MAX_RESULTTEXT];

void CL_ParseResults( sizebuf_t *buf )
{
	byte	num_spawned[MAX_TEAMS];
	byte	num_alive[MAX_TEAMS];
	byte	num_kills[MAX_TEAMS][MAX_TEAMS];
	byte	winner, we;
	char	*enemy;
	int		i, j, num, res, kills;

	// get number of teams
	num = MSG_ReadByte( buf );
	if ( num > MAX_TEAMS )
		Sys_Error( "Too many teams in result message\n" );

	// get winning team
	winner = MSG_ReadByte( buf );
	we = cls.team;
	enemy = (sv_maxclients->value == 1) ? "Alien" : "Enemy";

	// get spawn and alive count
	for ( i = 0; i < num; i++ ) 
	{
		num_spawned[i] = MSG_ReadByte( buf );
		num_alive[i] = MSG_ReadByte( buf );
	}

	// get kills
	for ( i = 0; i < num; i++ ) 
		for ( j = 0; j < num; j++ )
			num_kills[i][j] = MSG_ReadByte( buf );

	// read terminator
	if ( MSG_ReadByte( buf ) != NONE )
		Com_Printf( "WARNING: bad result message\n" );

	// init result text
	menuText = resultText;

	// alien stats
	for ( i = 1, kills = 0; i < num; i++ ) kills += (i == we) ? 0 : num_kills[we][i];
	sprintf( resultText, "%ss killed\t%i\n", enemy, kills );
	for ( i = 1, res = 0; i < num; i++ ) res += (i == we) ? 0 : num_alive[i];
	strcat( resultText, va( "%s survivors\t%i\n\n", enemy, res ) );

	// team stats
	strcat( resultText, va( "Team losses\t%i\n", num_spawned[we] - num_alive[we] ) );
	strcat( resultText, va( "Friendly fire losses\t%i\n", num_kills[we][we] ) );
	strcat( resultText, va( "Team survivors\t%i\n\n", num_alive[we] ) );

	// kill civilians on campaign, if not won
	if ( curCampaign && num_alive[TEAM_CIVILIAN] && winner != we )
	{
		num_kills[TEAM_ALIEN][TEAM_CIVILIAN] += num_alive[TEAM_CIVILIAN];
		num_alive[TEAM_CIVILIAN] = 0;
	}

	// civilian stats
	for ( i = 1, res = 0; i < num; i++ ) res += (i == we) ? 0 : num_kills[i][TEAM_CIVILIAN];
	strcat( resultText, va( "Civilians killed by %ss\t%i\n", enemy, res ) );
	strcat( resultText, va( "Civilians killed by team\t%i\n", num_kills[we][TEAM_CIVILIAN] ) );
	strcat( resultText, va( "Civilians saved\t%i\n\n\n", num_alive[TEAM_CIVILIAN] ) );

	MN_PopMenu( true );
	if ( !curCampaign )
	{
		// get correct menus
		Cvar_Set( "mn_main", "main" );
		Cvar_Set( "mn_active", "" );
		MN_PushMenu( "main" );
		return;
	} else {
		mission_t	*ms;

		// get correct menus
		Cvar_Set( "mn_main", "singleplayer" );
		Cvar_Set( "mn_active", "map" );
		MN_PushMenu( "map" );

		// show money stats
		ms = &missions[selMission];
		cReward = 0;
		if ( winner == we )
		{
			strcat( resultText, va( "Collected alien technology\t%i $\n", ms->cr_win ) );
			cReward += ms->cr_win;
		}
		if ( kills )
		{
			strcat( resultText, va( "Aliens killed\t%i $\n", kills * ms->cr_alien ) );
			cReward += kills * ms->cr_alien;
		}
		if ( winner == we && num_alive[TEAM_CIVILIAN] )
		{
			strcat( resultText, va( "Civilians saved\t%i $\n", num_alive[TEAM_CIVILIAN] * ms->cr_civilian ) );
			cReward += num_alive[TEAM_CIVILIAN] * ms->cr_civilian;
		}
		strcat( resultText, va( "Total reward\t%i $\n\n\n", cReward ) );

		// recruits
		if ( winner == we && ms->recruits )
			strcat( resultText, va( "New Recruits\t%i\n", ms->recruits ) );
	}

	// disconnect and show win screen
	if ( winner == we ) MN_PushMenu( "won" );
	else MN_PushMenu( "lost" );
	Cbuf_AddText( "disconnect\n" );
	Cbuf_Execute();
}


/*
======================
CL_ParseNames
======================
*/
void CL_ParseNames( char *title, char **text )
{
	nameCategory_t	*nc;
	char	*errhead = "CL_ParseNames: unexptected end of file (names ";
	char	*token;
	int		i;
	
	// check for additions to existing name categories
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !strcmp( nc->title, title ) )
			break;

	// reset new category
	if ( i == numNameCats )
	{
		memset( nc, 0, sizeof( nameCategory_t ) );
		numNameCats++;
	}
	strncpy( nc->title, title, MAX_VAR );

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( "CL_ParseNames: name def \"%s\" without body ignored\n", title );
		if ( numNameCats - 1 == nc - nameCat ) numNameCats--;
		return;
	}

	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( i = 0; i < NAME_NUM_TYPES; i++ )
			if ( !strcmp( token, name_strings[i] ) )
			{
				// initialize list
				nc->names[i] = infoPos;
				nc->numNames[i] = 0;

				token = COM_EParse( text, errhead, title );
				if ( !*text ) break;
				if ( *token != '{' ) break;

				do {
					// get a name
					token = COM_EParse( text, errhead, title );
					if ( !*text ) break;
					if ( *token == '}' ) break;

					strcpy( infoPos, token );
					infoPos += strlen( token ) + 1;
					nc->numNames[i]++;
				} 
				while ( *text );				

				// lastname is different
				if ( i == NAME_LAST )
					for ( i = NAME_NUM_TYPES-1; i > NAME_LAST; i-- )
					{
						nc->names[i] = nc->names[NAME_LAST];
						nc->numNames[i] = nc->numNames[NAME_LAST];
					}
				break;
			}

		if ( i == NAME_NUM_TYPES )
			Com_Printf( "CL_ParseNames: unknown token \"%s\" ignored (names %s)\n", token, title );

	} while ( *text );
}

/*
======================
CL_ParseActors
======================
*/
void CL_ParseActors( char *title, char **text )
{
	nameCategory_t	*nc;
	char	*errhead = "CL_ParseActors: unexptected end of file (actors ";
	char	*token;
	int		i, j;
	
	// check for additions to existing name categories
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !strcmp( nc->title, title ) )
			break;

	// reset new category
	if ( i == numNameCats )
	{
		if ( numNameCats < MAX_NAMECATS )
		{
			memset( nc, 0, sizeof( nameCategory_t ) );
			numNameCats++;
		} else {
			Com_Printf( "Too many name categories, '%s' ignored.\n", title );
			return;
		}
	}
	strncpy( nc->title, title, MAX_VAR );

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( "CL_ParseActors: actor def \"%s\" without body ignored\n", title );
		if ( numNameCats - 1 == nc - nameCat ) numNameCats--;
		return;
	}

	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( i = 0; i < NAME_NUM_TYPES; i++ )
			if ( !strcmp( token, name_strings[i] ) )
			{
				// initialize list
				nc->models[i] = infoPos;
				nc->numModels[i] = 0;

				token = COM_EParse( text, errhead, title );
				if ( !*text ) break;
				if ( *token != '{' ) break;

				do {
					// get the path, body, head and skin
					for ( j = 0; j < 4; j++ )
					{
						token = COM_EParse( text, errhead, title );
						if ( !*text ) break;
						if ( *token == '}' ) break;

						if ( j == 3 && *token == '*' )
							*infoPos++ = 0;
						else
						{
							strcpy( infoPos, token );
							infoPos += strlen( token ) + 1;
						}
					}

					// only add complete actor info
					if ( j == 4 ) nc->numModels[i]++;
					else break;
				} 
				while ( *text );
				break;
			}

		if ( i == NAME_NUM_TYPES )
			Com_Printf( "CL_ParseNames: unknown token \"%s\" ignored (actors %s)\n", token, title );

	} while ( *text );
}


/*
======================
CL_ParseTeam
======================
*/
void CL_ParseTeam( char *title, char **text )
{
	nameCategory_t	*nc;
	teamDef_t		*td;
	char	*errhead = "CL_ParseTeam: unexptected end of file (team ";
	char	*token;
	int		i;
	
	// check for additions to existing name categories
	for ( i = 0, td = teamDef; i < numTeamDefs; i++, td++ )
		if ( !strcmp( td->title, title ) )
			break;

	// reset new category
	if ( i == numTeamDefs )
	{
		if ( numTeamDefs < MAX_TEAMDEFS ) 
		{
			memset( td, 0, sizeof( teamDef_t ) );
			numTeamDefs++;
		} else {
			Com_Printf( "Too many team definitions, '%s' ignored.\n", title );
			return;
		}
	}
	strncpy( td->title, title, MAX_VAR );

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( "CL_ParseTeam: team def \"%s\" without body ignored\n", title );
		if ( numTeamDefs - 1 == td - teamDef ) numTeamDefs--;
		return;
	}

	// initialize list
	td->cats = infoPos;
	td->num = 0;

	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
			if ( !strcmp( token, nc->title ) )
			{
				*infoPos++ = (char)i;
				td->num++;
				break;
			}

		if ( i == numNameCats )
			Com_Printf( "CL_ParseTeam: unknown token \"%s\" ignored (team %s)\n", token, title );

	} while ( *text );
}
