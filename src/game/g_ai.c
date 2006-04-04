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
#define SPREAD_FACTOR		8.0
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
	gi.GridPosToVec( gi.map, to, ent->origin );

	move = gi.MoveLength( gi.map, to, true );
	tu = ent->TU - move;
	if ( ent->i.c[gi.csi->idRight] && ent->i.c[gi.csi->idRight]->item.m != NONE )
		od = &gi.csi->ods[ent->i.c[gi.csi->idRight]->item.m];
	else
		od = NULL;

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

			nspread = SPREAD_NORM( (fd->spread[0]+fd->spread[1])*GET_ACC(ent->chr.skills[ABILITY_ACCURACY], fd->weaponSkill)/2 );
			shots = tu / fd->time;
			if ( shots )
			{
				// search best target
				for ( i = 0, check = g_edicts; i < globals.num_edicts; i++, check++ )
					if ( check->inuse && check->type == ET_ACTOR && ent != check
						&& (check->team != ent->team || ent->state & STATE_INSANE)
						&& !(check->state & STATE_DEAD) )
					{
						// don't shoot civilians in mp
						if ( check->team == TEAM_CIVILIAN && (int)sv_maxclients->value > 1 && !(ent->state & STATE_INSANE) )
							continue;

						// check range
						dist = VectorDist( ent->origin, check->origin );
						if ( dist > fd->range ) continue;

						// check FF
						if ( AI_CheckFF( ent, check->origin, fd->spread[0] ) && !(ent->state & STATE_INSANE) )
							continue;

						// calculate expected damage
						dmg = G_ActorVis( ent->origin, check, true );
						if ( dmg == 0.0 ) continue;

						dmg *= fd->damage[0] * fd->shots * shots;
						if ( nspread && dist > nspread ) dmg *= nspread / dist;
						if ( dmg > 100.0 ) dmg = 100.0;

						// add kill bonus
						if ( dmg > check->HP ) dmg += GUETE_KILL;

						// civilian malus
						if ( check->team == TEAM_CIVILIAN && !(ent->state & STATE_INSANE) )
							dmg *= GUETE_CIV_FACTOR;

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

	if ( ent->state & STATE_RAGE ) return guete;

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
				delta = gi.MoveLength( gi.map, ent->pos, false );
				if ( delta > tu ) continue;
				tu -= delta;

				// visibility
				gi.GridPosToVec( gi.map, ent->pos, ent->origin );
				if ( G_TestVis( -ent->team, ent, VT_PERISH|VT_NOFRUSTOM ) & VIS_YES ) continue;

				// found a hiding spot
				VectorCopy( ent->pos, aia->stop );
				guete += GUETE_SHOOT_HIDE;
				break;
			}
			if ( ent->pos[0] <= maxX ) break;
		}
	}

	return guete;
}


/*
=================
AI_CivilianCalcGuete
=================
*/
#define GUETE_CIV_RANDOM	30
#define GUETE_RUN_AWAY		50
#define GUETE_CIV_LAZINESS	5
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
	gi.GridPosToVec( gi.map, to, ent->origin );

	move = gi.MoveLength( gi.map, to, true );
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

	// add laziness
	guete += GUETE_CIV_LAZINESS * tu / ent->TU;

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

//	Com_Printf( "AI_ActorThink (ent %i, frame %i)\n", ent->number, level.framenum );

	// calculate move table
	G_MoveCalc( 0, ent->pos, MAX_ROUTE );
	gi.MoveStore( gi.map );

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
				if ( gi.MoveLength( gi.map, to, true ) < 0xFF )
				{
					if ( ent->team == TEAM_CIVILIAN || ent->state & STATE_PANIC )
						guete = AI_CivilianCalcGuete( ent, to, &aia );
					else
						guete = AI_FighterCalcGuete( ent, to, &aia );

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
	G_ClientMove( player, 0, ent->number, bestAia.to, false );

//	Com_Printf( "(%i %i %i) (%i %i %i)\n",
//		(int)bestAia.to[0], (int)bestAia.to[1], (int)bestAia.to[2],
//		(int)bestAia.stop[0], (int)bestAia.stop[1], (int)bestAia.stop[2] );

	// shoot('n'hide)
	if ( bestAia.target )
	{
		for ( i = 0; i < bestAia.shots; i++ )
			G_ClientShoot( player, ent->number, bestAia.target->pos, bestAia.mode );
		G_ClientMove( player, ent->team, ent->number, bestAia.stop, false );
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
	int		i, j;

	// don't run this too often to prevent overflows
	if ( level.framenum % 10 )
		return;

	for ( i = 0, player = game.players + game.maxplayers; i < game.maxplayers; i++, player++ )
		if ( player->inuse && player->pers.ai && level.activeTeam == player->pers.team )
		{
			// find next actor to handle
			if ( !player->pers.last ) ent = g_edicts;
			else ent = player->pers.last + 1;

			for ( j = ent - g_edicts; j < globals.num_edicts; j++, ent++ )
				if ( ent->inuse && ent->team == player->pers.team && ent->type == ET_ACTOR && !(ent->state & STATE_DEAD) && ent->TU )
				{
					AI_ActorThink( player, ent );
					player->pers.last = ent;
					return;
				}

			// nothing left to do, request endround
			if ( j >= globals.num_edicts )
			{
				G_ClientEndRound( player );
				player->pers.last = g_edicts + globals.num_edicts;
			}
			return;
		}
}




/*
=================
G_SpawnAIPlayer
=================
*/
#define MAX_SPAWNPOINTS		64
int spawnPoints[MAX_SPAWNPOINTS];

void G_SpawnAIPlayer( player_t *player, int numSpawn )
{
	edict_t	*ent;
	byte	equip[MAX_OBJDEFS];
	int i, j, numPoints, team;
	int ammo, num;

	// search spawn points
	team = player->pers.team;
	numPoints = 0;
	for ( i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++ )
		if ( ent->inuse && ent->type == ET_ACTORSPAWN && ent->team == team )
			spawnPoints[numPoints++] = i;

	// check spawn point number
	if ( numPoints < numSpawn )
	{
		Com_Printf( _("Not enough spawn points for team %i\n"), team );
		numSpawn = numPoints;
	}

	// prepare equipment
	if ( team != TEAM_CIVILIAN )
	{
		equipDef_t	*ed;
		char name[MAX_VAR];

		strcpy( name, gi.cvar_string( "ai_equipment" ) );
		for ( i = 0, ed = gi.csi->eds; i < gi.csi->numEDs; i++, ed++ )
			if ( !strcmp( name, ed->name ) )
				break;
		if ( i == gi.csi->numEDs ) ed = &gi.csi->eds[0];
		else ed = &gi.csi->eds[i];

		for ( i = 0; i < gi.csi->numODs; i++ )
			equip[i] = ed->num[i];
	}

	// spawn players
	for ( j = 0; j < numSpawn; j++ )
	{
		// select spawnpoint
		while ( ent->type != ET_ACTORSPAWN )
			ent = &g_edicts[ spawnPoints[(int)(frand()*numPoints)] ];

		if ( team != TEAM_CIVILIAN )
		{
			// spawn
			level.num_spawned[team]++;
			level.num_alive[team]++;
			ent->chr.skin = gi.GetModelAndName( gi.cvar_string( "ai_alien" ), ent->chr.path, ent->chr.body, ent->chr.head, ent->chr.name );
			strcpy( ent->chr.name, _("Alien") );
			ent->type = ET_ACTOR;
			ent->pnum = player->num;
			gi.linkentity( ent );

			// skills
			Com_CharGenAbilitySkills( &ent->chr, 0, 100, 0, 100 );
			ent->chr.skills[ABILITY_MIND] += 100;
			if ( ent->chr.skills[ABILITY_MIND] >= MAX_SKILL ) ent->chr.skills[ABILITY_MIND] = MAX_SKILL;

			ent->HP = GET_HP( ent->chr.skills[ABILITY_POWER] );
			ent->AP = 100;
			ent->morale = GET_MORALE( ent->chr.skills[ABILITY_MIND] );
			if ( ent->morale >= MAX_SKILL ) ent->morale = MAX_SKILL;

			// search for weapons
			num = 0;
			for ( i = 0; i < gi.csi->numODs; i++ )
				if ( equip[i] && gi.csi->ods[i].weapon )
					num++;

			if ( num )
			{
				// add weapon
				item_t item;
				item.m = NONE;

				num = (int)(frand()*num);
				for ( i = 0; i < gi.csi->numODs; i++ )
					if ( equip[i] && gi.csi->ods[i].weapon )
					{
						if ( num ) num--;
						else break;
					}
				item.t = i;
				equip[i]--;

				if ( gi.csi->ods[i].reload )
				{
					for ( ammo = 0; ammo < gi.csi->numODs; ammo++ )
						if ( equip[ammo] && gi.csi->ods[ammo].link == i )
							break;
					if ( ammo < gi.csi->numODs )
					{
						item.a = gi.csi->ods[i].ammo;
						item.m = ammo;
						equip[ammo]--;
						if ( equip[ammo] > equip[i] )
						{
							item_t mun;
							mun.t = ammo;
							Com_AddToInventory( &ent->i, mun, gi.csi->idBelt, 0, 0 );
							equip[ammo]--;
						}
					}
					else item.a = 0;
					Com_AddToInventory( &ent->i, item, gi.csi->idRight, 0, 0 );
				} else {
					item.a = gi.csi->ods[i].ammo;
					item.m = i;
				}

				// set model
				ent->chr.inv = &ent->i;
				ent->body = gi.modelindex( Com_CharGetBody( &ent->chr ) );
				ent->head = gi.modelindex( Com_CharGetHead( &ent->chr ) );
				ent->skin = ent->chr.skin;
			}
			else
			{
				// nothing left
				Com_Printf( _("Not enough weapons in equipment '%s'\n"), gi.cvar_string( "ai_equipment" ) );
			}
		} else {
			// spawn
			level.num_spawned[TEAM_CIVILIAN]++;
			level.num_alive[TEAM_CIVILIAN]++;

			Com_CharGenAbilitySkills( &ent->chr, 0, 20, 0, 20 );
			ent->HP = GET_HP( ent->chr.skills[ABILITY_POWER] ) / 2;
			ent->AP = 100;
			ent->morale = GET_MORALE( ent->chr.skills[ABILITY_MIND] );

			ent->chr.skin = gi.GetModelAndName( gi.cvar_string( "ai_civilian" ), ent->chr.path, ent->chr.body, ent->chr.head, ent->chr.name );
			ent->chr.inv = &ent->i;
			ent->body = gi.modelindex( Com_CharGetBody( &ent->chr ) );
			ent->head = gi.modelindex( Com_CharGetHead( &ent->chr ) );
			ent->skin = ent->chr.skin;
			ent->type = ET_ACTOR;
			ent->pnum = player->num;
			gi.linkentity( ent );
		}
	}

	// show visible actors
	G_ClearVisFlags( team );
	G_CheckVis( NULL, false );

	// give time
	G_GiveTimeUnits( team );
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
			else if ( sv_maxclients->value == 1 ) G_SpawnAIPlayer( p, ai_numaliens->value );
			else G_SpawnAIPlayer( p, ai_numactors->value );
			Com_Printf( _("Created AI player (team %i)\n"), team );
			return p;
		}

	// nothing free
	return NULL;
}
