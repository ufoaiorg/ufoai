/**
 * @file cp_basedefence_callbacks.h
 * @brief Header file for menu callback functions used for basedefence menu
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#ifndef CLIENT_CAMPAIGN_CL_BASEDEFENCE_CALLBACKS_H
#define CLIENT_CAMPAIGN_CL_BASEDEFENCE_CALLBACKS_H

aircraftSlot_t *BDEF_SelectBaseSlot(base_t *base, const int airequipID);
aircraftSlot_t *BDEF_SelectInstallationSlot(installation_t *installation, const int airequipID);

void BDEF_InitCallbacks(void);
void BDEF_ShutdownCallbacks(void);

#endif /* CLIENT_CAMPAIGN_CL_FIGHTEQUIP_CALLBACKS_H */
