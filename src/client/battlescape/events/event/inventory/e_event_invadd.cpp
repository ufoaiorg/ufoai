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
#include "e_event_invadd.h"

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the Item should not be needed here - because
 * the amount is only valid for CID_FLOOR and CID_EQUIP
 */
static void CL_NetReceiveItem (dbuffer* buf, Item* item, containerIndex_t* container, int* x, int* y)
{
	const eventRegister_t* eventData = CL_GetEvent(EV_INV_TRANSFER);

	/* reset */
	int itemIdx, ammoIdx;
	int ammoleft = NONE_AMMO;
	int amount = 0;
	item->setDef(nullptr);
	item->setAmmoDef(nullptr);
	NET_ReadFormat(buf, eventData->formatString, &itemIdx, &ammoleft, &ammoIdx, container, x, y, &item->rotated, &amount);
	item->setAmmoLeft(ammoleft);
	item->setAmount(amount);
	item->setDef(INVSH_GetItemByIDX(itemIdx));
	item->setAmmoDef(INVSH_GetItemByIDX(ammoIdx));

	if (!item->def())
		Com_Error(ERR_DROP, "no weapon given for item");
}

/**
 * @brief Decides if following events should be delayed.
 */
int CL_InvAddTime (const struct eventRegister_s* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	if (eventTiming->parsedShot) {
		if (eventTiming->parsedDeath) { /* drop items after death (caused by impact) */
			/* EV_INV_ADD messages are the last events sent after a death */
			/** @todo but if the dying actor didn't have any item in its inventory this event won't be sent,
			 * the next unrelated EV_INV_ADD event might be badly scheduled */
			eventTiming->parsedDeath = false;
			return eventTiming->impactTime + 400;
		} else if (eventTiming->impactTime > cl.time) { /* item thrown on the ground */
			return eventTiming->impactTime + 75;
		}
	}

	return eventTiming->nextTime;
}

/**
 * @sa CL_InvDel
 * @sa G_SendInventory
 * @sa EV_INV_ADD
 */
void CL_InvAdd (const eventRegister_t* self, dbuffer* msg)
{
	const int number = NET_ReadShort(msg);
	le_t* le = LE_Get(number);
	int nr = NET_ReadShort(msg);

	if (!le)
		LE_NotFoundError(number);

	le->flags &= ~LE_REMOVE_NEXT_FRAME;

	for (; nr-- > 0;) {
		Item item;
		containerIndex_t container;
		int x, y;
		CL_NetReceiveItem(msg, &item, &container, &x, &y);

		if (LE_IsItem(le)) {
			if (container != CID_FLOOR)
				Com_Error(ERR_DROP, "InvAdd for ET_ITEM but target container is not the floor but %i", container);
		} else if (INVDEF(container)->temp) {
			Com_Error(ERR_DROP, "InvAdd for %i to temp container %i", le->type, container);
		}

		if (cls.i.addToInventory(&le->inv, &item, INVDEF(container), x, y, item.getAmount()) == nullptr)
			Com_Error(ERR_DROP, "InvAdd failed - could not add %i item(s) of %s to container %i",
					item.getAmount(), item.def()->id, container);

		if (container == CID_RIGHT)
			le->right = item.def()->idx;
		else if (container == CID_LEFT)
			le->left = item.def()->idx;
		else if (container == CID_HEADGEAR)
			le->headgear = item.def()->idx;
	}

	switch (le->type) {
	case ET_ACTOR:
	case ET_ACTOR2x2:
		if (LE_IsSelected(le))
			Cmd_ExecuteString("hud_updateactorload");
		LE_SetThink(le, LET_StartIdle);
		break;
	case ET_ITEM:
		LE_PlaceItem(le);
		break;
	default:
		break;
	}
}
