/**
 * @file cp_produce.h
 * @brief Header for production related stuff.
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

#ifndef CLIENT_CP_PRODUCE
#define CLIENT_CP_PRODUCE

/** @brief Maximum number of productions queued in any one base. */
#define MAX_PRODUCTIONS		256
#define MAX_PRODUCTIONS_PER_WORKSHOP 5
/** Maximum number of produced items. */
#define MAX_PRODUCTION_AMOUNT 500

/** Size of a UGV in hangar capacity */
#define UGV_SIZE 300

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
	const objDef_t *item;			/**< Item to be produced. */
	const struct aircraft_s *aircraft;	/**< Aircraft (sample) to be produced. */
	struct storedUFO_s *ufo;

	signed int amount;	/**< How much are we producing. */
	float percentDone;		/**< Fraction of the item which is already produced.
							 * 0 if production is not started, 1 if production is over */
	qboolean spaceMessage;	/**< Used in No Free Space message adding. */
	qboolean creditMessage;	/**< Used in No Credits message adding. */
	/** @todo remove production flag */
	qboolean production;	/**< True if this is real production, false when disassembling. */
} production_t;

/**
 * @brief A production queue. Lists all items to be produced.
 * @sa production_t
 */
typedef struct production_queue_s
{
	int				numItems;		/**< The number of items in the queue. */
	struct production_s	items[MAX_PRODUCTIONS];	/**< Actual production items (in order). */
} production_queue_t;

#define PR_GetProductionForBase(base) (&((base)->productions))

void PR_ProductionInit(void);
void PR_ProductionRun(void);

qboolean PR_ItemIsProduceable(const objDef_t const *item);

struct base_s *PR_ProductionBase(const production_t *production);

int PR_IncreaseProduction(production_t *prod, int amount);
int PR_DecreaseProduction(production_t *prod, int amount);

void PR_UpdateProductionCap(struct base_s *base);

void PR_UpdateRequiredItemsInBasestorage(struct base_s *base, int amount, const requirements_t const *reqs);
int PR_RequirementsMet(int amount, const requirements_t const *reqs, struct base_s *base);

float PR_CalculateProductionPercentDone(const struct base_s *base, const technology_t *tech, const struct storedUFO_s *const storedUFO);

production_t *PR_QueueNew(struct base_s *base, const productionData_t *data, signed int amount);
void PR_QueueMove(production_queue_t *queue, int index, int dir);
void PR_QueueDelete(struct base_s *base, production_queue_t *queue, int index);
void PR_QueueNext(struct base_s *base);
int PR_QueueFreeSpace(const struct base_s* base);

#endif /* CLIENT_CP_PRODUCE */
