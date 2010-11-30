/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#ifndef INCLUDED_PREFERENCEDICTIONARY_H
#define INCLUDED_PREFERENCEDICTIONARY_H

#include "preferencesystem.h"

class PreferenceDictionary: public PreferenceSystem
{
	public:
		typedef PreferenceSystem Type;
		STRING_CONSTANT(Name, "*");

		PreferenceSystem* getTable ()
		{
			return this;
		}

	public:

		// greebo: Use this to add a preference constructor to the internal list. They get called when time comes.
		void addConstructor (PreferenceConstructor* constructor)
		{
			// greebo: pass the call to the global preference dialog (note the capital P, this smells like a pitfall!)
			g_Preferences.addConstructor(constructor);
		}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<PreferenceDictionary> PreferenceSystemModule;
typedef Static<PreferenceSystemModule> StaticPreferenceSystemModule;
StaticRegisterModule staticRegisterPreferenceSystem(StaticPreferenceSystemModule::instance());

#endif
