#ifndef BASICCONTAINER_H_
#define BASICCONTAINER_H_

#include "scenelib.h"

namespace map {

/**
 * greebo: This is a temporary container (node) used during map object import.
 */
class BasicContainer: public scene::Node, public scene::Traversable
{
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts (void)
				{
				}
				NodeTypeCastTable& get (void)
				{
					return m_casts;
				}
		};

		TraversableNodeSet m_traverse;
	public:

		typedef LazyStatic<TypeCasts> StaticTypeCasts;

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


		BasicContainer () :
			scene::Node(this, StaticTypeCasts::instance().get())
		{
		}

		scene::Node& node (void)
		{
			return *this;
		}
};

} // namespace map

#endif /*BASICCONTAINER_H_*/
