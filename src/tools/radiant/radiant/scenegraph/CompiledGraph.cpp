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

#include "CompiledGraph.h"

#include "debugging/debugging.h"

#include <set>
#include <string>

#include "scenelib.h"
#include "instancelib.h"
#include "treemodel.h"

CompiledGraph::CompiledGraph () :
	_treeModel(graph_tree_model_new())
{
}

CompiledGraph::~CompiledGraph ()
{
	graph_tree_model_delete(_treeModel);
}

GraphTreeModel* CompiledGraph::getTreeModel ()
{
	return _treeModel;
}

void CompiledGraph::addSceneObserver(scene::Graph::Observer* observer) {
	if (observer != NULL) {
		// Add the passed observer to the list
		_sceneObservers.push_back(observer);
	}
}

void CompiledGraph::removeSceneObserver(scene::Graph::Observer* observer) {
	// Cycle through the list of observers and call the moved method
	for (ObserverList::iterator i = _sceneObservers.begin(); i != _sceneObservers.end(); ++i) {
		scene::Graph::Observer* registered = *i;

		if (registered == observer) {
			_sceneObservers.erase(i++);
			return; // Don't continue the loop, the iterator is obsolete
		}
	}
}

void CompiledGraph::sceneChanged ()
{
	for (ObserverList::iterator i = _sceneObservers.begin(); i != _sceneObservers.end(); ++i) {
		scene::Graph::Observer* observer = *i;
		observer->onSceneGraphChange();
	}
}

void CompiledGraph::notifyErase (scene::Instance* instance)
{
	for (EraseObservers::iterator i = m_eraseObservers.begin(); i != m_eraseObservers.end(); ++i) {
		(*i)->onErase(instance);
	}
}

void CompiledGraph::removeEraseObserver (scene::EraseObserver *observer)
{
	for (EraseObservers::iterator i = m_eraseObservers.begin(); i != m_eraseObservers.end(); ++i) {
		if (*i == observer) {
			m_eraseObservers.erase(i);
			break;
		}
	}
}

void CompiledGraph::addEraseObserver (scene::EraseObserver *observer)
{
	m_eraseObservers.push_back(observer);
}

scene::Node& CompiledGraph::root ()
{
	ASSERT_MESSAGE(!m_rootpath.empty(), "scenegraph root does not exist");
	return m_rootpath.top();
}

void CompiledGraph::insert_root (scene::Node& root)
{
	ASSERT_MESSAGE(m_rootpath.empty(), "scenegraph root already exists");

	root.IncRef();

	Node_traverseSubgraph(root, InstanceSubgraphWalker(this, scene::Path(), 0));

	m_rootpath.push(makeReference(root));
}
void CompiledGraph::erase_root ()
{
	ASSERT_MESSAGE(!m_rootpath.empty(), "scenegraph root does not exist");

	scene::Node & root = m_rootpath.top();

	m_rootpath.pop();

	Node_traverseSubgraph(root, UninstanceSubgraphWalker(this, scene::Path()));

	root.DecRef();
}
void CompiledGraph::boundsChanged ()
{
	m_boundsChanged();
}

void CompiledGraph::traverse (const Walker& walker)
{
	traverse_subgraph(walker, m_instances.begin());
}

void CompiledGraph::traverse_subgraph (const Walker& walker, const scene::Path& start)
{
	if (!m_instances.empty()) {
		traverse_subgraph(walker, m_instances.find(PathConstReference(start)));
	}
}

scene::Instance* CompiledGraph::find (const scene::Path& path)
{
	InstanceMap::iterator i = m_instances.find(PathConstReference(path));
	if (i == m_instances.end()) {
		return 0;
	}
	return (*i).second;
}

scene::Instance* CompiledGraph::find (scene::Node& node)
{
	scene::Path path;
	path.push(makeReference(node));
	return find(path);
}

void CompiledGraph::insert (scene::Instance* instance)
{
	m_instances.insert(InstanceMap::value_type(PathConstReference(instance->path()), instance));

	// Notify the graph tree model about the change
	sceneChanged();

	for (ObserverList::iterator i = _sceneObservers.begin(); i != _sceneObservers.end(); ++i) {
		(*i)->onSceneNodeInsert(*instance);
	}

	graph_tree_model_insert(_treeModel, *instance);
}

void CompiledGraph::erase (scene::Instance* instance)
{
	notifyErase(instance);

	// Notify the graph tree model about the change
	sceneChanged();

	for (ObserverList::iterator i = _sceneObservers.begin(); i != _sceneObservers.end(); ++i) {
		(*i)->onSceneNodeErase(*instance);
	}

	graph_tree_model_erase(_treeModel, *instance);

	m_instances.erase(PathConstReference(instance->path()));
}

SignalHandlerId CompiledGraph::addBoundsChangedCallback (const SignalHandler& boundsChanged)
{
	return m_boundsChanged.connectLast(boundsChanged);
}
void CompiledGraph::removeBoundsChangedCallback (SignalHandlerId id)
{
	m_boundsChanged.disconnect(id);
}

bool CompiledGraph::pre (const Walker& walker, const InstanceMap::iterator& i)
{
	return walker.pre(i->first, *i->second);
}

void CompiledGraph::post (const Walker& walker, const InstanceMap::iterator& i)
{
	walker.post(i->first, *i->second);
}

void CompiledGraph::traverse_subgraph (const Walker& walker, InstanceMap::iterator i)
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
