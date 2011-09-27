/**
 * @file cl_hud_callbacks.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CL_HUD_CALLBACKS_H_
#define CL_HUD_CALLBACKS_H_

void HUD_InitCallbacks(void);

/** @todo remove me */
invList_t* HUD_GetLeftHandWeapon(const le_t *actor, containerIndex_t *container);
const fireDef_t *HUD_GetFireDefinitionForHand(const le_t * actor, const actorHands_t hand);
int HUD_CalcReloadTime(const le_t *le, const objDef_t *weapon, int toContainer);

#endif /* CL_HUD_CALLBACKS_H_ */
