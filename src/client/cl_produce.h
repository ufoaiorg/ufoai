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

/** @brief maximum number of productions queued in any one base */
#define MAX_PRODUCTIONS		256

/** @note:
 * We can get the tech pointer from csi.ods
 * the tech struct holds the time that is needed to produce
 * the selected equipment
 */
typedef struct production_s
{
	signed int objID; /* object id from global csi.ods struct */
	signed int amount; /* how much are we producing */
	int timeLeft; /* get this from tech */
} production_t;

/** @brief
 * a production queue
 */
typedef struct production_queue_s
{
	int 			numItems;					/* the number of items in the queue */
	production_t	items[MAX_PRODUCTIONS];		/* actual production items (in order) */
} production_queue_t;

void PR_ResetProduction(void);
void PR_ProductionRun(void);
void PR_ProductionInit(void);

#endif /* CLIENT_CL_PROOUCE */
