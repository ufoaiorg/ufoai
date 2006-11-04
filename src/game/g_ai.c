// g_ai.c -- artificial intelligence
//

#include "g_local.h"

// ====================================================================

typedef struct 
{
	pos3_t	to, stop;
	byte	mode, shots;
	edict_t	*target;
} ai_action_t;

/*
=================
AI_VisFactor
=================
*/
float AI_VisFactor( edict_t *ent, edict_t *check )
{
	vec3_t	test, dir;
	float	delta;
	int		i, n;

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

	// side shifting -> better checks
	dir[0] = ent->origin[1] - check->origin[1];
	dir[1] = check->origin[0] - ent->origin[0];
	dir[2] = 0;
	VectorNormalize( dir );
	VectorMA( test, -7, dir, test );

	// do 3 tests
	n = 0;
	for ( i = 0; i < 3; i++ )
	{
		if ( !gi.TestLine( ent->origin, test ) )
			n++;

		// look further down or stop
		if ( !delta ) { if ( n > 0 ) return 1.0; else return 0.0; }
		VectorMA( test, 7, dir, test );
		test[2] -= delta;
	}

	// return factor
	if ( n >= 3 ) return 1.0;
	else if ( n == 2 ) return 0.5;
	else if ( n == 1 ) return 0.1;
	else return 0.0;
}


/*
=================
AI_CheckFF
=================
*/
qboolean AI_CheckFF( edict_t *ent, vec3_t target, float spread )
{
	edict_t	*check;
	vec3_t	dtarget, dcheck, back;
	float	cosSpread;
	int		i;

	// spread data
	if ( spread < 1.0 ) spread = 1.0;
	spread *= M_PI / 180;
	cosSpread = cos( spread );
	VectorSubtract( target, ent->origin, dtarget );
	VectorNormalize( dtarget );
	VectorScale( dtarget, PLAYER_WIDTH / spread, back );

	for ( i = 0, check = g_edicts; i < globals.num_edicts; i++, check++ )
		if ( check->inuse && check->type == ET_ACTOR && ent != check &&
			check->team == ent->team && !(check->state & STATE_DEAD) )
		{
			// found ally
			VectorSubtract( check->origin, ent->origin, dcheck );
			if ( DotProduct( dtarget, dcheck ) > 0.0 ) 
			{
				// ally in front of player
				VectorAdd( dcheck, back, dcheck );
				VectorNormalize( dcheck );
				if ( DotProduct( dtarget, dcheck ) > cosSpread )
					return true;
			}
		}

	// no ally in danger
	return false;
}


/*
=================
AI_FighterCalcGuete
=================
*/
#define GUETE_HIDE			30
#define GUETE_SHOOT_HIDE	40
#define GUETE_CLOSE_IN		8
#define GUETE_KILL			30
#define GUETE_RANDOM		10
#define GUETE_CIV_FACTOR	0.25
		
#define CLOSE_IN_DIST		1200.0
#define SPREAD_FACTOR		12.0
#define	SPREAD_NORM(x)		(x > 0 ? SPREAD_FACTOR/(x*M_PI/180) : 0)
#define HIDE_DIST			3

float AI_FighterCalcGuete( edict_t *ent, pos3_t to, ai_action_t *aia )
{
	objDef_t	*od;
	fireDef_t	*fd;
	edict_t		*check;
	int		move, delta, tu;
	int		i, fm, shots;
	float	dist, minDist, nspread;
	float	guete, dmg, maxDmg;

	// set basic parameters
	guete = 0.0;
	aia->target = NULL;
	VectorCopy( to, ent->pos );
	VectorCopy( to, aia->to );
	VectorCopy( to, aia->stop );
	gi.GridPosToVec( to, ent->origin );
	
	move = gi.MoveLength( to, true );
	tu = ent->TU - move;
	od = ent->i.right.t != NONE ? &gi.csi->ods[ent->i.right.t] : NULL;

	// test for time
	if ( tu < 0 ) return 0.0;

	// shooting
	if ( od )
	{
		maxDmg = 0.0;
		for ( fm = 0; fm < 2; fm++ )
		{
			fd = &od->fd[fm];
			if ( !fd->time ) continue;

			nspread = SPREAD_NORM( (fd->spread[0]+fd->spread[1])*DEX_ACC(ent->chr.dexterity)/2 );
			shots = tu / fd->time;
			if ( shots ) 
			{
				// search best target
				for ( i = 0, check = g_edicts; i < globals.num_edicts; i++, check++ )
					if ( check->inuse && check->type == ET_ACTOR && 
						check->team != ent->team && !(check->state & STATE_DEAD) )
					{
						// check range
						dist = VectorDist( ent->origin, check->origin );
						if ( dist > fd->range ) continue;

						// check FF
						if ( AI_CheckFF( ent, check->origin, fd->spread[0] ) ) continue;

						// calculate expected damage
						dmg = AI_VisFactor( ent, check );
						if ( dmg == 0.0 ) continue;

						dmg *= fd->damage[0] * fd->shots * shots;
						if ( nspread && dist > nspread ) dmg *= nspread / dist;
						if ( dmg > 100.0 ) dmg = 100.0;

						// add kill bonus
						if ( dmg > check->HP ) dmg += GUETE_KILL;

						// civilian malus
						if ( check->team == 0 ) dmg *= GUETE_CIV_FACTOR;

						// check if most damage can be done here
						if ( dmg > maxDmg )
						{
							maxDmg = dmg;
							aia->mode = fm;
							aia->shots = shots;
							aia->target = check;
						}
					}
			}
		}

		// add damage to guete
		if ( aia->target )
		{
			guete += maxDmg;
			tu -= od->fd[aia->mode].time * aia->shots;
		}
	}

	// hide
	if ( !(G_TestVis( -ent->team, ent, VT_PERISH|VT_NOFRUSTOM ) & VIS_YES) ) 
	{
		// is a hiding spot
		guete += GUETE_HIDE;
	} 
	else if ( aia->target && tu >= 2 )
	{
		// search hiding spot after shooting
		byte	minX, maxX, minY, maxY;

		G_MoveCalc( 0, to, HIDE_DIST );
		ent->pos[2] = to[2];
		minX = to[0] - HIDE_DIST > 0 ? to[0] - HIDE_DIST : 0;
		minY = to[1] - HIDE_DIST > 0 ? to[1] - HIDE_DIST : 0;
		maxX = to[0] + HIDE_DIST < 254 ? to[0] + HIDE_DIST : 254;
		maxY = to[1] + HIDE_DIST < 254 ? to[1] + HIDE_DIST : 254;

		for ( ent->pos[1] = minY; ent->pos[1] <= maxY; ent->pos[1]++ )
		{
			for ( ent->pos[0] = minX; ent->pos[0] <= maxX; ent->pos[0]++ )
			{
				// time
				delta = gi.MoveLength( ent->pos, false );
				if ( delta > tu ) continue;
				tu -= delta;

				// visibility
				gi.GridPosToVec( ent->pos, ent->origin );
				if ( G_TestVis( -ent->team, ent, VT_PERISH|VT_NOFRUSTOM ) & VIS_YES ) continue;

				// found a hiding spot
				VectorCopy( ent->pos, aia->stop );
				guete += GUETE_SHOOT_HIDE;
				break;
			}
			if ( ent->pos[0] <= maxX ) break;
		}
	}

	// close in
	minDist = CLOSE_IN_DIST;
	for ( i = 0, check = g_edicts; i < globals.num_edicts; i++, check++ )
		if ( check->inuse && check->team != ent->team && !(check->state & STATE_DEAD) )
		{
			dist = VectorDist( ent->origin, check->origin );
			if ( dist < minDist ) minDist = dist;
		}
	guete += GUETE_CLOSE_IN * (1.0 - minDist / CLOSE_IN_DIST);

	// add random effects
	guete += GUETE_RANDOM * frand();

	return guete;
}


/*
=================
AI_CivilianCalcGuete
=================
*/
#define GUETE_CIV_RANDOM	30
#define GUETE_RUN_AWAY		50
#define GUETE_CIV_FAULHEIT	5
#define RUN_AWAY_DIST		160

float AI_CivilianCalcGuete( edict_t *ent, pos3_t to, ai_action_t *aia )
{
	edict_t		*check;
	int		i, move, tu;
	float	dist, minDist;
	float	guete;

	// set basic parameters
	guete = 0.0;
	aia->target = NULL;
	VectorCopy( to, ent->pos );
	VectorCopy( to, aia->to );
	VectorCopy( to, aia->stop );
	gi.GridPosToVec( to, ent->origin );
	
	move = gi.MoveLength( to, true );
	tu = ent->TU - move;

	// test for time
	if ( tu < 0 ) return 0.0;

	// run away
	minDist = RUN_AWAY_DIST;
	for ( i = 0, check = g_edicts; i < globals.num_edicts; i++, check++ )
		if ( check->inuse && check->team != ent->team && !(check->state & STATE_DEAD) )
		{
			dist = VectorDist( ent->origin, check->origin );
			if ( dist < minDist ) minDist = dist;
		}
	guete += GUETE_RUN_AWAY * minDist / RUN_AWAY_DIST;

	// add Faulheit
	guete += GUETE_CIV_FAULHEIT * tu / ent->TU;

	// add random effects
	guete += GUETE_CIV_RANDOM * frand();

	return guete;
}


/*
=================
AI_ActorThink
=================
*/
#define AI_MAX_DIST	30

void AI_ActorThink( player_t *player, edict_t *ent )
{
	ai_action_t	aia, bestAia;
	pos3_t		oldPos, to;
	vec3_t		oldOrigin;
	int			xl, yl, xh, yh;
	int			i;
	float		guete, best;

//	Com_Printf( "AI_ActorThink (ent %i)\n", ent->number );

	// calculate move table
	G_MoveCalc( 0, ent->pos, MAX_ROUTE );
	gi.MoveStore();

	// set borders
	xl = (int)ent->pos[0] - AI_MAX_DIST; if ( xl < 0 ) xl = 0;
	yl = (int)ent->pos[1] - AI_MAX_DIST; if ( yl < 0 ) yl = 0;
	xh = (int)ent->pos[0] + AI_MAX_DIST; if ( xh > WIDTH ) xl = WIDTH;
	yh = (int)ent->pos[1] + AI_MAX_DIST; if ( yh > WIDTH ) yh = WIDTH;

	// search best action
	best = 0.0;
	VectorCopy( ent->pos, oldPos );
	VectorCopy( ent->origin, oldOrigin );

	for ( to[2] = 0; to[2] < HEIGHT; to[2]++ )
		for ( to[1] = yl; to[1] < yh; to[1]++ )
			for ( to[0] = xl; to[0] < xh; to[0]++ )
				if ( gi.MoveLength( to, true ) < 0x3F )
				{
					if ( ent->team != 0 )
						guete = AI_FighterCalcGuete( ent, to, &aia );
					else
						guete = AI_CivilianCalcGuete( ent, to, &aia );

					if ( guete > best )
					{
						bestAia = aia;
						best = guete;
					}
				}

	VectorCopy( oldPos, ent->pos );
	VectorCopy( oldOrigin, ent->origin );

	if ( best == 0.0 )
		// nothing found to do
		return;

	// do the first move	
	G_ClientMove( player, ent->number, bestAia.to, false );

//	Com_Printf( "(%i %i %i) (%i %i %i)\n",
//		(int)bestAia.to[0], (int)bestAia.to[1], (int)bestAia.to[2],
//		(int)bestAia.stop[0], (int)bestAia.stop[1], (int)bestAia.stop[2] );

	// shoot('n'hide)
	if ( ent->team != TEAM_CIVILIAN && bestAia.target )
	{
		for ( i = 0; i < bestAia.shots; i++ )
			G_ClientShoot( player, ent->number, bestAia.target->pos, bestAia.mode );
		G_ClientMove( player, ent->number, bestAia.stop, false );
	}
}


/*
=================
AI_Run

Every server frame one single actor is handled - always in the same order
=================
*/
void AI_Run( void )
{
	player_t *player;
	edict_t	*ent;
	int		i;

	for ( i = 0, player = game.players + game.maxplayers; i < game.maxplayers; i++, player++ )
		if ( player->inuse && player->pers.ai && level.activeTeam == player->pers.team )
		{
			// spawn, if not yet done
			if ( !level.num_spawned[player->pers.team] ) 
			{
				if ( player->pers.team == TEAM_CIVILIAN ) G_SpawnAIPlayer( player, ai_numcivilians->value );
				else  G_SpawnAIPlayer( player, ai_numaliens->value );
			}

			// find next actor to handle
			if ( !player->pers.last ) { ent = g_edicts; i = 0; }
			else { ent = player->pers.last + 1; i = ent - g_edicts; }

			for ( ; i < globals.num_edicts; i++, ent++ )
				if ( ent->inuse && ent->team == player->pers.team && !(ent->state & STATE_DEAD) && ent->TU ) 
				{
					AI_ActorThink( player, ent );
					player->pers.last = ent;
					break;
				}

			if ( i >= globals.num_edicts ) 
			{
				// nothing left to do -> end his round
				player->pers.last = g_edicts + globals.num_edicts;
				G_ClientEndRound( player );
			}
		}
}


/*
=================
AI_CreatePlayer
=================
*/
player_t *AI_CreatePlayer( int team )
{
	player_t *p;
	int i;

	if ( !sv_ai->value )
		return NULL;

	Com_Printf( "Creating ai player...\n" );
	for ( i = 0, p = game.players + game.maxplayers; i < game.maxplayers; i++, p++ )
		if ( !p->inuse )
		{
			memset( p, 0, sizeof( player_t ) );
			p->inuse = true;
			p->num = p - game.players;
			p->ping = 0;
			p->pers.team = team;
			p->pers.ai = true;
			if ( team == TEAM_CIVILIAN ) G_SpawnAIPlayer( p, ai_numcivilians->value );
			else  G_SpawnAIPlayer( p, ai_numaliens->value );
			return p;
		}

	// nothing free
	return NULL;
}
