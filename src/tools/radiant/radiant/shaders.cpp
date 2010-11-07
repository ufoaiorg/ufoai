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

#include "ishadersystem.h"
#include "ifilesystem.h"
#include "itextures.h"
#include "iscriplib.h"
#include "iradiant.h"

#include "string/string.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

#include "shaders/shaders.h"

class ShadersDependencies: public GlobalFileSystemModuleRef,
		public GlobalTexturesModuleRef,
		public GlobalScripLibModuleRef,
		public GlobalRadiantModuleRef
{
};

class ShadersAPI
{
		ShaderSystem* m_shaders;
	public:
		typedef ShaderSystem Type;
		STRING_CONSTANT(Name, "*");

		ShadersAPI (ShadersDependencies& dependencies)
		{
			Shaders_Construct();
			m_shaders = &GetShaderSystem();
		}
		~ShadersAPI ()
		{
			Shaders_Destroy();
		}
		ShaderSystem* getTable ()
		{
			return m_shaders;
		}
};

typedef SingletonModule<ShadersAPI, ShadersDependencies, DependenciesAPIConstructor<ShadersAPI, ShadersDependencies> >
		ShadersModule;

typedef Static<ShadersModule> StaticShadersModule;
StaticRegisterModule staticRegisterShaders(StaticShadersModule::instance());
