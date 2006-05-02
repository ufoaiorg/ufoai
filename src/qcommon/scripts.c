// scripts.c -- ufo scripts used in client and server
#include "qcommon.h"

/*
==============================================================================

			OBJECT DEFINITION INTERPRETER

==============================================================================
*/

char *skillNames[SKILL_NUM_TYPES - ABILITY_NUM_TYPES] =
{
	"close",
	"heavy",
	"assault",
	"precise",
	"explosive"
};

#define	ODOFS(x)	(int)&(((objDef_t *)0)->x)
#define	FDOFS(x)	(int)&(((fireDef_t *)0)->x)

typedef enum objdefs
{
	OD_PRIMARY,
	OD_SECONDARY,
	OD_PROTECTION,
	OD_HARDNESS
} objdef_t;

value_t od_vals[] =
{
	{ "primary",	V_NULL,			0 },
	{ "secondary",	V_NULL,			0 },
	{ "protection",	V_NULL,			0 },
	{ "hardness",	V_NULL,			0 },

	{ "name",		V_TRANSLATION_STRING,		ODOFS( name ) },
	{ "model",		V_STRING,		ODOFS( model ) },
	{ "image",		V_STRING,		ODOFS( image ) },
	{ "type",		V_STRING,		ODOFS( type ) },
	{ "category",	V_CHAR,			ODOFS( category ) },
	{ "shape",		V_SHAPE_SMALL,	ODOFS( shape ) },
	{ "scale",		V_FLOAT,		ODOFS( scale ) },
	{ "center",		V_VECTOR,		ODOFS( center ) },
	{ "weapon",		V_BOOL,			ODOFS( weapon ) },
	{ "twohanded",	V_BOOL,			ODOFS( twohanded ) },
	{ "thrown",		V_BOOL,			ODOFS( thrown ) },
	{ "ammo",		V_INT,			ODOFS( ammo ) },
	{ "reload",		V_INT,			ODOFS( reload ) },
	{ "price",		V_INT,			ODOFS( price ) },
	{ "buytype",	V_INT,			ODOFS( buytype ) },
	{ NULL,			V_NULL,			0}
};

// ===========================================================

#define FD_PRIMARY		0
#define FD_SECONDARY	1

value_t fdps[] =
{
	{ "name",		V_TRANSLATION_STRING,	FDOFS( name ) },
	{ "shotorg",	V_POS,		FDOFS( shotOrg ) },
	{ "projtl",		V_STRING,	FDOFS( projectile ) },
	{ "impact",		V_STRING,	FDOFS( impact ) },
	{ "hitbody",	V_STRING,	FDOFS( hitBody ) },
	{ "firesnd",	V_STRING,	FDOFS( fireSound ) },
	{ "impsnd",		V_STRING,	FDOFS( impactSound ) },
	{ "bodysnd",	V_STRING,	FDOFS( hitBodySound ) },
	{ "bncsnd",		V_STRING,	FDOFS( bounceSound ) },
	{ "sndonce",	V_BOOL,		FDOFS( soundOnce ) },
	{ "gravity",	V_BOOL,		FDOFS( gravity ) },
	{ "selfdet",	V_BOOL,		FDOFS( selfDetonate ) },
	{ "delay",		V_INT,		FDOFS( delay ) },
	{ "bounce",		V_INT,		FDOFS( bounce ) },
	{ "bncfac",		V_FLOAT,	FDOFS( bounceFac ) },
	{ "speed",		V_FLOAT,	FDOFS( speed ) },
	{ "spread",		V_POS,		FDOFS( spread ) },
	{ "crouch",		V_FLOAT,	FDOFS( crouch ) },
	{ "range",		V_FLOAT,	FDOFS( range ) },
	{ "shots",		V_INT,		FDOFS( shots ) },
	{ "ammo",		V_INT,		FDOFS( ammo ) },
	{ "rof",		V_FLOAT,	FDOFS( rof ) },
	{ "time",		V_INT,		FDOFS( time ) },
	{ "damage",		V_POS,		FDOFS( damage ) },
	{ "spldmg",		V_POS,		FDOFS( spldmg ) },
	{ "splrad",		V_FLOAT,	FDOFS( splrad ) },
	{ "dmgtype",	V_DMGTYPE,	FDOFS( dmgtype ) },
	{ "irgoggles",	V_BOOL,		FDOFS( irgoggles ) },

	{ NULL,	0, 0 }
};

// ===========================================================


/*
======================
Com_ParseFire
======================
*/
void Com_ParseFire( char *name, char **text, fireDef_t *fd )
{
	value_t	*fdp;
	char	*errhead = _("Com_ParseFire: unexptected end of file");
	char	*token;

	// get its body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseFire: fire definition \"%s\" without body ignored\n", name );
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) return;
		if ( *token == '}' ) return;

		for ( fdp = fdps; fdp->string; fdp++ )
			if ( !Q_stricmp( token, fdp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				Com_ParseValue( fd, token, fdp->type, fdp->ofs );
				break;
			}

		if ( !fdp->string )
		{
			if ( !Q_strncmp( token, "skill", 5 ) )
			{
				int skill;
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				for ( skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++ )
					if ( !Q_stricmp( skillNames[skill - ABILITY_NUM_TYPES], token ) )
					{
						fd->weaponSkill = skill;
						break;
					}
				if ( skill >= SKILL_NUM_TYPES )
					Com_Printf( "Com_ParseFire: unknown weapon skill \"%s\" ignored (weapon %s)\n", token, name );
			}
			else Com_Printf( "Com_ParseFire: unknown token \"%s\" ignored (weapon %s)\n", token, name );
		}
	} while ( *text );
}


/*
======================
Com_ParseArmor
======================
*/
void Com_ParseArmor( char *name, char **text, short *ad )
{
	char	*errhead = "Com_ParseFire: unexptected end of file";
	char	*token;
	int		i;

	// get its body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseArmor: armor definition \"%s\" without body ignored\n", name );
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) return;
		if ( *token == '}' ) return;

		for ( i = 0; i < csi.numDTs; i++ )
			if ( !Q_strncmp( token, csi.dts[i], MAX_VAR ) )
			{
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				ad[i] = atoi( token );
				break;
			}

		if ( i >= csi.numDTs )
			Com_Printf( "Com_ParseArmor: unknown damage type \"%s\" ignored (in %s)\n", token, name );
	} while ( *text );
}


/*
======================
Com_ParseItem
======================
*/
void Com_ParseItem( char *name, char **text )
{
	char		*errhead = "Com_ParseItem: unexptected end of file (weapon ";
	value_t		*val;
	objDef_t	*od;
	char		*token;
	int			i;

	// search for menus with same name
	for ( i = 0; i < csi.numODs; i++ )
		if ( !Q_strncmp( name, csi.ods[i].name, MAX_VAR ) )
			break;

	if ( i < csi.numODs )
	{
		Com_Printf( "Com_ParseItem: weapon def \"%s\" with same name found, second ignored\n", name );
		return;
	}

	// initialize the menu
	od = &csi.ods[csi.numODs++];
	memset( od, 0, sizeof(objDef_t) );

	Q_strncpyz( od->kurz, name, MAX_VAR );

	// get it's body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseItem: weapon def \"%s\" without body ignored\n", name );
		csi.numODs--;
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( val = od_vals, i = 0; val->string; val++, i++ )
			if ( !Q_stricmp( token, val->string ) )
			{
				// found a definition
				if ( val->type != V_NULL )
				{
					// parse a value
					token = COM_EParse( text, errhead, name );
					if ( !*text ) break;

					Com_ParseValue( od, token, val->type, val->ofs );
				}
				else
				{
					// parse fire definitions
					if ( i == OD_PRIMARY ) Com_ParseFire( name, text, &od->fd[FD_PRIMARY] );
					else if ( i == OD_SECONDARY ) Com_ParseFire( name, text, &od->fd[FD_SECONDARY] );
					else if ( i == OD_PROTECTION ) Com_ParseArmor( name, text, od->protection );
					else if ( i == OD_HARDNESS ) Com_ParseArmor( name, text, od->hardness );
				}
				break;
			}

		if ( !val->string )
			Com_Printf( "Com_ParseItem: unknown token \"%s\" ignored (weapon %s)\n", token, name );

	} while ( *text );

	// get size
	for ( i = 7; i >= 0; i-- )
		if ( od->shape & (0x01010101 << i ) )
			break;
	od->sx = i+1;

	for ( i = 3; i >= 0; i-- )
		if ( od->shape & (0xFF << (i*8)) )
			break;
	od->sy = i+1;
}


/*
==============================================================================

			INVENTORY DEFINITION INTERPRETER

==============================================================================
*/

#define	IDOFS(x)	(int)&(((invDef_t *)0)->x)

value_t idps[] =
{
	{ "shape",	V_SHAPE_BIG,	IDOFS( shape ) },
	{ "single",	V_BOOL,			IDOFS( single ) },
	{ "armor",	V_BOOL,			IDOFS( armor ) },
	{ "all",	V_BOOL,			IDOFS( all ) },
	{ "temp",	V_BOOL,			IDOFS( temp ) },
	{ "in",		V_INT,			IDOFS( in ) },
	{ "out",	V_INT,			IDOFS( out ) },

	{ NULL,	0, 0 }
};

/*
======================
Com_ParseInventory
======================
*/
void Com_ParseInventory( char *name, char **text )
{
	char		*errhead = "Com_ParseInventory: unexptected end of file (inventory ";
	invDef_t	*id;
	value_t		*idp;
	char		*token;
	int			i;

	// search for containers with same name
	for ( i = 0; i < csi.numIDs; i++ )
		if ( !Q_strncmp( name, csi.ids[i].name, MAX_VAR ) )
			break;

	if ( i < csi.numIDs )
	{
		Com_Printf( "Com_ParseInventory: inventory def \"%s\" with same name found, second ignored\n", name );
		return;
	}

	// initialize the menu
	id = &csi.ids[csi.numIDs++];
	memset( id, 0, sizeof(invDef_t) );

	Q_strncpyz( id->name, name, MAX_VAR );

	// get it's body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseInventory: inventory def \"%s\" without body ignored\n", name );
		csi.numIDs--;
		return;
	}

	// special IDs
	if ( !Q_strncmp( name, "right", 5 ) ) csi.idRight = id - csi.ids;
	else if ( !Q_strncmp( name, "left", 4 ) ) csi.idLeft = id - csi.ids;
	else if ( !Q_strncmp( name, "belt", 4 ) ) csi.idBelt = id - csi.ids;
	else if ( !Q_strncmp( name, "armor", 5 ) ) csi.idArmor = id - csi.ids;
	else if ( !Q_strncmp( name, "floor", 5 ) ) csi.idFloor = id - csi.ids;
	else if ( !Q_strncmp( name, "equip", 5 ) ) csi.idEquip = id - csi.ids;

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) return;
		if ( *token == '}' ) return;

		for ( idp = idps; idp->string; idp++ )
			if ( !Q_stricmp( token, idp->string ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, name );
				if ( !*text ) return;

				Com_ParseValue( id, token, idp->type, idp->ofs );
				break;
			}

		if ( !idp->string )
			Com_Printf( "Com_ParseInventory: unknown token \"%s\" ignored (inventory %s)\n", token, name );

	} while ( *text );
}


/*
==============================================================================

			EQUIPMENT DEFINITION INTERPRETER

==============================================================================
*/

#define MAX_NAMECATS	64
#define MAX_INFOSTRING	65536

typedef enum model_script_s
{
	MODEL_PATH,
	MODEL_BODY,
	MODEL_HEAD,
	MODEL_SKIN,

	MODEL_NUM_TYPES
} model_script_t;

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

teamDesc_t	teamDesc[MAX_TEAMDEFS];

nameCategory_t	nameCat[MAX_NAMECATS];
teamDef_t		teamDef[MAX_TEAMDEFS];
char	infoStr[MAX_INFOSTRING];
char	*infoPos;
int		numNameCats = 0;
int		numTeamDefs = 0;
int		numTeamDesc = 0;

char *name_strings[NAME_NUM_TYPES] =
{
	"neutral",
	"female",
	"male",
	"lastname",
	"female_lastname",
	"male_lastname"
};


/*
======================
Com_ParseEquipment
======================
*/
void Com_ParseEquipment( char *name, char **text )
{
	char		*errhead = "Com_ParseEquipment: unexptected end of file (equipment ";
	equipDef_t	*ed;
	char		*token;
	int			i, n;

	// search for containers with same name
	for ( i = 0; i < csi.numEDs; i++ )
		if ( !Q_strncmp( name, csi.eds[i].name, MAX_VAR ) )
			break;

	if ( i < csi.numEDs )
	{
		Com_Printf( "Com_ParseEquipment: equipment def \"%s\" with same name found, second ignored\n", name );
		return;
	}

	// initialize the menu
	ed = &csi.eds[csi.numEDs++];
	memset( ed, 0, sizeof(equipDef_t) );

	Q_strncpyz( ed->name, name, MAX_VAR );

	// get it's body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseEquipment: equipment def \"%s\" without body ignored\n", name );
		csi.numEDs--;
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text || *token == '}' ) return;

		for ( i = 0; i < csi.numODs; i++ )
			if ( !Q_strncmp( token, csi.ods[i].kurz, MAX_VAR ) )
			{
				token = COM_EParse( text, errhead, name );
				if ( !*text || *token == '}' )
				{
					Com_Printf( "Com_ParseEquipment: unexpected end of equipment def \"%s\"\n", name );
					return;
				}
				n = atoi( token );
				if ( n ) ed->num[i] = n;
				break;
			}

		if ( i == csi.numODs )
			Com_Printf( "Com_ParseEquipment: unknown token \"%s\" ignored (equipment %s)\n", token, name );

	} while ( *text );
}


/*
==============================================================================

			NAME AND TEAM DEFINITION INTERPRETER

==============================================================================
*/

/*
======================
Com_GiveName
======================
*/
char returnName[MAX_VAR];
char *Com_GiveName( int gender, char *category )
{
	nameCategory_t	*nc;
	char	*pos;
	int		i, j, name;

	// search the name
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !Q_strncmp( category, nc->title, MAX_VAR ) )
		{
			// found category
			if ( !nc->numNames[gender] ) return NULL;
			name = nc->numNames[gender] * frand();

			// skip names
			pos = nc->names[gender];
			for ( j = 0; j < name; j++ )
				pos += strlen( pos ) + 1;

			// store the name
			Q_strncpyz( returnName, pos, MAX_VAR );
			return returnName;
		}

	// nothing found
	return NULL;
}

/*
======================
Com_GiveModel
======================
*/
//char returnModel[MAX_VAR];
char *Com_GiveModel( int type, int gender, char *category )
{
	nameCategory_t	*nc;
	char	*str;
	int		i, j, num;

	// search the name
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !Q_strncmp( category, nc->title, MAX_VAR ) )
		{
			// found category
			if ( !nc->numModels[gender] ) return NULL;
			num = (int)(nc->numModels[gender] * frand()) * 4;
			num += type;

			// skip models and unwanted info
			str = nc->models[gender];
			for ( j = 0; j < num; j++ )
				str += strlen( str ) + 1;

			// return the value
			return str;
		}

	// nothing found
	return NULL;
}

/*
======================
Com_GetModelAndName
======================
*/
int Com_GetModelAndName( char *team, char *path, char *body, char *head, char *name )
{
	teamDef_t	*td;
	char		*str;
	int		i, gender, category = 0;

	// get team definition
	for ( i = 0; i < numTeamDefs; i++ )
		if ( !Q_strncmp( team, teamDef[i].title, MAX_VAR ) )
			break;
	if ( i < numTeamDefs ) td = &teamDef[i];
	else
	{
		// search in name categories, if it isn't a team definition
		td = NULL;
		for ( i = 0; i < numNameCats; i++ )
			if ( !Q_strncmp( team, nameCat[i].title, MAX_VAR ) )
				break;
		if ( i == numNameCats )
		{
			// use default team
			if ( !numTeamDefs ) return 0;
			else td = &teamDef[0];
		}
		else category = i;
	}

	// get the models
	while ( team )
	{
		gender = frand()*NAME_LAST;
		if ( td ) category = (int)td->cats[(int)(frand()*td->num)];

		// get name
		if ( name )
		{
			str = Com_GiveName( gender, nameCat[category].title );
			if ( !str ) continue;
			Q_strncpyz( name, str, MAX_VAR );
			Q_strcat( name, MAX_VAR, " " );

			str = Com_GiveName( gender + LASTNAME, nameCat[category].title );
			if ( !str ) continue;
			Q_strcat( name, MAX_VAR, str );
		}

		// get model
		if ( path )
		{
			str = Com_GiveModel( MODEL_PATH, gender, nameCat[category].title );
			if ( !str ) continue;
			Q_strncpyz( path, str, MAX_VAR );
		}

		if ( body )
		{
			str = Com_GiveModel( MODEL_BODY, gender, nameCat[category].title );
			if ( !str ) continue;
			Q_strncpyz( body, str, MAX_VAR );
		}

		if ( head )
		{
			str = Com_GiveModel( MODEL_HEAD, gender, nameCat[category].title );
			if ( !str ) continue;
			Q_strncpyz( head, str, MAX_VAR );
		}

		str = Com_GiveModel( MODEL_SKIN, gender, nameCat[category].title );
		if ( !str ) continue;
		return atoi( str );
	}
	return 0;
}

/*
======================
Com_ParseNames
======================
*/
void Com_ParseNames( char *title, char **text )
{
	nameCategory_t	*nc;
	char	*errhead = "Com_ParseNames: unexptected end of file (names ";
	char	*token;
	int		i;

	// check for additions to existing name categories
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !Q_strncmp( nc->title, title, MAX_VAR ) )
			break;

	// reset new category
	if ( i == numNameCats )
	{
		memset( nc, 0, sizeof( nameCategory_t ) );
		numNameCats++;
	}
	Q_strncpyz( nc->title, title, MAX_VAR );

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseNames: name def \"%s\" without body ignored\n", title );
		if ( numNameCats - 1 == nc - nameCat ) numNameCats--;
		return;
	}

	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( i = 0; i < NAME_NUM_TYPES; i++ )
			if ( !Q_strcmp( token, name_strings[i] ) )
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
			Com_Printf( "Com_ParseNames: unknown token \"%s\" ignored (names %s)\n", token, title );

	} while ( *text );
}


/*
======================
Com_ParseActors
======================
*/
void Com_ParseActors( char *title, char **text )
{
	nameCategory_t	*nc;
	char	*errhead = "Com_ParseActors: unexptected end of file (actors ";
	char	*token;
	int		i, j;

	// check for additions to existing name categories
	for ( i = 0, nc = nameCat; i < numNameCats; i++, nc++ )
		if ( !Q_strncmp( nc->title, title, MAX_VAR ) )
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
	Q_strncpyz( nc->title, title, MAX_VAR );

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseActors: actor def \"%s\" without body ignored\n", title );
		if ( numNameCats - 1 == nc - nameCat ) numNameCats--;
		return;
	}

	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( i = 0; i < NAME_NUM_TYPES; i++ )
			if ( !Q_strcmp( token, name_strings[i] ) )
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
			Com_Printf( "Com_ParseNames: unknown token \"%s\" ignored (actors %s)\n", token, title );

	} while ( *text );
}

#define	PARSETEAMDESC(x)	(int)&(((teamDesc_t *)0)->x)

value_t teamDescValues[] =
{
	{ "name",	V_TRANSLATION_STRING,	PARSETEAMDESC( name ) },
	{ "text",	V_TRANSLATION2_STRING,	PARSETEAMDESC( text ) },
	{ "notes",	V_TRANSLATION2_STRING,	PARSETEAMDESC( notes ) },
	{ "autopsy",	V_TRANSLATION2_STRING,	PARSETEAMDESC( autopsy ) },

	{ NULL,	0, 0 }
};


/*
======================
Com_ParseTeamDesc
======================
*/
void Com_ParseTeamDesc( char *title, char **text )
{
	teamDesc_t		*td;
	char	*errhead = _("Com_ParseTeamDesc: unexptected end of file (teamdesc ");
	char	*token;
	int	i;
	value_t	*v;

	// check for additions to existing team descriptions
	for ( i = 0, td = teamDesc; i < numTeamDesc; i++, td++ )
		if ( !Q_strncmp( td->title, title, MAX_VAR ) )
			break;

	// reset new category
	if ( i >= MAX_TEAMDEFS )
	{
		Com_Printf( "Too many team descriptions, '%s' ignored.\n", title );
		return;
	}
	memset( td, 0, sizeof( teamDesc_t ) );
	numTeamDesc++;
	Q_strncpyz( td->title, title, MAX_VAR );

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseTeamDesc: team desc \"%s\" without body ignored\n", title );
		if ( numTeamDesc - 1 == td - teamDesc )
			numTeamDesc--;
		return;
	}

	do {
		// get the name type
		token = COM_EParse( text, errhead, title );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		for ( v = teamDescValues; v->string; v++ )
			if ( !Q_strncmp( token, v->string, sizeof(v->string) ) )
			{
				// found a definition
				token = COM_EParse( text, errhead, title );
				if ( !*text ) return;

				Com_ParseValue( td, token, v->type, v->ofs );
				break;
			}

		if ( !v->string )
			Com_Printf( "Com_ParseTeamDesc: unknown token \"%s\" ignored (teamdesc %s)\n", token, title );
	} while ( *text );
}

/*
======================
Com_ParseTeam
======================
*/
void Com_ParseTeam( char *title, char **text )
{
	nameCategory_t	*nc;
	teamDef_t		*td;
	char	*errhead = "Com_ParseTeam: unexptected end of file (team ";
	char	*token;
	int		i;

	// check for additions to existing name categories
	for ( i = 0, td = teamDef; i < numTeamDefs; i++, td++ )
		if ( !Q_strncmp( td->title, title, MAX_VAR ) )
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
	Q_strncpyz( td->title, title, MAX_VAR );

	// get name list body body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseTeam: team def \"%s\" without body ignored\n", title );
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
			if ( !Q_strncmp( token, nc->title, MAX_VAR ) )
			{
				*infoPos++ = (char)i;
				td->num++;
				break;
			}

		if ( i == numNameCats )
			Com_Printf( "Com_ParseTeam: unknown token \"%s\" ignored (team %s)\n", token, title );

	} while ( *text );
}


/*
==============================================================================

			DAMAGE TYPES INTERPRETER

==============================================================================
*/

void Com_ParseDamageTypes( char *name, char **text )
{
	char		*errhead = "Com_ParseTypes: unexptected end of file (weapon ";
	char		*token;
	int			i;

	// get it's body
	token = COM_Parse( text );

	if ( !*text || *token != '{' )
	{
		Com_Printf( "Com_ParseTypes: damage type list \"%s\" without body ignored\n", name );
		csi.numODs--;
		return;
	}

	do {
		token = COM_EParse( text, errhead, name );
		if ( !*text ) break;
		if ( *token == '}' ) break;

		// search for damage types with same name
		for ( i = 0; i < csi.numDTs; i++ )
			if ( !Q_strncmp( token, csi.dts[i], MAX_VAR ) )
				break;

		if ( i >= csi.numDTs )
		{
			if ( csi.numDTs >= MAX_DAMAGETYPES )
				Sys_Error( "Com_ParseTypes: Too many damage types.\n" );

			Q_strncpyz( csi.dts[csi.numDTs], token, MAX_VAR );
			csi.numDTs++;
		}
	} while ( *text );
}


/*
==============================================================================

			MAIN SCRIPT PARSING FUNCTION

==============================================================================
*/

/*
=================
Com_AddObjectLinks
=================
*/
void Com_AddObjectLinks( void )
{
	objDef_t	*od;
	char	kurz[MAX_VAR];
	char	*underline;
	int		i, j;

	// reset links
	for ( i = 0, od = csi.ods; i < csi.numODs; i++, od++ )
		od->link = NONE;

	// add weapon link to ammo
	for ( i = 0, od = csi.ods; i < csi.numODs; i++, od++ )
		if ( !Q_strncmp( od->type, "ammo", 4 ) )
		{
			// check for the underline
			Q_strncpyz( kurz, od->kurz, MAX_VAR );
			underline = strchr( kurz, '_' );
			if ( !underline ) continue;
			*underline = 0;

			// search corresponding weapon
			for ( j = 0; j < csi.numODs; j++ )
				if ( !Q_strncmp( csi.ods[j].kurz, kurz, MAX_VAR ) )
				{
					csi.ods[i].link = j;
					break;
				}
		}
}


/*
=================
Com_ParseScripts
=================
*/
void Com_ParseScripts( void )
{
	char		*type, *name, *text;

	// reset csi basic info
	Com_InitCSI( &csi );
	csi.idLeft = csi.idRight = csi.idFloor = csi.idEquip = NONE;

	// reset name and team def counters
	numNameCats = 0;
	numTeamDefs = 0;
	infoPos = infoStr;

	// pre-stage parsing
	FS_BuildFileList( "ufos/*.ufo" );
	text = NULL;

	while ( ( type = FS_NextScriptHeader( "ufos/*.ufo", &name, &text ) ) )
	{
		if ( !Q_strncmp( type, "damagetypes", 11 ) ) Com_ParseDamageTypes( name, &text );
	}

	// stage one parsing
	FS_NextScriptHeader( NULL, NULL, NULL );
	text = NULL;

	while ( ( type = FS_NextScriptHeader( "ufos/*.ufo", &name, &text ) ) )
	{
		// server/client scripts
		if ( !Q_strncmp( type, "item", 4 ) ) Com_ParseItem( name, &text );
		else if ( !Q_strncmp( type, "inventory", 9 ) ) Com_ParseInventory( name, &text );
		else if ( !Q_strncmp( type, "names", 5 ) ) Com_ParseNames( name, &text );
		else if ( !Q_strncmp( type, "actors", 6 ) ) Com_ParseActors( name, &text );
		else if ( !dedicated->value ) CL_ParseScriptFirst( type, name, &text );
	}

	// add object links
	Com_AddObjectLinks();

	// stage two parsing (weapon/inventory dependant stuff)
	FS_NextScriptHeader( NULL, NULL, NULL );
	text = NULL;

	while ( ( type = FS_NextScriptHeader( "ufos/*.ufo", &name, &text ) ) )
	{
		// server/client scripts
		if ( !Q_strncmp( type, "equipment", 9 ) ) Com_ParseEquipment( name, &text );
		else if ( !Q_strncmp( type, "teamdesc", 8 ) ) Com_ParseTeamDesc( name, &text );
		else if ( !Q_strncmp( type, "team", 4 ) ) Com_ParseTeam( name, &text );
		else if ( !dedicated->value ) CL_ParseScriptSecond( type, name, &text );
	}

	Com_Printf( "Shared Client/Server Info loaded\n" );
}

