/**
 * @file cl_uforecovery.h
 * @brief UFO recovery
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_UFORECOVERY_H
#define CLIENT_CL_UFORECOVERY_H

void UFO_UpdateUFOHangarCapForAll(base_t *base);
void UFO_PrepareRecovery(base_t *base);
void UFO_Recovery(void);
qboolean UFO_ConditionsForStoring(const base_t *base, const aircraft_t *ufocraft);
void UR_InitStartup(void);
void CP_UFOSendMail(const aircraft_t *ufocraft, const base_t *base);

#endif
