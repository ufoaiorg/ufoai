/**
 * @file g_ai.c
 * @brief Artificial Intelligence.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "g_local.h"

/* ==================================================================== */

typedef struct {
	pos3_t to, stop;
	byte mode, shots;
	edict_t *target;
} ai_action_t;

/*
=================
AI_CheckFF
=================
*/
static qboolean AI_CheckFF(edict_t * ent, vec3_t target, float spread)
{
	edict_t *check;
	vec3_t dtarget, dcheck, back;
	float cosSpread;
	int i;

	/* spread data */
	if (spread < 1.0)
		spread = 1.0;
	spread *= M_PI / 180;
	cosSpread = cos(spread);
	VectorSubtract(target, ent->origin, dtarget);
	VectorNormalize(dtarget);
	VectorScale(dtarget, PLAYER_WIDTH / spread, back);

	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
		if (check->inuse && check->type == ET_ACTOR && ent != check && check->team == ent->team && !(check->state & STATE_DEAD)) {
			/* found ally */
			VectorSubtract(check->origin, ent->origin, dcheck);
			if (DotProduct(dtarget, dcheck) > 0.0) {
				/* ally in front of player */
				VectorAdd(dcheck, back, dcheck);
				VectorNormalize(dcheck);
				if (DotProduct(dtarget, dcheck) > cosSpread)
					return qtrue;
			}
		}

	/* no ally in danger */
	return qfalse;
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

static float AI_FighterCalcGuete(edict_t * ent, pos3_t to, ai_action_t * aia)
{
	objDef_t *od;
	fireDef_t *fd;
	edict_t *check;
	int move, delta, tu;
	int i, fm, shots;
	float dist, minDist, nspread;
	float guete, dmg, maxDmg;

	/* set basic parameters */
	guete = 0.0;
	aia->target = NULL;
	VectorCopy(to, ent->pos);
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	gi.GridPosToVec(gi.map, to, ent->origin);

	move = gi.MoveLength(gi.map, to, qtrue);
	tu = ent->TU - move;
	if (ent->i.c[gi.csi->idRight] && ent->i.c[gi.csi->idRight]->item.m != NONE)
		od = &gi.csi->ods[ent->i.c[gi.csi->idRight]->item.m];
	else
		od = NULL;

	/* test for time */
	if (tu < 0)
		return 0.0;

	/* shooting */
	if (od) {
		maxDmg = 0.0;
		for (fm = 0; fm < 2; fm++) {
			fd = &od->fd[fm];
			if (!fd->time)
				continue;

			nspread = SPREAD_NORM((fd->spread[0] + fd->spread[1]) * GET_ACC(ent->chr.skills[ABILITY_ACCURACY], fd->weaponSkill) / 2);
			shots = tu / fd->time;
			if (shots) {
				/* search best target */
				for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
					if (check->inuse && check->type == ET_ACTOR && ent != check && (check->team != ent->team || ent->state & STATE_INSANE)
						&& !(check->state & STATE_DEAD)) {
						/* don't shoot civilians in mp */
						if (check->team == TEAM_CIVILIAN && (int) sv_maxclients->value > 1 && !(ent->state & STATE_INSANE))
							continue;

						/* check range */
						dist = VectorDist(ent->origin, check->origin);
						if (dist > fd->range)
							continue;

						/* check FF */
						if (AI_CheckFF(ent, check->origin, fd->spread[0]) && !(ent->state & STATE_INSANE))
							continue;

						/* calculate expected damage */
						dmg = G_ActorVis(ent->origin, check, qtrue);
						if (dmg == 0.0)
							continue;

						dmg *= fd->damage[0] * fd->shots * shots;
						if (nspread && dist > nspread)
							dmg *= nspread / dist;
						if (dmg > 100.0)
							dmg = 100.0;

						/* add kill bonus */
						if (dmg > check->HP)
							dmg += GUETE_KILL;

						/* civilian malus */
						if (check->team == TEAM_CIVILIAN && !(ent->state & STATE_INSANE))
							dmg *= GUETE_CIV_FACTOR;

						/* check if most damage can be done here */
						if (dmg > maxDmg) {
							maxDmg = dmg;
							aia->mode = fm;
							aia->shots = shots;
							aia->target = check;
						}
					}
			}
		}

		/* add damage to guete */
		if (aia->target) {
			guete += maxDmg;
			tu -= od->fd[aia->mode].time * aia->shots;
		}
	}

	/* close in */
	minDist = CLOSE_IN_DIST;
	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
		if (check->inuse && check->team != ent->team && !(check->state & STATE_DEAD)) {
			dist = VectorDist(ent->origin, check->origin);
			if (dist < minDist)
				minDist = dist;
		}
	guete += GUETE_CLOSE_IN * (1.0 - minDist / CLOSE_IN_DIST);

	/* add random effects */
	guete += GUETE_RANDOM * frand();

	if (ent->state & STATE_RAGE)
		return guete;

	/* hide */
	if (!(G_TestVis(-ent->team, ent, VT_PERISH | VT_NOFRUSTOM) & VIS_YES)) {
		/* is a hiding spot */
		guete += GUETE_HIDE;
	} else if (aia->target && tu >= 2) {
		/* search hiding spot after shooting */
		byte minX, maxX, minY, maxY;

		G_MoveCalc(0, to, HIDE_DIST);
		ent->pos[2] = to[2];
		minX = to[0] - HIDE_DIST > 0 ? to[0] - HIDE_DIST : 0;
		minY = to[1] - HIDE_DIST > 0 ? to[1] - HIDE_DIST : 0;
		maxX = to[0] + HIDE_DIST < 254 ? to[0] + HIDE_DIST : 254;
		maxY = to[1] + HIDE_DIST < 254 ? to[1] + HIDE_DIST : 254;

		for (ent->pos[1] = minY; ent->pos[1] <= maxY; ent->pos[1]++) {
			for (ent->pos[0] = minX; ent->pos[0] <= maxX; ent->pos[0]++) {
				/* time */
				delta = gi.MoveLength(gi.map, ent->pos, qfalse);
				if (delta > tu)
					continue;
				tu -= delta;

				/* visibility */
				gi.GridPosToVec(gi.map, ent->pos, ent->origin);
				if (G_TestVis(-ent->team, ent, VT_PERISH | VT_NOFRUSTOM) & VIS_YES)
					continue;

				/* found a hiding spot */
				VectorCopy(ent->pos, aia->stop);
				guete += GUETE_SHOOT_HIDE;
				break;
			}
			if (ent->pos[0] <= maxX)
				break;
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

static float AI_CivilianCalcGuete(edict_t * ent, pos3_t to, ai_action_t * aia)
{
	edict_t *check;
	int i, move, tu;
	float dist, minDist;
	float guete;

	/* set basic parameters */
	guete = 0.0;
	aia->target = NULL;
	VectorCopy(to, ent->pos);
	VectorCopy(to, aia->to);
	VectorCopy(to, aia->stop);
	gi.GridPosToVec(gi.map, to, ent->origin);

	move = gi.MoveLength(gi.map, to, qtrue);
	tu = ent->TU - move;

	/* test for time */
	if (tu < 0)
		return 0.0;

	/* run away */
	minDist = RUN_AWAY_DIST;
	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++)
		if (check->inuse && check->team != ent->team && !(check->state & STATE_DEAD)) {
			dist = VectorDist(ent->origin, check->origin);
			if (dist < minDist)
				minDist = dist;
		}
	guete += GUETE_RUN_AWAY * minDist / RUN_AWAY_DIST;

	/* add laziness */
	guete += GUETE_CIV_LAZINESS * tu / ent->TU;

	/* add random effects */
	guete += GUETE_CIV_RANDOM * frand();

	return guete;
}


/*
=================
AI_ActorThink
=================
*/
#define AI_MAX_DIST	30

void AI_ActorThink(player_t * player, edict_t * ent)
{
	ai_action_t aia, bestAia;
	pos3_t oldPos, to;
	vec3_t oldOrigin;
	int xl, yl, xh, yh;
	int i;
	float guete, best;

/*	Com_Printf( "AI_ActorThink (ent %i, frame %i)\n", ent->number, level.framenum ); */

	aia.mode = 0;
	aia.shots = 0;

	/* calculate move table */
	G_MoveCalc(0, ent->pos, MAX_ROUTE);
	gi.MoveStore(gi.map);

	/* set borders */
	xl = (int) ent->pos[0] - AI_MAX_DIST;
	if (xl < 0)
		xl = 0;
	yl = (int) ent->pos[1] - AI_MAX_DIST;
	if (yl < 0)
		yl = 0;
	xh = (int) ent->pos[0] + AI_MAX_DIST;
	if (xh > WIDTH)
		xl = WIDTH;
	yh = (int) ent->pos[1] + AI_MAX_DIST;
	if (yh > WIDTH)
		yh = WIDTH;

	/* search best action */
	best = 0.0;
	VectorCopy(ent->pos, oldPos);
	VectorCopy(ent->origin, oldOrigin);

	for (to[2] = 0; to[2] < HEIGHT; to[2]++)
		for (to[1] = yl; to[1] < yh; to[1]++)
			for (to[0] = xl; to[0] < xh; to[0]++)
				if (gi.MoveLength(gi.map, to, qtrue) < 0xFF) {
					if (ent->team == TEAM_CIVILIAN || ent->state & STATE_PANIC)
						guete = AI_CivilianCalcGuete(ent, to, &aia);
					else
						guete = AI_FighterCalcGuete(ent, to, &aia);

					if (guete > best) {
						bestAia = aia;
						best = guete;
					}
				}

	VectorCopy(oldPos, ent->pos);
	VectorCopy(oldOrigin, ent->origin);

	/* nothing found to do */
	if (best == 0.0)
		return;

	/* do the first move */
	G_ClientMove(player, 0, ent->number, bestAia.to, qfalse);

/*	Com_Printf( "(%i %i %i) (%i %i %i)\n", */
/*		(int)bestAia.to[0], (int)bestAia.to[1], (int)bestAia.to[2], */
/*		(int)bestAia.stop[0], (int)bestAia.stop[1], (int)bestAia.stop[2] ); */

	/* shoot('n'hide) */
	if (bestAia.target) {
		for (i = 0; i < bestAia.shots; i++)
			G_ClientShoot(player, ent->number, bestAia.target->pos, bestAia.mode);
		G_ClientMove(player, ent->team, ent->number, bestAia.stop, qfalse);
	}
}


/*
=================
AI_Run

Every server frame one single actor is handled - always in the same order
=================
*/
void AI_Run(void)
{
	player_t *player;
	edict_t *ent;
	int i, j;

	/* don't run this too often to prevent overflows */
	if (level.framenum % 10)
		return;

	for (i = 0, player = game.players + game.maxplayers; i < game.maxplayers; i++, player++)
		if (player->inuse && player->pers.ai && level.activeTeam == player->pers.team) {
			/* find next actor to handle */
			if (!player->pers.last)
				ent = g_edicts;
			else
				ent = player->pers.last + 1;

			for (j = ent - g_edicts; j < globals.num_edicts; j++, ent++)
				if (ent->inuse && ent->team == player->pers.team && ent->type == ET_ACTOR && !(ent->state & STATE_DEAD) && ent->TU) {
					AI_ActorThink(player, ent);
					player->pers.last = ent;
					return;
				}

			/* nothing left to do, request endround */
			if (j >= globals.num_edicts) {
				G_ClientEndRound(player);
				player->pers.last = g_edicts + globals.num_edicts;
			}
			return;
		}
}


/**
 * @brief Pack a weapon, possibly with some ammo
 * @param[in] ent The actor that will get the weapon
 * @param[in] weapon The weapon type index in gi.csi->ods
 * @param[in] equip The equipment that shows how many clips to pack
 *
 * TODO: choose between multiple ammo for the same weapon
 */
int G_PackAmmoAndWeapon(edict_t *ent, const int weapon, const byte * equip)
{
	int ammo;
	item_t item = {0,0,0};

	item.t = weapon;
	if ( gi.csi->ods[weapon].reload ) {
		for (ammo = 0; ammo < gi.csi->numODs; ammo++)
			if ( equip[ammo] && gi.csi->ods[ammo].link == weapon )
				break;
		if (ammo < gi.csi->numODs) {
			int num;

			/* load ammo */
			item.a = gi.csi->ods[weapon].ammo;
			item.m = ammo;
			num = 
				equip[ammo] / equip[weapon] 
				+ (equip[ammo] % equip[weapon] 
				   > frand() * equip[weapon]);
			num = num > 3 ? 3 : num;
			while (--num) {
				item_t mun = {0,0,0};
				
				mun.t = ammo;
				/* ammo to backpack; belt reseved for knives and grenades */
				Com_TryAddToInventory(&ent->i, mun, gi.csi->idBackpack);
				/* no problem if no space left --- one ammo already loaded */
			}
		} else
			Com_Printf("G_PackAmmoAndWeapon: no ammo for sidearm or primary weapon '%s' in equipment '%s'.\n", gi.csi->ods[weapon].kurz, gi.cvar_string("ai_equipment"));
	}
	/* now try to pack the weapon */
	return
		Com_TryAddToInventory(&ent->i, item, gi.csi->idRight)
		|| Com_TryAddToInventory(&ent->i, item, gi.csi->idLeft)
		|| Com_TryAddToInventory(&ent->i, item, gi.csi->idBelt)
		|| Com_TryAddToInventory(&ent->i, item, gi.csi->idHolster)
		|| Com_TryAddToInventory(&ent->i, item, gi.csi->idBackpack);
}


/**
 * @brief Fully equip one AI player
 * @param[in] ent The actor that will get the weapons
 * @param[in] equip The equipment that shows what is available
 * @note The code below is a complete implementation 
 * of the scheme sketched at the beginning of equipment_missions.ufo.
 * However, aliens cannot yet swap weapons,
 * so only the weapon(s) in hands will be used.
 * The rest will be just player's loot.
 * If two weapons in the same category have the same price,
 * only one will be considered for inventory. 
 *
 * TODO: try and see if this creates a tolerable
 * initial equipment for human players
 * (of course this would result in random number of initial weapons),
 * though there is already CL_CheckInventory in cl_team.c.
 */

#define AKIMBO_CHANCE		0.2
#define HAS_WEAPON_BONUS	1.0
#define HAS_WEAPON_MALUS	-0.5

void G_EquipAIPlayer(edict_t *ent, const byte * equip)
{
	int i, weapon, max_price, prev_price;
	int has_weapon = 0, primary_tachyon = 0;
	objDef_t obj;

	/* primary weapons */
	max_price = INT_MAX;
	do {
		/* search for the most expensive primary weapon in the equipment */
		prev_price = max_price;
		max_price = 0;
		for (i = 0; i < gi.csi->numODs; i++) {
			obj = gi.csi->ods[i];
			if ( equip[i] && obj.weapon && obj.buytype == 0 ) {
				if ( obj.price > max_price && obj.price < prev_price ) {
					max_price = obj.price;
					weapon = i;
				}
			}
		}
		/* see if there is any */
		if (max_price) {
			/* see if the alien picks it */
			if ( equip[weapon] >= 8 * frand() ) {
				/* not decrementing equip[weapon]
				 * so that we get more possible squads */
				has_weapon += G_PackAmmoAndWeapon(ent, weapon, equip);
				if (has_weapon) {
					primary_tachyon = 
						(gi.csi->ods[weapon].fd[0].dmgtype 
						 == gi.csi->damTachyon);
					max_price = 0; /* one primary weapon is enough */
				}
			}
		}
	} while (max_price);

	/* sidearms (secondary weapons with reload) */
	max_price = primary_tachyon ? 0 : INT_MAX;
	do {
		prev_price = max_price;
		/* if primary is a tachyon weapon, we pick cheapest sidearms first */
		max_price = primary_tachyon ? INT_MAX : 0;
		for (i = 0; i < gi.csi->numODs; i++) {
			obj = gi.csi->ods[i];
			if ( equip[i] && obj.weapon && obj.buytype == 1 && obj.reload ) {
				if ( primary_tachyon
					 ? obj.price < max_price && obj.price > prev_price
					 : obj.price > max_price && obj.price < prev_price ) {
					max_price = obj.price;
					weapon = i;
				}
			}
		}
		if ( !(max_price == primary_tachyon ? INT_MAX : 0) ) {
			if (has_weapon) { 
				/* already got primary weapon */
				if ( HAS_WEAPON_MALUS + equip[weapon] >= 8 * frand() ) {
					if ( G_PackAmmoAndWeapon(ent, weapon, equip) ) {
						max_price = 0; /* then one sidearm is enough */
					}
				}
			} else {
				/* no primary weapon */
				if ( HAS_WEAPON_BONUS + equip[weapon] >= 8 * frand() ) {
					has_weapon += G_PackAmmoAndWeapon(ent, weapon, equip);
					if (has_weapon) {
						/* try to get the second akimbo pistol */
						if ( !gi.csi->ods[weapon].twohanded
							 && frand() < AKIMBO_CHANCE ) {
							G_PackAmmoAndWeapon(ent, weapon, equip);
						}
						max_price = 0; /* enough sidearms */
					}
				}
			}
		}
	} while ( !(max_price == primary_tachyon ? INT_MAX : 0) );

	/* misc items and secondary weapons without reload */
	max_price = INT_MAX;
	do {
		prev_price = max_price;
		max_price = 0;
		for (i = 0; i < gi.csi->numODs; i++) {
			obj = gi.csi->ods[i];
			if ( equip[i] 
				 && ((obj.weapon && obj.buytype == 1 && !obj.reload)
					 || obj.buytype == 2) ) {
				if ( obj.price > max_price && obj.price < prev_price ) {
					max_price = obj.price;
					weapon = i;
				}
			}
		}
		if (max_price) {
			int num;

			/* still no weapon even at this point? */
			num = 
				equip[weapon] / 8 
				+ ((has_weapon ? HAS_WEAPON_MALUS : 2 * HAS_WEAPON_BONUS
					+ equip[weapon] % 8)
				   > frand() * equip[weapon]);
			while (num--)
				has_weapon += G_PackAmmoAndWeapon(ent, weapon, equip);
		}
	} while (max_price);
}


/*
=================
G_SpawnAIPlayer
=================
*/
#define MAX_SPAWNPOINTS		64
static int spawnPoints[MAX_SPAWNPOINTS];

/**
 * @brief Spawn civilians and aliens
 * @param
 * @sa
 */
static void G_SpawnAIPlayer(player_t * player, int numSpawn)
{
	edict_t *ent;
	byte equip[MAX_OBJDEFS];
	int i, j, numPoints, team;
	int ammo, num;
	item_t item = {0,0,0};
	char *ref;

	/* search spawn points */
	team = player->pers.team;
	numPoints = 0;
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && ent->type == ET_ACTORSPAWN && ent->team == team)
			spawnPoints[numPoints++] = i;

	/* check spawn point number */
	if (numPoints < numSpawn) {
		Com_Printf("Not enough spawn points for team %i\n", team);
		numSpawn = numPoints;
	}

	/* prepare equipment */
	if (team != TEAM_CIVILIAN) {
		equipDef_t *ed;
		char name[MAX_VAR];

		Q_strncpyz(name, gi.cvar_string("ai_equipment"), MAX_VAR);
		for (i = 0, ed = gi.csi->eds; i < gi.csi->numEDs; i++, ed++)
			if (!Q_strncmp(name, ed->name, MAX_VAR))
				break;
		if (i == gi.csi->numEDs)
			ed = &gi.csi->eds[0];
		else
			ed = &gi.csi->eds[i];

		for (i = 0; i < gi.csi->numODs; i++)
			equip[i] = ed->num[i];
	}

	/* spawn players */
	for (j = 0; j < numSpawn; j++) {
		/* select spawnpoint */
		while (ent->type != ET_ACTORSPAWN)
			ent = &g_edicts[spawnPoints[(int) (frand() * numPoints)]];

		/* spawn */
		level.num_spawned[team]++;
		level.num_alive[team]++;
		/* FIXME: Aliens with more than one unit */
		ent->chr.fieldSize = ACTOR_SIZE_NORMAL;
		if (team != TEAM_CIVILIAN) {
			ent->chr.skin = gi.GetModelAndName(gi.cvar_string("ai_alien"), ent->chr.path, ent->chr.body, ent->chr.head, ent->chr.name);

			/* search the armor definition */
			ref = gi.cvar_string("ai_armor");
			for (item.t = 0; item.t < gi.csi->numODs; item.t++)
				if (!Q_strncmp(ref, gi.csi->ods[item.t].kurz, MAX_VAR))
					break;

			/* found */
			if (item.t < gi.csi->numODs && item.t != NONE) {
				if (!Q_strncmp(gi.csi->ods[item.t].type, "armor", MAX_VAR)) {
					item.a = 1; /* FIXME */
					Com_AddToInventory(&ent->i, item, gi.csi->idArmor, 0, 0);
				} else
					Com_Printf("No valid alien armor '%s'\n", ref);
			} else if (*ref)
				Com_Printf("Could not find alien armor '%s'\n", ref);

			/* FIXME: chr.name should be Alien: Ortnok e.g. */
			Q_strncpyz(ent->chr.name, _("Alien"), MAX_VAR);
			ent->type = ET_ACTOR;
			ent->pnum = player->num;
			gi.linkentity(ent);

			/* skills */
			Com_CharGenAbilitySkills(&ent->chr, 0, 100, 0, 100);
			ent->chr.skills[ABILITY_MIND] += 100;
			if (ent->chr.skills[ABILITY_MIND] >= MAX_SKILL)
				ent->chr.skills[ABILITY_MIND] = MAX_SKILL;

			ent->HP = GET_HP(ent->chr.skills[ABILITY_POWER]);
			ent->AP = 100;
			ent->STUN = 100;
			ent->morale = GET_MORALE(ent->chr.skills[ABILITY_MIND]);
			if (ent->morale >= MAX_SKILL)
				ent->morale = MAX_SKILL;

			/* search for weapons */
			num = 0;
			for (i = 0; i < gi.csi->numODs; i++)
				if (equip[i] && gi.csi->ods[i].weapon)
					num++;

			if (num) {
				/* add weapon */
				item.m = NONE;

				num = (int) (frand() * num);
				for (i = 0; i < gi.csi->numODs; i++)
					if (equip[i] && gi.csi->ods[i].weapon) {
						if (num)
							num--;
						else
							break;
					}
				item.t = i;
				equip[i]--;

				if (gi.csi->ods[i].reload) {
					for (ammo = 0; ammo < gi.csi->numODs; ammo++)
						if (equip[ammo] && gi.csi->ods[ammo].link == i)
							break;
					if (ammo < gi.csi->numODs) {
						item.a = gi.csi->ods[i].ammo;
						item.m = ammo;
						equip[ammo]--;
						if (equip[ammo] > equip[i]) {
							item_t mun = {0,0,0};

							mun.t = ammo;
							Com_AddToInventory(&ent->i, mun, gi.csi->idBelt, 0, 0);
							equip[ammo]--;
						}
					} else
						item.a = 0;
					Com_AddToInventory(&ent->i, item, gi.csi->idRight, 0, 0);
				} else {
					item.a = gi.csi->ods[i].ammo;
					item.m = i;
				}
				/* FIXME: Com_AddToInventory here ?? */

				/* set model */
				ent->chr.inv = &ent->i;

/*				ent->chr.inv->c[gi.csi->idArmor]*/
				ent->body = gi.modelindex(Com_CharGetBody(&ent->chr));
				ent->head = gi.modelindex(Com_CharGetHead(&ent->chr));
				ent->skin = ent->chr.skin;
			} else {
				/* nothing left */
				Com_Printf("Not enough weapons in equipment '%s'\n", gi.cvar_string("ai_equipment"));
			}
		} else {
			Com_CharGenAbilitySkills(&ent->chr, 0, 20, 0, 20);
			ent->HP = GET_HP(ent->chr.skills[ABILITY_POWER]) / 2;
			ent->AP = 100;
			ent->STUN = 100;
			ent->morale = GET_MORALE(ent->chr.skills[ABILITY_MIND]);

			ent->chr.skin = gi.GetModelAndName(gi.cvar_string("ai_civilian"), ent->chr.path, ent->chr.body, ent->chr.head, ent->chr.name);
			ent->chr.inv = &ent->i;
			/* FIXME: Maybe we have civilians with armor, too - police and so on */
			ent->body = gi.modelindex(Com_CharGetBody(&ent->chr));
			ent->head = gi.modelindex(Com_CharGetHead(&ent->chr));
			ent->skin = ent->chr.skin;
			ent->type = ET_ACTOR;
			ent->pnum = player->num;
			gi.linkentity(ent);
		}
	}

	/* show visible actors */
	G_ClearVisFlags(team);
	G_CheckVis(NULL, qfalse);

	/* give time */
	G_GiveTimeUnits(team);
}


/*
=================
AI_CreatePlayer
=================
*/
player_t *AI_CreatePlayer(int team)
{
	player_t *p;
	int i;

	if (!sv_ai->value)
		return NULL;

	for (i = 0, p = game.players + game.maxplayers; i < game.maxplayers; i++, p++)
		if (!p->inuse) {
			memset(p, 0, sizeof(player_t));
			p->inuse = qtrue;
			p->num = p - game.players;
			p->ping = 0;
			p->pers.team = team;
			p->pers.ai = qtrue;
			if (team == TEAM_CIVILIAN)
				G_SpawnAIPlayer(p, ai_numcivilians->value);
			else if (sv_maxclients->value == 1)
				G_SpawnAIPlayer(p, ai_numaliens->value);
			else
				G_SpawnAIPlayer(p, ai_numactors->value);
			Com_Printf("Created AI player (team %i)\n", team);
			return p;
		}

	/* nothing free */
	return NULL;
}
