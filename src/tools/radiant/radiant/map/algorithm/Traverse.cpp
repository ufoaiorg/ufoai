#include "Traverse.h"

#include "scenelib.h"

namespace map {

class IncludeSelectedWalker: public scene::Traversable::Walker
{
		const scene::Traversable::Walker& m_walker;
		mutable std::size_t m_selected;
		mutable bool m_skip;

		bool selectedParent () const
		{
			return m_selected != 0;
		}
	public:
		IncludeSelectedWalker (const scene::Traversable::Walker& walker) :
			m_walker(walker), m_selected(0), m_skip(false)
		{
		}
		bool pre (scene::Node& node) const
		{
			// include node if:
			// node is not a 'root' AND ( node is selected OR any child of node is selected OR any parent of node is selected )
			if (!node.isRoot() && (Node_selectedDescendant(node) || selectedParent())) {
				if (Node_instanceSelected(node)) {
					++m_selected;
				}
				m_walker.pre(node);
				return true;
			} else {
				m_skip = true;
				return false;
			}
		}
		void post (scene::Node& node) const
		{
			if (m_skip) {
				m_skip = false;
			} else {
				if (Node_instanceSelected(node)) {
					--m_selected;
				}
				m_walker.post(node);
			}
		}
};


void Map_Traverse_Selected (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		// Create a wrapper which calls the nodeExporter only for selected and related items
		traversable->traverse(IncludeSelectedWalker(walker));
	}
}

void Map_Traverse (scene::Node& root, const scene::Traversable::Walker& walker)
{
	scene::Traversable* traversable = Node_getTraversable(root);
	if (traversable != 0) {
		traversable->traverse(walker);
	}
}

} // namespace map
