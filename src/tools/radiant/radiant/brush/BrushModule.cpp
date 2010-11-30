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

#include "BrushModule.h"
#include "radiant_i18n.h"

#include "iradiant.h"

#include "TexDef.h"
#include "ibrush.h"
#include "ifilter.h"

#include "BrushNode.h"
#include "brushmanip.h"
#include "BrushVisit.h"

#include "preferencesystem.h"
#include "stringio.h"

#include "../map/map.h"
#include "../dialog.h"

BrushModuleClass::BrushModuleClass():_textureLockEnabled(GlobalRegistry().get(RKEY_ENABLE_TEXTURE_LOCK) == "1")
{
	GlobalRegistry().addKeyObserver(this, RKEY_ENABLE_TEXTURE_LOCK);
}

void BrushModuleClass::construct ()
{
	Brush_registerCommands();

	BrushClipPlane::constructStatic();
	BrushInstance::constructStatic();
	Brush::constructStatic();

	Brush::m_maxWorldCoord = GlobalRegistry().getFloat("game/defaults/maxWorldCoord");
}

void BrushModuleClass::destroy ()
{
	Brush::m_maxWorldCoord = 0;

	Brush::destroyStatic();
	BrushInstance::destroyStatic();
	BrushClipPlane::destroyStatic();
}

void BrushModuleClass::clipperColourChanged ()
{
	BrushClipPlane::destroyStatic();
	BrushClipPlane::constructStatic();
}

scene::Node& BrushModuleClass::createBrush ()
{
	return *(new BrushNode);
}

void BrushModuleClass::keyChanged(const std::string& changedKey, const std::string& newValue) {
	_textureLockEnabled = (GlobalRegistry().get(RKEY_ENABLE_TEXTURE_LOCK) == "1");
}

bool BrushModuleClass::textureLockEnabled() const {
	return _textureLockEnabled;
}

void BrushModuleClass::setTextureLock(bool enabled) {
	// Write the value to the registry, the keyChanged() method is triggered automatically
	GlobalRegistry().set(RKEY_ENABLE_TEXTURE_LOCK, enabled ? "1" : "0");
}

void BrushModuleClass::toggleTextureLock() {
	setTextureLock(!textureLockEnabled());
}

BrushModuleClass* GlobalBrush ()
{
	static BrushModuleClass _brushModule;
	return &_brushModule;
}

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class BrushDependencies: public GlobalRadiantModuleRef,
		public GlobalSceneGraphModuleRef,
		public GlobalShaderCacheModuleRef,
		public GlobalSelectionModuleRef,
		public GlobalOpenGLModuleRef,
		public GlobalUndoModuleRef,
		public GlobalFilterModuleRef
{
};

class BrushUFOAPI
{
		BrushCreator* m_brushufo;
	public:
		typedef BrushCreator Type;
		STRING_CONSTANT(Name, "*");

		BrushUFOAPI ()
		{
			GlobalBrush()->construct();

			m_brushufo = GlobalBrush();
		}
		~BrushUFOAPI ()
		{
			GlobalBrush()->destroy();
		}
		BrushCreator* getTable ()
		{
			return m_brushufo;
		}
};

typedef SingletonModule<BrushUFOAPI, BrushDependencies> BrushUFOModule;
typedef Static<BrushUFOModule> StaticBrushUFOModule;
StaticRegisterModule staticRegisterBrushUFO(StaticBrushUFOModule::instance());
