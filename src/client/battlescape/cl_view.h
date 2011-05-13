/**
 * @file cl_view.h
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

#ifndef CLIENT_CL_VIEW_H
#define CLIENT_CL_VIEW_H

void CL_ViewRender(void);
void CL_ViewUpdateRenderData(void);
void CL_ViewCenterAtGridPosition(const pos3_t pos);
void CL_ViewCalcFieldOfViewX(void);
void CL_ViewLoadMedia(void);
void CL_ViewInit(void);
void CL_ViewPrecacheModels(void);

extern cvar_t *cl_isometric;

/* Map debugging constants */
/** @brief cvar debug_map options:
 * debug_map is a bit mask, like the developer cvar.  There is no ALL bit.
 * 1 (1<<0) = Turn on pathfinding floor tiles.  Red can not be entered, green can be
 *     reached with current TUs, yellow can be reached but actor does not have needed
 *     TUs, and black should never be able to be reached because there is not floor to
 *     support an actor there (not tuned for flyers yet)
 * 2  (1<<1) = Turn on cursor debug text display.  The information at the top is the
 *     ceiling value where the cursor is and the (x, y, z) coordinates for the cursor
 *     *at the current level*.  The z value will always match the level - 1.  The bottom
 *     information is the floor value at teh cursor at the current level and the location
 *     of the actual displayed cursor.  The distinction is made because our GUI currently
 *     always places the cursor on the ground beneath the mouse pointer, even if the
 *     ground is a few levels beneath the current level.  Eventually, we will need to be
 *     able to disable that for actors that can fly and remain in the air between turns,
 *     if any.
 * 4 (1<<2) = Turn on obstruction arrows for the floor and the ceiling.  These arrows point
 *     to the ceiling surface and the floor surface for teh current cell.
 * 8 (1<<3) = Turn on obstruction arrows.  These currently look broken from what I've
 *     seen in screenshots.  These arrows are supposed to point to the brushes that limit
 *     movement in a given direction. The arrows either touch the floor/ceiling of the
 *     current cell in the tested direction if it is possible to move in that direction,
 *     or touch the obstruction (surface) that prevents movement in a given direction.
 *     I want to redo these anyhow, as they are meant for map editors to be able to see what
 *     surfaces are actually preventing movement at the routing level.  Feedback is encouraged.
*/
#define MAPDEBUG_TEXT		(1<<1) /* Creates arrows pointing at floors and ceilings at mouse cursor */
#define MAPDEBUG_PATHING	(1<<0) /* Turns on pathing tracing. */
#define MAPDEBUG_CELLS		(1<<2) /* Creates arrows pointing at floors and ceilings at mouse cursor */
#define MAPDEBUG_WALLS		(1<<3) /* Creates arrows pointing at obstructions in the 8 primary directions */

extern cvar_t* cl_map_debug;

#endif
