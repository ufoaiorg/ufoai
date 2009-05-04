/**
 * @file cp_produce.h
 * @brief Header for production related stuff.
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

#ifndef CLIENT_CL_PROOUCE
#define CLIENT_CL_PROOUCE

#include "../cl_inventory.h"

/** @brief Maximum number of productions queued in any one base. */
#define MAX_PRODUCTIONS		256
#define MAX_PRODUCTIONS_PER_WORKSHOP 5

extern const int PRODUCE_FACTOR;
extern const int PRODUCE_DIVISOR;

/**
 * @brief Holds all information for the production of one item-type.
 * @note
 * We can get the tech pointer from csi.ods.
 * The tech struct holds the time that is needed to produce
 * the selected equipment.
 */
typedef struct production_s
{
	int idx; /**< Self reference in the production list. Mainly used for moving/deleting them. */
	/**
	 * @note Only one pointer is supposed to be set at any time.
	 * @todo Make this a union
	 */
	objDef_t *item;			/**< Item to be produced. */
	struct aircraft_s *aircraft;	/**< Aircraft (sample) to be produced. @todo Is there any way to make this const without cl_produce.c to break?*/

	signed int amount;	/**< How much are we producing. */
	float percentDone;		/**< Fraction of the item which is already produced.
							 * 0 if production is not started, 1 if production is over */
	qboolean spacemessage;	/**< Used in No Free Space message adding. */
	qboolean creditmessage;	/**< Used in No Credits message adding. */
	qboolean production;	/**< True if this is real production, false when disassembling. */
	qboolean items_cached;	/**< If true the items required for production (of _one_ objID item) have been removed from production.
				 * They need to be added to the storage again if this queue is stopped or removed.
				 * The item-numbers from the requirement need to be multipled with 'amount' in order to get the overall number of cached items. */
} production_t;

/**
 * @brief A production queue. Lists all items to be produced.
 * @sa production_t
 */
typedef struct production_queue_s
{
	int				numItems;		/**< The number of items in the queue. */
	production_t	items[MAX_PRODUCTIONS];	/**< Actual production items (in order). */
} production_queue_t;

void PR_ProductionRun(void);
void PR_ProductionInit(void);
void PR_UpdateProductionCap(struct base_s *base);
qboolean PR_ItemIsProduceable(const objDef_t const *item);
void PR_QueueMove (production_queue_t *queue, int index, int dir);
void PR_QueueDelete (base_t *base, production_queue_t *queue, int index);
void PR_UpdateRequiredItemsInBasestorage (base_t *base, int amount, requirements_t *reqs);
float PR_CalculateProductionPercentDone (const base_t *base, const technology_t *tech, const components_t *comp, qboolean disassembly);

#endif /* CLIENT_CL_PROOUCE */
