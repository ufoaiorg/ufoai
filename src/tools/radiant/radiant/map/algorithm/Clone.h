#ifndef CLONEALLWALKER_H_
#define CLONEALLWALKER_H_

#include "scenelib.h"

namespace map
{

inline scene::Cloneable* Node_getCloneable (scene::Node& node)
{
	return dynamic_cast<scene::Cloneable*> (&node);
}

inline scene::Node& node_clone (scene::Node& node)
{
	scene::Cloneable* cloneable = Node_getCloneable(node);
	if (cloneable != 0) {
		return cloneable->clone();
	}

	return *(new scene::Node);
}

class CloneAll: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		CloneAll (scene::Node& root) :
			m_path(makeReference(root))
		{
		}
		bool pre (scene::Node& node) const
		{
			if (node.isRoot()) {
				return false;
			}

			m_path.push(makeReference(node_clone(node)));
			m_path.top().get().IncRef();

			return true;
		}
		void post (scene::Node& node) const
		{
			if (node.isRoot()) {
				return;
			}

			Node_getTraversable(m_path.parent())->insert(m_path.top());

			m_path.top().get().DecRef();
			m_path.pop();
		}
};

inline scene::Node& Node_Clone (scene::Node& node)
{
	scene::Node& clone = node_clone(node);
	scene::Traversable* traversable = Node_getTraversable(node);
	if (traversable != 0) {
		traversable->traverse(CloneAll(clone));
	}
	return clone;
}

} // namespace map

#endif /*CLONEALLWALKER_H_*/
