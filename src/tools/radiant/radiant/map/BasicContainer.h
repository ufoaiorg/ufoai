#ifndef BASICCONTAINER_H_
#define BASICCONTAINER_H_

#include "scenelib.h"

namespace map {

/**
 * greebo: This is a temporary container (node) used during map object import.
 */
class BasicContainer: public scene::Node, public scene::Traversable
{
		TraversableNodeSet m_traverse;
	public:

		// Traversable implementation
		void insert(Node& node) {
			m_traverse.insert(node);
		}

		void erase(Node& node) {
			m_traverse.erase(node);
		}

		void traverse(const Walker& walker) {
			m_traverse.traverse(walker);
		}

		bool empty() const {
			return m_traverse.empty();
		}


		BasicContainer ()
		{
		}

		scene::Node& node (void)
		{
			return *this;
		}
};

} // namespace map

#endif /*BASICCONTAINER_H_*/
