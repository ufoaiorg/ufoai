/**
 * @file cl_hud.c
 * @brief HUD related routines.
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

#ifndef CL_HUD_H
#define CL_HUD_H

extern int hitProbability;

/* these should probably not be called from other places than menu */
qboolean HUD_DisplayImpossibleReaction(const le_t * actor);
void HUD_DisplayPossibleReaction(const le_t * actor);
void HUD_HideFiremodes(void);
/* check above for removal */

void HUD_InitStartup(void);
void HUD_DisplayMessage(const char * text);
void HUD_ActorUpdateCvars(void);

#endif
