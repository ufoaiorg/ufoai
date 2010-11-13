#ifndef BASICCONTAINER_H_
#define BASICCONTAINER_H_

#include "scenelib.h"

namespace map {

/**
 * greebo: This is a temporary container (node) used during map object import.
 */
class BasicContainer: public scene::Node
{
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts (void)
				{
					NodeContainedCast<BasicContainer, scene::Traversable>::install(m_casts);
				}
				NodeTypeCastTable& get (void)
				{
					return m_casts;
				}
		};

		TraversableNodeSet m_traverse;
	public:

		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		scene::Traversable& get (NullType<scene::Traversable> )
		{
			return m_traverse;
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
