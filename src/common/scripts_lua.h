/**
 * @file
 * @brief Header for lua script functions
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

/**
 * @brief callback signatures for functions defined in Lua
 */
//typedef void (*LUA_MODULE_CALLBACK)(void);
//typedef void (*LUA_NODE_CALLBACK)(void);

/**
 * @brief holds a reference to a lua global function
 */
typedef int LUA_FUNCTION;
/**
 * @brief holds a reference to a lua event handler
 */
typedef int LUA_EVENT;

/**
 * @brief holds a reference to a lua table instance, so we can perform calls to table methods in our callback
 * mechanism
 */
typedef int LUA_INSTANCE;



