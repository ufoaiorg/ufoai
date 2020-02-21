/**
 * @file
 * @brief Item cargo class header
 */

/*
Copyright (C) 2002-2020 UFO: Alien Invasion.

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

#pragma once

#include "cp_cgame_callbacks.h"

/**
 * @brief item cargo entry
 */
typedef struct itemCargo_s {
	const objDef_t* objDef;		/**< Pointer to object definition structure */
	int amount;			/**< Number of items of this type in this cargo */
	int looseAmount;		/**< Number of loose/partial items - like bullets for clip */
} itemCargo_t;

/**
 * @brief Item cargo class
 */
class ItemCargo {
	protected:
		linkedList_t* cargo;
	public:
		virtual bool add (const objDef_t* od, int amount, int looseAmount);
		virtual bool add (const char* objDefId, int amount, int looseAmount);

		void empty (void);
		bool isEmpty (void) const;

		itemCargo_t* get (const objDef_t* od) const;
		int getAmount (const objDef_t* od) const;
		int getLooseAmount (const objDef_t* od) const;
		linkedList_t* list (void) const;

		int count (void) const;
		int size (void) const;

		bool load(xmlNode_t* root);
		bool save(xmlNode_t* root) const;

		ItemCargo (void);
		ItemCargo (ItemCargo& itemCargo);
		virtual ~ItemCargo (void);
};
