/**
 * @file cp_popup.h
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_POPUP_H
#define CLIENT_CL_POPUP_H

qboolean CL_DisplayHomebasePopup(aircraft_t *aircraft, qboolean alwaysDisplay);
void CL_PopupInit(void);
void CL_DisplayPopupAircraft(aircraft_t* aircraft);
void CL_DisplayPopupInterceptUFO(aircraft_t* ufo);
void CL_DisplayPopupInterceptMission(mission_t* mission);
void CP_PopupList(const char *title, const char *text);
void CP_Popup(const char *title, const char *text);

#endif
