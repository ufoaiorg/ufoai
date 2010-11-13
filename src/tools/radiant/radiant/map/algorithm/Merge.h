#ifndef MERGE_H_
#define MERGE_H_

#include "iscenegraph.h"
#include "ientity.h"
#include "scenelib.h"

namespace map {

class MapMergeAll: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		MapMergeAll (const scene::Path& root) :
			m_path(root)
		{
		}
		bool pre (scene::Node& node) const
		{
			Node_getTraversable(m_path.top())->insert(node);
			m_path.push(makeReference(node));
			selectPath(m_path, true);
			return false;
		}
		void post (scene::Node& node) const
		{
			m_path.pop();
		}
};

class MapMergeEntities: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		MapMergeEntities (const scene::Path& root) :
			m_path(root)
		{
		}
		bool pre (scene::Node& node) const
		{
			if (node_is_worldspawn(node)) {
				scene::Node* world_node = GlobalMap().findWorldspawn();
				if (world_node == 0) {
					GlobalMap().setWorldspawn(&node);
					Node_getTraversable(m_path.top().get())->insert(node);
					m_path.push(makeReference(node));
					Node_getTraversable(node)->traverse(SelectChildren(m_path));
				} else {
					m_path.push(makeReference(*world_node));
					Node_getTraversable(node)->traverse(MapMergeAll(m_path));
				}
			} else {
				Node_getTraversable(m_path.top())->insert(node);
				m_path.push(makeReference(node));
				if (node_is_group(node)) {
					Node_getTraversable(node)->traverse(SelectChildren(m_path));
				} else {
					selectPath(m_path, true);
				}
			}
			return false;
		}
		void post (scene::Node& node) const
		{
			m_path.pop();
		}
};

/// Merges the map graph rooted at \p node into the global scene-graph.
inline void MergeMap (scene::Node& node)
{
	Node_getTraversable(node)->traverse(MapMergeEntities(scene::Path(makeReference(GlobalSceneGraph().root()))));
}

} // namespace map

#endif /*MERGE_H_*/
