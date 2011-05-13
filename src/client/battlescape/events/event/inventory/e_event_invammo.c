/**
 * @file e_event_invammo.c
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
#include "e_event_invammo.h"

void CL_InvAmmo (const eventRegister_t *self, struct dbuffer *msg)
{
	invList_t	*ic;
	le_t	*le;
	int		number;
	int		ammo, type, x, y;
	containerIndex_t container;

	NET_ReadFormat(msg, self->formatString, &number, &ammo, &type, &container, &x, &y);

	le = LE_Get(number);
	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "InvAmmo message ignored... LE not found\n");
		return;
	}

	if (le->team != cls.team)
		return;

	assert(container >= 0);
	assert(container < MAX_INVDEFS);
	ic = INVSH_SearchInInventory(&le->i, INVDEF(container), x, y);
	if (!ic)
		return;

	/* set new ammo */
	ic->item.a = ammo;
	ic->item.m = INVSH_GetItemByIDX(type);
}
