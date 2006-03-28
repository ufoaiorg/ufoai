// cl_menu.c -- client menu functions

#include "client.h"

#define	NOFS(x)		(int)&(((menuNode_t *)0)->x)

#define MAX_MENUS			64
#define MAX_MENUNODES		2048
#define MAX_MENUACTIONS		4096
#define MAX_MENUSTACK		16

#define MAX_MENU_COMMAND	32
#define MAX_MENU_PICLINK	64

#define C_UNIT				25
#define C_UNDEFINED			0xFE

// ===========================================================

typedef struct menuAction_s
{
	int	type;
	void	*data;
	struct	menuAction_s *next;
} menuAction_t;

typedef struct menuDepends_s
{
	char var[MAX_VAR];
	char value[MAX_VAR];
} menuDepends_t;

typedef struct menuNode_s
{
	void		*data[6]; // needs to be first
	char	name[MAX_VAR];
	int		type;
	vec3_t	origin, scale, angles, center;
	vec2_t	pos, size, texh, texl;
	byte		state;
	byte		align;
	byte		invis, blend;
	int		mousefx;
	int		num, height;
	vec4_t	color;
	menuAction_t	*click, *rclick, *mclick, *mouseIn, *mouseOut;
	menuDepends_t	depends;
	struct menuNode_s	*next;
} menuNode_t;

typedef struct menu_s
{
	char		name[MAX_VAR];
	menuNode_t	*firstNode, *initNode, *closeNode, *renderNode, *popupNode;
} menu_t;

// ===========================================================

typedef enum ea_s
{
	EA_NULL,
	EA_CMD,

	EA_CALL,
	EA_NODE,
	EA_VAR,

	EA_NUM_EVENTACTION
} ea_t;

char *ea_strings[EA_NUM_EVENTACTION] =
{
	"",
	"cmd",

	"",
	"*",
	"&"
};

int ea_values[EA_NUM_EVENTACTION] =
{
	V_NULL,
	V_LONGSTRING,

	V_NULL,
	V_NULL,
	V_NULL
};

// ===========================================================

typedef enum ne_s
{
	NE_NULL,
	NE_CLICK,
	NE_RCLICK,
	NE_MCLICK,
	NE_MOUSEIN,
	NE_MOUSEOUT,

	NE_NUM_NODEEVENT
} ne_t;

char *ne_strings[NE_NUM_NODEEVENT] =
{
	"",
	"click",
	"rclick",
	"mclick",
	"in",
	"out"
};

int	ne_values[NE_NUM_NODEEVENT] =
{
	0,
	NOFS( click ),
	NOFS( rclick ),
	NOFS( mclick ),
	NOFS( mouseIn ),
	NOFS( mouseOut )
};

// ===========================================================

value_t nps[] =
{
	{ "invis",		V_BOOL,		NOFS( invis ) },
	{ "mousefx",	V_BOOL,		NOFS( mousefx ) },
	{ "blend",		V_BOOL,		NOFS( blend ) },
	{ "pos",		V_POS,		NOFS( pos ) },
	{ "size",		V_POS,		NOFS( size ) },
	{ "texh",		V_POS,		NOFS( texh ) },
	{ "texl",		V_POS,		NOFS( texl ) },
	{ "format",		V_POS,		NOFS( texh ) },
	{ "origin",		V_VECTOR,	NOFS( origin ) },
	{ "center",		V_VECTOR,	NOFS( center ) },
	{ "scale",		V_VECTOR,	NOFS( scale ) },
	{ "angles",		V_VECTOR,	NOFS( angles ) },
	{ "num",		V_INT,		NOFS( num ) },
	// 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t
	{ "tooltip",		V_STRING,	-5 },
	{ "image",		V_STRING,	0 },
	{ "md2",		V_STRING,	0 },
	{ "anim",		V_STRING,	-1 },
	{ "tag",		V_STRING,	-2 },
	{ "skin",		V_STRING,	-3 },
	// -4 is animation state
	{ "string",		V_STRING,	0 },
	{ "font",		V_STRING,	-1 },
	{ "max",		V_FLOAT,	0 },
	{ "min",		V_FLOAT,	-1 },
	{ "current",	V_FLOAT,	-2 },
	{ "weapon",		V_STRING,	0 },
	{ "color",		V_COLOR,	NOFS( color ) },
	{ "align",		V_ALIGN,	NOFS( align ) },
	{ "if",			V_IF,		NOFS( depends ) },
	{ NULL,			V_NULL,		0 },
};


// ===========================================================

typedef enum mn_s
{
	MN_NULL,
	MN_CONFUNC,
	MN_CVARFUNC,
	MN_FUNC,
	MN_ZONE,
	MN_PIC,
	MN_STRING,
	MN_TEXT,
	MN_BAR,
	MN_MODEL,
	MN_CONTAINER,
	MN_ITEM,
	MN_MAP,
	MN_BASEMAP,

	MN_NUM_NODETYPE
} mn_t;

char *nt_strings[MN_NUM_NODETYPE] =
{
	"",
	"confunc",
	"cvarfunc",
	"func",
	"zone",
	"pic",
	"string",
	"text",
	"bar",
	"model",
	"container",
	"item",
	"map",
	"basemap"
};


// ===========================================================


menuAction_t	menuActions[MAX_MENUACTIONS];
menuNode_t		menuNodes[MAX_MENUNODES];
menu_t			menus[MAX_MENUS];

int			numActions;
int			numNodes;
int			numMenus;

byte		*adata, *curadata;
int			adataize = 0;

menu_t		*menuStack[MAX_MENUSTACK];
int			menuStackPos;

inventory_t	*menuInventory = NULL;
char		*menuText[MAX_MENUTEXTS];

cvar_t		*mn_escpop;

/*
==============================================================

ACTION EXECUTION

==============================================================
*/

/*
=================
MN_GetNode
=================
*/
menuNode_t *MN_GetNode( menu_t *menu, char *name )
{
	menuNode_t	*node;

	for ( node = menu->firstNode; node; node = node->next )
		if ( !strcmp( name, node->name ) )
			break;

	return node;
}


/*
=================
MN_GetReferenceString
=================
*/
char *MN_GetReferenceString( menu_t *menu, char *ref )
{
	if ( !ref ) return NULL;
	if ( *ref == '*' )
	{
		char	ident[MAX_VAR];
		char	*text;
		char	*token;

		// get the reference and the name
		text = ref+1;
		token = COM_Parse( &text );
		if ( !text ) return NULL;
		strncpy( ident, token, MAX_VAR );
		token = COM_Parse( &text );
		if ( !text ) return NULL;

		if ( !strcmp( ident, "cvar" ) )
		{
			// get the cvar value
			return Cvar_VariableString( token );
		}
		else
		{
			menuNode_t	*refNode;
			value_t		*val;

			// draw a reference to a node property
			refNode = MN_GetNode( menu, ident );
			if ( !refNode )
				return NULL;

			// get the property
			for ( val = nps; val->type; val++ )
				if ( !strcmp( token, val->string ) )
					break;

			if ( !val->type )
				return NULL;

			// get the string
			if ( val->ofs > 0 )
				return Com_ValueToStr( refNode, val->type, val->ofs );
			else
				return Com_ValueToStr( refNode->data[-val->ofs], val->type, 0 );
		}
	}
	else
	{
		// just get the data
		return ref;
	}
}


/*
=================
MN_GetReferenceFloat
=================
*/
float MN_GetReferenceFloat( menu_t *menu, void *ref )
{
	if ( !ref ) return 0.0;
	if ( *(char *)ref == '*' )
	{
		char	ident[MAX_VAR];
		char	*text;
		char	*token;

		// get the reference and the name
		text = (char *)ref+1;
		token = COM_Parse( &text );
		if ( !text ) return 0.0;
		strncpy( ident, token, MAX_VAR );
		token = COM_Parse( &text );
		if ( !text ) return 0.0;

		if ( !strcmp( ident, "cvar" ) )
		{
			// get the cvar value
			return Cvar_VariableValue( token );
		}
		else
		{
			menuNode_t	*refNode;
			value_t		*val;

			// draw a reference to a node property
			refNode = MN_GetNode( menu, ident );
			if ( !refNode )
				return 0.0;

			// get the property
			for ( val = nps; val->type; val++ )
				if ( !strcmp( token, val->string ) )
					break;

			if ( val->type != V_FLOAT )
				return 0.0;

			// get the string
			if ( val->ofs > 0.0 )
				return *(float *)((byte *)refNode + val->ofs);
			else
				return *(float *)refNode->data[-val->ofs];
		}
	}
	else
	{
		// just get the data
		return *(float *)ref;
	}
}

/*=================
MN_StartServer

starts a server and checks if the server
loads a team unless he is a dedicated
server admin
=================*/
void MN_StartServer ( void )
{
	if ( Cmd_Argc() <= 1 )
	{
		Com_Printf( _("Usage: mn_startserver <name>\n") );
		return;
	}
	else
		Com_Printf("MN_StartServer\n");

	if ( Cvar_VariableValue("dedicated") > 0 )
		Com_DPrintf("Dedicated server needs no team\n");
	else if ( !numOnTeam )
	{
		MN_Popup( _("Error"), _("Assemble a team first") );
		return;
	}
	else
		Com_DPrintf("There are %i members on team\n", numOnTeam );

	if ( Cvar_VariableValue("sv_teamplay")
	  && Cvar_VariableValue("maxsoldiersperplayer") > Cvar_VariableValue("maxsoldiers") )
	{
		MN_Popup( _("Settings doesn't make sense"), _("Set playersoldiers lower than teamsoldiers") );
		return;
	}

	Cbuf_ExecuteText( EXEC_NOW, va("map %s\n", Cmd_Argv(1) ) );
}

/*
* Com_MergeShapes
* Will merge the second shape (=itemshape) into the first one on the position x/y
*/
void Com_MergeShapes(int *shape, int itemshape, int x, int y)
{
	//TODO-security: needs some checks for max-values!
	int i;
	for ( i=0; (i<4)&&(y+i<16); i++ ) {
		shape[y+i] |= ((itemshape >> i*8) & 0xFF) << x;
	}
}

/*
* Com_CheckShape
* Checks the shape if there is a 1-bit on the position x/y.
*/
qboolean Com_CheckShape( int shape[16], int x, int y )
{
	int row = shape[y];
	int position=pow(2, x);
	if ((row & position) == 0)
		return false;
	else
		return true;
}

/*
* MN_DrawFree
* Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel).
*/
void MN_DrawFree (int posx, int posy, int sizex, int sizey)
{
	vec4_t color;
	VectorSet( color, 0.0f, 1.0f, 0.0f );
	color[3] = 0.7f;
	re.DrawFill(posx, posy, sizex, sizey, ALIGN_UL, color);
	re.DrawColor( NULL );
}

/*
* MN_InvDrawFree
* Draws the free and useable inventory positions when dragging an item.
*/
void MN_InvDrawFree(inventory_t *inv, menuNode_t *node)
{
	int item = dragItem.t; // get the 'type' of the draged item
	int container = node->mousefx;
	int itemshape;
	int free[16];	//The shapoe of the free positions.
	int j;
	int x, y;

	if (mouseSpace == MS_DRAG) {// && !(inv->c[csi.idEquip])) { // Draw only in dragging-mode (and not for the equip-'floor'?)
		assert(inv);
		for ( j=0; j<16; j++ ) free[j] = 0;

		/* TODO: add armor support
		if (csi.ids[container].armor) {
			if (	Com_CheckToInventory( inv, item, container, 0, 0 )		//if container free OR..
			||	(node->mousefx==dragFrom) ) {	// ..OR the dragged-item is in it
				MN_DrawFree( node->pos[0], node->pos[1],  node->size[0], node->size[1] );
			}
		} else*/
		if (csi.ids[container].single) { // if  single container (hands)
			if (	Com_CheckToInventory( inv, item, container, 0, 0 )	//if container free OR..
			||	(node->mousefx==dragFrom) ) {	// ..OR the dragged-item is in it
				MN_DrawFree( node->pos[0], node->pos[1],  node->size[0], node->size[1] );
			}
		} else {
			for ( y = 0; y < 16; y++ ) {
				for ( x = 0; x < 32; x++ ) {
					if ( Com_CheckToInventory( inv, item, container, x, y ))  { //Check if the curretn position is useable (topleft of the item)
						itemshape = csi.ods[dragItem.t].shape;
						Com_MergeShapes(free, itemshape, x, y); //add '1's to each position the item is 'blocking'
					}
					if (Com_CheckShape(csi.ids[container].shape, x, y)) { //Only draw on existing positions
						if (Com_CheckShape(free, x, y)) {
							MN_DrawFree( node->pos[0]+x*C_UNIT, node->pos[1]+y*C_UNIT, C_UNIT, C_UNIT );
						}
					}
				} // for x
			} //for y
		}
	}
}


/*=================
MN_Popup

Popup in geoscape
=================*/
void MN_Popup (char* title, char* text)
{
	menuText[TEXT_POPUP] = title;
	menuText[TEXT_POPUP_INFO] = text;
	Cbuf_ExecuteText( EXEC_NOW, "game_timestop" );
	MN_PushMenu( "popup" );
}

/*
=================
MN_ExecuteActions
=================
*/
void MN_ExecuteActions( menu_t *menu, menuAction_t *first )
{
	menuAction_t	*action;
	byte	*data;

	for ( action = first; action; action = action->next )
		switch ( action->type )
		{
		case EA_NULL:
			// do nothing
			break;
		case EA_CMD:
			// execute a command
			if ( action->data ) Cbuf_AddText( va( "%s\n", action->data ) );
			break;
		case EA_CALL:
			// call another function
			MN_ExecuteActions( menu, **(menuAction_t ***)action->data );
			break;
		case EA_NODE:
			// set a property
			if ( action->data )
			{
				menuNode_t	*node;
				int		np;

				data = action->data;
				data += strlen( action->data ) + 1;
				np = *((int *)data);
				data += sizeof(int);

				// search the node
				node = MN_GetNode( menu, (char *)action->data );

				if ( !node )
				{
					// didn't find node -> "kill" action and print error
					action->type = EA_NULL;
					Com_Printf( _("MN_ExecuteActions: node \"%s\" doesn't exist\n"), (char *)action->data );
					break;
				}

				if ( nps[np].ofs > 0 )
					Com_SetValue( node, (char *)data, nps[np].type, nps[np].ofs );
				else
					node->data[-(nps[np].ofs)] = data;

			}
			break;
		default:
			Sys_Error( _("unknown action type\n") );
			break;
		}
}


/*
=================
MN_Command
=================
*/
void MN_Command( void )
{
	menuNode_t	*node;
	char	*name;
	int		i;

	name = Cmd_Argv( 0 );
	for ( i = 0; i < numMenus; i++ )
		for ( node = menus[i].firstNode; node; node = node->next )
			if ( node->type == MN_CONFUNC && !strcmp( node->name, name ) )
			{
				// found the node
				MN_ExecuteActions( &menus[i], node->click );
				return;
			}
}


/*
==============================================================

MENU ZONE DETECTION

==============================================================
*/

/*
=================
MN_MapToScreen
=================
*/
qboolean MN_MapToScreen( menuNode_t *node, vec2_t pos, int *x, int *y )
{
	float sx;

	// get "raw" position
	sx = pos[0]/360 + ccs.center[0]-0.5;

	// shift it on screen
	if ( sx < -0.5 ) sx += 1.0;
	else if ( sx > +0.5 ) sx -= 1.0;

	*x = node->pos[0] + 0.5*node->size[0] - sx * node->size[0] * ccs.zoom;
	*y = node->pos[1] + 0.5*node->size[1] - (pos[1]/180 + ccs.center[1]-0.5) * node->size[1] * ccs.zoom;

	if ( *x < node->pos[0] && *y < node->pos[1] && *x > node->pos[0]+node->size[0] && *y > node->pos[1]+node->size[1] ) return false;
	return true;
}


/*
=================
MN_ScreenToMap
=================
*/
void MN_ScreenToMap( menuNode_t *node, int x, int y, vec2_t pos )
{
	pos[0] = (((node->pos[0] - x) / node->size[0] + 0.5) / ccs.zoom - (ccs.center[0]-0.5)) * 360.0;
	pos[1] = (((node->pos[1] - y) / node->size[1] + 0.5) / ccs.zoom - (ccs.center[1]-0.5)) * 180.0;

	while ( pos[0] > 180.0 ) pos[0] -= 360.0;
	while ( pos[0] <-180.0 ) pos[0] += 360.0;
}


/*
=================
PolarToVec
=================
*/
void PolarToVec( vec2_t a, vec3_t v )
{
	float p, t;
	p = a[0]*M_PI/180;
	t = a[1]*M_PI/180;
	VectorSet( v, cos(p)*cos(t), sin(p)*cos(t), sin(t) );
}


/*
=================
VecToPolar
=================
*/
void VecToPolar( vec3_t v, vec2_t a )
{
	a[0] = 180/M_PI * atan2( v[1], v[0] );
	a[1] = 90 - 180/M_PI * acos(v[2]);
}


/*
=================
MN_MapCalcLine
=================
*/
void MN_MapCalcLine( vec2_t start, vec2_t end, mapline_t *line )
{
	vec3_t s, e, v;
	vec3_t normal;
	vec2_t trafo, sa, ea;
	float cosTrafo, sinTrafo;
	float phiStart, phiEnd, dPhi, phi;
	float *p, *last;
	int   i, n;

	// get plane normal
	PolarToVec( start, s );
	PolarToVec( end, e );
	CrossProduct( s, e, normal );
	VectorNormalize( normal );

	// get transformation
	VecToPolar( normal, trafo );
	cosTrafo = cos(trafo[1]*M_PI/180);
	sinTrafo = sin(trafo[1]*M_PI/180);

	sa[0] = start[0]-trafo[0];
	sa[1] = start[1];
	PolarToVec( sa, s );
	ea[0] = end[0]-trafo[0];
	ea[1] = end[1];
	PolarToVec( ea, e );

	phiStart = atan2( s[1], cosTrafo*s[2] - sinTrafo*s[0] );
	phiEnd   = atan2( e[1], cosTrafo*e[2] - sinTrafo*e[0] );

	// get waypoints
	if ( phiEnd < phiStart - M_PI ) phiEnd += 2*M_PI;
	if ( phiEnd > phiStart + M_PI ) phiEnd -= 2*M_PI;

	n = (phiEnd - phiStart)/M_PI * LINE_MAXSEG;
	if ( n > 0 ) n = n+1;
	else n = -n+1;

//	Com_Printf( "#(%3.1f %3.1f) -> (%3.1f %3.1f)\n", start[0], start[1], end[0], end[1] );

	line->dist = fabs(phiEnd - phiStart) / n * 180/M_PI;
	line->n = n+1;
	dPhi = (phiEnd - phiStart) / n;
	p = NULL;
	for ( phi = phiStart, i = 0; i <= n; phi += dPhi, i++ )
	{
		last = p;
		p = line->p[i];
		VectorSet( v, -sinTrafo*cos(phi), sin(phi), cosTrafo*cos(phi) );
		VecToPolar( v, p );
		p[0] += trafo[0];

		if ( !last )
		{
			while ( p[0] < -180.0 ) p[0] += 360.0;
			while ( p[0] > +180.0 ) p[0] -= 360.0;
		} else {
			while ( p[0] - last[0] > +180.0 ) p[0] -= 360.0;
			while ( p[0] - last[0] < -180.0 ) p[0] += 360.0;
		}
	}
}


/*
=================
MN_MapDrawLine
=================
*/
void MN_MapDrawLine( menuNode_t *node, mapline_t *line )
{
	vec4_t color;
	int pts[LINE_MAXPTS*2];
	int *p;
	int i, start, old;

	// set color
	VectorSet( color, 1.0f, 0.5f, 0.5f );
	color[3] = 1.0f;
	re.DrawColor( color );

	// draw
	start = 0;
	old = 512;
	for ( i = 0, p = pts; i < line->n; i++, p += 2 )
	{
		MN_MapToScreen( node, line->p[i], p, p+1 );

		if ( i > start && abs(p[0] - old) > 512 )
		{
			// shift last point
			int diff;
			if ( p[0] - old > 512 ) diff = -node->size[0] * ccs.zoom;
			else diff = node->size[0] * ccs.zoom;
			p[0] += diff;

			// wrap around screen border
			re.DrawLineStrip( i-start+1, pts );

			// shift first point, continue drawing
			start = --i;
			pts[0] = p[-2] - diff;
			pts[1] = p[-1];
			p = pts;
		}
		old = p[0];
	}

	re.DrawLineStrip( i-start, pts );
	re.DrawColor( NULL );
}


/*
=================
MN_FindContainer
=================
*/
void MN_FindContainer( menuNode_t *node )
{
	invDef_t	*id;
	int		i, j;

	for ( i = 0, id = csi.ids; i < csi.numIDs; id++, i++ )
		if ( !strcmp( node->name, id->name ) )
			break;

	if ( i == csi.numIDs ) node->mousefx = NONE;
	else node->mousefx = id - csi.ids;

	for ( i = 31; i >= 0; i-- )
	{
		for ( j = 0; j < 16; j++ )
			if ( id->shape[j] & (1<<i) )
				break;
		if ( j < 16 ) break;
	}
	node->size[0] = C_UNIT * (i+1) + 0.01;

	for ( i = 15; i >= 0; i-- )
		if ( id->shape[i] & 0xFFFFFFFF )
			break;
	node->size[1] = C_UNIT * (i+1) + 0.01;
}

/*
=================
MN_CheckNodeZone
=================
*/
qboolean MN_CheckNodeZone( menuNode_t *node, int x, int y )
{
	int		sx, sy, tx, ty;

	// containers
	if ( node->type == MN_CONTAINER )
	{
		if ( node->mousefx == C_UNDEFINED ) MN_FindContainer( node );
		if ( node->mousefx == NONE ) return false;

		// check bounding box
		if ( x < node->pos[0] || y < node->pos[1] || x > node->pos[0] + node->size[0] || y > node->pos[1] + node->size[1] )
			return false;

		// found a container
		return true;
	}

	// check for click action
	if ( node->invis || ( !node->click && !node->rclick && !node->mclick &&
		!node->mouseIn && !node->mouseOut ) )
		return false;

	// get the rectangle size out of the pic if necessary
	if ( !node->size[0] || !node->size[1] )
	{
		if ( node->type == MN_PIC && node->data[0] )
		{
			if ( node->texh[0] && node->texh[1] ) {
				sx = node->texh[0] - node->texl[0];
				sy = node->texh[1] - node->texl[1];
			} else
				re.DrawGetPicSize( &sx, &sy, node->data[0] );
		}
		else return false;
	}
	else
	{
		sx = node->size[0];
		sy = node->size[1];
	}

	// rectangle test
	tx = x - node->pos[0];
	ty = y - node->pos[1];

	if ( tx < 0 || ty < 0 || tx > sx || ty > sy )
		return false;

	// on the node
	if ( node->type == MN_TEXT ) return (int)(ty / node->texh[0]) + 1;
	else return true;
}


/*
=================
MN_CursorOnMenu
=================
*/
qboolean MN_CursorOnMenu( int x, int y )
{
	menuNode_t	*node;
	menu_t		*menu;
	int		sp;

	sp = menuStackPos;

	while ( sp > 0 )
	{
		menu = menuStack[--sp];
		for ( node = menu->firstNode; node; node = node->next )
			if ( MN_CheckNodeZone( node, x, y ) )
			{
				// found an element
				return true;
			}

		if ( menu->renderNode )
		{
			// don't care about non-rendered windows
			if ( menu->renderNode->invis ) return true;
			else return false;
		}
	}

	return false;
}


/*
=================
MN_Drag
=================
*/
void MN_Drag( menuNode_t *node, int x, int y )
{
	int		px, py;

	if ( !menuInventory )
		return;

	if ( mouseSpace == MS_MENU )
	{
		invList_t *ic;

		px = (int)(x - node->pos[0]) / C_UNIT;
		py = (int)(y - node->pos[1]) / C_UNIT;

		// start drag (mousefx represents container number)
		ic = Com_SearchInInventory( menuInventory, node->mousefx, px, py );
		if ( ic )
		{
			// found item to drag
			mouseSpace = MS_DRAG;
			dragItem = ic->item;
			dragFrom = node->mousefx;
			dragFromX = ic->x;
			dragFromY = ic->y;
			CL_ItemDescription( ic->item.t );
		}
	} else {
		// end drag
		mouseSpace = MS_NULL;
		px = (int)((x - node->pos[0] - C_UNIT*((csi.ods[dragItem.t].sx-1)/2.0)) / C_UNIT);
		py = (int)((y - node->pos[1] - C_UNIT*((csi.ods[dragItem.t].sy-1)/2.0)) / C_UNIT);

		if ( selActor )
			MSG_WriteFormat( &cls.netchan.message, "bbsbbbbbb",
				clc_action, PA_INVMOVE, selActor->entnum, dragFrom, dragFromX, dragFromY, node->mousefx, px, py );
		else
		{
			invList_t *i;
			int et, sel;

			// sort equipment (tiny hack)
			i = NULL;
			et = -1;
			if ( node->mousefx == csi.idEquip )
			{
				// special item sorting for equipment
				i = Com_SearchInInventory( menuInventory, dragFrom, dragFromX, dragFromY );
				if ( i )
				{
					et = csi.ods[i->item.t].buytype;
					if ( et != equipType )
					{
						menuInventory->c[csi.idEquip] = equipment.c[et];
						Com_FindSpace( menuInventory, i->item.t, csi.idEquip, &px, &py );
						if ( px >= 32 && py >= 16 )
						{
							menuInventory->c[csi.idEquip] = equipment.c[equipType];
							return;
						}
					}
				}
			}

			// move the item
			Com_MoveInInventory( menuInventory, dragFrom, dragFromX, dragFromY, node->mousefx, px, py, NULL, NULL );

			// end of hack
			if ( i && et != equipType )
			{
				equipment.c[et] = menuInventory->c[csi.idEquip];
				menuInventory->c[csi.idEquip] = equipment.c[equipType];
			}
			else equipment.c[equipType] = menuInventory->c[csi.idEquip];

			// update character info (for armor changes)
			sel = cl_selected->value;
			if ( sel >= 0 && sel < numWholeTeam )
				CL_CharacterCvars( &curTeam[sel] );
		}
	}

}


/*
=================
MN_BarClick
=================
*/
void MN_BarClick( menu_t *menu, menuNode_t *node, int x )
{
	char	var[MAX_VAR];
	float	frac, min;

	if ( !node->mousefx )
		return;

	strncpy( var, node->data[2], MAX_VAR );
	if ( !strcmp( var, "*cvar" ) )
		return;

	frac = (float)(x - node->pos[0]) / node->size[0];
	min = MN_GetReferenceFloat( menu, node->data[1] );
	Cvar_SetValue( &var[6], min + frac * ( MN_GetReferenceFloat( menu, node->data[0] ) - min ) );
}


/*
======================
MN_MapClick
======================
*/
void MN_MapClick( menuNode_t *node, int x, int y )
{
	aircraft_t *air;
	actMis_t *ms;
	int i, msx, msy;
	vec2_t	pos;

	// get map position
	MN_ScreenToMap( node, x, y, pos );

	// new base construction
	if ( mapAction == MA_NEWBASE )
	{
		newBasePos[0] = pos[0];
		newBasePos[1] = pos[1];
		MN_PushMenu( "popup_newbase" );
		return;
	}

	// mission selection
	for ( i = 0, ms = ccs.mission; i < ccs.numMissions; i++, ms++ )
	{
		if ( !MN_MapToScreen( node, ms->realPos, &msx, &msy ) )
			continue;
		if ( x >= msx-8 && x <= msx+8 && y >= msy-8 && y <= msy+8 )
		{
			selMis = ms;
			return;
		}
	}

	// base selection
	for ( i = 0; i < MAX_BASES; i++ )
	{
		if ( !MN_MapToScreen( node, bmBases[i].pos, &msx, &msy ) )
			continue;
		if ( x >= msx-8 && x <= msx+8 && y >= msy-8 && y <= msy+8 )
		{
			baseCurrent = &bmBases[i];
			Cvar_SetValue( "mn_base_id", i );
			MN_PushMenu( "bases" );
			return;
		}
	}

	// draw aircraft
	for ( i = 0, air = ccs.air; i < ccs.numAir; i++, air++ )
	{
		air = &ccs.air[i];
		if ( air->status > AIR_HOME )
		{
			MN_MapCalcLine( air->pos, pos, &air->route );
			air->status = AIR_TRANSIT;
			air->time = 0;
			air->point = 0;
		}
	}
}


/*
======================
MN_BaseMapClick
======================
*/
void MN_BaseMapClick( menuNode_t *node, int x, int y )
{
	int	a, b;
	assert(baseCurrent);

	if ( baseCurrent->buildingCurrent && baseCurrent->buildingCurrent->buildingStatus[baseCurrent->buildingCurrent->howManyOfThisType] == B_NOT_SET )
	{
		for ( b = BASE_SIZE-1; b >= 0; b-- )
			for ( a = 0; a < BASE_SIZE; a++ )
				if ( baseCurrent->map[b][a][baseCurrent->baseLevel] == -1
				  && x >= baseCurrent->posX[b][a][baseCurrent->baseLevel] && x < baseCurrent->posX[b][a][baseCurrent->baseLevel] + picWidth*ccs.basezoom
				  && y >= baseCurrent->posY[b][a][baseCurrent->baseLevel] && y < baseCurrent->posY[b][a][baseCurrent->baseLevel] + picHeight*ccs.basezoom )
				{
					MN_SetBuildingByClick( a, b );
					return;
				}
	}

	for ( b = BASE_SIZE-1; b >= 0; b-- )
		for ( a = 0; a < BASE_SIZE; a++ )
			if ( baseCurrent->map[b][a][baseCurrent->baseLevel] != -1
			  && x >= baseCurrent->posX[b][a][baseCurrent->baseLevel] && x < baseCurrent->posX[b][a][baseCurrent->baseLevel] + picWidth*ccs.basezoom
			  && y >= baseCurrent->posY[b][a][baseCurrent->baseLevel] && y < baseCurrent->posY[b][a][baseCurrent->baseLevel] + picHeight*ccs.basezoom )
			{
				building_t* entry = &bmBuildings[baseCurrent->id][ B_GetIDFromList( baseCurrent->map[b][a][baseCurrent->baseLevel] ) ];
				if ( entry->onClick[0] != '\0' )
					Cbuf_ExecuteText( EXEC_NOW, entry->onClick );
				else
					UP_OpenWith( entry->pedia );

				return;
			}
}


/*
=================
MN_ModelClick
=================
*/
void MN_ModelClick( menuNode_t *node )
{
	mouseSpace = MS_ROTATE;
	rotateAngles = node->angles;
}


/*
=================
MN_TextClick
=================
*/
void MN_TextClick( menuNode_t *node, int mouseOver )
{
	Cbuf_AddText( va( "%s_click %i\n", node->name, mouseOver-1 ) );
}

/*
=================
MN_Click
=================
*/
void MN_Click( int x, int y )
{
	menuNode_t		*node;
	menu_t			*menu;
	int		sp, mouseOver;

	sp = menuStackPos;

	while ( sp > 0 )
	{
		menu = menuStack[--sp];
		for ( node = menu->firstNode; node; node = node->next )
		{
			if ( node->type != MN_CONTAINER && !node->click ) continue;
			mouseOver = MN_CheckNodeZone( node, x, y );
			if ( !mouseOver ) continue;

			// found a node -> do actions
			if ( node->type == MN_CONTAINER ) MN_Drag( node, x, y );
			else if ( node->type == MN_BAR ) MN_BarClick( menu, node, x );
			else if ( node->type == MN_BASEMAP ) MN_BaseMapClick( node, x, y );
			else if ( node->type == MN_MAP ) MN_MapClick( node, x, y );
			else if ( node->type == MN_MODEL ) MN_ModelClick( node );
			else if ( node->type == MN_TEXT ) MN_TextClick( node, mouseOver );
			else MN_ExecuteActions( menu, node->click );
		}

		if ( menu->renderNode || menu->popupNode )
			// don't care about non-rendered windows
			return;
	}
}


/*
=================
MN_RightClick
=================
*/
void MN_RightClick( int x, int y )
{
	menuNode_t		*node;
	menu_t			*menu;
	int		sp, mouseOver;

	sp = menuStackPos;

	while ( sp > 0 )
	{
		menu = menuStack[--sp];
		for ( node = menu->firstNode; node; node = node->next )
		{
			if ( !node->rclick ) continue;
			mouseOver = MN_CheckNodeZone( node, x, y );
			if ( !mouseOver ) continue;

			// found a node -> do actions
			if ( node->type == MN_BASEMAP ) mouseSpace = MS_SHIFTBASEMAP;
			else if ( node->type == MN_MAP ) mouseSpace = MS_SHIFTMAP;
			else MN_ExecuteActions( menu, node->rclick );
		}

		if ( menu->renderNode || menu->popupNode )
			// don't care about non-rendered windows
			return;
	}
}


/*
=================
MN_MiddleClick
=================
*/
void MN_MiddleClick( int x, int y )
{
	menuNode_t		*node;
	menu_t			*menu;
	int		sp, mouseOver;

	sp = menuStackPos;

	while ( sp > 0 )
	{
		menu = menuStack[--sp];
		for ( node = menu->firstNode; node; node = node->next )
		{
			if ( !node->mclick ) continue;
			mouseOver = MN_CheckNodeZone( node, x, y );
			if ( !mouseOver ) continue;

			// found a node -> do actions
			if ( node->type == MN_BASEMAP ) mouseSpace = MS_ZOOMBASEMAP;
			else if ( node->type == MN_MAP ) mouseSpace = MS_ZOOMMAP;
			else MN_ExecuteActions( menu, node->mclick );
		}

		if ( menu->renderNode || menu->popupNode )
			// don't care about non-rendered windows
			return;
	}
}


/*
=================
MN_SetViewRect
=================
*/
void MN_SetViewRect( void )
{
	menuNode_t	*rn;
	int			sp;

	sp = menuStackPos;

	while ( sp > 0 )
	{
		rn = menuStack[--sp]->renderNode;
		if ( rn )
		{
			if ( rn->invis )
			{
				// don't draw the scene
				memset( &scr_vrect, 0, sizeof(scr_vrect) );
				return;
			}

			// the menu has a view size specified
			scr_vrect.x = rn->pos[0] * viddef.rx;
			scr_vrect.y = rn->pos[1] * viddef.ry;
			scr_vrect.width  = rn->size[0] * viddef.rx;
			scr_vrect.height = rn->size[1] * viddef.ry;
			return;
		}
	}

	// no menu in stack has a render node
	// render the full screen
	scr_vrect.x = 0;
	scr_vrect.y = 0;
	scr_vrect.width  = viddef.width;
	scr_vrect.height = viddef.height;
}


/*
==============================================================

MENU DRAWING

==============================================================
*/

/*
=================
MN_DrawItem
=================
*/
void MN_DrawItem( vec3_t org, item_t item, int sx, int sy, int x, int y, vec3_t scale, vec4_t color )
{
	modelInfo_t	mi;
	objDef_t	*od;
	vec3_t	angles = {-10, 160, 70};
	vec3_t	origin;
	vec3_t	size;
	vec4_t	col;

	if ( item.t == NONE )
		return;

	od = &csi.ods[item.t];

	if ( od->image[0] )
	{
		// draw the image
		// fix fixed size
		re.DrawNormPic( org[0] + C_UNIT/2.0*sx + C_UNIT*x, org[1] + C_UNIT/2.0*sy + C_UNIT*y,
			80, 80, 0, 0, 0, 0, ALIGN_CC, true, od->image );
	}
	else if ( od->model[0] )
	{
		// draw model, if there is no image
		mi.name = od->model;
		mi.origin = origin;
		mi.angles = angles;
		mi.center = od->center;
		mi.scale = size;

		if ( od->scale ) VectorScale( scale, od->scale, size );
		else VectorCopy( scale, size );

		VectorCopy( org, origin );
		if ( x || y || sx || sy )
		{
			origin[0] += C_UNIT / 2.0 * sx;
			origin[1] += C_UNIT / 2.0 * sy;
			origin[0] += C_UNIT * x;

			origin[1] += C_UNIT * y;
		}

		mi.frame = 0;
		mi.oldframe = 0;
		mi.backlerp = 0;

		Vector4Copy( color, col );
		if ( od->weapon && od->ammo && !item.a ) { col[1] *= 0.5; col[2] *= 0.5; }
		mi.color = col;

		// draw the model
		re.DrawModelDirect( &mi, NULL, NULL );
	}
}


/*
=================
MN_DrawMapMarkers
=================
*/
void MN_DrawMapMarkers( menuNode_t *node )
{
	aircraft_t *air;
	actMis_t *ms;
	int i, x, y;

	// draw mission pics
	menuText[TEXT_STANDARD] = NULL;
	for ( i = 0; i < ccs.numMissions; i++ )
	{
		ms = &ccs.mission[i];
		if ( !MN_MapToScreen( node, ms->realPos, &x, &y ) )
			continue;
		re.DrawNormPic( x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, false, "cross" );
		if ( ms == selMis )
		{
			menuText[TEXT_STANDARD] = ms->def->text;
			re.DrawNormPic( x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, true, "circle" );

			if ( CL_MapIsNight( ms->realPos ) ) Cvar_Set( "mn_mapdaytime", _("Night") );
			else Cvar_Set( "mn_mapdaytime", _("Day") );
		}
	}

	// draw base pics
	for ( i = 0; i < MAX_BASES; i++ )
		if ( bmBases[i].founded )
		{
			if ( !MN_MapToScreen( node, bmBases[i].pos, &x, &y ) )
				continue;
			re.DrawNormPic( x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, false, "base" );
		}

	// draw aircraft
	for ( i = 0, air = ccs.air; i < ccs.numAir; i++, air++ )
		if ( air->status != AIR_HOME )
		{
			if ( !MN_MapToScreen( node, air->pos, &x, &y ) )
				continue;
			re.DrawNormPic( x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, false, "aircraft" );

			if ( air->status >= AIR_TRANSIT )
			{
				mapline_t path;
				path.n = air->route.n - air->point;
				memcpy( path.p+1, air->route.p + air->point+1, (path.n-1) * sizeof(vec2_t) );
				memcpy( path.p, air->pos, sizeof(vec2_t) );
				MN_MapDrawLine( node, &path );
			}
		}
}

/*
=================
MN_Tooltip
=================
*/
void MN_Tooltip ( menuNode_t* node, int x, int y )
{
	int l;
	char* tooltip;
	vec4_t color;
	// tooltips
	if ( node->data[5] )
	{
		VectorSet( color, 0.0f, 0.0f, 0.0f );
		color[3] = 0.7f;
		x += 5;
		y += 5;
		tooltip = (char *)node->data[5];
		l = re.DrawPropLength( "f_small", _(tooltip) );
		if ( x + l > viddef.width )
			x -= l;
		re.DrawFill(x - 3, y - 3, l + 6, 20, 0, color );
		VectorSet( color, 0.0f, 0.8f, 0.0f );
		color[3] = 1.0f;
		re.DrawColor( color );
		re.DrawPropString("f_small", 0, x, y, _(tooltip) );
		re.DrawColor( NULL );
	}
}

/*
=================
MN_DrawMenus
=================
*/
void MN_DrawMenus( void )
{
	modelInfo_t	mi, pmi;
	menuNode_t	*node, *hover = NULL;
	menu_t		*menu;
	animState_t	*as;
	char		*ref, *font;
	char		source[MAX_VAR];
	int			sp, pp;
	item_t		item;
	vec4_t		color;
	int		mouseOver = 0;

	// render every menu on top of a menu with a render node
	pp = 0;
	sp = menuStackPos;
	while ( sp > 0 )
	{
		if ( menuStack[--sp]->renderNode ) break;
		if ( menuStack[sp]->popupNode ) pp = sp;
	}
	if ( pp < sp ) pp = sp;

	while ( sp < menuStackPos )
	{
		menu = menuStack[sp++];
		for ( node = menu->firstNode; node; node = node->next )
		{
			if ( !node->invis && (node->data[0] ||
				node->type == MN_CONTAINER || node->type == MN_TEXT || node->type == MN_BASEMAP || node->type == MN_MAP) )
			{
				// if construct
				if ( node->depends.var && strcmp( node->depends.value, (Cvar_Get( node->depends.var, node->depends.value, 0 ))->string ) )
					continue;

				// mouse effects
				if ( sp > pp )
				{
					// in and out events
					mouseOver = MN_CheckNodeZone( node, mx, my );
					if ( mouseOver != node->state )
					{
						// maybe we are leaving to another menu
						hover = NULL;
						if ( mouseOver )
							MN_ExecuteActions( menu, node->mouseIn );
						else
							MN_ExecuteActions( menu, node->mouseOut );
						node->state = mouseOver;
					}
				}

				// mouse darken effect
				VectorScale( node->color, 0.8, color );
				color[3] = node->color[3];
				if ( node->mousefx && node->type == MN_PIC && mouseOver && sp > pp ) re.DrawColor( color );
				else re.DrawColor( node->color );

				// get the reference
				if ( node->type != MN_BAR && node->type != MN_CONTAINER && node->type != MN_BASEMAP && node->type != MN_TEXT && node->type != MN_MAP )
				{
					ref = MN_GetReferenceString( menu, node->data[0] );
					if ( !ref )
					{
						// bad reference
						node->invis = true;
						Com_Printf( "MN_DrawActiveMenus: node \"%s\" bad reference \"%s\"\n", node->name, node->data );
						continue;
					}
					strncpy( source, ref, MAX_VAR );
				}
				else ref = NULL;

				switch ( node->type )
				{
				case MN_PIC:
					// hack for ekg pics
					if ( !strncmp( node->name, "ekg_", 4 ) )
					{
						int pt;
						if ( node->name[4] == 'm' ) pt = Cvar_VariableValue( "mn_morale" ) / 2;
						else pt = Cvar_VariableValue( "mn_hp" );
						node->texl[1] = (3 - (int)(pt<60 ? pt : 60) / 20) * 32;
						node->texh[1] = node->texl[1] + 32;
						node->texl[0] = -(int)(0.01 * (node->name[4]-'a') * cl.time) % 64;
						node->texh[0] = node->texl[0] + node->size[0];
					}
					re.DrawNormPic( node->pos[0], node->pos[1], node->size[0], node->size[1],
						node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, ref );
					break;

				case MN_STRING:
					if ( node->data[1] ) font = MN_GetReferenceString( menu, node->data[1] );
					else font = "f_small";
					if ( !node->mousefx || cl.time % 1000 < 500 )
						re.DrawPropString( font, node->align, node->pos[0], node->pos[1], ref );
					else
						re.DrawPropString( font, node->align, node->pos[0], node->pos[1], va( "%s*\n", ref ) );
					break;

				case MN_TEXT:
					if ( menuText[node->num] )
					{
						char textCopy[MAX_MENUTEXTLEN];
						int len;
						char *pos, *tab1, *tab2, *end;
						int  y, line;

						strncpy( textCopy, menuText[node->num], MAX_MENUTEXTLEN );
						len = strlen(textCopy);
						if ( len < MAX_MENUTEXTLEN - 1 && textCopy[len] != '\n' )
							strcat( textCopy, "\n" );

						if ( node->data[1] )
							font = MN_GetReferenceString( menu, node->data[1] );
						else
							font = "f_small";

						pos = textCopy;
						y = node->pos[1];
						line = 0;
						do {
							line++;
							end = strchr( pos, '\n' );
							if ( !end ) break;
							*end = 0;

							tab1 = strchr( pos, '\t' );
							if ( tab1 ) *tab1 = 0;

							if ( node->mousefx && line == mouseOver ) re.DrawColor( color );

							re.DrawPropString( font, ALIGN_UL, node->pos[0], y, pos );
							if ( tab1 )
							{
								*tab1 = '\t';
								tab2 = strchr( tab1+1,'\t' );
								if ( tab2 )
								{
									*tab2 = 0;
									re.DrawPropString( font, node->align, node->texh[1], y, tab1+1 );
									*tab2 = '\t';
									re.DrawPropString( font, ALIGN_UR, node->pos[0] + node->size[0], y, tab2+1 );
								}
								else
								{
									if ( !node->texh[1] ) re.DrawPropString( font, ALIGN_UR, node->pos[0] + node->size[0], y, tab1+1 );
									else re.DrawPropString( font, node->align, node->texh[1], y, tab1+1 );
								}
							}
							*end = '\n';

							if ( node->mousefx && line == mouseOver ) re.DrawColor( node->color );

							pos = end+1;
							y += node->texh[0];
						}
						while ( end );
					}
					break;

				case MN_BAR:
					{
						float fac, width;
						fac = node->size[0] / ( MN_GetReferenceFloat( menu, node->data[0] ) - MN_GetReferenceFloat( menu, node->data[1] ) );
						width = ( MN_GetReferenceFloat( menu, node->data[2] ) - MN_GetReferenceFloat( menu, node->data[1] ) ) * fac;
						re.DrawFill( node->pos[0], node->pos[1], width, node->size[1], node->align, mouseOver ? color : node->color);
					}
					break;

				case MN_CONTAINER:
					if ( menuInventory )
					{
						vec3_t		scale;
						invList_t	*ic;

						VectorSet( scale, 3.5, 3.5, 3.5 );
						color[0] = color[1] = color[2] = 0.5;
						color[3] = 1;

						if ( node->mousefx == C_UNDEFINED ) MN_FindContainer( node );
						if ( node->mousefx == NONE ) break;

						if ( csi.ids[node->mousefx].single )
						{
							// single item container (special case for left hand)
							if ( node->mousefx == csi.idLeft && !menuInventory->c[csi.idLeft] )
							{
								color[3] = 0.5;
								if ( menuInventory->c[csi.idRight] && csi.ods[menuInventory->c[csi.idRight]->item.t].twohanded )
									MN_DrawItem( node->pos, menuInventory->c[csi.idRight]->item, node->size[0]/C_UNIT, node->size[1]/C_UNIT, 0, 0, scale, color );
							}
							else if ( menuInventory->c[node->mousefx] )
								MN_DrawItem( node->pos, menuInventory->c[node->mousefx]->item, node->size[0]/C_UNIT, node->size[1]/C_UNIT, 0, 0, scale, color );
						}
						else
						{
							// standard container
							for ( ic = menuInventory->c[node->mousefx]; ic; ic = ic->next )
								MN_DrawItem( node->pos, ic->item, csi.ods[ic->item.t].sx, csi.ods[ic->item.t].sy, ic->x, ic->y, scale, color );
						}
						MN_InvDrawFree(menuInventory, node); //draw free space if dragging
					}
					break;

				case MN_ITEM:
					color[0] = color[1] = color[2] = 0.5;
					color[3] = 1;

					if ( node->mousefx == C_UNDEFINED ) MN_FindContainer( node );
					if ( node->mousefx == NONE ) {
						Com_Printf( _("no container...\n") );
						break;
					}

					for ( item.t = 0; item.t < csi.numODs; item.t++ )
						if ( !strcmp( ref, csi.ods[item.t].kurz ) )
							break;
					if ( item.t == csi.numODs || item.t == NONE )
						break;

					item.a = 1;

					MN_DrawItem( node->pos, item, 0, 0, 0, 0, node->scale, color );
					break;

				case MN_MODEL:
					// set model properties
					mi.model = re.RegisterModel( source );
					if ( !mi.model )
						break;

					mi.name = source;
					mi.origin = node->origin;
					mi.angles = node->angles;
					mi.scale = node->scale;
					mi.center = node->center;
					mi.color = node->color;

					// autoscale?
					if ( !node->scale[0] )
					{
						mi.scale = NULL;
						mi.center = node->size;
					}

					// get skin
					if ( node->data[3] && *(char *)node->data[3] )
						mi.skin = atoi( MN_GetReferenceString( menu, node->data[3] ) );
					else
						mi.skin = 0;

					// do animations
					if ( node->data[1] && *(char *)node->data[1] )
					{
						ref = MN_GetReferenceString( menu, node->data[1] );
						if ( !node->data[4] )
						{
							// new anim state
							as = (animState_t *)curadata;
							curadata += sizeof( animState_t );
							memset( as, 0, sizeof( animState_t ) );
							re.AnimChange( as, mi.model, ref );
							node->data[4] = as;
						}
						else
						{
							// change anim if needed
							char *anim;
							as = node->data[4];
							anim = re.AnimGetName( as, mi.model );
							if ( anim && strcmp( anim, ref ) )
								re.AnimChange( as, mi.model, ref );
						}
						re.AnimRun( as, mi.model, cls.frametime*1000 );

						mi.frame = as->frame;
						mi.oldframe = as->oldframe;
						mi.backlerp = as->backlerp;
					}
					else
					{
						// no animation
						mi.frame = 0;
						mi.oldframe = 0;
						mi.backlerp = 0;
					}

					// place on tag
					if ( node->data[2] )
					{
						menuNode_t	*search;
						char	parent[MAX_VAR];
						char	*tag;

						strncpy( parent, MN_GetReferenceString( menu, node->data[2] ), MAX_VAR );
						tag = parent;
						while ( *tag && *tag != ' ' ) tag++;
						*tag++ = 0;

						for ( search = menu->firstNode; search != node && search; search = search->next )
							if ( search->type == MN_MODEL && !strcmp( search->name, parent ) )
							{
								char model[MAX_VAR];
								strncpy( model, MN_GetReferenceString( menu, search->data[0] ), MAX_VAR );

								pmi.model = re.RegisterModel( model );
								if ( !pmi.model ) break;

								pmi.name = model;
								pmi.origin = search->origin;
								pmi.angles = search->angles;
								pmi.scale = search->scale;
								pmi.center = search->center;

								// autoscale?
								if ( !node->scale[0] )
								{
									mi.scale = NULL;
									mi.center = node->size;
								}

								as = search->data[4];
								pmi.frame = as->frame;
								pmi.oldframe = as->oldframe;
								pmi.backlerp = as->backlerp;

								re.DrawModelDirect( &mi, &pmi, tag );
								break;
							}
					} else re.DrawModelDirect( &mi, NULL, NULL );
					break;

				case MN_MAP:
					{
						float q;

						// advance time
						if ( !curCampaign ) break;
						CL_CampaignRun();

						// draw the map
						q = (ccs.date.day % 365 + (float)(ccs.date.sec/(3600*6))/4) * 2*M_PI/365 - M_PI;
						re.DrawDayAndNight( node->pos[0], node->pos[1], node->size[0], node->size[1], (float)ccs.date.sec/(3600*24), q,
							ccs.center[0], ccs.center[1], 0.5/ccs.zoom );

						// draw markers
						MN_DrawMapMarkers( node );

						// display text
						switch ( mapAction )
						{
						case MA_NEWBASE:
							menuText[TEXT_STANDARD] = _("Select the desired location of the\nnew base on the map.\n");
							break;
						}
					}
					break;

				case MN_BASEMAP:
					MN_DrawBase();
					break;
				} // switch

				// mouseover?
				if ( node->state == true )
					hover = node;
			} // if
		} // for
		if ( hover && cl_show_tooltips->value )
			MN_Tooltip(hover, mx, my);
	}
	re.DrawColor( NULL );
}


/*
==============================================================

GENERIC MENU FUNCTIONS

==============================================================
*/

/*
=================
MN_DeleteMenu
=================
*/
void MN_DeleteMenu( menu_t *menu )
{
	int		i;

	for ( i = 0; i < menuStackPos; i++ )
		if ( menuStack[i] == menu )
		{
			for ( menuStackPos--; i < menuStackPos; i++ )
				menuStack[i] = menuStack[i+1];
			return;
		}
}


/*
=================
MN_PushMenu
=================
*/
void MN_PushMenu( char *name )
{
	int		i;

	for ( i = 0; i < numMenus; i++ )
		if ( !strcmp( menus[i].name, name ) )
		{
			// found the correct add it to stack or bring it on top
			MN_DeleteMenu( &menus[i] );
			if ( menuStackPos < MAX_MENUSTACK )
				menuStack[menuStackPos++] = &menus[i];
			else
				Com_Printf( _("Menu stack overflow\n") );

			// initialize it
			if ( menus[i].initNode )
				MN_ExecuteActions( &menus[i], menus[i].initNode->click );

			cls.key_dest = key_game;
			return;
		}

	Com_Printf( _("Didn't find menu \"%s\"\n"), name );
}

void MN_PushMenu_f( void )
{
	if ( Cmd_Argc() > 1 )
		MN_PushMenu( Cmd_Argv(1) );
	else
		Com_Printf( _("usage: mn_push <name>\n") );
}


/*
=================
MN_PopMenu
=================
*/
void MN_PopMenu( qboolean all )
{
	if ( all )
		while ( menuStackPos > 0 )
		{
			menuStackPos--;
			if ( menuStack[menuStackPos]->closeNode )
				MN_ExecuteActions( menuStack[menuStackPos], menuStack[menuStackPos]->closeNode->click );
		}

	if ( menuStackPos > 0 )
	{
		menuStackPos--;
		if ( menuStack[menuStackPos]->closeNode )
			MN_ExecuteActions( menuStack[menuStackPos], menuStack[menuStackPos]->closeNode->click );
	}

	if ( !all && menuStackPos == 0 )
	{
		if ( !strcmp( menuStack[0]->name, mn_main->string ) )
		{
			if ( *mn_active->string )
				MN_PushMenu( mn_active->string );
			if ( !menuStackPos )
				MN_PushMenu( mn_main->string );
		} else {
			if ( *mn_main->string )
				MN_PushMenu( mn_main->string );
			if ( !menuStackPos )
				MN_PushMenu( mn_active->string );
		}
	}

	cls.key_dest = key_game;
}

void MN_PopMenu_f( void )
{
	if ( Cmd_Argc() < 2 || strcmp( Cmd_Argv(1), "esc" ) )
		MN_PopMenu( false );
	else
	{
		int i;
		for ( i = 0; i < (int)mn_escpop->value; i++ )
			MN_PopMenu( false );
		Cvar_Set( "mn_escpop", "1" );
	}
}


/*
=================
MN_Modify_f
=================
*/
void MN_Modify_f( void )
{
	float	value;

	if ( Cmd_Argc() < 5 )
		Com_Printf( _("usage: mn_modify <name> <amount> <min> <max>\n") );

	value = Cvar_VariableValue( Cmd_Argv(1) );
	value += atof( Cmd_Argv(2) );
	if ( value < atof( Cmd_Argv(3) ) ) value = atof( Cmd_Argv(3) );
	else if ( value > atof( Cmd_Argv(4) ) ) value = atof( Cmd_Argv(4) );

	Cvar_SetValue( Cmd_Argv(1), value );
}


/*
=================
MN_ModifyWrap_f
=================
*/
void MN_ModifyWrap_f( void )
{
	float	value;

	if ( Cmd_Argc() < 5 )
		Com_Printf( _("usage: mn_modifywrap <name> <amount> <min> <max>\n") );

	value = Cvar_VariableValue( Cmd_Argv(1) );
	value += atof( Cmd_Argv(2) );
	if ( value < atof( Cmd_Argv(3) ) ) value = atof( Cmd_Argv(4) );
	else if ( value > atof( Cmd_Argv(4) ) ) value = atof( Cmd_Argv(3) );

	Cvar_SetValue( Cmd_Argv(1), value );
}


/*
=================
MN_ModifyString_f
=================
*/
void MN_ModifyString_f( void )
{
	qboolean next;
	char	*current, *list, *tp;
	char	token[MAX_VAR], last[MAX_VAR], first[MAX_VAR];
	int		add;

	if ( Cmd_Argc() < 4 )
		Com_Printf( "usage: mn_modifystring <name> <amount> <list>\n" );

	current = Cvar_VariableString( Cmd_Argv(1) );
	add = atoi( Cmd_Argv(2) );
	list = Cmd_Argv(3);
	last[0] = 0;
	first[0] = 0;
	next = false;

	while ( add )
	{
		tp = token;
		while ( *list && *list != ':' ) *tp++ = *list++;
		if ( *list ) list++;
		*tp = 0;

		if ( *token && !first[0] ) strncpy( first, token, MAX_VAR );

		if ( !*token )
		{
			if ( add < 0 || next ) Cvar_Set( Cmd_Argv(1), last );
			else Cvar_Set( Cmd_Argv(1), first );
			return;
		}

		if ( next )
		{
			Cvar_Set( Cmd_Argv(1), token );
			return;
		}

		if ( !strcmp( token, current ) )
		{
			if ( add < 0 )
			{
				if ( last[0] ) Cvar_Set( Cmd_Argv(1), last );
				else Cvar_Set( Cmd_Argv(1), first );
				return;
			}
			else next = true;
		}
		strncpy( last, token, MAX_VAR );
	}
}


/*
 * MN_Translate_f
 *
 * Shows the corresponding strings in menu
 * Example: Optionsmenu - fullscreen: yes
**/
void MN_Translate_f( void )
{
	qboolean next;
	char	*current, *list, *orig, *trans;
	char	original[MAX_VAR], translation[MAX_VAR];

	if ( Cmd_Argc() < 4 )
		Com_Printf( "usage: mn_translate <source> <dest> <list>\n" );

	current = Cvar_VariableString( Cmd_Argv(1) );
	list = Cmd_Argv(3);
	next = false;

	while ( *list )
	{
		orig = original;
		while ( *list && *list != ':' ) *orig++ = *list++;
		*orig = 0;
		list++;

		trans = translation;
		while ( *list && *list != ',' ) *trans++ = *list++;
		*trans = 0;
		list++;

		if ( !strcmp( current, original ) )
		{
			Cvar_Set( Cmd_Argv(2), translation );
			return;
		}
	}

	// nothing found, copy value
	Cvar_Set( Cmd_Argv(2), current );
}

/*
=================
MN_ResetMenus
=================
*/
void MN_ResetMenus( void )
{
	int i;

	// reset menu structures
	numActions = 0;
	numNodes = 0;
	numMenus = 0;
	menuStackPos = 0;

	// add commands
	mn_escpop = Cvar_Get( "mn_escpop", "1", 0 );
	Cvar_Set( "mn_main", "main" );
	Cvar_Set( "mn_sequence", "sequence" );

//	Cmd_AddCommand( "maplist", CL_ListMaps_f );
	Cmd_AddCommand( "getmaps", MN_GetMaps_f );
	Cmd_AddCommand( "mn_startserver", MN_StartServer );
	Cmd_AddCommand( "mn_nextmap", MN_NextMap );
	Cmd_AddCommand( "mn_prevmap", MN_PrevMap );
	Cmd_AddCommand( "mn_push", MN_PushMenu_f );
	Cmd_AddCommand( "mn_pop", MN_PopMenu_f );
	Cmd_AddCommand( "mn_modify", MN_Modify_f );
	Cmd_AddCommand( "mn_modifywrap", MN_ModifyWrap_f );
	Cmd_AddCommand( "mn_modifystring", MN_ModifyString_f );
	Cmd_AddCommand( "mn_translate", MN_Translate_f );

	// get action data memory
	if ( adataize )
		memset( adata, 0, adataize );
	else
	{
		Hunk_Begin( 0x10000 );
		adata = Hunk_Alloc( 0x10000 );
		adataize = Hunk_End();
	}
	curadata = adata;

	// reset menu texts
	for ( i = 0; i < MAX_MENUTEXTS; i++ )
		menuText[i] = NULL;

	// reset ufopedia & basemanagement
	MN_ResetUfopedia();
	MN_ResetBaseManagement();
	MN_ResetResearch();
}

/*
=================
MN_Shutdown
=================
*/
void MN_Shutdown( void )
{
	// free the memory
	if ( adataize ) Hunk_Free( adata );
}

/*
==============================================================

MENU PARSING

==============================================================
*/

/*
=================
MN_ParseAction
=================
*/
qboolean MN_ParseAction( menuAction_t *action, char **text, char **token )
{
	char			*errhead = _("MN_ParseAction: unexptected end of file (in event)");
	menuAction_t	*lastAction;
	menuNode_t		*node;
	qboolean		found;
	value_t		*val;
	int			i;

	lastAction = NULL;

	do
	{
		// get new token
		*token = COM_EParse( text, errhead, NULL );
		if ( !*token ) return false;

		// get actions
		do
		{
			found = false;

			// standard function execution
			for ( i = 0; i < EA_CALL; i++ )
				if ( !strcmp( *token, ea_strings[i] ) )
				{
//					Com_Printf( "   %s", *token );

					// add the action
					if ( lastAction )
					{
						action = &menuActions[numActions++];
						memset( action, 0, sizeof(menuAction_t) );
						lastAction->next = action;
					}
					action->type = i;

					if ( ea_values[i] != V_NULL )
					{
						// get parameter values
						*token = COM_EParse( text, errhead, NULL );
						if ( !*text ) return false;

//						Com_Printf( " %s", *token );

						// get the value
						action->data = curadata;
						curadata += Com_ParseValue( curadata, *token, ea_values[i], 0 );
					}

//					Com_Printf( "\n" );

					// get next token
					*token = COM_EParse( text, errhead, NULL );
					if ( !*text ) return false;

					lastAction = action;
					found = true;
					break;
				}

			// node property setting
			if ( **token == '*' )
			{
//				Com_Printf( "   %s", *token );

				// add the action
				if ( lastAction )
				{
					action = &menuActions[numActions++];
					memset( action, 0, sizeof(menuAction_t) );
					lastAction->next = action;
				}
				action->type = EA_NODE;

				// get the node name
				action->data = curadata;
				strcpy( (char *)curadata, &(*token)[1] );
				curadata += strlen( (char *)curadata ) + 1;

				// get the node property
				*token = COM_EParse( text, errhead, NULL );
				if ( !*text ) return false;

//				Com_Printf( " %s", *token );

				for ( val = nps, i = 0; val->type; val++, i++ )
					if ( !strcmp( *token, val->string ) )
					{
						*(int *)curadata = i;
						break;
					}

				if ( !val->type )
				{
					Com_Printf( _("MN_ParseAction: token \"%s\" isn't a node property (in event)\n"), *token );
					curadata = action->data;
					if ( lastAction )
					{
						lastAction->next = NULL;
						numActions--;
					}
					break;
				}

				// get the value
				*token = COM_EParse( text, errhead, NULL );
				if ( !*text ) return false;

//				Com_Printf( " %s\n", *token );

				curadata += sizeof(int);
				curadata += Com_ParseValue( curadata, *token, val->type, 0 );

				// get next token
				*token = COM_EParse( text, errhead, NULL );
				if ( !*text ) return false;

				lastAction = action;
				found = true;
			}

			// function calls
			for ( node = menus[numMenus-1].firstNode; node; node = node->next )
				if ( (node->type == MN_FUNC || node->type == MN_CONFUNC) && !strcmp( node->name, *token ) )
				{
//					Com_Printf( "   %s\n", node->name );

					// add the action
					if ( lastAction )
					{
						action = &menuActions[numActions++];
						memset( action, 0, sizeof(menuAction_t) );
						lastAction->next = action;
					}
					action->type = EA_CALL;

					action->data = curadata;
					*(menuAction_t ***)curadata = &node->click;
					curadata += sizeof( menuAction_t * );

					// get next token
					*token = COM_EParse( text, errhead, NULL );
					if ( !*text ) return false;

					lastAction = action;
					found = true;
					break;
				}
		} while ( found );

		// test for end or unknown token
		if ( !strcmp( *token, "}" ) )
		{
			// finished
			return true;
		} else {
			// unknown token, print message and continue
			Com_Printf( _("MN_ParseAction: unknown token \"%s\" ignored (in event)\n"), *token );
		}
	} while ( *text );

	return false;
}

/*
=================
MN_ParseNodeBody
=================
*/
qboolean MN_ParseNodeBody( menuNode_t *node, char **text, char **token )
{
	char		*errhead = _("MN_ParseNodeBody: unexptected end of file (node");
	qboolean	found;
	value_t	*val;
	int		i;

	// functions are a special case
	if ( node->type == MN_CONFUNC || node->type == MN_FUNC )
	{
		menuAction_t **action;

		// add new actions to end of list
		action = &node->click;
		for ( ; *action; action = &(*action)->next );

		*action = &menuActions[numActions++];
		memset( *action, 0, sizeof(menuAction_t) );

		if ( node->type == MN_CONFUNC )
			Cmd_AddCommand( node->name, MN_Command );

		return MN_ParseAction( *action, text, token );
	}

	do
	{
		// get new token
		*token = COM_EParse( text, errhead, node->name );
		if ( !*text ) return false;

		// get properties, events and actions
		do
		{
			found = false;

			for ( val = nps; val->type; val++ )
				if ( !strcmp( *token, val->string ) )
				{
//					Com_Printf( "  %s", *token );

					if ( val->type != V_NULL )
					{
						// get parameter values
						*token = COM_EParse( text, errhead, node->name );
						if ( !*text ) return false;

//						Com_Printf( " %s", *token );

						// get the value
						if ( val->ofs > 0 )
							Com_ParseValue( node, *token, val->type, val->ofs );
						else
						{
							// a reference to data is handled like this
// 							Com_Printf( "%i %s\n", val->ofs, *token );
							node->data[-(val->ofs)] = curadata;
							if ( **token == '*' )
								curadata += Com_ParseValue( curadata, *token, V_STRING, 0 );
							else
								curadata += Com_ParseValue( curadata, *token, val->type, 0 );
						}
					}

//					Com_Printf( "\n" );

					// get next token
					*token = COM_EParse( text, errhead, node->name );
					if ( !*text ) return false;

					found = true;
					break;
				}

			for ( i = 0; i < NE_NUM_NODEEVENT; i++ )
				if ( !strcmp( *token, ne_strings[i] ) )
				{
					menuAction_t	**action;

//					Com_Printf( "  %s\n", *token );

					// add new actions to end of list
					action = (menuAction_t **)((byte *)node+ne_values[i]);
					for ( ; *action; action = &(*action)->next );

					*action = &menuActions[numActions++];
					memset( *action, 0, sizeof(menuAction_t) );

					// get the action body
					*token = COM_EParse( text, errhead, node->name );
					if ( !*text ) return false;

					if ( !strcmp( *token, "{" ) )
					{
						MN_ParseAction( *action, text, token );

						// get next token
						*token = COM_EParse( text, errhead, node->name );
						if ( !*text ) return false;
					}

					found = true;
					break;
				}
		} while ( found );

		// test for end or unknown token
		if ( !strcmp( *token, "}" ) )
		{
			// finished
			return true;
		} else {
			// unknown token, print message and continue
			Com_Printf( _("MN_ParseNodeBody: unknown token \"%s\" ignored (node \"%s\")\n"), *token, node->name );
		}
	} while ( *text );

	return false;
}


/*
=================
MN_ParseMenuBody
=================
*/
qboolean MN_ParseMenuBody( menu_t *menu, char **text )
{
	char		*errhead = _("MN_ParseMenuBody: unexptected end of file (menu");
	char		*token;
	qboolean	found;
	menuNode_t	*node, *lastNode;
	int		i;

	lastNode = NULL;

	do
	{
		// get new token
		token = COM_EParse( text, errhead, menu->name );
		if ( !*text ) return false;

		// get node type
		do
		{
			found = false;

			for ( i = 0; i < MN_NUM_NODETYPE; i++ )
				if ( !strcmp( token, nt_strings[i] ) )
				{
					// found node
					// get name
					token = COM_EParse( text, errhead, menu->name );
					if ( !*text ) return false;

					// test if node already exists
					for ( node = menu->firstNode; node; node = node->next )
						if ( !strcmp( token, node->name ) )
						{
							if ( node->type != i )
								Com_Printf( _("MN_ParseMenuBody: node prototype type change (menu \"%s\")\n"), menu->name );
							break;
						}

					// initialize node
					if ( !node )
					{
						node = &menuNodes[numNodes++];
						memset( node, 0, sizeof(menuNode_t) );
						strncpy( node->name, token, MAX_VAR );

						// link it in
						if ( lastNode ) lastNode->next = node;
						else menu->firstNode = node;

						lastNode = node;
					}

					node->type = i;

//					Com_Printf( " %s %s\n", nt_strings[i], *token );

					// check for special nodes
					if ( node->type == MN_FUNC && !strcmp( node->name, "init" ) )
					{
						if ( !menu->initNode )
							menu->initNode = node;
						else
							Com_Printf( _("MN_ParseMenuBody: second init function ignored (menu \"%s\")\n"), menu->name );
					}
					if ( node->type == MN_FUNC && !strcmp( node->name, "close" ) )
					{
						if ( !menu->closeNode )
							menu->closeNode = node;
						else
							Com_Printf( _("MN_ParseMenuBody: second close function ignored (menu \"%s\")\n"), menu->name );
					}
					if ( node->type == MN_ZONE && !strcmp( node->name, "render" ) )
					{
						if ( !menu->renderNode )
							menu->renderNode = node;
						else
							Com_Printf( _("MN_ParseMenuBody: second render node ignored (menu \"%s\")\n"), menu->name );
					}
					if ( node->type == MN_ZONE && !strcmp( node->name, "popup" ) )
					{
						if ( !menu->popupNode )
							menu->popupNode = node;
						else
							Com_Printf( _("MN_ParseMenuBody: second popup node ignored (menu \"%s\")\n"), menu->name );
					}
					if ( node->type == MN_CONTAINER )
						node->mousefx = C_UNDEFINED;

					// set standard texture coordinates
//					node->texl[0] = 0; node->texl[1] = 0;
//					node->texh[0] = 1; node->texh[1] = 1;

					// get parameters
					token = COM_EParse( text, errhead, menu->name );
					if ( !*text ) return false;

					if ( !strcmp( token, "{" ) )
					{
						if ( !MN_ParseNodeBody( node, text, &token ) )
						{
							Com_Printf( _("MN_ParseMenuBody: node with bad body ignored (menu \"%s\")\n"), menu->name );
							numNodes--;
							continue;
						}

						token = COM_EParse( text, errhead, menu->name );
						if ( !*text ) return false;
					}

					// set standard color
					if ( !node->color[3] )
						node->color[0] = node->color[1] = node->color[2] = node->color[3] = 1.0f;

					found = true;
					break;
				}
		} while ( found );

		// test for end or unknown token
		if ( !strcmp( token, "}" ) )
		{
			// finished
			return true;
		} else {
			// unknown token, print message and continue
			Com_Printf( _("MN_ParseMenuBody: unknown token \"%s\" ignored (menu \"%s\")\n"), *token, menu->name );
		}

	} while ( *text );

	return false;
}


/*
=================
MN_ParseMenu
=================
*/
void MN_ParseMenu( char *name, char **text )
{
	menu_t	*menu;
	menuNode_t	*node;
	char	*token;
	int		i;

	// search for menus with same name
	for ( i = 0; i < numMenus; i++ )
		if ( !strcmp( name, menus[i].name ) )
			break;

	if ( i < numMenus )
	{
		Com_Printf( _("MN_ParseMenus: menu \"%s\" with same name found, second ignored\n"), name );
		return;
	}

	// initialize the menu
	menu = &menus[numMenus++];
	memset( menu, 0, sizeof(menu_t) );

	strncpy( menu->name, name, MAX_VAR );

	// get it's body
	token = COM_Parse( text );

	if ( !*text || strcmp( token, "{" ) )
	{
		Com_Printf( _("MN_ParseMenus: menu \"%s\" without body ignored\n"), menu->name );
		numMenus--;
		return;
	}

	// parse it's body
	if ( !MN_ParseMenuBody( menu, text ) )
	{
		Com_Printf( _("MN_ParseMenus: menu \"%s\" with bad body ignored\n"), menu->name );
		numMenus--;
		return;
	}

	for ( i = 0; i < numMenus; i++ )
		for ( node = menus[i].firstNode; node; node = node->next )
			if ( node->num >= MAX_MENUTEXTS )
				Sys_Error( _("Error in menu %s - max menu num exeeded (%i)"), menus[i].name, MAX_MENUTEXTS );

	Com_DPrintf( "Nodes: %4i Menu data: %i\n", numNodes, curadata - adata );
}

/*
================
CL_ListMaps_f
================
*/
void CL_ListMaps_f ( void )
{
	int i;
	if ( ! mapsInstalledInit )
		FS_GetMaps();

	for ( i = 0; i < anzInstalledMaps-1; i++ )
	{
		Com_Printf("%s\n", maps[i]);
	}
}

/*
================
MN_MapInfo
================
*/
void MN_MapInfo ( void )
{
	// maybe mn_next/prev_map are called before getmaps???
	if ( !mapsInstalledInit )
		FS_GetMaps();

	Cvar_ForceSet("mapname", maps[mapInstalledIndex] );
	if ( FS_CheckFile ( va ( "pics/maps/shots/%s.jpg", maps[mapInstalledIndex] ) ) != -1 )
	{
		Cvar_Set("mn_mappic", va ( "maps/shots/%s.jpg", maps[mapInstalledIndex] ) );
	}
	else
	{
		Cvar_Set("mn_mappic", va ( "maps/shots/na.jpg", maps[mapInstalledIndex] ) );
	}

	if ( FS_CheckFile ( va ( "pics/maps/shots/%s_2.jpg", maps[mapInstalledIndex] ) ) != -1 )
	{
		Cvar_Set("mn_mappic2", va ( "maps/shots/%s_2.jpg", maps[mapInstalledIndex] ) );
	}
	else
	{
		Cvar_Set("mn_mappic2", va ( "maps/shots/na.jpg", maps[mapInstalledIndex] ) );
	}

	if ( FS_CheckFile ( va ( "pics/maps/shots/%s_3.jpg", maps[mapInstalledIndex] ) ) != -1 )
	{
		Cvar_Set("mn_mappic3", va ( "maps/shots/%s_3.jpg", maps[mapInstalledIndex] ) );
	}
	else
	{
		Cvar_Set("mn_mappic3", va ( "maps/shots/na.jpg", maps[mapInstalledIndex] ) );
	}
}

/*
================
MN_GetMaps_f
================
*/
void MN_GetMaps_f ( void )
{
	if ( ! mapsInstalledInit )
		FS_GetMaps();

	MN_MapInfo();
}

/*
================
MN_NextMap
================
*/
void MN_NextMap ( void )
{
	if ( mapInstalledIndex < anzInstalledMaps-1 )
		mapInstalledIndex++;
	else
		mapInstalledIndex = 0;
	MN_MapInfo();
}

/*
================
MN_PrevMap
================
*/
void MN_PrevMap ( void )
{
	if ( mapInstalledIndex > 0 )
		mapInstalledIndex--;
	else
		mapInstalledIndex = anzInstalledMaps-1;
	MN_MapInfo();
}
