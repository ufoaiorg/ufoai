/**
 * @file e_event_invdel.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "e_event_invdel.h"

/**
 * @sa CL_InvAdd
 */
void CL_InvDel (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t	*le;
	int		number;
	int		container, x, y;
	invList_t	*ic;

	NET_ReadFormat(msg, self->formatString, &number, &container, &x, &y);

	le = LE_Get(number);
	if (!le)
		Com_Error(ERR_DROP, "InvDel message ignored... LE not found\n");

	ic = INVSH_SearchInInventory(&le->i, &csi.ids[container], x, y);

	if (!ic) {
		Com_DPrintf(DEBUG_CLIENT, "CL_InvDel: No item found at location x/y=%i/%i.\n", x, y);
		return;
	}

	if (!cls.i.RemoveFromInventory(&cls.i, &le->i, &csi.ids[container], ic))
		Com_Error(ERR_DROP, "CL_InvDel: No item was removed from container %i", container);

	if (container == csi.idRight)
		le->right = NONE;
	else if (container == csi.idLeft)
		le->left = NONE;
	else if (container == csi.idExtension)
		le->extension = NONE;
	else if (container == csi.idHeadgear)
		le->headgear = NONE;

	switch (le->type) {
	case ET_ACTOR:
	case ET_ACTOR2x2:
		LE_SetThink(le, LET_StartIdle);
		break;
	case ET_ITEM:
		/* update the rendered item */
		LE_PlaceItem(le);
		break;
	}
}

