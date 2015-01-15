/**
 * @file
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "../../../cl_actor.h"
#include "../../../../cgame/cl_game.h"
#include "e_event_invreload.h"

int CL_InvReloadTime (const eventRegister_t* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	const int eventTime = eventTiming->nextTime;
	eventTiming->nextTime += 600;
	return eventTime;
}

void CL_InvReload (const eventRegister_t* self, dbuffer* msg)
{
	int		number;
	int		ammo, type, x, y;
	containerIndex_t container;

	NET_ReadFormat(msg, self->formatString, &number, &ammo, &type, &container, &x, &y);

	le_t* le = LE_Get(number);
	if (!le)
		return;

	if (le->team != cls.team)
		return;

	assert(container >= 0);
	assert(container < MAX_INVDEFS);
	Item* ic = le->inv.getItemAtPos(INVDEF(container), x, y);
	if (!ic)
		return;

	S_LoadAndPlaySample(ic->def()->reloadSound, le->origin, ic->def()->reloadAttenuation, SND_VOLUME_WEAPONS);

	/* if the displaced clip had any remaining bullets
	 * store them as loose, unless the removed clip was full */
	equipDef_t* ed = GAME_GetEquipmentDefinition();
	if (ed && ic->getAmmoLeft() > 0 && ic->getAmmoLeft() != ic->def()->ammo) {
		assert(ammo == ic->def()->ammo);
		/* Accumulate loose ammo into clips (only accessible post-mission) */
		ed->addClip(ic);
	}

	/* set new ammo */
	ic->setAmmoLeft(ammo);
	ic->setAmmoDef(INVSH_GetItemByIDX(type));

	if (le == selActor)
		Cmd_ExecuteString("hud_updateactorload");
}
