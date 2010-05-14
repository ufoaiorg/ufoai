/**
 * @file m_expression.h
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

#ifndef CLIENT_MENU_M_CONDITION_H
#define CLIENT_MENU_M_CONDITION_H

#include "../../common/common.h"

/* prototype */
struct menuNode_s;
struct menuAction_s;
struct menuCallContext_s;

struct menuAction_s *MN_AllocStaticStringCondition(const char *description) __attribute__ ((warn_unused_result));
struct menuAction_s *MN_ParseExpression(const char **text, const char *errhead, const struct menuNode_s *source) __attribute__ ((warn_unused_result));

qboolean MN_GetBooleanFromExpression(struct menuAction_s *expression, const struct menuCallContext_s *context) __attribute__ ((warn_unused_result));
float MN_GetFloatFromExpression(struct menuAction_s *expression, const struct menuCallContext_s *context) __attribute__ ((warn_unused_result));
const char *MN_GetStringFromExpression(struct menuAction_s *expression, const struct menuCallContext_s *context) __attribute__ ((warn_unused_result));


#endif
