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

#include "skincache.h"

#include "ifilesystem.h"
#include "iscriplib.h"
#include "iarchive.h"
#include "modelskin.h"

#include <map>
#include <string>
#include <vector>

#include "stream/stringstream.h"
#include "generic/callback.h"
#include "container/cache.h"
#include "container/hashfunc.h"
#include "os/path.h"
#include "moduleobservers.h"
#include "modulesystem/singletonmodule.h"
#include "stringio.h"

/** A single instance of a UFO model skin. This structure stores a set of maps
 * between an existing texture and a new texture, and possibly the name of the model
 * that this skin is associated with.
 */
class UFOModelSkin
{
		// Map of texture switches
		typedef std::map<CopiedString, CopiedString> Remaps;
		Remaps m_remaps;
		// Associated model
		std::string _model;

	public:
		// Constructor
		UFOModelSkin () :
			_model("")
		{
		}

		const char* getRemap (const char* name) const
		{
			Remaps::const_iterator i = m_remaps.find(name);
			if (i != m_remaps.end()) {
				return (*i).second.c_str();
			}
			return "";
		}
		void forEachRemap (const SkinRemapCallback& callback) const
		{
			for (Remaps::const_iterator i = m_remaps.begin(); i != m_remaps.end(); ++i) {
				callback(SkinRemap((*i).first.c_str(), (*i).second.c_str()));
			}
		}

		// Return the model associated with this skin.
		std::string getModel ()
		{
			return _model;
		}
};

/** Singleton class to retain a mapping between skin names and skin objects.
 */
class GlobalSkins
{
	public:
		// Map of skin names to skin objects
		typedef std::map<CopiedString, UFOModelSkin> SkinMap;
		SkinMap m_skins;

		// Map between model paths and a vector of names of the associated skins,
		// which are contained in the main SkinMap.
		typedef std::map<std::string, std::vector<std::string> > ModelSkinMap;
		ModelSkinMap _mSkinMap;

		UFOModelSkin g_nullSkin;

		UFOModelSkin& getSkin (const char* name)
		{
			SkinMap::iterator i = m_skins.find(name);
			if (i != m_skins.end()) {
				return (*i).second;
			}

			return g_nullSkin;
		}

		// Return the vector of skin names corresponding to the given model
		const std::vector<std::string>& getSkinsForModel (const std::string& model)
		{
			return _mSkinMap[model];
		}

		void construct ()
		{
		}

		void destroy ()
		{
			m_skins.clear();
		}

		void realise ()
		{
			construct();
		}
		void unrealise ()
		{
			destroy();
		}
};

static GlobalSkins g_skins;

class UFOModelSkinCacheElement: public ModelSkin
{
		ModuleObservers m_observers;
		UFOModelSkin* m_skin;
	public:
		UFOModelSkinCacheElement () :
			m_skin(0)
		{
		}
		void attach (ModuleObserver& observer)
		{
			m_observers.attach(observer);
			if (realised()) {
				observer.realise();
			}
		}
		void detach (ModuleObserver& observer)
		{
			if (realised()) {
				observer.unrealise();
			}
			m_observers.detach(observer);
		}
		bool realised () const
		{
			return m_skin != 0;
		}
		void realise (const char* name)
		{
			ASSERT_MESSAGE(!realised(), "UFOModelSkinCacheElement::realise: already realised");
			m_skin = &g_skins.getSkin(name);
			m_observers.realise();
		}
		void unrealise ()
		{
			ASSERT_MESSAGE(realised(), "UFOModelSkinCacheElement::unrealise: not realised");
			m_observers.unrealise();
			m_skin = 0;
		}
		const char* getRemap (const char* name) const
		{
			ASSERT_MESSAGE(realised(), "UFOModelSkinCacheElement::getRemap: not realised");
			return m_skin->getRemap(name);
		}
		void forEachRemap (const SkinRemapCallback& callback) const
		{
			ASSERT_MESSAGE(realised(), "UFOModelSkinCacheElement::forEachRemap: not realised");
			m_skin->forEachRemap(callback);
		}
};

class UFOModelSkinCache: public ModelSkinCache, public ModuleObserver
{
		class CreateUFOModelSkin
		{
				UFOModelSkinCache& m_cache;
			public:
				explicit CreateUFOModelSkin (UFOModelSkinCache& cache) :
					m_cache(cache)
				{
				}
				UFOModelSkinCacheElement* construct (const CopiedString& name)
				{
					UFOModelSkinCacheElement* skin = new UFOModelSkinCacheElement;
					if (m_cache.realised()) {
						skin->realise(name.c_str());
					}
					return skin;
				}
				void destroy (UFOModelSkinCacheElement* skin)
				{
					if (m_cache.realised()) {
						skin->unrealise();
					}
					delete skin;
				}
		};

		typedef HashedCache<CopiedString, UFOModelSkinCacheElement, HashString, std::equal_to<CopiedString>,
				CreateUFOModelSkin> Cache;
		Cache m_cache;
		bool m_realised;

	public:
		typedef ModelSkinCache Type;
		STRING_CONSTANT(Name, "*");
		ModelSkinCache* getTable ()
		{
			return this;
		}

		UFOModelSkinCache () :
			m_cache(CreateUFOModelSkin(*this)), m_realised(false)
		{
			GlobalFileSystem().attach(*this);
		}
		~UFOModelSkinCache ()
		{
			GlobalFileSystem().detach(*this);
		}

		ModelSkin& capture (const char* name)
		{
			return *m_cache.capture(name);
		}
		void release (const char* name)
		{
			m_cache.release(name);
		}

		bool realised () const
		{
			return m_realised;
		}
		void realise ()
		{
			g_skins.realise();
			m_realised = true;
			for (Cache::iterator i = m_cache.begin(); i != m_cache.end(); ++i) {
				(*i).value->realise((*i).key.c_str());
			}
		}
		void unrealise ()
		{
			m_realised = false;
			for (Cache::iterator i = m_cache.begin(); i != m_cache.end(); ++i) {
				(*i).value->unrealise();
			}
			g_skins.unrealise();
		}

		// Get the vector of skin names corresponding to the given model.
		const ModelSkinList& getSkinsForModel (const std::string& model)
		{
			// Pass on call to the GlobalSkins class
			return g_skins.getSkinsForModel(model);
		}
};

class UFOModelSkinCacheDependencies: public GlobalFileSystemModuleRef, public GlobalScripLibModuleRef
{
};

typedef SingletonModule<UFOModelSkinCache, UFOModelSkinCacheDependencies> UFOModelSkinCacheModule;

UFOModelSkinCacheModule g_UFOModelSkinCacheModule;

void UFOModelSkinCacheModule_selfRegister (ModuleServer& server)
{
	g_UFOModelSkinCacheModule.selfRegister();
}
