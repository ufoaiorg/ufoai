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
	int		type;
	void	*data;
	struct	menuAction_s *next;
} menuAction_t;

typedef struct menuNode_s
{
	void		*data[5];			// needs to be first
	char		name[MAX_VAR];
	int			type;
	vec3_t		pos, size, angles;
	vec2_t		texh, texl;
	byte		state;
	byte		align;
	byte		invis, mousefx;
	vec4_t		color;
	menuAction_t		*click, *mouseIn, *mouseOut;
	struct menuNode_s	*next;
} menuNode_t;

typedef struct menu_s
{
	char	name[MAX_VAR];
	menuNode_t		*firstNode, *initNode, *closeNode, *renderNode;
} menu_t;


// ===========================================================

typedef enum
{
	EA_NULL,
	EA_CMD,

	EA_CALL,
	EA_NODE,
	EA_VAR,

	EA_NUM_EVENTACTION
};

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
	V_STRING,

	V_NULL,
	V_NULL,
	V_NULL
};

// ===========================================================

typedef enum
{
	NE_NULL,
	NE_CLICK,
	NE_MOUSEIN,
	NE_MOUSEOUT,

	NE_NUM_NODEEVENT
};

char *ne_strings[NE_NUM_NODEEVENT] =
{
	"",
	"click",
	"in",
	"out"
};

int	ne_values[NE_NUM_NODEEVENT] =
{
	0,
	NOFS( click ),
	NOFS( mouseIn ),
	NOFS( mouseOut )
};

// ===========================================================

value_t nps[] =
{
	{ "invis",		V_BOOL,		NOFS( invis ) },
	{ "mousefx",	V_BOOL,		NOFS( mousefx ) },
	{ "pos",		V_POS,		NOFS( pos ) },
	{ "size",		V_POS,		NOFS( size ) },
	{ "texh",		V_POS,		NOFS( texh ) },
	{ "texl",		V_POS,		NOFS( texl ) },
	{ "origin",		V_VECTOR,	NOFS( pos ) },
	{ "scale",		V_VECTOR,	NOFS( size ) },
	{ "angles",		V_VECTOR,	NOFS( angles ) },
	{ "image",		V_STRING,	0 },
	{ "md2",		V_STRING,	0 },
	{ "anim",		V_STRING,	-1 },
	{ "tag",		V_STRING,	-2 },
	{ "skin",		V_STRING,	-3 },
	{ "string",		V_STRING,	0 },
	{ "max",		V_FLOAT,	0 },
	{ "current",	V_FLOAT,	-1 },
	{ "weapon",		V_STRING,	0 },
	{ "color",		V_COLOR,	NOFS( color ) },
	{ "align",		V_ALIGN,	NOFS( align ) },
	{ NULL,			V_NULL,		0 },
};

// ===========================================================

typedef enum
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

	MN_NUM_NODETYPE
};

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
	"map"
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
char		*menuText = NULL;

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
	if ( *(char *)ref == '*' )
	{
		char	ident[MAX_VAR];
		char	*text;
		char	*token;

		// get the reference and the name
		text = (char *)ref+1;
		token = COM_Parse( &text );
		if ( !text ) return 0;
		strncpy( ident, token, MAX_VAR );
		token = COM_Parse( &text );
		if ( !text ) return 0;

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
				return 0;

			// get the property
			for ( val = nps; val->type; val++ )
				if ( !strcmp( token, val->string ) )
					break;

			if ( val->type != V_FLOAT ) 
				return 0;

			// get the string
			if ( val->ofs > 0 )
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
					Com_Printf( "MN_ExecuteActions: node \"%s\" doesn't exist\n", (char *)action->data );
					break;
				}

				if ( nps[np].ofs > 0 )
					Com_SetValue( node, (char *)data, nps[np].type, nps[np].ofs );
				else
					node->data[-nps[np].ofs] = data;

			}
			break;
		default:
			Sys_Error( "unknown action type\n" );
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
int MN_CheckNodeZone( menuNode_t *node, int x, int y )
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
	if ( !node->click && !node->mouseIn && !node->mouseOut )
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
	return true;
}


/*
=================
MN_CursorOnMenu
=================
*/
int MN_CursorOnMenu( int x, int y )
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
				// found an element
				return true;

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
		invChain_t *ic;

		px = (int)(x - node->pos[0]) / C_UNIT;
		py = (int)(y - node->pos[1]) / C_UNIT;

		// start drag
		ic = Com_SearchInInventory( menuInventory, node->mousefx, px, py );
		if ( ic )
		{
			// found item to drag
			mouseSpace = MS_DRAG;
			dragItem = ic->item; 
			dragFrom = ic->container;
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
			Com_MoveInInventory( menuInventory, dragFrom, dragFromX, dragFromY, node->mousefx, px, py, NULL, NULL );
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
	float	frac;

	if ( !node->mousefx )
		return;

	strncpy( var, node->data[1], MAX_VAR );
	if ( !strcmp( var, "*cvar" ) )
		return;

	frac = (float)(x - node->pos[0]) / node->size[0];
	Cvar_SetValue( &var[6], frac * MN_GetReferenceFloat( menu, node->data[0] ) );
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
	int		sp;

	sp = menuStackPos;

	while ( sp > 0 )
	{
		menu = menuStack[--sp];
		for ( node = menu->firstNode; node; node = node->next )
		{
			if ( !MN_CheckNodeZone( node, x, y ) )
				continue;

			// found a node -> do actions
			if ( node->type == MN_CONTAINER ) MN_Drag( node, x, y );
			else if ( node->type == MN_BAR ) MN_BarClick( menu, node, x );
			else if ( node->type == MN_MAP ) CL_MapClick( x, y );
			else if ( mouseSpace == MS_MENU ) MN_ExecuteActions( menu, node->click );
		}

		if ( menu->renderNode )
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
	if ( strcmp( "ammo", od->type ) && !item.a ) { col[1] *= 0.5; col[2] *= 0.5; }
	mi.color = col;

	// draw the model
	re.DrawModelDirect( &mi, NULL, NULL );
}


/*
=================
MN_DrawMenus
=================
*/
void MN_DrawMenus( void )
{
	modelInfo_t	mi, pmi;
	menuNode_t	*node;
	menu_t		*menu;
	animState_t	*as;
	char		*ref;
	char		source[MAX_VAR];
	int			sp;
	item_t		item;
	vec4_t		color;
	float		width;
	qboolean	mouseOver;

	// render every menu on top of a menu with a render node
	sp = menuStackPos;
	while ( sp > 0 )
		if ( menuStack[--sp]->renderNode ) break;

	while ( sp < menuStackPos )
	{
		menu = menuStack[sp++];
		for ( node = menu->firstNode; node; node = node->next )
			if ( !node->invis && (node->data[0] || 
				node->type == MN_CONTAINER || node->type == MN_TEXT || node->type == MN_MAP) )
			{
				// mouse effects
				mouseOver = MN_CheckNodeZone( node, mx, my );
				if ( mouseOver != node->state )
				{
					if ( mouseOver ) MN_ExecuteActions( menu, node->mouseIn );
					else MN_ExecuteActions( menu, node->mouseOut );
					node->state = mouseOver;
				}

				// mouse darken effect
				if ( node->mousefx && mouseOver )
				{
					VectorScale( node->color, 0.8, color );
					color[3] = node->color[3];
					re.DrawColor( color );		
				} else {
					VectorCopy( node->color, color );
					color[3] = node->color[3];
					re.DrawColor( node->color );
				}

				// get the reference
				if ( node->type != MN_BAR && node->type != MN_CONTAINER && node->type != MN_TEXT && node->type != MN_MAP )
				{
					ref = MN_GetReferenceString( menu, node->data[0] );
					if ( !ref ) 
					{
						// bad reference
		//				node->invis = true;
		//				Com_Printf( "MN_DrawActiveMenus: node \"%s\" bad reference \"%s\"\n", node->name, node->data );
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
						if ( node->name[4] == 'm' ) pt = Cvar_VariableValue( "mn_moral" ) / 2;
						else pt = Cvar_VariableValue( "mn_hp" );
						node->texl[1] = (3 - (int)(pt<60 ? pt : 60) / 20) * 32;
						node->texh[1] = node->texl[1] + 32;
						node->texl[0] = -(int)(0.01 * (node->name[4]-'a') * cl.time) % 64;
						node->texh[0] = node->texl[0] + node->size[0];
					}
					re.DrawNormPic( node->pos[0], node->pos[1], node->size[0], node->size[1], 
						node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, false, ref );
					break;

				case MN_STRING:
					if ( !node->mousefx || cl.time % 1000 < 500 )
						re.DrawPropString( "f_small", node->align, node->pos[0], node->pos[1], ref );
					else
						re.DrawPropString( "f_small", node->align, node->pos[0], node->pos[1], va( "%sI\n", ref ) );
					break;

				case MN_TEXT:
					if ( menuText )
					{
            char textCopy[256];
						char *pos, *tab, *end, *font;
						int  y;

            strncpy( textCopy, menuText, 256 );
						pos = textCopy;
						y = node->pos[1];
						do {
							if ( *pos == '\\' ) { pos++; font = "f_big"; } 
							else font = "f_small";

							end = strchr( pos, '\n' );
							if ( !end ) break;
							tab = strchr( pos, '\t' );

							if ( tab && tab < end ) *tab = 0;
							*end = 0;
							re.DrawPropString( font, ALIGN_UL, node->pos[0], y, pos );
							if ( tab && tab < end ) 
							{
								*tab = '\t';
								re.DrawPropString( font, ALIGN_UR, node->pos[0] + node->size[0], y, tab+1 );
							}
							*end = '\n';

							pos = end+1;

							if ( !strcmp( font, "f_big" ) ) y += 68;
							else y += 20;
						} 
						while ( end );
					}
					break;

				case MN_BAR:
					width = node->size[0] * MN_GetReferenceFloat( menu, node->data[1] ) / MN_GetReferenceFloat( menu, node->data[0] );
					re.DrawFill( node->pos[0], node->pos[1], width, node->size[1], node->align, color );
					break;

				case MN_CONTAINER:
					if ( menuInventory )
					{
						vec3_t		scale;
						invChain_t	*ic;

						VectorSet( scale, 3.5, 3.5, 3.5 );
						color[0] = color[1] = color[2] = 0.5;
						color[3] = 1;

						if ( node->mousefx == C_UNDEFINED ) MN_FindContainer( node );
						if ( node->mousefx == NONE ) break;

						if ( node->mousefx == csi.idRight ) 
						{ 
							// right hand
							MN_DrawItem( node->pos, menuInventory->right, node->size[0]/C_UNIT, node->size[1]/C_UNIT, 0, 0, scale, color );
						}
						else if ( node->mousefx == csi.idLeft ) 
						{
							// left hand
							if ( menuInventory->left.t == NONE )
							{
								color[3] = 0.5;
								if ( menuInventory->right.t != NONE && csi.ods[menuInventory->right.t].twohanded )
									MN_DrawItem( node->pos, menuInventory->right, node->size[0]/C_UNIT, node->size[1]/C_UNIT, 0, 0, scale, color );
							}
							else MN_DrawItem( node->pos, menuInventory->left, node->size[0]/C_UNIT, node->size[1]/C_UNIT, 0, 0, scale, color );
						}
						else if ( node->mousefx == csi.idFloor || node->mousefx == csi.idEquip )
						{
							// floor container
							if ( menuInventory->floor )
								for ( ic = menuInventory->floor->inv; ic; ic = ic->next )
									MN_DrawItem( node->pos, ic->item, csi.ods[ic->item.t].sx, csi.ods[ic->item.t].sy, ic->x, ic->y, scale, color );
						}
						else
						{
							// standard container
							for ( ic = menuInventory->inv; ic; ic = ic->next )
								if ( ic->container == node->mousefx )
									MN_DrawItem( node->pos, ic->item, csi.ods[ic->item.t].sx, csi.ods[ic->item.t].sy, ic->x, ic->y, scale, color );
						}
					}
					break;

				case MN_ITEM:
					color[0] = color[1] = color[2] = 0.5;
					color[3] = 1;

					if ( node->mousefx == C_UNDEFINED ) MN_FindContainer( node );
					if ( node->mousefx == NONE ) break;

					for ( item.t = 0; item.t < csi.numODs; item.t++ )
						if ( !strcmp( ref, csi.ods[item.t].kurz ) )
							break;
					if ( item.t == csi.numODs )
						break;

					item.a = 1;
					MN_DrawItem( node->pos, item, 0, 0, 0, 0, node->size, color );
					break;

				case MN_MODEL:
					// set model properties
					mi.model = re.RegisterModel(source);
					if ( !mi.model ) break;

					mi.name = source;
					mi.origin = node->pos;
					mi.angles = node->angles;
					mi.scale = node->size;
					mi.center = 0;
					mi.color = color;

					// get skin
					if ( node->data[3] && *(char *)node->data[3] )
						mi.skin = atoi( MN_GetReferenceString( menu, node->data[3] ) );
					else
						mi.skin = 0;

					// do animations
					if ( node->data[1] )
					{
						ref = MN_GetReferenceString( menu, node->data[1] );
						if ( !node->data[4] )
						{
							// new anim state
							as = (animState_t *)curadata;
							curadata += sizeof( animState_t );
							memset( as, 0, sizeof( animState_t ) );
							re.AnimAppend( as, mi.model, (char *)node->data[1] );
							node->data[4] = as;
						} 
						else as = node->data[4];
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

						for ( search = menu->firstNode; search != node; search = search->next )
							if ( search->type == MN_MODEL && !strcmp( search->name, parent ) )
							{
								pmi.name = MN_GetReferenceString( menu, search->data[0] );
								pmi.origin = search->pos;
								pmi.angles = search->angles;
								pmi.scale = search->size;
								pmi.center = 0;

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
						mission_t	*ms;
						int	i;

						menuText = NULL;
						for ( i = 0; i < numActMis; i++ )
						{
							ms = &missions[actMis[i]];
							re.DrawNormPic( ms->pos[0], ms->pos[1], 32, 32, 0, 0, 0, 0, ALIGN_CC, false, "cross" );
							if ( actMis[i] == selMission )
							{
								menuText = ms->text;
								re.DrawNormPic( ms->pos[0], ms->pos[1], 32, 32, 0, 0, 0, 0, ALIGN_CC, true, "circle" );
							}
						}
						if ( !numOnTeam || selMission == -1 )
							menuText = "Assemble a team and select\na mission on the map above.\n";
					}
					break;
				}
		}
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
				Com_Printf( "Menu stack overflow\n" );

			// initialize it
			if ( menus[i].initNode )
				MN_ExecuteActions( &menus[i], menus[i].initNode->click );

			cls.key_dest = key_game;
			return;
		}

	Com_Printf( "Didn't find menu \"%s\"\n", name );
}

void MN_PushMenu_f( void )
{
	if ( Cmd_Argc() > 1 )
		MN_PushMenu( Cmd_Argv(1) );
	else
		Com_Printf( "usage: mn_push <name>\n" );
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
		if ( !strcmp( menuStack[0]->name, mn_main->string ) )
		{
			if ( *mn_active->string ) MN_PushMenu( mn_active->string );
			if ( !menuStackPos ) MN_PushMenu( mn_main->string );
		} else {
			if ( *mn_main->string ) MN_PushMenu( mn_main->string );
			if ( !menuStackPos ) MN_PushMenu( mn_active->string );
		}

//	cls.key_dest = key_game;
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

void MN_Modify_f( void )
{
	float	value;

	if ( Cmd_Argc() < 5 )
		Com_Printf( "usage: mn_modify <name> <amount> <min> <max>\n" );

	value = Cvar_VariableValue( Cmd_Argv(1) );
	value += atof( Cmd_Argv(2) );
	if ( value < atof( Cmd_Argv(3) ) ) value = atof( Cmd_Argv(3) );
	else if ( value > atof( Cmd_Argv(4) ) ) value = atof( Cmd_Argv(4) );

	Cvar_SetValue( Cmd_Argv(1), value );
}


/*
=================
MN_ResetMenus
=================
*/
void MN_ResetMenus( void )
{
	// reset menu structures
	numActions = 0;
	numNodes = 0;
	numMenus = 0;
	menuStackPos = 0;

	// add commands
	mn_escpop = Cvar_Get( "mn_escpop", "1", 0 );

	Cmd_AddCommand( "mn_push", MN_PushMenu_f );
	Cmd_AddCommand( "mn_pop", MN_PopMenu_f );
	Cmd_AddCommand( "mn_modify", MN_Modify_f );

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
int MN_ParseAction( menuAction_t *action, char **text, char **token )
{
	char			*errhead = "MN_ParseAction: unexptected end of file (in event)";
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
					Com_Printf( "MN_ParseAction: token \"%s\" isn't a node property (in event)\n", *token );
					curadata = action->data;
					lastAction->next = NULL;
					numActions--;
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
			Com_Printf( "MN_ParseAction: unknown token \"%s\" ignored (in event)\n", *token );
		}
	} while ( *text );

	return false;
}

/*
=================
MN_ParseNodeBody
=================
*/
int MN_ParseNodeBody( menuNode_t *node, char **text, char **token )
{
	char		*errhead = "MN_ParseNodeBody: unexptected end of file (node";
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
							node->data[-val->ofs] = curadata;
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
			Com_Printf( "MN_ParseNodeBody: unknown token \"%s\" ignored (node \"%s\")\n", *token, node->name );
		}
	} while ( *text );

	return false;
}


/*
=================
MN_ParseMenuBody
=================
*/
int MN_ParseMenuBody( menu_t *menu, char **text )
{
	char		*errhead = "MN_ParseMenuBody: unexptected end of file (menu";
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
								Com_Printf( "MN_ParseMenuBody: node prototype type change (menu \"%s\")\n", menu->name );
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
							Com_Printf( "MN_ParseMenuBody: second init function ignored (menu \"%s\")\n", menu->name );
					}
					if ( node->type == MN_FUNC && !strcmp( node->name, "close" ) )
					{
						if ( !menu->closeNode )
							menu->closeNode = node;
						else
							Com_Printf( "MN_ParseMenuBody: second close function ignored (menu \"%s\")\n", menu->name );
					}
					if ( node->type == MN_ZONE && !strcmp( node->name, "render" ) )
					{
						if ( !menu->renderNode )
							menu->renderNode = node;
						else
							Com_Printf( "MN_ParseMenuBody: second render node ignored (menu \"%s\")\n", menu->name );
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
							Com_Printf( "MN_ParseMenuBody: node with bad body ignored (menu \"%s\")\n", menu->name );
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
			Com_Printf( "MN_ParseMenuBody: unknown token \"%s\" ignored (menu \"%s\")\n", *token, menu->name );
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
	char	*token;
	int		i;

	// search for menus with same name
	for ( i = 0; i < numMenus; i++ )
		if ( !strcmp( name, menus[i].name ) )
			break;

	if ( i < numMenus )
	{
		Com_Printf( "MN_ParseMenus: menu \"%s\" with same name found, second ignored\n", name );
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
		Com_Printf( "MN_ParseMenus: menu \"%s\" without body ignored\n", menu->name );
		numMenus--;
		return;
	}

	// parse it's body
	if ( !MN_ParseMenuBody( menu, text ) )
	{
		Com_Printf( "MN_ParseMenus: menu \"%s\" with bad body ignored\n", menu->name );
		numMenus--;
		return;
	}

//	Com_Printf( "Nodes: %4i Menu data: %i\n", numNodes, curadata - adata );
}


