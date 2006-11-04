// g_client.c -- main part of the game logic

#include "g_local.h"

// ====================================================================


#define VIS_APPEAR	1
#define VIS_PERISH	2


/*
=================
G_VisToPlayerMask
=================
*/
int G_VisToPlayerMask( int vis_mask ) 
{
	player_t *player;
	int		player_mask, i;

	player_mask = 0;

	for ( i = 0, player = game.players; i < game.maxplayers; i++, player++ )
		if ( player->inuse && (vis_mask & (1<<player->pers.team)) )
			player_mask |= (1<<i);

	return player_mask;
}


/*
=================
G_SendStats
=================
*/
void G_SendStats( edict_t *ent )
{
	gi.AddEvent( G_VisToPlayerMask(1<<ent->team), EV_ACTOR_STATS );
	gi.WriteShort( ent->number );
	gi.WriteByte( ent->TU );
	gi.WriteByte( ent->HP );
	gi.WriteByte( ent->moral );
}


/*
=================
G_SendPlayerStats
=================
*/
void G_SendPlayerStats( player_t *player )
{
	edict_t	*ent;
	int		i;

	for ( i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++ )
		if ( ent->inuse && ent->type == ET_ACTOR && ent->team == player->pers.team ) 
			G_SendStats( ent );
}


/*
=================
G_GiveTimeUnits
=================
*/
void G_GiveTimeUnits( int team )
{
	edict_t	*ent;
	int		i;

	for ( i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++ )
		if ( ent->inuse && !(ent->state & STATE_DEAD) && ent->type == ET_ACTOR && ent->team == team ) 
		{
			ent->TU = SWF_TU( ent->chr.swiftness );
			G_SendStats( ent );
		}
}


/*
=================
G_SendInventory
=================
*/
void G_SendInventory( int player_mask, edict_t *ent )
{
	invChain_t *ic;

	// test for pointless player mask
	if ( !player_mask ) return;

	gi.AddEvent( player_mask, EV_INV_ADD );
	gi.WriteShort( ent->number );

	for ( ic = ent->i.inv; ic; ic = ic->next )
	{
		// send a single item
		gi.WriteByte( ic->item.t );
		gi.WriteByte( ic->item.a );
		gi.WriteByte( ic->container );
		gi.WriteByte( ic->x );
		gi.WriteByte( ic->y );
//		Com_Printf( "Sent item: %i %i %i\n", ic->item, ic->container, ic->pos );
	}

	// terminate list
	gi.WriteByte( NONE );
}


/*
=================
G_AppearPerishEvent
=================
*/
void G_AppearPerishEvent( int player_mask, int appear, edict_t *check )
{
	if ( appear )
	{
		// appear
		switch ( check->type )
		{
		case ET_ACTOR:
			gi.AddEvent( player_mask, EV_ACTOR_APPEAR );
			gi.WriteShort( check->number );
			gi.WriteByte( check->team );
			gi.WriteByte( check->pnum );
			gi.WriteGPos( check->pos );
			gi.WriteByte( check->dir );
			gi.WriteByte( check->state );
			gi.WriteByte( check->i.left.t );
			gi.WriteByte( check->i.left.a );
			gi.WriteByte( check->i.right.t );
			gi.WriteByte( check->i.right.a );
			gi.WriteShort( check->body );
			gi.WriteShort( check->head );
			gi.WriteByte( check->skin );
			G_SendInventory( G_VisToPlayerMask(1<<check->team) & player_mask, check );
			break;

		case ET_ITEM:
			gi.AddEvent( player_mask, EV_ENT_APPEAR );
			gi.WriteShort( check->number );
			gi.WriteByte( ET_ITEM );
			gi.WriteGPos( check->pos );
			G_SendInventory( player_mask, check );
			break;

		default:
/*			gi.AddEvent( player_mask, EV_ENT_APPEAR );
			gi.WriteShort( check->number );
			gi.WriteByte( check->type );
			gi.WriteGPos( check->pos );*/
			break;
		}
	}
	else if ( check->type == ET_ACTOR || check->type == ET_ITEM )
	{
		// disappear
		gi.AddEvent( player_mask, EV_ENT_PERISH );
		gi.WriteShort( check->number );
	}
}




/*
=================
G_FrustomVis
=================
*/
qboolean G_FrustomVis( edict_t *from, vec3_t point )
{
	// view frustom check
	vec3_t	delta;
	byte	dv;

	delta[0] = point[0] - from->origin[0];
	delta[1] = point[1] - from->origin[1];
	delta[2] = 0;
	VectorNormalize( delta );
	dv = from->dir & 7;

	// test 120° frustom (cos 60° = 0.5)
	if ( (delta[0]*dvecsn[dv][0] + delta[1]*dvecsn[dv][1]) < 0.5 ) return false;
	else return true;
}


/*
=================
G_TeamPointVis
=================
*/
qboolean G_TeamPointVis( int team, vec3_t point )
{
	edict_t	*from;
	vec3_t	eye;
	int		i;

	// test if check is visible
	for ( i = 0, from = g_edicts; i < globals.num_edicts; i++, from++ )
		if ( from->inuse && from->type == ET_ACTOR && !(from->state & STATE_DEAD) && 
			from->team == team && G_FrustomVis( from, point ) )
		{
			// get viewers eye height
			VectorCopy( from->origin, eye );
			if ( from->state & (STATE_CROUCHED|STATE_PANIC) ) eye[2] += EYE_CROUCH;
			else eye[2] += EYE_STAND;

			// line of sight
			if ( !gi.TestLine( eye, point ) )
				return true;
		}

	// not visible
	return false;
}


/*
=================
G_ActorVis
=================
*/
qboolean G_ActorVis( vec3_t from, edict_t *check )
{
	vec3_t	test;
	float	delta;
	int		i;

	// start on eye height
	VectorCopy( check->origin, test );
	if ( check->state & STATE_DEAD ) {
		test[2] += PLAYER_DEAD;
		delta = 0;
	} else if ( check->state & (STATE_CROUCHED|STATE_PANIC) ) {
		test[2] += PLAYER_CROUCH - 2;
		delta = (PLAYER_CROUCH - PLAYER_MIN) / 2 - 2;
	} else {
		test[2] += PLAYER_STAND;
		delta = (PLAYER_STAND - PLAYER_MIN) / 2 - 2;
	}

	// do 3 tests
	for ( i = 0; i < 3; i++ )
	{
		if ( !gi.TestLine( from, test ) )
			return true;

		// look further down or stop
		if ( !delta ) return false;
		test[2] -= delta;
	}

	// no viewing line
	return false;
}


/*
=================
G_Vis
=================
*/
qboolean G_Vis( int team, edict_t *from, edict_t *check, int flags )
{
	vec3_t	eye;

	// if any of them isn't in use, then they're not visible
	if ( !from->inuse || !check->inuse  )
		return false;

	// only actors can see anything
	if ( from->type != ET_ACTOR || (from->state & STATE_DEAD) )
		return false;

	// living team members are always visible
	if ( team >= 0 && check->team == team && !(check->state & STATE_DEAD) )
		return true;

	// standard team rules
	if ( team >= 0 && from->team != team )
		return false;

	// inverse team rules
	if ( team < 0 && (from->team == -team || from->team == TEAM_CIVILIAN || check->team != -team) )
		return false;

	// check for same pos
	if ( VectorCompare( from->pos, check->pos ) ) 
		return true;
		
	// view distance check
	if ( VectorDistSqr( from->origin, check->origin ) > MAX_SPOT_DIST*MAX_SPOT_DIST  )
		return false;

	// view frustom check
	if ( !(flags & VT_NOFRUSTOM) && !G_FrustomVis( from, check->origin ) )
		return false;

	// get viewers eye height
	VectorCopy( from->origin, eye );
	if ( from->state & (STATE_CROUCHED|STATE_PANIC) ) eye[2] += EYE_CROUCH;
	else eye[2] += EYE_STAND;

	// line trace check
	switch ( check->type )
	{
	case ET_ACTOR:
		if ( G_ActorVis( eye, check ) ) return true;
		else return false;

	case ET_ITEM:
		if ( !gi.TestLine( eye, check->origin ) ) return true;
		else return false;

	default:
		return false;
	}
}


/*
=================
G_TestVis
=================
*/
int G_TestVis( int team, edict_t *check, int flags ) 
{
	int		i, old;
	edict_t	*from;

	// store old flag
	old = ( check->visflags & (1 << team) ) ? 1 : 0;
	if ( !(flags & VT_PERISH) && old ) return VIS_YES;

	// test if check is visible
	for ( i = 0, from = g_edicts; i < globals.num_edicts; i++, from++ )
		if ( G_Vis( team, from, check, flags ) )
			// visible
			return VIS_YES | !old;

	// not visible
	return old;
}


/*
=================
G_CheckVis
=================
*/
int G_CheckVis( int player_mask, int team, edict_t *check, qboolean perish ) 

{
	int		vis, i, end;
	int		status;

	status = 0;

	// decide whether to check all entities or a specific one
	if ( !check )
	{
		check = g_edicts;
		end = globals.num_edicts;
	}
	else end = 1;

	// check visibility
	for ( i = 0; i < end; i++, check++ )
		if ( check->inuse )
		{
			// check if he's visible
			vis = G_TestVis( team, check, perish );

			if ( vis & VIS_CHANGE )
			{
				check->visflags ^= (1 << team);
				G_AppearPerishEvent( player_mask, vis & VIS_YES, check );

				if ( vis & VIS_YES ) 
				{
					status |= VIS_APPEAR;
					if ( check->type == ET_ACTOR && !(check->state & STATE_DEAD) ) 
						status |= VIS_STOP;
				}
				else status |= VIS_PERISH;
			}
		}

	return status;
}


/*
=================
G_ClearVisFlags
=================
*/
void G_ClearVisFlags( int team ) 
{
	edict_t	*ent;
	int		i, mask;

	mask = ~(1<<team);
	for ( i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++ )
		if ( ent->inuse ) ent->visflags &= mask;
}


// ====================================================================


/*
=================
G_DoTurn
=================
*/
int G_DoTurn( int player_mask, edict_t *ent, byte toDV ) 
{
	float	angleDiv;
	const	byte *rot;
	int		i, num;
	int		status;

	// return if no rotation needs to be done
	if ( (ent->dir) == (toDV&7) ) 
		return 0;

	// calculate angle difference
	angleDiv = dangle[toDV&7] - dangle[ent->dir];
	if ( angleDiv >  180.0 ) angleDiv -= 360.0;
	if ( angleDiv < -180.0 ) angleDiv += 360.0;

	// prepare rotation
	if ( angleDiv > 0 ) 
	{
		rot = dvleft;
		num = (+angleDiv + 22.5) / 45.0;
	} else {
		rot = dvright;
		num = (-angleDiv + 22.5) / 45.0;
	}

	// do rotation and vis checks
	status = 0;

	for ( i = 0; i < num; i++ )
	{
		ent->dir = rot[ent->dir];

		status |= G_CheckVis( player_mask, ent->team, NULL, false );
//		if ( stop && (status & VIS_STOP) )
			// stop turning if a new living player appears
//			break;
	}

	return status;
}


// ====================================================================


/*
=================
G_ActionCheck
=================
*/
qboolean G_ActionCheck( player_t *player, edict_t *ent, int TU )
{
	// a generic tester if an action could be possible

	if ( level.activeTeam != player->pers.team )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - this isn't your round!\n" );
		return false;
	}

	if ( !ent || !ent->inuse )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - object not present!\n" );
		return false;
	}

	if ( ent->type != ET_ACTOR )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - not an actor!\n" );
		return false;
	}

	if ( ent->state & STATE_DEAD )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - actor is dead!\n" );
		return false;
	}	

	if ( ent->team != player->pers.team )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - not on same team!\n" );
		return false;
	}

	if ( ent->pnum != player->num )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - no control over allied actors!\n" );
		return false;
	}

	if ( ent->TU < TU )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - not enough TUs!\n" );
		return false;
	}

	// could be possible
	return true;
}


/*
=================
G_GetFloorItems
=================
*/
edict_t *G_GetFloorItems( edict_t *ent )
{
	edict_t *floor;

	for ( floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++ )
	{
		if ( !floor->inuse || floor->type != ET_ITEM ) 
			continue;
		if ( !VectorCompare( ent->pos, floor->pos ) )
			continue;

		// found items
		ent->i.floor = &floor->i;
		return floor;
	}

	// no items on ground found
	ent->i.floor = NULL;
	return NULL;
}


/*
=================
G_InventoryToFloor
=================
*/
void G_InventoryToFloor( edict_t *ent )
{
	invChain_t	*ic, *next;
	edict_t		*floor;
	int			i;
	int			x, y;

	// check for items
	if ( ent->i.right.t == NONE && ent->i.left.t == NONE && !ent->i.inv )
		return;

	// find the floor
	floor = G_GetFloorItems( ent );
	if ( !floor )
	{
		floor = G_Spawn();
		floor->classname = "item";
		floor->type = ET_ITEM;
		VectorCopy( ent->pos, floor->pos );
		gi.GridPosToVec( floor->pos, floor->origin );
		floor->origin[2] -= 24;
	} else {
		gi.AddEvent( G_VisToPlayerMask( floor->visflags ), EV_ENT_PERISH );
		gi.WriteShort( floor->number );
		floor->visflags = 0;
	}

	// drop items in the hands
	floor->i.floor = &floor->i;
	if ( ent->i.right.t != NONE ) 
	{
		Com_FindSpace(&floor->i, ent->i.right.t, gi.csi->idFloor, &x, &y);
		Com_AddToInventory( &floor->i, ent->i.right.t, ent->i.right.a, gi.csi->idFloor, x, y );
	}
	if ( ent->i.left.t != NONE ) 
	{
		Com_FindSpace(&floor->i, ent->i.left.t, gi.csi->idFloor, &x, &y);
		Com_AddToInventory( &floor->i, ent->i.left.t, ent->i.left.a, gi.csi->idFloor, x, y );
	}

	// drop the rest
	for ( ic = ent->i.inv; ic; ic = next )
	{
		next = ic->next;	
		ic->container = gi.csi->idFloor;

		Com_FindSpace( &floor->i, ent->i.right.t, gi.csi->idFloor, &ic->x, &ic->y );
		if ( ic->x >= 32 || ic->y >= 16 ) 
		{
			ent->i.inv = ic;
			Com_DestroyInventory( &ent->i );
			return;
		}
		ic->next = floor->i.inv;
		floor->i.inv = ic;
	}

	// destroy link
	ent->i.inv = NULL;

	// send item info to the clients

	for ( i = 0; i < MAX_TEAMS; i++ )
		if ( level.num_alive[i] ) 
			G_CheckVis( G_VisToPlayerMask(1<<i), i, floor, true );
}


/*
=================
G_InventoryMove
=================
*/
void G_InventoryMove( player_t *player, int num, int from, int fx, int fy, int to, int tx, int ty )
{
	edict_t	*ent, *floor;
	inventory_t	tfloor;
	invChain_t	*ic;
	int		mask;
	int		ia;

	ent = g_edicts + num;
	
	// check if action is possible
	if ( !G_ActionCheck( player, ent, 1 ) ) 
		return;

	// "get floor ready"
	floor = G_GetFloorItems( ent );
	if ( to == gi.csi->idFloor && !floor )
	{
		memset( &tfloor, 0, sizeof(tfloor) );
		ent->i.floor = &tfloor;
	}

	// search for space
	if ( tx == NONE || ty == NONE )
		Com_FindSpace( &ent->i, Com_SearchInInventory(&ent->i, from, fx, fy)->item.t, gi.csi->idFloor, &tx, &ty );
	if ( tx == NONE || ty == NONE )
		return;

	if ( ia = Com_MoveInInventory( &ent->i, from, fx, fy, to, tx, ty, &ent->TU, &ic ) )
	{
		if ( ia == IA_NOTIME )
		{
			gi.cprintf( player, PRINT_HIGH, "Can't perform action - not enough TUs!\n" );
			return;
		}
		if ( ia == IA_NORELOAD )
		{
			gi.cprintf( player, PRINT_HIGH, "Can't perform action - weapon already loaded!\n" );
			return;
		}

		// successful inventory change
		if ( from == gi.csi->idFloor )
		{
			if ( floor->i.inv )
			{
				gi.AddEvent( G_VisToPlayerMask( floor->visflags ), EV_INV_DEL );
				gi.WriteShort( floor->number );
				gi.WriteByte( from );
				gi.WriteByte( fx );
				gi.WriteByte( fy );
			} else {
				gi.AddEvent( G_VisToPlayerMask( floor->visflags ), EV_ENT_PERISH );
				gi.WriteShort( floor->number );
				ent->i.floor = NULL;
				G_FreeEdict( floor );
			}
		} else {
			gi.AddEvent( G_VisToPlayerMask(1<<ent->team), EV_INV_DEL );
			gi.WriteShort( num );
			gi.WriteByte( from );
			gi.WriteByte( fx );
			gi.WriteByte( fy );
		}

		// send tu's
		G_SendStats( ent );

		if ( ia == IA_RELOAD )
		{
			// reload
			if ( to == gi.csi->idFloor ) mask = G_VisToPlayerMask( floor->visflags );
			else if ( to == gi.csi->idRight || to == gi.csi->idLeft ) mask = G_VisToPlayerMask( ent->visflags );
			else mask = G_VisToPlayerMask( 1<<ent->team );

			// send ammo
			gi.AddEvent( mask, EV_INV_AMMO );
			gi.WriteShort( to == gi.csi->idFloor ? floor->number : ent->number );
			gi.WriteByte( gi.csi->ods[ic->item.t].ammo );
			gi.WriteByte( ic->container );
			gi.WriteByte( ic->x );
			gi.WriteByte( ic->y );

			gi.EndEvents();
			return;
		}

		// add it
		if ( to == gi.csi->idFloor )
		{
			if ( ent->i.floor == &tfloor )
			{
				int			i;

				floor = G_Spawn();
				floor->classname = "item";
				floor->type = ET_ITEM;
				floor->i = tfloor;
				VectorCopy( ent->pos, floor->pos );
				gi.GridPosToVec( floor->pos, floor->origin );
				floor->origin[2] -= 24;
				ent->i.floor = &floor->i;

				// send item info to the clients
				for ( i = 0; i < MAX_TEAMS; i++ )
					if ( level.num_alive[i] ) 
						G_CheckVis( G_VisToPlayerMask(1<<i), i, floor, true );
			} else {
				// add the item
				gi.AddEvent( G_VisToPlayerMask( floor->visflags ), EV_INV_ADD );
				gi.WriteShort( floor->number );
				gi.WriteByte( ic->item.t );
				gi.WriteByte( ic->item.a );
				gi.WriteByte( to );
				gi.WriteByte( tx );
				gi.WriteByte( ty );
				gi.WriteByte( NONE );
			}
		} else {
			gi.AddEvent( G_VisToPlayerMask(1<<ent->team), EV_INV_ADD );
			gi.WriteShort( num );
			gi.WriteByte( ic->item.t );
			gi.WriteByte( ic->item.a );
			gi.WriteByte( to );
			gi.WriteByte( tx );
			gi.WriteByte( ty );
			gi.WriteByte( NONE );
		}

		// other players receive weapon info only
		mask = G_VisToPlayerMask( ent->visflags ) & ~G_VisToPlayerMask(1<<ent->team);
		if ( mask )
		{
			if ( from == gi.csi->idRight || from == gi.csi->idLeft )
			{
				gi.AddEvent( mask, EV_INV_DEL );
				gi.WriteShort( num );
				gi.WriteByte( from );
				gi.WriteByte( fx );
				gi.WriteByte( fy );
			}
			if ( to == gi.csi->idRight || to == gi.csi->idLeft )
			{
				gi.AddEvent( mask, EV_INV_ADD );
				gi.WriteShort( num );
				gi.WriteByte( ic->item.t );
				gi.WriteByte( ic->item.a );
				gi.WriteByte( to );
				gi.WriteByte( tx );
				gi.WriteByte( ty );
				gi.WriteByte( NONE );
			}
		}

		gi.EndEvents();
		return;
	}
}


/*
=================
G_BuildForbiddenList
=================
*/
byte	*fb_list[MAX_FB_LIST];
int		fb_length;	

void G_BuildForbiddenList( int team )
{
	edict_t	*ent;
	int		vis_mask;
	int		i;

	fb_length = 0;
	if ( team ) vis_mask = 1 << team;
	else vis_mask = 0xFFFFFFFF;

	for ( i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++ )
		if ( ent->inuse && ent->type == ET_ACTOR && !(ent->state & STATE_DEAD) && (ent->visflags & vis_mask) )
			fb_list[fb_length++] = ent->pos;

	if ( fb_length > MAX_FB_LIST )
		gi.error( "G_BuildForbiddenList: list too long\n" );
}


/*
=================
G_MoveCalc
=================
*/
void G_MoveCalc( int team, pos3_t from, int distance )
{
	G_BuildForbiddenList( team );
	gi.MoveCalc( from, distance, fb_list, fb_length );
}


/*
=================
G_ClientMove
=================
*/
void G_ClientMove( player_t *player, int num, pos3_t to, qboolean stop )
{
	edict_t *ent;
	int		length, status;
	byte	dvtab[32];
	byte	dv, numdv, steps;
	pos3_t	pos;
	int		i;
	float	div, tu;

	ent = g_edicts + num;

	// check if action is possible
	if ( !G_ActionCheck( player, ent, 2 ) )
		return;

	// calculate move table
	G_MoveCalc( player->pers.team, ent->pos, MAX_ROUTE );
	length = gi.MoveLength( to, false );

	if ( length && length < 0x3F )
	{
		// start move
		gi.AddEvent( G_VisToPlayerMask(1<<ent->team), EV_ACTOR_START_MOVE );

		gi.WriteShort( ent->number );

		// assemble dv-encoded move data
		VectorCopy( to, pos );
		numdv = 0;
		tu = 0;

		while ( (dv = gi.MoveNext( pos )) < 0xFF )
		{
			// store the inverted dv 
			// (invert by flipping the first bit and add the old height)
			dvtab[numdv++] = (dv^1)&7 | (pos[2]<<3);
			PosAddDV( pos, dv );
		}
		
		if ( VectorCompare( pos, ent->pos ) )
		{
			// everything ok, found valid route
			// create movement related events
			steps = 0;
			ent->i.floor = NULL;

			while ( numdv > 0 )
			{
				// get next dv
				numdv--;

				// turn around first
				status = G_DoTurn( G_VisToPlayerMask(1<<ent->team), ent, dvtab[numdv] );
				if ( status )
				{
					// send the turn
					gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_TURN );
					gi.WriteShort( ent->number );
					gi.WriteByte( ent->dir );
				}
				if ( stop && (status & VIS_STOP) ) break;
				if ( status ) steps = 0;
				
				// decrease TUs
				div = ((dvtab[numdv]&7) < 4) ? 2 : 3;
				if ( ent->state & STATE_CROUCHED ) div *= 1.5;
				if ( (int)(tu + div) > ent->TU ) break;
				tu += div;

				// move
				PosAddDV( ent->pos, dvtab[numdv] );
				gi.GridPosToVec( ent->pos, ent->origin );

				// write move header if not yet done
				if ( !steps )
				{
					gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_MOVE );
					gi.WriteShort( num );
					gi.WriteNewSave( 1 );
				}

				// write step
				steps++;
				gi.WriteByte( dvtab[numdv] );
				gi.WriteToSave( steps );

				//gi.dprintf( "move: (pm: %i)\n", ent->visflags>>1 );

				// check if the player appears/perishes, seen from other teams
				for ( i = 0; i < MAX_TEAMS; i++ )
					if ( level.num_alive[i] )
					{
						status = G_CheckVis( G_VisToPlayerMask(1<<i), i, ent, true );
						if ( status & (VIS_APPEAR | VIS_PERISH) ) 
							steps = 0;
					}
				
				// check for players appearing/perishing, seen by the "moving one"
				status = G_CheckVis( G_VisToPlayerMask(1<<ent->team), ent->team, NULL, false );
				if ( stop && (status & VIS_STOP) ) break;
				if ( status ) steps = 0;
			}

			// submit the TUs / round down
			ent->TU -= (int)tu;
			G_SendStats( ent );

			// end the move
			G_GetFloorItems( ent );
			gi.EndEvents();

			// link it at new position
			gi.linkentity( ent );
		}
	}

}


/*
=================
G_ClientTurn
=================
*/
void G_ClientTurn( player_t *player, int num, int dv )
{
	edict_t	*ent;

	ent = g_edicts + num;

	// check if action is possible
	if ( !G_ActionCheck( player, ent, 1 ) )
		return;

	// start move
	gi.AddEvent( G_VisToPlayerMask(1<<ent->team), EV_ACTOR_START_MOVE );
	gi.WriteShort( ent->number );

	// do the turn
	G_DoTurn( G_VisToPlayerMask(1<<ent->team), ent, (byte)dv );
	ent->TU--;

	// send the turn
	gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_TURN );
	gi.WriteShort( ent->number );
	gi.WriteByte( ent->dir );

	// send the new TUs
	G_SendStats( ent );

	// end the event
	gi.EndEvents();
}


/*
=================
G_ClientStateChange
=================
*/
void G_ClientStateChange( player_t *player, int num, int newState )
{
	edict_t		*ent;
	int			changeState;
	int			i;

	ent = g_edicts + num;

	// check if any action is possible
	if ( !G_ActionCheck( player, ent, 0 ) )
		return;

	changeState = ent->state ^ newState;
	if ( !changeState )
		return;

	if ( changeState & STATE_CROUCHED )
		// check if any action is possible
		if ( G_ActionCheck( player, ent, 1 ) )
		{
			ent->state ^= STATE_CROUCHED;
			ent->TU -= 1;
			// link it
			if ( ent->state & STATE_CROUCHED ) VectorSet( ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH );
			else VectorSet( ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND );
			gi.linkentity( ent );
		}

	// send the state change
	gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_STATECHANGE );
	gi.WriteShort( ent->number );
	gi.WriteByte( ent->state );

	// check if the player appears/perishes, seen from other teams
	for ( i = 0; i < MAX_TEAMS; i++ )
		if ( level.num_alive[i] )
			G_CheckVis( G_VisToPlayerMask(1<<i), i, ent, true );
				
	// calc new vis for this player
	G_CheckVis( G_VisToPlayerMask(1<<ent->team), ent->team, NULL, false );

	// send the new TUs
	G_SendStats( ent );

	// end the event
	gi.EndEvents();
}


/*
=================
G_Moral
=================
*/
typedef enum
{
	ML_WOUND,
	ML_DEATH
} moral_modifiers;

#define MORAL_RANDOM( mod )		((mod) * (1.0 + 0.3*crand()));

#define MOB_DEATH			15
#define MOB_WOUND			0.08
#define MOF_VICTIM			-6.0
#define MOF_TEAMMATE		-1.0
#define MOF_CIVILIAN		-0.2
#define MOF_ENEMY			0.5
#define MOF_ATTACKER		2

void G_Moral( int type, edict_t *victim, edict_t *attacker, int param )
{
	edict_t	*ent;
	int		i, newMoral;
	float	mod;

	for ( i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++ )
		if ( ent->inuse && ent->type == ET_ACTOR && !(ent->state & STATE_DEAD) && ent->team != TEAM_CIVILIAN )
		{
			switch ( type )
			{
			case ML_WOUND:
			case ML_DEATH:
				mod = MOB_WOUND * param;
				if ( type == ML_DEATH ) mod += MOB_DEATH;
				if ( attacker == ent ) mod *= MOF_ATTACKER;
				if ( ent == victim ) mod *= MOF_VICTIM;
				else if ( ent->team == victim->team ) mod *= MOF_TEAMMATE;
				else if ( ent->team == attacker->team ) mod *= MOF_ENEMY;
				else if ( victim->team == TEAM_CIVILIAN && ent->team != TEAM_ALIEN && (int)sv_maxclients->value == 1 ) mod *= MOF_CIVILIAN;
				break;
			default:
				Com_Printf( "Undefined moral modifier type %i\n", type );
				mod = 0;
			}
			// clamp new moral
			newMoral = ent->moral + MORAL_RANDOM( mod );
			if ( newMoral > CRG_MORAL( ent->chr.courage ) ) ent->moral = CRG_MORAL( ent->chr.courage );
			else if ( newMoral < 0 ) ent->moral = 0;
			else ent->moral = newMoral;

			// send phys data
			G_SendStats( ent );
		}
}


/*
=================
G_MoralBehaviour
=================
*/
#define MORAL_REGENERATE	12
#define MORAL_PANIC			30

void G_MoralBehaviour( player_t *player )
{
	edict_t	*ent;
	int		i, j, newMoral;
	pos3_t	pos;

	for ( i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++ )
		if ( ent->inuse && ent->type == ET_ACTOR && ent->team == level.activeTeam && !(ent->state & STATE_DEAD) )
		{
			if ( level.activeTeam == TEAM_CIVILIAN )
				ent->moral = (int)(frand()*MORAL_PANIC*2);

			// check for panic
			if ( ent->moral <= MORAL_PANIC )
			{
//				gi.cprintf( NULL, PRINT_HIGH, "%s panics!\n", ent->chr.name );
				if ( !(ent->state & STATE_PANIC) )
				{
					// drop items in hands
					if ( ent->i.right.t != NONE )
						G_InventoryMove( player, ent->number, gi.csi->idRight, 0, 0, gi.csi->idFloor, NONE, NONE );
					if ( ent->i.left.t != NONE )
						G_InventoryMove( player, ent->number, gi.csi->idLeft, 0, 0, gi.csi->idFloor, NONE, NONE );

					// send panic
					ent->state |= STATE_PANIC;
					gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_STATECHANGE );
					gi.WriteShort( ent->number );
					gi.WriteByte( ent->state );
				}

				// move around a bit
				G_MoveCalc( ent->team, ent->pos, MAX_ROUTE );
				for ( j = 0; j < 100; j++ )
				{
					pos[0] = ent->pos[0] + (int)(crand()*10);
					pos[1] = ent->pos[1] + (int)(crand()*10);
					pos[2] = frand()*8;
					if ( ent->TU >= gi.MoveLength( pos, false ) ) break;
				}
				if ( j < 100 )
				{
					gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_CENTERVIEW );
					gi.WriteGPos( ent->pos );
					G_ClientMove( player, ent->number, pos, false );
				}

				// kill TUs
				ent->TU = 0;
			} 
			else if ( ent->state & STATE_PANIC )
			{
				ent->state &= ~STATE_PANIC;
				gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_STATECHANGE );
				gi.WriteShort( ent->number );
				gi.WriteByte( ent->state );
			}

			// set correct bounding box
			if ( ent->state & (STATE_CROUCHED|STATE_PANIC) ) VectorSet( ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_CROUCH );
			else VectorSet( ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND );

			// regenerate moral
			newMoral = ent->moral + MORAL_RANDOM( MORAL_REGENERATE );
			if ( newMoral > CRG_MORAL( ent->chr.courage ) ) ent->moral = CRG_MORAL( ent->chr.courage );
			else ent->moral = newMoral;

			// send phys data and state
			G_SendStats( ent );
			gi.EndEvents();
		}
}


/*
=================
G_Damage
=================
*/
void G_Damage( edict_t *ent, int damage, edict_t *attacker )
{
	// don't die again
	if ( ent->state & STATE_DEAD )
		return;

	// apply difficulty settings
	if ( sv_maxclients->value == 1 )
	{
		if ( attacker->team == TEAM_ALIEN && ent->team < TEAM_ALIEN )
			damage *= pow( 1.2,  difficulty->value );
		else if ( attacker->team < TEAM_ALIEN && ent->team == TEAM_ALIEN )
			damage *= pow( 1.2, -difficulty->value );
	}
	if ( damage == 0 ) damage = 1;

	if ( ent->HP <= damage )
	{
		int	i;

		// die
		ent->HP = 0;
		ent->state |= (int)(1 + frand()*3);
		VectorSet( ent->maxs, PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD );
		gi.linkentity( ent );
		level.num_alive[ent->team]--;
		level.num_kills[attacker->team][ent->team]++;
			
		// send death	
		gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_DIE );
		gi.WriteShort( ent->number );
		gi.WriteByte( ent->state );

		// apply moral changes
		G_Moral( ML_DEATH, ent, attacker, damage );

		// handle inventory
		G_InventoryToFloor( ent );

		// check visibility
		for ( i = 0; i < MAX_TEAMS; i++ )
			if ( level.num_alive[i] )
			{
				if ( i != ent->team )
					G_CheckVis( G_VisToPlayerMask(1<<i), i, ent, false );
				else
					G_CheckVis( G_VisToPlayerMask(1<<i), i, NULL, false );
			}

		// check for win conditions
		G_CheckEndGame();
	} else {
		// hit
		ent->HP -= damage;
		G_Moral( ML_WOUND, ent, attacker, damage );
		G_SendStats( ent );
	}
}


/*
=================
G_ShootSingle
=================
*/
void G_ShootSingle( edict_t *ent, fireDef_t *fd, int type, pos3_t from, pos3_t at, int mask )
{
	vec3_t	dir, angles;
	vec3_t	muzzle, impact;
	float	acc;
	trace_t	tr;
	byte	flags;
	int		i;

	// calc dir
	gi.GridPosToVec( from, muzzle );
	gi.GridPosToVec( at, impact );
	VectorSubtract( impact, muzzle, dir );
	VectorNormalize( dir );
	VectorMA( muzzle, 8, dir, muzzle );
	VecToAngles( dir, angles );

	// add random effects and get new dir
	acc = DEX_ACC( ent->chr.dexterity );
	if ( (ent->state & STATE_CROUCHED) && fd->crouch ) 
	{
		angles[PITCH] += crand() * fd->spread[0] * fd->crouch * acc;
		angles[YAW] += crand() * fd->spread[1] * fd->crouch * acc;
	} else {
		angles[PITCH] += crand() * fd->spread[0] * acc;
		angles[YAW] += crand() * fd->spread[1] * acc;
	}
	AngleVectors( angles, dir, NULL, NULL );

	// calc impact vector
	VectorMA( muzzle, fd->range, dir, impact );

	// do the trace
	tr = gi.trace( muzzle, NULL, NULL, impact, ent, MASK_SHOT );
	VectorCopy( tr.endpos, impact );

	// set flags
	flags = 0;
	if ( tr.fraction < 1.0 )
	{
		flags |= SF_IMPACT;
		if ( tr.ent ) flags |= SF_BODY;
	}

	// calculate additional visibility
	for ( i = 0; i < MAX_TEAMS; i++ )
		if ( G_TeamPointVis( i, impact ) )
			mask |= 1 << i;

	// victims see shots
	if ( tr.ent && tr.ent->type == ET_ACTOR )
		mask |= 1 << tr.ent->team;

	// send shot
	gi.AddEvent( G_VisToPlayerMask( mask ), EV_ACTOR_SHOOT );
	gi.WriteShort( ent->number );
	gi.WriteByte( type );
	gi.WriteByte( flags );
	gi.WritePos( muzzle );
	gi.WritePos( impact );
	gi.WriteDir( tr.plane.normal );

	// send shot sound to the others
	gi.AddEvent( G_VisToPlayerMask( ~mask ), EV_ACTOR_SHOOT_HIDDEN );
	gi.WriteByte( false );
	gi.WriteByte( type );

	// do damage
	if ( tr.ent && tr.ent->type == ET_ACTOR )
	{
		int damage;

		damage = fd->damage[0] + fd->damage[1] * crand();
		if ( damage < 1 ) damage = 1;

		G_Damage( tr.ent, damage, ent );
	}

	// do splash damage
	if ( fd->splrad )
	{
		edict_t	*check;
		float	dist;
		int		damage;
		int		i;

		VectorMA( impact, 8, tr.plane.normal, impact );

		for ( i = 0, check = g_edicts; i < globals.num_edicts; i++, check++ )
		{
			// check basic info
			if ( !check->inuse || check->type != ET_ACTOR )
				continue;

			// check for distance
			dist = VectorDist( impact, check->origin );
			dist = dist > US/2 ? dist - US/2 : 0;
			if ( dist > fd->splrad )
				continue;

			// check for walls
			if ( !G_ActorVis( impact, check ) )
				continue;

			// do damage
			damage = (fd->spldmg[0] + fd->spldmg[1] * crand()) * (1.0 - dist / fd->splrad);
			G_Damage( check, damage, ent );
		}		
	}

}



/*
=================
G_ClientShoot
=================
*/
void G_ClientShoot( player_t *player, int num, pos3_t at, int type )
{
	fireDef_t	*fd;
	edict_t	*ent;
	vec3_t	dir, target, center;
	pos3_t	view;
	int		i, ammo, wi, shots;
	int		mask;

	ent = g_edicts + num;

	// test shoot type
	if ( type >= ST_NUM_SHOOT_TYPES )
		gi.error( "G_ClientShoot: unknown shoot type %i.\n", type );

	if ( type < ST_LEFT_PRIMARY ) 
	{
		fd = &gi.csi->ods[ent->i.right.t].fd[type%2];
		wi = ent->i.right.t | ((type%2) << 7);
		ammo = ent->i.right.a;
	}
	else 
	{
		fd = &gi.csi->ods[ent->i.left.t].fd[type%2];
		wi = ent->i.left.t | ((type%2) << 7);
		ammo = ent->i.left.a;
	}

	// check if action is possible
	if ( !G_ActionCheck( player, ent, fd->time ) )
		return;

	if ( !ammo )
	{
		gi.cprintf( player, PRINT_HIGH, "Can't perform action - no ammo!\n" );
		return;
	}

	// rotate the player
	VectorSubtract( at, ent->pos, dir );
	ent->dir = AngleToDV( (int)(atan2( dir[1], dir[0] ) * 180 / M_PI) );

	G_CheckVis( G_VisToPlayerMask(1<<ent->team), player->pers.team, NULL, false );

	gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_ACTOR_TURN );
	gi.WriteShort( num );
	gi.WriteByte( ent->dir );

	// fire shots
	if ( fd->ammo ) 
	{
		if ( ammo < fd->ammo )
		{
			ammo = 0;
			shots = fd->shots * ammo / fd->ammo;
		} else if ( !player->pers.ai ) {
			ammo -= fd->ammo;
			shots = fd->shots;
		}
	}
	else shots = fd->shots;

	// view center
	view[0] = (ent->pos[0] + at[0]) / 2;
	view[1] = (ent->pos[1] + at[1]) / 2;
	view[2] = at[2];

	// calculate visibility
	gi.GridPosToVec( at, target );
	VectorSubtract( target, ent->origin, dir );
	VectorMA( ent->origin, 0.5, dir, center );
	mask = 0;
	for ( i = 0; i < MAX_TEAMS; i++ )
		if ( ent->visflags & (1<<i) || G_TeamPointVis( i, target ) || G_TeamPointVis( i, center ) )
			mask |= 1 << i;

	// start shoot
	gi.AddEvent( G_VisToPlayerMask( mask ), EV_ACTOR_START_SHOOT );
	gi.WriteShort( ent->number );
	gi.WriteByte( wi );
	gi.WriteGPos( view );

	// send shot sound to the others
	gi.AddEvent( G_VisToPlayerMask( ~mask ), EV_ACTOR_SHOOT_HIDDEN );
	gi.WriteByte( true );
	gi.WriteByte( wi );

	// fire all shots
	for ( i = 0; i < fd->shots; i++ )
		G_ShootSingle( ent, fd, wi, ent->pos, at, mask );

	// send TUs
	if ( ent->inuse )
	{
		ent->TU -= fd->time;
		G_SendStats( ent );

		gi.AddEvent( G_VisToPlayerMask(ent->visflags), EV_INV_AMMO );
		gi.WriteShort( num );
		gi.WriteByte( ammo );
		if ( type < ST_LEFT_PRIMARY ) 
		{
			ent->i.right.a = ammo;
			gi.WriteByte( gi.csi->idRight );
		} else {
			ent->i.left.a = ammo;
			gi.WriteByte( gi.csi->idLeft );
		}
		gi.WriteByte( 0 );
		gi.WriteByte( 0 );
	}

	// end events
	gi.EndEvents();
}


/*
=================
G_ClientInvMove
=================
*/
void G_ClientInvMove( player_t *player, int num )
{
	int		from, fx, fy, to, tx, ty;

	from = gi.ReadByte();
	fx = gi.ReadByte();
	fy = gi.ReadByte();
	to = gi.ReadByte();
	tx = gi.ReadByte();
	ty = gi.ReadByte();

	G_InventoryMove( player, num, from, fx, fy, to, tx, ty );
}


/*
=================
G_ClientAction
=================
*/
void G_ClientAction( player_t *player )
{
	int		action;
	int		num;
	pos3_t	pos;

	// read the header
	action = gi.ReadByte();
	num = gi.ReadShort();

	switch ( action )
	{
	case PA_NULL:
		// do nothing on a null action
		break;

	case PA_TURN:
		G_ClientTurn( player, num, gi.ReadByte() );
		break;

	case PA_MOVE:
		gi.ReadGPos( pos );
		G_ClientMove( player, num, pos, true );
		break;

	case PA_STATE:
		G_ClientStateChange( player, num, gi.ReadByte() );
		break;

	case PA_SHOOT:
		gi.ReadGPos( pos );
		G_ClientShoot( player, num, pos, gi.ReadByte() );
		break;

	case PA_INVMOVE:
		G_ClientInvMove( player, num );
		break;

	default:
		gi.error ( "G_ClientAction: Unknown action!\n" );
		break;
	}
}


/*
=================
G_GetTeam
=================
*/
void G_GetTeam( player_t *player )
{
	player_t *p;
	int i, j;

	if ( player->pers.team ) 
		return;

	// find a team	
	if ( sv_maxclients->value == 1 )
	{
		player->pers.team = 1;
	}
	else if ( sv_teamplay->value )
	{
		// set the team specified in the userinfo
		i = atoi( Info_ValueForKey( player->pers.userinfo, "teamnum" ) );
		if ( i > 0 ) player->pers.team = i;
		else player->pers.team = 1;
	} 
	else 
	{
		// search team
		for ( i = 1; i < MAX_TEAMS; i++ )
			if ( level.num_spawnpoints[i] ) 
			{
				// check if team is in use
				for ( j = 0, p = game.players; j < game.maxplayers; j++, p++ )
					if ( p->inuse && p->pers.team == i )
						// team already in use
						break;
				if ( j >= game.maxplayers ) break;
			}

		// set the team
		if ( i < MAX_TEAMS ) 
		{
			// remove ai player
			for ( j = 0, p = game.players + game.maxplayers; j < game.maxplayers; j++, p++ )
				if ( p->inuse && p->pers.team == i )
				{
					gi.bprintf( PRINT_HIGH, "Removing ai player..." );
					p->inuse = false;
					break;
				}
			player->pers.team = i;
		}
		else player->pers.team = -1;
	}
}


/*
=================
G_ClientTeamInfo
=================
*/
void G_ClientTeamInfo( player_t *player )
{
	equipDef_t *ed;
	edict_t *ent;
	char	*name;
	int		i, j, length;
	int		item, ammo, container, x, y;
	byte	count[MAX_OBJDEFS];

	// find a team
	G_GetTeam( player );
	length = gi.ReadByte();

	// find actors
	for ( j = 0, ent = g_edicts; j < globals.num_edicts; j++, ent++ )
		if ( ent->type == ET_ACTORSPAWN && player->pers.team == ent->team )
			break;

	// search equipment definition
	name = gi.cvar_string( "equip" );
	for ( i = 0, ed = gi.csi->eds; i < gi.csi->numEDs; i++, ed++ )
		if ( !strcmp( name, ed->name ) )
			break;
	if ( i == gi.csi->numEDs ) 
	{
		ed = NULL;
		Com_Printf( "Equipment '%s' not found, accepting any equipment!\n", name );
	}

	memset( count, 0, sizeof( count ) );
	for ( i = 0; i < length; i++ )
	{
		if ( j < globals.num_edicts )
		{
			// here the actors actually spawn

			level.num_alive[ent->team]++;
			level.num_spawned[ent->team]++;
			ent->type = ET_ACTOR;
			ent->pnum = player->num;
			gi.linkentity( ent );

			// model
			ent->chr.ucn = gi.ReadShort();
			strcpy( ent->chr.name, gi.ReadString() );
			strcpy( ent->chr.body, gi.ReadString() );
			strcpy( ent->chr.head, gi.ReadString() );
			ent->chr.skin = gi.ReadByte();
			ent->body = gi.modelindex( ent->chr.body );
			ent->head = gi.modelindex( ent->chr.head );
			ent->skin = ent->chr.skin;

			// attributes
			ent->chr.strength = gi.ReadByte();
			ent->chr.dexterity = gi.ReadByte();
			ent->chr.swiftness = gi.ReadByte();
			ent->chr.intelligence = gi.ReadByte();
			ent->chr.courage = gi.ReadByte();
			ent->HP = STR_HP( ent->chr.strength );
			ent->moral = CRG_MORAL( ent->chr.courage );

			// inventory
			item = gi.ReadByte();
			while ( item != NONE )
			{
				// read info
				ammo = gi.ReadByte();
				container = gi.ReadByte();
				x = gi.ReadByte();
				y = gi.ReadByte();

				// check info and add item if ok
//				if ( !ed ) 
					Com_AddToInventory( &ent->i, item, ammo, container, x, y );
/*				else
				{
					count[item]++;
					if ( count[item] > ed->num[k] )
						gi.cprintf( player, PRINT_HIGH, "Item '%s' not allowed and removed from inventory.\n", gi.csi->ods[item].name );
					else
						Com_AddToInventory( &ent->i, item, ammo, container, x, y );
				}
*/
				// get next item
				item = gi.ReadByte();
			}
		} else {
			// just do nothing with the info
			gi.ReadString();
			gi.ReadString();
			while ( gi.ReadByte() != NONE );
		}

		// find actors
		for ( ; j < globals.num_edicts; j++, ent++ )
			if ( ent->type == ET_ACTORSPAWN && player->pers.team == ent->team )
				break;
	}
}


/*
=================
G_ClientEndRound
=================
*/
void G_ClientEndRound( player_t *player )
{
	player_t	*p;
	int			i, lastTeam, nextTeam;

	// inactive players can't end their inactive round :)
	if ( level.activeTeam != player->pers.team ) 
		return;

	// check for "team oszillation"
	if ( level.framenum < level.nextEndRound )
		return;
	level.nextEndRound = level.framenum + 50;

	// let all the invisible players perish now
	G_CheckVis( G_VisToPlayerMask(1<<level.activeTeam), level.activeTeam, NULL, true );

	lastTeam = player->pers.team;
	level.activeTeam = -1;

	p = NULL;
	while ( level.activeTeam == -1 )
	{
		// search next team
		nextTeam = -1;
		for ( i = lastTeam+1; i != lastTeam; i++ )
		{
			if ( i >= MAX_TEAMS ) i = 0;
			if ( (level.num_alive[i] || level.num_spawnpoints[i] && !level.num_spawned[i]) && i != lastTeam ) 
			{
				nextTeam = i;
				break;
			}
		}
		if ( nextTeam == -1 ) 
		{
//			gi.bprintf( PRINT_HIGH, "Can't change round - no living actors left.\n" );
			level.activeTeam = lastTeam;
			return;
		}

		// search corresponding player
		for ( i = 0, p = game.players; i < game.maxplayers*2; i++, p++ )
			if ( p->inuse && p->pers.team == nextTeam )
			{
				// found next player
				level.activeTeam = nextTeam;
				break;
			}

		if ( level.activeTeam == -1 && sv_ai->value )
		{
			// no corresponding player found - create ai player
			p = AI_CreatePlayer( nextTeam );
			if ( p ) level.activeTeam = nextTeam;
		}

		lastTeam = nextTeam;
	}

	// communicate next player in row to clients
	gi.AddEvent( PM_ALL, EV_ENDROUND );
	gi.WriteByte( level.activeTeam );

	// give his players their TUs
	G_GiveTimeUnits( level.activeTeam );

	// apply moral behaviour
	G_MoralBehaviour( p );

	// start ai
	p->pers.last = NULL;

	// finish off events
	gi.EndEvents();
}


/*
=================
G_ClientBegin
=================
*/
void G_ClientBegin( player_t *player ) 
{
	// get a team
	G_GetTeam( player );

	// activate his round if he's first to join
	if ( level.activeTeam == -1 ) 
		level.activeTeam = player->pers.team;

	level.numplayers++;

	// do all the init events here...
	// reset the data
	gi.AddEvent( P_MASK(player), EV_RESET | INSTANTLY );
	gi.WriteByte( player->pers.team );
	gi.WriteByte( level.activeTeam );

	// show visible players and submit stats
	G_ClearVisFlags( player->pers.team );
	G_CheckVis( G_VisToPlayerMask(1<<player->pers.team), player->pers.team, NULL, true );
	G_SendPlayerStats( player );

	// give time units if necessary
	if ( level.activeTeam == player->pers.team )
		G_GiveTimeUnits( player->pers.team );

	// spawn camera (starts client rendering)
	gi.AddEvent( P_MASK(player), EV_START );

	// send events
	gi.EndEvents();

	// inform all clients
	gi.bprintf (PRINT_HIGH, "%s has taken control over team %i.\n", player->pers.netname, player->pers.team );
}


/*
===========
G_ClientUserInfoChanged

called whenever the player updates a userinfo variable.

The game can override any of the settings in place
(forcing skins or names, etc) before copying it off.
============
*/
void G_ClientUserinfoChanged(player_t *player, char *userinfo)
{
	char	*s;

	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
	{
		strcpy (userinfo, "\\name\\badinfo");
	}

	// set name
	s = Info_ValueForKey (userinfo, "name");
	strncpy (player->pers.netname, s, sizeof(player->pers.netname)-1);

	// set spectator
	s = Info_ValueForKey (userinfo, "spectator");
	if (strcmp(s, "1"))
		player->pers.spectator = true;
	else
		player->pers.spectator = false;

	strncpy (player->pers.userinfo, userinfo, sizeof(player->pers.userinfo)-1);
}


/*
=================
G_ClientConnect
=================
*/
qboolean G_ClientConnect(player_t *player, char *userinfo)
{
	char *value;

	// check to see if they are on the banned IP list
	value = Info_ValueForKey (userinfo, "ip");
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return false;
	}

	// reset persistant data
	memset( &player->pers, 0, sizeof( client_persistant_t ) );
	strncpy (player->pers.userinfo, userinfo, sizeof(player->pers.userinfo)-1);

	gi.bprintf (PRINT_HIGH, "%s is connecting...\n", Info_ValueForKey (userinfo, "name") );
	return true;
}


/*
=================
G_ClientDisconnect
=================
*/
void G_ClientDisconnect(player_t *player)
{
	level.numplayers--;

	if ( level.activeTeam == player->pers.team )
		G_ClientEndRound( player );

	gi.bprintf (PRINT_HIGH, "%s disconnected.\n", player->pers.netname );
}


