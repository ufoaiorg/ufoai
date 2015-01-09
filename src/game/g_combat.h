/**
 * @file
 * @brief All parts of the main game logic that are combat related
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "g_local.h"

/** @brief used in shot probability calculations (pseudo shots) */
typedef struct shot_mock_s {
	int enemyCount;			/**< shot would hit that much enemies */
	int friendCount;		/**< shot would hit that much friends */
	int civilian;			/**< shot would hit that much civilians */
	int self;				/**< @todo incorrect actor facing or shotOrg, or bug in trace code? */
	int damage;
	bool allow_self;

	inline shot_mock_s () {
		OBJZERO(*this);
	}
} shot_mock_t;

int G_ApplyProtection(const Edict* target, const byte dmgWeight, int damage);
void G_CheckDeathOrKnockout(Actor* target, Actor* attacker, const fireDef_t* fd, int damage);
bool G_ClientShoot(const Player& player, Actor* actor, const pos3_t at, shoot_types_t shootType, fireDefIndex_t firemode, shot_mock_t* mock, bool allowReaction, int z_align);
void G_CalcEffectiveSpread(const Actor* ent, const fireDef_t* fd, vec2_t effSpread);
