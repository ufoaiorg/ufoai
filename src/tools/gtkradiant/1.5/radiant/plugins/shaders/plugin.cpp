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

#include "plugin.h"

#include "ishaders.h"
#include "ifilesystem.h"
#include "itextures.h"
#include "iscriplib.h"
#include "qerplugin.h"

#include "string/string.h"
#include "modulesystem/singletonmodule.h"

#include "shaders.h"

class ShadersDependencies :
			public GlobalFileSystemModuleRef,
			public GlobalTexturesModuleRef,
			public GlobalScripLibModuleRef,
			public GlobalRadiantModuleRef {
	ImageModuleRef m_bitmapModule;
public:
	ShadersDependencies() :
			m_bitmapModule("bmp") {
	}
	ImageModuleRef& getBitmapModule() {
		return m_bitmapModule;
	}
};

class ShadersQ3API {
	ShaderSystem* m_shadersq3;
public:
	typedef ShaderSystem Type;
	STRING_CONSTANT(Name, "quake3");

	ShadersQ3API(ShadersDependencies& dependencies) {
		g_shadersExtension = "shader";
		g_shadersDirectory = "scripts/";
		g_bitmapModule = dependencies.getBitmapModule().getTable();
		Shaders_Construct();
		m_shadersq3 = &GetShaderSystem();
	}
	~ShadersQ3API() {
		Shaders_Destroy();
	}
	ShaderSystem* getTable() {
		return m_shadersq3;
	}
};

typedef SingletonModule<ShadersQ3API, ShadersDependencies, DependenciesAPIConstructor<ShadersQ3API, ShadersDependencies> > ShadersQ3Module;

ShadersQ3Module g_ShadersQ3Module;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules(ModuleServer& server) {
	initialiseModule(server);

	g_ShadersQ3Module.selfRegister();
}
