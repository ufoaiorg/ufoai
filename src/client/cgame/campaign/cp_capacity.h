/**
 * @file cp_capacity.h
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

#ifndef CP_CAPACITY_H
#define CP_CAPACITY_H

/** @brief All possible capacities in base. */
typedef enum {
	CAP_ALIENS,		/**< Live aliens stored in base. */
	CAP_AIRCRAFT_SMALL,	/**< Small aircraft in base. */
	CAP_AIRCRAFT_BIG,	/**< Big aircraft in base. */
	CAP_EMPLOYEES,		/**< Personel in base. */
	CAP_ITEMS,		/**< Items in base. */
	CAP_LABSPACE,		/**< Space for scientists in laboratory. */
	CAP_WORKSPACE,		/**< Space for workers in workshop. */
	CAP_ANTIMATTER,		/**< Space for Antimatter Storage. */

	MAX_CAP
} baseCapacities_t;

/** @brief Store capacities in base. */
typedef struct capacities_s {
	int max;		/**< Maximum capacity. */
	int cur;		/**< Currently used capacity. */
} capacities_t;

void CAP_UpdateStorageCap(struct base_s *base);
/**
 * @brief Capacity macros
 */
#define CAP_Get(base, capacity) &((base)->capacities[(capacity)])
#define CAP_GetMax(base, capacity) (base)->capacities[(capacity)].max
#define CAP_GetCurrent(base, capacity) (base)->capacities[(capacity)].cur
void CAP_SetMax(struct base_s* base, baseCapacities_t capacity, int value);
void CAP_AddMax(struct base_s* base, baseCapacities_t capacity, int value);
void CAP_SetCurrent(struct base_s* base, baseCapacities_t capacity, int value);
void CAP_AddCurrent(struct base_s* base, baseCapacities_t capacity, int value);

void CAP_RemoveAircraftExceedingCapacity(struct base_s* base, baseCapacities_t capacity);
void CAP_RemoveItemsExceedingCapacity(struct base_s *base);
void CAP_RemoveAntimatterExceedingCapacity(struct base_s *base);

int CAP_GetFreeCapacity(const struct base_s *base, baseCapacities_t cap);
void CAP_CheckOverflow(void);

#endif
