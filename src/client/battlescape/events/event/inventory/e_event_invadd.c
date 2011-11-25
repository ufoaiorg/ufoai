/**
 * @file e_event_invadd.c
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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "e_event_invadd.h"

/**
 * @sa G_WriteItem
 * @sa G_ReadItem
 * @note The amount of the item_t struct should not be needed here - because
 * the amount is only valid for idFloor and idEquip
 */
static void CL_NetReceiveItem (struct dbuffer *buf, item_t *item, containerIndex_t *container, int *x, int *y)
{
	const eventRegister_t *eventData = CL_GetEvent(EV_INV_TRANSFER);

	/* reset */
	int t, m;
	item->t = item->m = NULL;
	item->a = NONE_AMMO;
	NET_ReadFormat(buf, eventData->formatString, &t, &item->a, &m, container, x, y, &item->rotated, &item->amount);

	item->t = INVSH_GetItemByIDX(t);
	item->m = INVSH_GetItemByIDX(m);

	if (!item->t)
		Com_Error(ERR_DROP, "no weapon given for item");
}

/**
 * @brief Decides if following events should be delayed.
 */
int CL_InvAddTime (const struct eventRegister_s *self, struct dbuffer *msg, eventTiming_t *eventTiming)
{
	if (eventTiming->parsedDeath) { /* drop items after death (caused by impact) */
		/* EV_INV_ADD messages are the last events sent after a death */
		eventTiming->parsedDeath = qfalse;
		return eventTiming->impactTime + 400;
	} else if (eventTiming->impactTime > cl.time) { /* item thrown on the ground */
		return eventTiming->impactTime + 75;
	}

	return eventTiming->nextTime;
}

/**
 * @sa CL_InvDel
 * @sa G_SendInventory
 * @sa EV_INV_ADD
 */
void CL_InvAdd (const eventRegister_t *self, struct dbuffer *msg)
{
	const int number = NET_ReadShort(msg);
	le_t *le = LE_Get(number);
	int nr = NET_ReadShort(msg);

	if (!le)
		LE_NotFoundError(number);

	le->removeNextFrame = qfalse;

	for (; nr-- > 0;) {
		item_t item;
		containerIndex_t container;
		int x, y;
		CL_NetReceiveItem(msg, &item, &container, &x, &y);

		if (LE_IsItem(le)) {
			if (container != csi.idFloor)
				Com_Error(ERR_DROP, "InvAdd for ET_ITEM but target container is not the floor but %i", container);
		} else if (INVDEF(container)->temp) {
			Com_Error(ERR_DROP, "InvAdd for %i to temp container %i", le->type, container);
		}

		if (cls.i.AddToInventory(&cls.i, &le->i, &item, INVDEF(container), x, y, item.amount) == NULL)
			Com_Error(ERR_DROP, "InvAdd failed - could not add %i item(s) of %s to container %i",
					item.amount, item.t->id, container);

		if (container == csi.idRight)
			le->right = item.t->idx;
		else if (container == csi.idLeft)
			le->left = item.t->idx;
		else if (container == csi.idExtension)
			le->extension = item.t->idx;
		else if (container == csi.idHeadgear)
			le->headgear = item.t->idx;
	}

	switch (le->type) {
	case ET_ACTOR:
	case ET_ACTOR2x2:
		LE_SetThink(le, LET_StartIdle);
		break;
	case ET_ITEM:
		LE_PlaceItem(le);
		break;
	default:
		break;
	}
}
