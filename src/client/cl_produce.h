/**
 * @file cl_produce.h
 * @brief Header for production related stuff.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_PROOUCE
#define CLIENT_CL_PROOUCE

typedef struct production_s
{
	int odjID; /* object id from global csi.ods struct */
	int amount; /* how much are we producing */
} production_t;

void PR_ResetProduction(void);

#endif /* CLIENT_CL_PROOUCE */
