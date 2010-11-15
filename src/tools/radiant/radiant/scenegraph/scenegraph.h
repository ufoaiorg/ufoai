#ifndef INCLUDED_SCENEGRAPH_H
#define INCLUDED_SCENEGRAPH_H

#include <map>
#include <vector>

#include "signal/signal.h"
#include "scenelib.h"

/** greebo: This is the actual implementation of the scene::Graph
 * 			defined in iscenegraph.h. This keeps track of all
 * 			the instances.
 */
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

	public:

		CompiledGraph (scene::Instantiable::Observer* observer);

		void addSceneChangedCallback (const SignalHandler& handler);

		// Triggers a call to all the connected Scene::Graph::Observers
		void sceneChanged (void);

		void notifyErase (scene::Instance* instance);

		void removeEraseObserver (scene::EraseObserver *observer);

		void addEraseObserver (scene::EraseObserver *observer);

		// Root node accessor methods
		scene::Node& root (void);
		void insert_root (scene::Node& root);
		void erase_root (void);

		// greebo: Emits the "bounds changed" signal to all connected observers
		// Note: these are the WorkZone and the SelectionSystem, AFAIK
		void boundsChanged (void);

		SignalHandlerId addBoundsChangedCallback (const SignalHandler& boundsChanged);
		void removeBoundsChangedCallback (SignalHandlerId id);

		void traverse (const Walker& walker);
		void traverse_subgraph (const Walker& walker, const scene::Path& start);

		scene::Instance* find (const scene::Path& path);
		scene::Instance* find (scene::Node& node);

		// scene::Instantiable::Observer implementation
		void insert (scene::Instance* instance);
		void erase (scene::Instance* instance);

	private:

		bool pre (const Walker& walker, const InstanceMap::iterator& i);
		void post (const Walker& walker, const InstanceMap::iterator& i);

		void traverse_subgraph (const Walker& walker, InstanceMap::iterator i);
}; // class CompiledGraph

#endif /*INCLUDED_SCENEGRAPH_H*/
