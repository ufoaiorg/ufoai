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

#include "iscenegraph.h"
#include "irender.h"
#include "iselection.h"
#include "ientity.h"
#include "iundo.h"
#include "ieclass.h"
#include "igl.h"
#include "ireference.h"
#include "ifilter.h"
#include "iregistry.h"
#include "preferencesystem.h"
#include "iradiant.h"
#include "inamespace.h"
#include "EntityCreator.h"
#include "itextures.h"
#include "modulesystem/singletonmodule.h"

class EntityDependencies: public GlobalRadiantModuleRef,
		public GlobalOpenGLModuleRef,
		public GlobalUndoModuleRef,
		public GlobalSceneGraphModuleRef,
		public GlobalShaderCacheModuleRef,
		public GlobalSelectionModuleRef,
		public GlobalTexturesModuleRef,
		public GlobalReferenceModuleRef,
		public GlobalFilterModuleRef,
		public GlobalPreferenceSystemModuleRef,
		public GlobalNamespaceModuleRef,
		public GlobalRegistryModuleRef
{
};

class EntityAPI
{
		EntityCreator* m_entity;
	public:
		typedef EntityCreator Type;
		STRING_CONSTANT(Name, "*");

		EntityAPI ()
		{
			P_Entity_Construct();

			m_entity = &GetEntityCreator();

			GlobalReferenceCache().setEntityCreator(*m_entity);
		}
		~EntityAPI ()
		{
			P_Entity_Destroy();
		}
		EntityCreator* getTable ()
		{
			return m_entity;
		}
};

typedef SingletonModule<EntityAPI, EntityDependencies> EntityModule;

typedef Static<EntityModule> StaticEntityModule;
StaticRegisterModule staticRegisterEntity(StaticEntityModule::instance());
