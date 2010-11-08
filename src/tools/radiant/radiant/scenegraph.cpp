/**
 * @file scenegraph.cpp
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

#include "debugging/debugging.h"

#include <map>
#include <set>
#include <vector>
#include <string>

#include "string/string.h"
#include "signal/signal.h"
#include "scenelib.h"
#include "instancelib.h"
#include "treemodel.h"

class StringEqualPredicate
{
		const std::string& m_string;
	public:
		StringEqualPredicate (const std::string& string) :
			m_string(string)
		{
		}
		bool operator() (const std::string& other) const
		{
			return m_string == other;
		}
};

template<std::size_t SIZE>
class TypeIdMap
{
		typedef std::vector<std::string> TypeMap;
		TypeMap m_typeNames;

	public:
		TypeIdMap ()
		{
		}
		TypeId getTypeId (const std::string& name)
		{
			int pos = 0;
			for	(TypeMap::const_iterator i = m_typeNames.begin(); i != m_typeNames.end();
				++i) {
				if (*i == name)
					return pos;
				++pos;
			}

			m_typeNames.push_back(name);
			return pos;
		}
};

class CompiledGraph: public scene::Graph, public scene::Instantiable::Observer
{
		typedef std::map<PathConstReference, scene::Instance*> InstanceMap;

		typedef std::vector<scene::EraseObserver*> EraseObservers;

		InstanceMap m_instances;
		scene::Instantiable::Observer* m_observer;
		Signal0 m_boundsChanged;
		scene::Path m_rootpath;
		Signal0 m_sceneChangedCallbacks;

		EraseObservers m_eraseObservers;

		TypeIdMap<NODETYPEID_MAX> m_nodeTypeIds;
		TypeIdMap<INSTANCETYPEID_MAX> m_instanceTypeIds;

	public:

		CompiledGraph (scene::Instantiable::Observer* observer) :
			m_observer(observer)
		{
		}

		void addSceneChangedCallback (const SignalHandler& handler)
		{
			m_sceneChangedCallbacks.connectLast(handler);
		}
		void sceneChanged (void)
		{
			m_sceneChangedCallbacks();
		}

		void notifyErase (scene::Instance* instance)
		{
			for (EraseObservers::iterator i = m_eraseObservers.begin(); i != m_eraseObservers.end(); ++i) {
				(*i)->onErase(instance);
			}
		}

		void removeEraseObserver(scene::EraseObserver *observer) {
			for (EraseObservers::iterator i = m_eraseObservers.begin(); i != m_eraseObservers.end(); ++i) {
				if (*i == observer) {
					m_eraseObservers.erase(i);
					break;
				}
			}
		}

		void addEraseObserver(scene::EraseObserver *observer) {
			m_eraseObservers.push_back(observer);
		}

		scene::Node& root (void)
		{
			ASSERT_MESSAGE(!m_rootpath.empty(), "scenegraph root does not exist");
			return m_rootpath.top();
		}
		void insert_root (scene::Node& root)
		{
			ASSERT_MESSAGE(m_rootpath.empty(), "scenegraph root already exists");

			root.IncRef();

			Node_traverseSubgraph(root, InstanceSubgraphWalker(this, scene::Path(), 0));

			m_rootpath.push(makeReference(root));
		}
		void erase_root (void)
		{
			ASSERT_MESSAGE(!m_rootpath.empty(), "scenegraph root does not exist");

			scene::Node & root = m_rootpath.top();

			m_rootpath.pop();

			Node_traverseSubgraph(root, UninstanceSubgraphWalker(this, scene::Path()));

			root.DecRef();
		}
		void boundsChanged (void)
		{
			m_boundsChanged();
		}

		void traverse (const Walker& walker)
		{
			traverse_subgraph(walker, m_instances.begin());
		}

		void traverse_subgraph (const Walker& walker, const scene::Path& start)
		{
			if (!m_instances.empty()) {
				traverse_subgraph(walker, m_instances.find(PathConstReference(start)));
			}
		}

		scene::Instance* find (const scene::Path& path)
		{
			InstanceMap::iterator i = m_instances.find(PathConstReference(path));
			if (i == m_instances.end()) {
				return 0;
			}
			return (*i).second;
		}

		scene::Instance* find (scene::Node& node)
		{
			scene::Path path;
			path.push(makeReference(node));
			return find(path);
		}

		void insert (scene::Instance* instance)
		{
			m_instances.insert(InstanceMap::value_type(PathConstReference(instance->path()), instance));

			m_observer->insert(instance);
		}
		void erase (scene::Instance* instance)
		{
			notifyErase(instance);
			m_observer->erase(instance);

			m_instances.erase(PathConstReference(instance->path()));
		}

		SignalHandlerId addBoundsChangedCallback (const SignalHandler& boundsChanged)
		{
			return m_boundsChanged.connectLast(boundsChanged);
		}
		void removeBoundsChangedCallback (SignalHandlerId id)
		{
			m_boundsChanged.disconnect(id);
		}

		TypeId getNodeTypeId (const std::string& name)
		{
			return m_nodeTypeIds.getTypeId(name);
		}

		TypeId getInstanceTypeId (const std::string& name)
		{
			return m_instanceTypeIds.getTypeId(name);
		}

	private:

		bool pre (const Walker& walker, const InstanceMap::iterator& i)
		{
			return walker.pre(i->first, *i->second);
		}

		void post (const Walker& walker, const InstanceMap::iterator& i)
		{
			walker.post(i->first, *i->second);
		}

		void traverse_subgraph (const Walker& walker, InstanceMap::iterator i)
		{
			Stack<InstanceMap::iterator> stack;
			if (i != m_instances.end()) {
				const std::size_t startSize = (*i).first.get().size();
				do {
					if (i != m_instances.end() && stack.size() < ((*i).first.get().size() - startSize + 1)) {
						stack.push(i);
						++i;
						if (!pre(walker, stack.top())) {
							// skip subgraph
							while (i != m_instances.end() && stack.size() < ((*i).first.get().size() - startSize + 1)) {
								++i;
							}
						}
					} else {
						post(walker, stack.top());
						stack.pop();
					}
				} while (!stack.empty());
			}
		}
};

namespace
{
	CompiledGraph* g_sceneGraph;
	GraphTreeModel* g_tree_model;
}

GraphTreeModel* scene_graph_get_tree_model (void)
{
	return g_tree_model;
}

class SceneGraphObserver: public scene::Instantiable::Observer
{
	public:
		void insert (scene::Instance* instance)
		{
			g_sceneGraph->sceneChanged();
			graph_tree_model_insert(g_tree_model, *instance);
		}
		void erase (scene::Instance* instance)
		{
			g_sceneGraph->notifyErase(instance);
			g_sceneGraph->sceneChanged();
			graph_tree_model_erase(g_tree_model, *instance);
		}
};

SceneGraphObserver g_SceneGraphObserver;

void SceneGraph_Construct (void)
{
	g_tree_model = graph_tree_model_new();

	g_sceneGraph = new CompiledGraph(&g_SceneGraphObserver);
}

void SceneGraph_Destroy (void)
{
	delete g_sceneGraph;

	graph_tree_model_delete(g_tree_model);
}

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class SceneGraphAPI
{
		scene::Graph* m_scenegraph;
	public:
		typedef scene::Graph Type;
		STRING_CONSTANT(Name, "*");

		SceneGraphAPI (void)
		{
			SceneGraph_Construct();

			m_scenegraph = g_sceneGraph;
		}
		~SceneGraphAPI (void)
		{
			SceneGraph_Destroy();
		}
		scene::Graph* getTable (void)
		{
			return m_scenegraph;
		}
};

typedef SingletonModule<SceneGraphAPI> SceneGraphModule;
typedef Static<SceneGraphModule> StaticSceneGraphModule;
StaticRegisterModule staticRegisterSceneGraph(StaticSceneGraphModule::instance());
