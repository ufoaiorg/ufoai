/**
 * @file cp_xvi.h
 * @brief Campaign XVI header
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

#ifndef CP_XVI_H
#define CP_XVI_H

void CP_XVIInit(void);
void CP_SpreadXVIAtPos(const vec2_t pos);
qboolean CP_IsXVIResearched(void);
void CP_SpreadXVI(void);
void CP_ReduceXVIEverywhere(void);
void CP_UpdateNationXVIInfection(void);
int CP_GetAverageXVIRate(void);
void CP_UpdateXVIMapButton(void);
void CP_StartXVISpreading_f(void);

#endif
