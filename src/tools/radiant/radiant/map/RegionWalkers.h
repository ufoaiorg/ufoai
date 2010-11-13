#ifndef REGIONWALKERS_H_
#define REGIONWALKERS_H_

#include "scenelib.h"

namespace map {

inline void exclude_node (scene::Node& node, bool exclude)
{
	if (exclude)
		node.enable(scene::Node::eExcluded);
	else
		node.disable(scene::Node::eExcluded);
}

class ExcludeAllWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeAllWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), m_exclude);

			return true;
		}
};

inline void Scene_Exclude_All (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeAllWalker(exclude));
}

class ExcludeSelectedWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeSelectedWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), (instance.isSelected() || instance.childSelected() || instance.parentSelected())
					== m_exclude);
			return true;
		}
};

inline void Scene_Exclude_Selected (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeSelectedWalker(exclude));
}

class ExcludeRegionedWalker: public scene::Graph::Walker
{
		bool m_exclude;
	public:
		ExcludeRegionedWalker (bool exclude) :
			m_exclude(exclude)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			exclude_node(path.top(), !((aabb_intersects_aabb(instance.worldAABB(), AABB::createFromMinMax(
					GlobalMap().getRegionMins(), GlobalMap().getRegionMaxs())) != 0) ^ m_exclude));

			return true;
		}
};

inline void Scene_Exclude_Region (bool exclude)
{
	GlobalSceneGraph().traverse(ExcludeRegionedWalker(exclude));
}

} // namespace map

#endif /*REGIONWALKERS_H_*/
