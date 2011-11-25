/**
 * @file cp_alien_interest.h
 * @brief Alien interest header
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

#ifndef CP_ALIEN_INTEREST_H
#define CP_ALIEN_INTEREST_H

void INT_ResetAlienInterest(void);
void INT_IncreaseAlienInterest(const campaign_t *campaign);
void INT_ChangeIndividualInterest(float percentage, interestCategory_t category);

#ifdef DEBUG
const char* INT_InterestCategoryToName(interestCategory_t category);
#endif

void INT_InitStartup(void);
void INT_Shutdown(void);
#endif
