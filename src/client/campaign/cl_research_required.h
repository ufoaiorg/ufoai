/**
 * @file cl_research_required.h
 * @brief Header for functions used by callback and research, but nowhere else
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


#ifndef CLIENT_CAMPAIGN_CL_RESEARCH_REQUIRED_H
#define CLIENT_CAMPAIGN_CL_RESEARCH_REQUIRED_H

qboolean RS_RequirementsMet (const requirements_t *required_AND, const requirements_t *required_OR, const base_t* base);

#endif /* CLIENT_CAMPAIGN_CL_RESEARCH_REQUIRED_H */
