#include "scenelib.h"

namespace scene
{
	class AABBAccumulateWalker: public scene::Graph::Walker
	{
		AABB& m_aabb;
		mutable std::size_t m_depth;
	public:
		AABBAccumulateWalker (AABB& aabb) :
				m_aabb(aabb), m_depth(0)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (m_depth == 1) {
				m_aabb.includeAABB(instance.worldAABB());
			}
			return ++m_depth != 2;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			--m_depth;
		}
	};

	class TransformChangedWalker: public scene::Graph::Walker
	{
	public:
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			instance.transformChangedLocal();
			return true;
		}
	};

	class ParentSelectedChangedWalker: public scene::Graph::Walker
	{
	public:
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			instance.parentSelectedChanged();
			return true;
		}
	};

	class ChildSelectedWalker: public scene::Graph::Walker
	{
		bool& m_childSelected;
		mutable std::size_t m_depth;
	public:
		ChildSelectedWalker (bool& childSelected) :
				m_childSelected(childSelected), m_depth(0)
		{
			m_childSelected = false;
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (m_depth == 1 && !m_childSelected) {
				m_childSelected = instance.isSelected() || instance.childSelected();
			}
			return ++m_depth != 2;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			--m_depth;
		}
	};

	void Instance::evaluateTransform () const
	{
		if (m_transformChanged) {
			ASSERT_MESSAGE(!m_transformMutex, "re-entering transform evaluation");
			m_transformMutex = true;

			m_local2world = (m_parent != 0) ? m_parent->localToWorld() : Matrix4::getIdentity();
			TransformNode* transformNode = Node_getTransformNode(m_path.top());
			if (transformNode != 0) {
				matrix4_multiply_by_matrix4(m_local2world, transformNode->localToParent());
			}

			m_transformMutex = false;
			m_transformChanged = false;
		}
	}

	void Instance::evaluateChildBounds () const
	{
		if (m_childBoundsChanged) {
			ASSERT_MESSAGE(!m_childBoundsMutex, "re-entering bounds evaluation");
			m_childBoundsMutex = true;

			m_childBounds = AABB();

			GlobalSceneGraph().traverse_subgraph(AABBAccumulateWalker(m_childBounds), m_path);

			m_childBoundsMutex = false;
			m_childBoundsChanged = false;
		}
	}

	void Instance::evaluateBounds () const
	{
		if (m_boundsChanged) {
			ASSERT_MESSAGE(!m_boundsMutex, "re-entering bounds evaluation");
			m_boundsMutex = true;

			m_bounds = childBounds();

			const Bounded* bounded = dynamic_cast<const Bounded*>(this);
			if (bounded != 0) {
				m_bounds.includeAABB(aabb_for_oriented_aabb_safe(bounded->localAABB(), localToWorld()));
			}

			m_boundsMutex = false;
			m_boundsChanged = false;
		}
	}

	Instance::Instance (const scene::Path& path, Instance* parent) :
			m_path(path), m_parent(parent), m_local2world(Matrix4::getIdentity()), m_transformChanged(true), m_transformMutex(false), m_boundsChanged(
					true), m_boundsMutex(false), m_childBoundsChanged(true), m_childBoundsMutex(false), m_isSelectedChanged(true), m_childSelectedChanged(
					true), m_parentSelectedChanged(true), _filtered(false)
	{
		ASSERT_MESSAGE((parent == 0) == (path.size() == 1), "instance has invalid parent");
	}

	Instance::~Instance ()
	{
	}

	const scene::Path& Instance::path () const
	{
		return m_path;
	}

	bool Instance::visible () const
	{
		return m_path.top().get().visible();
	}

	const Matrix4& Instance::localToWorld () const
	{
		evaluateTransform();
		return m_local2world;
	}

	void Instance::transformChangedLocal ()
	{
		ASSERT_NOTNULL(m_parent);
		m_transformChanged = true;
		m_boundsChanged = true;
		m_childBoundsChanged = true;
		m_transformChangedCallback();
	}

	void Instance::transformChanged ()
	{
		GlobalSceneGraph().traverse_subgraph(TransformChangedWalker(), m_path);
		boundsChanged();
	}

	void Instance::setTransformChangedCallback (const Callback& callback)
	{
		m_transformChangedCallback = callback;
	}

	const AABB& Instance::worldAABB () const
	{
		evaluateBounds();
		return m_bounds;
	}

	const AABB& Instance::childBounds () const
	{
		evaluateChildBounds();
		return m_childBounds;
	}

	void Instance::boundsChanged ()
	{
		m_boundsChanged = true;
		m_childBoundsChanged = true;
		if (m_parent != 0) {
			m_parent->boundsChanged();
		}
		GlobalSceneGraph().boundsChanged();
	}

	void Instance::childSelectedChanged ()
	{
		m_childSelectedChanged = true;
		m_childSelectedChangedCallback();
		if (m_parent != 0) {
			m_parent->childSelectedChanged();
		}
	}

	bool Instance::childSelected () const
	{
		if (m_childSelectedChanged) {
			m_childSelectedChanged = false;
			GlobalSceneGraph().traverse_subgraph(ChildSelectedWalker(m_childSelected), m_path);
		}
		return m_childSelected;
	}

	void Instance::setChildSelectedChangedCallback (const Callback& callback)
	{
		m_childSelectedChangedCallback = callback;
	}

	void Instance::selectedChanged ()
	{
		m_isSelectedChanged = true;
		if (m_parent != 0) {
			m_parent->childSelectedChanged();
		}
		GlobalSceneGraph().traverse_subgraph(ParentSelectedChangedWalker(), m_path);
	}

	bool Instance::isSelected () const
	{
		if (m_isSelectedChanged) {
			m_isSelectedChanged = false;
			const Selectable* selectable = Instance_getSelectable(*this);
			m_isSelected = selectable != 0 && selectable->isSelected();
		}
		return m_isSelected;
	}

	void Instance::parentSelectedChanged ()
	{
		m_parentSelectedChanged = true;
	}

	bool Instance::parentSelected () const
	{
		if (m_parentSelectedChanged) {
			m_parentSelectedChanged = false;
			m_parentSelected = m_parent != 0 && (m_parent->isSelected() || m_parent->parentSelected());
		}
		return m_parentSelected;
	}

	Instance* Instance::getParent () const
	{
		return m_parent;
	}
}
