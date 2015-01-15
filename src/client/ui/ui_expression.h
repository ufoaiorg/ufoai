/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../common/common.h"

/* prototype */
struct uiNode_t;
struct uiAction_s;
struct uiCallContext_s;

struct uiAction_s* UI_AllocStaticStringCondition(const char* description) __attribute__ ((warn_unused_result));
struct uiAction_s* UI_ParseExpression(const char** text) __attribute__ ((warn_unused_result));

bool UI_GetBooleanFromExpression(struct uiAction_s* expression, const struct uiCallContext_s* context) __attribute__ ((warn_unused_result));
float UI_GetFloatFromExpression(struct uiAction_s* expression, const struct uiCallContext_s* context) __attribute__ ((warn_unused_result));
const char* UI_GetStringFromExpression(struct uiAction_s* expression, const struct uiCallContext_s* context) __attribute__ ((warn_unused_result));
uiNode_t* UI_GetNodeFromExpression(struct uiAction_s* expression, const struct uiCallContext_s* context, const struct value_s** property);
