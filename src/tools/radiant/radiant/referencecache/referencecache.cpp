/**
 * @file referencecache.cpp
 * @brief
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

#include "referencecache.h"
#include "radiant_i18n.h"

#include "debugging/debugging.h"

#include "../map/RootNode.h"
#include "AutoPtr.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"
#include "imap.h"
MapModules& ReferenceAPI_getMapModules ();
#include "imodel.h"
ModelModules& ReferenceAPI_getModelModules ();
#include "ifilesystem.h"
#include "iarchive.h"
#include "ifiletypes.h"
#include "ireference.h"
#include "ientity.h"
#include "iradiant.h"
#include "itextstream.h"

#include <list>

#include "container/cache.h"
#include "container/hashfunc.h"
#include "os/path.h"
#include "stream/textfilestream.h"
#include "nullmodel.h"
#include "stream/stringstream.h"
#include "os/file.h"
#include "moduleobserver.h"
#include "moduleobservers.h"

#include "../ui/mainframe/mainframe.h"
#include "../map/map.h"
#include "../map/algorithm/Traverse.h"
#include "../filetypes.h"

static bool References_Saved ();

void MapChanged ()
{
	GlobalMap().setModified(!References_Saved());
}

static EntityCreator* g_entityCreator = 0;

static bool MapResource_loadFile (const MapFormat& format, scene::Node& root, const std::string& filename)
{
	TextFileInputStream file(filename);
	if (!file.failed()) {
		format.readGraph(root, file, *g_entityCreator);
		return true;
	} else {
		globalErrorStream() << "ERROR: Could not load file: " << filename << "\n";
		return false;
	}
}

static NodeSmartReference MapResource_load (const MapFormat& format, const std::string& path, const std::string& name)
{
	NodeSmartReference root(NewMapRoot(name));
	std::string fullpath = path + name;

	if (g_path_is_absolute(fullpath.c_str())) {
		MapResource_loadFile(format, root, fullpath);
	} else {
		globalErrorStream() << "ERROR: map path is not fully qualified: " << fullpath << "\n";
	}

	return root;
}

/**
 * @sa Map_Write
 */
bool MapResource_saveFile (const MapFormat& format, scene::Node& root, GraphTraversalFunc traverse,
		const std::string& filename)
{
	TextFileOutputStream file(filename);
	if (!file.failed()) {
		ScopeDisableScreenUpdates disableScreenUpdates(os::getFilenameFromPath(filename), _("Saving Map"));
		format.writeGraph(root, traverse, file);
		return true;
	}

	globalErrorStream() << "ERROR: open file for writing failed: " << filename << "\n";
	return false;
}

static bool file_saveBackup (const std::string& path)
{
	if (file_writeable(path)) {
		std::string backup = os::stripExtension(path) + ".bak";

		return (!file_exists(backup) || file_remove(backup)) // remove backup
				&& file_move(path, backup); // rename current to backup
	}

	globalErrorStream() << "ERROR: map path is not writeable: " << path << "\n";
	return false;
}

/**
 * Save a map file (outer function). This function tries to backup the map
 * file before calling MapResource_saveFile() to do the actual saving of
 * data.
 */
bool MapResource_save (const MapFormat& format, scene::Node& root, const std::string& path, const std::string& name)
{
	std::string fullpath = path + name;

	if (g_path_is_absolute(fullpath.c_str())) {

		// Save a backup if possible. This is done by renaming the original,
		// which won't work if the existing map is currently open by another process
		// in the background.
		if (file_exists(fullpath) && !file_saveBackup(fullpath)) {
			globalErrorStream() << "ERROR: could not rename: " << fullpath << " to backup." << "\n";
		}

		// Save the actual file
		return MapResource_saveFile(format, root, map::Map_Traverse, fullpath);
	} else {
		globalErrorStream() << "ERROR: map path is not fully qualified: " << fullpath << "\n";
		return false;
	}
}

namespace
{
	NodeSmartReference g_nullNode(NewNullNode());
	NodeSmartReference g_nullModel(g_nullNode);
}

class NullModelLoader: public ModelLoader
{
	public:
		scene::Node& loadModel (ArchiveFile& file)
		{
			return g_nullModel;
		}

		model::IModelPtr loadModelFromPath (const std::string& name)
		{
			return model::IModelPtr();
		}
};

namespace
{
	NullModelLoader g_NullModelLoader;
}

/**
 * @brief Returns the model loader for the model \p type or 0 if the model \p type has no loader module
 */
ModelLoader* ModelLoader_forType (const std::string& type)
{
	const std::string moduleName = findModuleName(&GlobalFiletypes(), std::string(ModelLoader::Name()), type);
	if (!moduleName.empty()) {
		ModelLoader* table = ReferenceAPI_getModelModules().findModule(moduleName);
		if (table != 0) {
			return table;
		} else {
			globalErrorStream() << "ERROR:  Model type incorrectly registered: " << moduleName << "\n";
			return &g_NullModelLoader;
		}
	}
	return 0;
}

static NodeSmartReference ModelResource_load (ModelLoader* loader, const std::string& name)
{
	NodeSmartReference model(g_nullModel);

	{
		AutoPtr<ArchiveFile> file(GlobalFileSystem().openFile(name));
		if (file) {
			model = loader->loadModel(*file);
		} else {
			globalErrorStream() << "Model load failed: " << name << "\n";
		}
	}

	model.get().setIsRoot(true);

	return model;
}

static inline hash_t path_hash (const char* path, hash_t previous = 0)
{
#ifdef _WIN32
	return string_hash_nocase(path, previous);
#else // UNIX
	return string_hash(path, previous);
#endif
}

struct PathEqual
{
		bool operator() (const std::string& path, const std::string& other) const
		{
			return path_equal(path, other);
		}
};

struct PathHash
{
		typedef hash_t hash_type;
		hash_type operator() (const std::string& path) const
		{
			return path_hash(path.c_str());
		}
};

typedef std::pair<std::string, std::string> ModelKey;

struct ModelKeyEqual
{
		bool operator() (const ModelKey& key, const ModelKey& other) const
		{
			return path_equal(key.first, other.first) && path_equal(key.second, other.second);
		}
};

struct ModelKeyHash
{
		typedef hash_t hash_type;
		hash_type operator() (const ModelKey& key) const
		{
			return hash_combine(path_hash(key.first.c_str()), path_hash(key.second.c_str()));
		}
};

typedef HashTable<ModelKey, NodeSmartReference, ModelKeyHash, ModelKeyEqual> ModelCache;
static ModelCache g_modelCache;
static bool g_modelCache_enabled = true;

static ModelCache::iterator ModelCache_find (const std::string& path, const std::string& name)
{
	if (g_modelCache_enabled) {
		return g_modelCache.find(ModelKey(path, name));
	}
	return g_modelCache.end();
}

static ModelCache::iterator ModelCache_insert (const std::string& path, const std::string& name, scene::Node& node)
{
	if (g_modelCache_enabled) {
		return g_modelCache.insert(ModelKey(path, name), NodeSmartReference(node));
	}
	return g_modelCache.insert(ModelKey("", ""), g_nullModel);
}

void ModelCache_flush (const std::string& path, const std::string& name)
{
	ModelCache::iterator i = g_modelCache.find(ModelKey(path, name));
	if (i != g_modelCache.end()) {
		//ASSERT_MESSAGE((*i).value.getCount() == 0, "resource flushed while still in use: " << (*i).key.first.c_str() << (*i).key.second.c_str());
		g_modelCache.erase(i);
	}
}

void ModelCache_clear ()
{
	g_modelCache_enabled = false;
	g_modelCache.clear();
	g_modelCache_enabled = true;
}

NodeSmartReference Model_load (ModelLoader* loader, const std::string& path, const std::string& name, const std::string& type)
{
	if (loader != 0) {
		return ModelResource_load(loader, name);
	} else {
		// Get a loader module name for this type, if possible. If none is
		// found, try again with the "map" type, since we might be loading a
		// map with a different extension
		std::string moduleName = findModuleName(&GlobalFiletypes(), MapFormat::Name(), type);

		// Empty, try again with "map" type
		if (moduleName.empty()) {
			moduleName = findModuleName(&GlobalFiletypes(), MapFormat::Name(), "map");
		}

		const MapFormat* format = ReferenceAPI_getMapModules().findModule(moduleName);
		if (format != 0) {
			return MapResource_load(*format, path, name);
		} else {
			globalErrorStream() << "ERROR: Map type incorrectly registered: " << moduleName << "\n";
			return g_nullModel;
		}
	}
}

namespace
{
	bool g_realised = false;

	// name may be absolute or relative
	const std::string rootPath (const std::string& name)
	{
		return GlobalFileSystem().findRoot(g_path_is_absolute(name.c_str()) ? name : GlobalFileSystem().findFile(name));
	}
}

struct ModelResource: public Resource
{
		NodeSmartReference m_model;
		const std::string m_originalName;
		std::string m_path;
		std::string m_name;
		std::string m_type;
		ModelLoader* m_loader;
		ModuleObservers m_observers;
		std::time_t m_modified;
		std::size_t m_unrealised;

		ModelResource (const std::string& name) :
			m_model(g_nullModel), m_originalName(name), m_type(os::getExtension(name)), m_loader(0),
					m_modified(0), m_unrealised(1)
		{
			m_loader = ModelLoader_forType(m_type);

			if (g_realised) {
				realise();
			}
		}
		~ModelResource ()
		{
			if (realised()) {
				unrealise();
			}
			ASSERT_MESSAGE(!realised(), "ModelResource::~ModelResource: resource reference still realised: " << m_name);
		}
		// NOT COPYABLE
		ModelResource (const ModelResource&);
		// NOT ASSIGNABLE
		ModelResource& operator= (const ModelResource&);

		void setModel (const NodeSmartReference& model)
		{
			m_model = model;
		}
		void clearModel ()
		{
			m_model = g_nullModel;
		}

		void loadCached ()
		{
			if (g_modelCache_enabled) {
				// cache lookup
				ModelCache::iterator i = ModelCache_find(m_path, m_name);
				if (i == g_modelCache.end()) {
					i = ModelCache_insert(m_path, m_name, Model_load(m_loader, m_path, m_name, m_type));
				}

				setModel((*i).value);
			} else {
				setModel(Model_load(m_loader, m_path, m_name, m_type));
			}
		}

		void loadModel ()
		{
			loadCached();
			connectMap();
			mapSave();
		}

		bool load ()
		{
			ASSERT_MESSAGE(realised(), "resource not realised");
			if (m_model == g_nullModel) {
				loadModel();
			}

			return m_model != g_nullModel;
		}
		bool save ()
		{
			if (!mapSaved()) {
				const std::string moduleName = findModuleName(GetFileTypeRegistry(), MapFormat::Name(), m_type);
				if (!moduleName.empty()) {
					const MapFormat* format = ReferenceAPI_getMapModules().findModule(moduleName);
					if (format != 0 && MapResource_save(*format, m_model.get(), m_path, m_name)) {
						mapSave();
						return true;
					}
				}
			}
			return false;
		}
		void flush ()
		{
			if (realised()) {
				ModelCache_flush(m_path, m_name);
			}
		}
		scene::Node* getNode ()
		{
			return m_model.get_pointer();
		}
		void setNode (scene::Node* node)
		{
			ModelCache::iterator i = ModelCache_find(m_path, m_name);
			if (i != g_modelCache.end()) {
				(*i).value = NodeSmartReference(*node);
			}
			setModel(NodeSmartReference(*node));

			connectMap();
		}
		void attach (ModuleObserver& observer)
		{
			if (realised()) {
				observer.realise();
			}
			m_observers.attach(observer);
		}
		void detach (ModuleObserver& observer)
		{
			if (realised()) {
				observer.unrealise();
			}
			m_observers.detach(observer);
		}
		bool realised ()
		{
			return m_unrealised == 0;
		}
		void realise ()
		{
			ASSERT_MESSAGE(m_unrealised != 0, "ModelResource::realise: already realised");
			if (--m_unrealised == 0) {
				m_path = rootPath(m_originalName);
				m_name = path_make_relative(m_originalName, m_path);

				m_observers.realise();
			}
		}
		void unrealise ()
		{
			if (++m_unrealised == 1) {
				m_observers.unrealise();

				clearModel();
			}
		}
		bool isMap () const
		{
			return Node_getMapFile(m_model) != 0;
		}
		void connectMap ()
		{
			MapFile* map = Node_getMapFile(m_model);
			if (map != 0) {
				map->setChangedCallback(FreeCaller<MapChanged> ());
			}
		}
		std::time_t modified () const
		{
			const std::string fullpath = m_path + m_name;
			return file_modified(fullpath);
		}
		void mapSave ()
		{
			m_modified = modified();
			MapFile* map = Node_getMapFile(m_model);
			if (map != 0) {
				map->save();
			}
		}
		bool mapSaved () const
		{
			MapFile* map = Node_getMapFile(m_model);
			if (map != 0) {
				return m_modified == modified() && map->saved();
			}
			return true;
		}
		bool isModified () const
		{
			return ((!m_path.empty() // had or has an absolute path
					&& m_modified != modified()) // AND disk timestamp changed
					|| !path_equal(rootPath(m_originalName), m_path)); // OR absolute vfs-root changed
		}
		void refresh ()
		{
			if (isModified()) {
				flush();
				unrealise();
				realise();
			}
		}
};

class HashtableReferenceCache: public ReferenceCache, public ModuleObserver
{
		typedef HashedCache<std::string, ModelResource, PathHash, PathEqual> ModelReferences;
		ModelReferences m_references;
		std::size_t m_unrealised;

		class ModelReferencesSnapshot
		{
				ModelReferences& m_references;
				typedef std::list<ModelReferences::iterator> Iterators;
				Iterators m_iterators;
			public:
				typedef Iterators::iterator iterator;
				ModelReferencesSnapshot (ModelReferences& references) :
					m_references(references)
				{
					for (ModelReferences::iterator i = m_references.begin(); i != m_references.end(); ++i) {
						m_references.capture(i);
						m_iterators.push_back(i);
					}
				}
				~ModelReferencesSnapshot ()
				{
					for (Iterators::iterator i = m_iterators.begin(); i != m_iterators.end(); ++i) {
						m_references.release(*i);
					}
				}
				iterator begin ()
				{
					return m_iterators.begin();
				}
				iterator end ()
				{
					return m_iterators.end();
				}
		};

	public:

		typedef ModelReferences::iterator iterator;

		HashtableReferenceCache () :
			m_unrealised(1)
		{
		}

		iterator begin ()
		{
			return m_references.begin();
		}
		iterator end ()
		{
			return m_references.end();
		}

		void clear ()
		{
			m_references.clear();
		}

		Resource* capture (const std::string& path)
		{
			g_debug("capture: \"%s\"\n", path.c_str());
			return m_references.capture(path).get();
		}
		void release (const std::string& path)
		{
			m_references.release(path);
			g_debug("release: \"%s\"\n", path.c_str());
		}

		void setEntityCreator (EntityCreator& entityCreator)
		{
			g_entityCreator = &entityCreator;
		}

		bool realised () const
		{
			return m_unrealised == 0;
		}
		void realise ()
		{
			ASSERT_MESSAGE(m_unrealised != 0, "HashtableReferenceCache::realise: already realised");
			if (--m_unrealised == 0) {
				g_realised = true;

				{
					ModelReferencesSnapshot snapshot(m_references);
					for (ModelReferencesSnapshot::iterator i = snapshot.begin(); i != snapshot.end(); ++i) {
						ModelReferences::value_type& value = *(*i);
						if (value.value.count() != 1) {
							value.value.get()->realise();
						}
					}
				}
			}
		}
		void unrealise ()
		{
			if (++m_unrealised == 1) {
				g_realised = false;

				{
					ModelReferencesSnapshot snapshot(m_references);
					for (ModelReferencesSnapshot::iterator i = snapshot.begin(); i != snapshot.end(); ++i) {
						ModelReferences::value_type& value = *(*i);
						if (value.value.count() != 1) {
							value.value.get()->unrealise();
						}
					}
				}

				ModelCache_clear();
			}
		}
		void refresh ()
		{
			ModelReferencesSnapshot snapshot(m_references);
			for (ModelReferencesSnapshot::iterator i = snapshot.begin(); i != snapshot.end(); ++i) {
				ModelResource* resource = (*(*i)).value.get();
				if (!resource->isMap()) {
					resource->refresh();
				}
			}
		}
};

namespace
{
	HashtableReferenceCache g_referenceCache;
}

void SaveReferences ()
{
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Saving Map"));
	for (HashtableReferenceCache::iterator i = g_referenceCache.begin(); i != g_referenceCache.end(); ++i) {
		(*i).value->save();
	}
	MapChanged();
}

static bool References_Saved ()
{
	for (HashtableReferenceCache::iterator i = g_referenceCache.begin(); i != g_referenceCache.end(); ++i) {
		scene::Node* node = (*i).value->getNode();
		if (node != 0) {
			MapFile* map = Node_getMapFile(*node);
			if (map != 0 && !map->saved()) {
				return false;
			}
		}
	}
	return true;
}

void RefreshReferences ()
{
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Refreshing Models"));
	g_referenceCache.refresh();
}

void FlushReferences ()
{
	ModelCache_clear();

	g_referenceCache.clear();
}

ReferenceCache& GetReferenceCache ()
{
	return g_referenceCache;
}

#include "modulesystem/modulesmap.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class ReferenceDependencies: public GlobalRadiantModuleRef,
		public GlobalFileSystemModuleRef,
		public GlobalFiletypesModuleRef
{
		ModelModulesRef m_model_modules;
		MapModulesRef m_map_modules;
	public:
		ReferenceDependencies () :
			m_model_modules("*"), m_map_modules("*")
		{
		}
		ModelModules& getModelModules ()
		{
			return m_model_modules.get();
		}
		MapModules& getMapModules ()
		{
			return m_map_modules.get();
		}
};

class ReferenceAPI
{
		ReferenceCache* m_reference;
	public:
		typedef ReferenceCache Type;
		STRING_CONSTANT(Name, "*");

		ReferenceAPI ()
		{
			g_nullModel = NewNullModel();

			GlobalFileSystem().attach(g_referenceCache);

			m_reference = &GetReferenceCache();
		}
		~ReferenceAPI ()
		{
			GlobalFileSystem().detach(g_referenceCache);

			g_nullModel = g_nullNode;
		}
		ReferenceCache* getTable ()
		{
			return m_reference;
		}
};

typedef SingletonModule<ReferenceAPI, ReferenceDependencies> ReferenceModule;
typedef Static<ReferenceModule> StaticReferenceModule;
StaticRegisterModule staticRegisterReference(StaticReferenceModule::instance());

ModelModules& ReferenceAPI_getModelModules ()
{
	return StaticReferenceModule::instance().getDependencies().getModelModules();
}
MapModules& ReferenceAPI_getMapModules ()
{
	return StaticReferenceModule::instance().getDependencies().getMapModules();
}
