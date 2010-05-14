/**
 * @file cl_ugv.h
 * @brief UGV related headers
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CL_UGV_H
#define CL_UGV_H

/** @brief Defines a type of UGV/Robot */
typedef struct ugv_s {
	char *id;
	char weapon[MAX_VAR];
	char armour[MAX_VAR];
	int tu;
	char actors[MAX_VAR];
	int price;
} ugv_t;

void CL_ParseUGVs(const char *name, const char **text);
ugv_t *CL_GetUGVByIDSilent(const char *ugvID);
ugv_t *CL_GetUGVByID(const char *ugvID);

extern ugv_t ugvs[MAX_UGV];
extern int numUGV;

#endif
