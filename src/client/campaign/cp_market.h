/**
 * @file cp_market.h
 * @brief Header for single player market stuff.
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_MARKET_H
#define CLIENT_CL_MARKET_H

/** market structure */
typedef struct market_s {
	int num[MAX_OBJDEFS];					/**< number of items on the market */
	int bid[MAX_OBJDEFS];					/**< price of item for selling */
	int ask[MAX_OBJDEFS];					/**< price of item for buying */
	double currentEvolution[MAX_OBJDEFS];	/**< evolution of the market */
} market_t;

int BS_GetStorageSupply(const base_t *base, const char *aircraftID);
qboolean BS_CheckAndDoBuyItem(base_t* base, const objDef_t *item, int number);
void BS_ProcessCraftItemSale(const base_t *base, const objDef_t *craftitem, const int numItems);

void BS_InitMarket(qboolean load);
void CL_CampaignRunMarket(void);

#endif /* CLIENT_CL_MARKET_H */
