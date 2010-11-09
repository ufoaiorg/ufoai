/**
 * @file renderstate.cpp
 */

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
#include "imaterial.h"
#include "irender.h"
#include "itextures.h"
#include "igl.h"
#include "iglrender.h"
#include "renderable.h"
#include "iradiant.h"

#include <map>

#include "moduleobservers.h"

#include "backend/OpenGLShader.h"
#include "backend/OpenGLShaderPass.h"
#include "backend/OpenGLStateLess.h"
#include "backend/OpenGLStateMap.h"
#include "backend/OpenGLShaderCache.h"

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class OpenGLStateLibraryAPI {
		OpenGLStateMap m_stateMap;
	public:
		typedef OpenGLStateLibrary Type;
		STRING_CONSTANT(Name, "*");

		OpenGLStateLibraryAPI() {
		}

		~OpenGLStateLibraryAPI() {
		}

		OpenGLStateLibrary* getTable() {
			return &m_stateMap;
		}
};

typedef SingletonModule<OpenGLStateLibraryAPI> OpenGLStateLibraryModule;
typedef Static<OpenGLStateLibraryModule> StaticOpenGLStateLibraryModule;
StaticRegisterModule staticRegisterOpenGLStateLibrary(StaticOpenGLStateLibraryModule::instance());

class ShaderCacheDependencies: public GlobalShadersModuleRef,
		public GlobalTexturesModuleRef,
		public GlobalOpenGLStateLibraryModuleRef {
	public:
		ShaderCacheDependencies() :
			GlobalShadersModuleRef("*") {
		}
};

class ShaderCacheAPI {
		OpenGLShaderCache* m_shaderCache;
	public:
		typedef ShaderCache Type;
		STRING_CONSTANT(Name, "*");

		ShaderCacheAPI() {
			m_shaderCache = new OpenGLShaderCache;
			GlobalTexturesCache().attach(*m_shaderCache);
			GlobalShaderSystem().attach(*m_shaderCache);
		}

		~ShaderCacheAPI() {
			GlobalShaderSystem().detach(*m_shaderCache);
			GlobalTexturesCache().detach(*m_shaderCache);
			delete m_shaderCache;
		}

		ShaderCache* getTable() {
			return m_shaderCache;
		}
};

typedef SingletonModule<ShaderCacheAPI, ShaderCacheDependencies> ShaderCacheModule;
typedef Static<ShaderCacheModule> StaticShaderCacheModule;
StaticRegisterModule staticRegisterShaderCache(StaticShaderCacheModule::instance());
