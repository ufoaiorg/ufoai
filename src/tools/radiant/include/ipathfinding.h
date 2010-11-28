/**
 * @file ipathfinding.h
 * @brief Global pathfinding interface
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef IPATHFINDING_H
#define IPATHFINDING_H

#include "generic/constant.h"

#include <string>
#include <set>

class IPathfindingSystem
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "pathfinding");

		virtual ~IPathfindingSystem ()
		{
		}

		virtual void init () = 0;
};
#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IPathfindingSystem> GlobalPathfindingSystemModule;

// A reference to the call above.
template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IPathfindingSystem> GlobalPathfindingSystemModuleRef;

// This is the accessor for the pathfinding module
inline IPathfindingSystem& GlobalPathfindingSystem ()
{
	return GlobalPathfindingSystemModule::getTable();
}

#endif  /* IPATHFINDING_H */
