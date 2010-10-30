#ifndef BRUSHINSTANCE_H_
#define BRUSHINSTANCE_H_

#include "renderable.h"
#include "irender.h"
#include "scenelib.h"
#include "selectable.h"

#include "BrushClass.h"
#include "FaceInstance.h"
#include "EdgeInstance.h"
#include "VertexInstance.h"
#include "BrushClipPlane.h"

class BrushInstanceVisitor
{
	public:
		virtual ~BrushInstanceVisitor ()
		{
		}
		virtual void visit (FaceInstance& face) const = 0;
};

class BrushInstance: public BrushObserver,
		public scene::Instance,
		public Selectable,
		public Renderable,
		public SelectionTestable,
		public ComponentSelectionTestable,
		public ComponentEditable,
		public ComponentSnappable,
		public PlaneSelectable,
		public LightCullable
{
		class TypeCasts
		{
				InstanceTypeCastTable m_casts;
			public:
				TypeCasts ()
				{
					InstanceStaticCast<BrushInstance, Selectable>::install(m_casts);
					InstanceContainedCast<BrushInstance, Bounded>::install(m_casts);
					InstanceContainedCast<BrushInstance, Cullable>::install(m_casts);
					InstanceStaticCast<BrushInstance, Renderable>::install(m_casts);
					InstanceStaticCast<BrushInstance, SelectionTestable>::install(m_casts);
					InstanceStaticCast<BrushInstance, ComponentSelectionTestable>::install(m_casts);
					InstanceStaticCast<BrushInstance, ComponentEditable>::install(m_casts);
					InstanceStaticCast<BrushInstance, ComponentSnappable>::install(m_casts);
					InstanceStaticCast<BrushInstance, PlaneSelectable>::install(m_casts);
					InstanceIdentityCast<BrushInstance>::install(m_casts);
					InstanceContainedCast<BrushInstance, Transformable>::install(m_casts);
				}
				InstanceTypeCastTable& get ()
				{
					return m_casts;
				}
		};

		Brush& m_brush;

		FaceInstances m_faceInstances;

		typedef std::vector<EdgeInstance> EdgeInstances;
		EdgeInstances m_edgeInstances;
		typedef std::vector<VertexInstance> VertexInstances;
		VertexInstances m_vertexInstances;

		ObservedSelectable m_selectable;

		mutable RenderableWireframe m_render_wireframe;
		mutable RenderablePointVector m_render_selected;
		mutable AABB m_aabb_component;
		mutable Array<PointVertex> m_faceCentroidPointsCulled;
		RenderablePointArray m_render_faces_wireframe;
		mutable bool m_viewChanged; // requires re-evaluation of view-dependent cached data

		BrushClipPlane m_clipPlane;

		static Shader* m_state_selpoint;

		const LightList* m_lightList;

		TransformModifier m_transform;

		BrushInstance (const BrushInstance& other); // NOT COPYABLE
		BrushInstance& operator= (const BrushInstance& other); // NOT ASSIGNABLE
	public:
		static Counter* m_counter;

		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		void lightsChanged ()
		{
			m_lightList->lightsChanged();
		}
		typedef MemberCaller<BrushInstance, &BrushInstance::lightsChanged> LightsChangedCaller;

		STRING_CONSTANT(Name, "BrushInstance");

		BrushInstance (const scene::Path& path, scene::Instance* parent, Brush& brush) :
			Instance(path, parent, this, StaticTypeCasts::instance().get()), m_brush(brush), m_selectable(
					SelectedChangedCaller(*this)), m_render_selected(GL_POINTS), m_render_faces_wireframe(
					m_faceCentroidPointsCulled, GL_POINTS), m_viewChanged(false), m_transform(
					Brush::TransformChangedCaller(m_brush), ApplyTransformCaller(*this))
		{
			m_brush.instanceAttach(Instance::path());
			m_brush.attach(*this);
			m_counter->increment();

			m_lightList = &GlobalShaderCache().attach(*this);
			m_brush.m_lightsChanged = LightsChangedCaller(*this); ///\todo Make this work with instancing.

			Instance::setTransformChangedCallback(LightsChangedCaller(*this));
		}
		~BrushInstance ()
		{
			Instance::setTransformChangedCallback(Callback());

			m_brush.m_lightsChanged = Callback();
			GlobalShaderCache().detach(*this);

			m_counter->decrement();
			m_brush.detach(*this);
			m_brush.instanceDetach(Instance::path());
		}

		Brush& getBrush ()
		{
			return m_brush;
		}
		const Brush& getBrush () const
		{
			return m_brush;
		}

		Bounded& get (NullType<Bounded> )
		{
			return m_brush;
		}
		Cullable& get (NullType<Cullable> )
		{
			return m_brush;
		}
		Transformable& get (NullType<Transformable> )
		{
			return m_transform;
		}

		void selectedChanged (const Selectable& selectable)
		{
			GlobalSelectionSystem().getObserver(SelectionSystem::ePrimitive)(selectable);
			GlobalSelectionSystem().onSelectedChanged(*this, selectable);

			Instance::selectedChanged();
		}
		typedef MemberCaller1<BrushInstance, const Selectable&, &BrushInstance::selectedChanged> SelectedChangedCaller;

		void selectedChangedComponent (const Selectable& selectable)
		{
			GlobalSelectionSystem().getObserver(SelectionSystem::eComponent)(selectable);
			GlobalSelectionSystem().onComponentSelection(*this, selectable);
		}
		typedef MemberCaller1<BrushInstance, const Selectable&, &BrushInstance::selectedChangedComponent>
				SelectedChangedComponentCaller;

		const BrushInstanceVisitor& forEachFaceInstance (const BrushInstanceVisitor& visitor)
		{
			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				visitor.visit(*i);
			}
			return visitor;
		}

		static void constructStatic ()
		{
			m_state_selpoint = GlobalShaderCache().capture("$SELPOINT");
		}
		static void destroyStatic ()
		{
			GlobalShaderCache().release("$SELPOINT");
		}

		void clear ()
		{
			m_faceInstances.clear();
		}
		void reserve (std::size_t size)
		{
			m_faceInstances.reserve(size);
		}

		void push_back (Face& face)
		{
			m_faceInstances.push_back(FaceInstance(face, SelectedChangedComponentCaller(*this)));
		}
		void pop_back ()
		{
			ASSERT_MESSAGE(!m_faceInstances.empty(), "erasing invalid element");
			m_faceInstances.pop_back();
		}
		void erase (std::size_t index)
		{
			ASSERT_MESSAGE(index < m_faceInstances.size(), "erasing invalid element");
			m_faceInstances.erase(m_faceInstances.begin() + index);
		}
		void connectivityChanged ()
		{
			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).connectivityChanged();
			}
		}

		void edge_clear ()
		{
			m_edgeInstances.clear();
		}
		void edge_push_back (SelectableEdge& edge)
		{
			m_edgeInstances.push_back(EdgeInstance(m_faceInstances, edge));
		}

		void vertex_clear ()
		{
			m_vertexInstances.clear();
		}
		void vertex_push_back (SelectableVertex& vertex)
		{
			m_vertexInstances.push_back(VertexInstance(m_faceInstances, vertex));
		}

		void DEBUG_verify () const
		{
			ASSERT_MESSAGE(m_faceInstances.size() == m_brush.DEBUG_size(), "FATAL: mismatch");
		}

		bool isSelected () const
		{
			return m_selectable.isSelected();
		}
		void setSelected (bool select)
		{
			m_selectable.setSelected(select);
		}

		void update_selected () const
		{
			m_render_selected.clear();
			for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				if ((*i).getFace().contributes()) {
					(*i).iterate_selected(m_render_selected);
				}
			}
		}

		void evaluateViewDependent (const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			if (m_viewChanged) {
				m_viewChanged = false;

				bool faces_visible[c_brush_maxFaces];
				{
					bool* j = faces_visible;
					ContentsFlagsValue val = m_brush.getFlags();
					if (GlobalFilterSystem().isVisible("contentflags", val.m_contentFlags)) {
						for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i, ++j) {
							// Check if face is filtered before adding to visibility matrix
							if (GlobalFilterSystem().isVisible("texture", i->getFace().GetShader()) &&
								GlobalFilterSystem().isVisible("surfaceflags", i->getFace().GetFlags().m_surfaceFlags))
								*j = i->intersectVolume(volume, localToWorld);
							else
								*j = false;
						}
					} else {
						*j = false;
					}
				}

				m_brush.update_wireframe(m_render_wireframe, faces_visible);
				m_brush.update_faces_wireframe(m_faceCentroidPointsCulled, faces_visible);
			}
		}

		void renderComponentsSelected (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			m_brush.evaluateBRep();

			update_selected();
			if (!m_render_selected.empty()) {
				renderer.Highlight(Renderer::ePrimitive, false);
				renderer.SetState(m_state_selpoint, Renderer::eWireframeOnly);
				renderer.SetState(m_state_selpoint, Renderer::eFullMaterials);
				renderer.addRenderable(m_render_selected, localToWorld);
			}
		}

		void renderComponents (Renderer& renderer, const VolumeTest& volume) const
		{
			m_brush.evaluateBRep();

			const Matrix4& localToWorld = Instance::localToWorld();

			renderer.SetState(m_brush.m_state_point, Renderer::eWireframeOnly);
			renderer.SetState(m_brush.m_state_point, Renderer::eFullMaterials);

			if (volume.fill() && GlobalSelectionSystem().ComponentMode() == SelectionSystem::eFace) {
				evaluateViewDependent(volume, localToWorld);
				renderer.addRenderable(m_render_faces_wireframe, localToWorld);
			} else {
				m_brush.renderComponents(GlobalSelectionSystem().ComponentMode(), renderer, volume, localToWorld);
			}
		}

		void renderClipPlane (Renderer& renderer, const VolumeTest& volume) const
		{
			if (GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip && isSelected()) {
				m_clipPlane.render(renderer, volume, localToWorld());
			}
		}

		void renderCommon (Renderer& renderer, const VolumeTest& volume) const
		{
			bool componentMode = GlobalSelectionSystem().Mode() == SelectionSystem::eComponent;

			if (componentMode && isSelected()) {
				renderComponents(renderer, volume);
			}

			if (parentSelected()) {
				if (!componentMode) {
					renderer.Highlight(Renderer::eFace);
				}
				renderer.Highlight(Renderer::ePrimitive);
			}
		}

		void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			//renderCommon(renderer, volume);

			m_lightList->evaluateLights();

			for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				renderer.setLights((*i).m_lights);
				(*i).render(renderer, volume, localToWorld);
			}

			renderComponentsSelected(renderer, volume, localToWorld);
		}

		void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			//renderCommon(renderer, volume);

			evaluateViewDependent(volume, localToWorld);

			if (m_render_wireframe.m_size != 0) {
				renderer.addRenderable(m_render_wireframe, localToWorld);
			}

			renderComponentsSelected(renderer, volume, localToWorld);
		}

		void renderSolid (Renderer& renderer, const VolumeTest& volume) const
		{
			m_brush.evaluateBRep();

			renderClipPlane(renderer, volume);

			renderSolid(renderer, volume, localToWorld());
		}

		void renderWireframe (Renderer& renderer, const VolumeTest& volume) const
		{
			m_brush.evaluateBRep();

			renderClipPlane(renderer, volume);

			renderWireframe(renderer, volume, localToWorld());
		}

		void viewChanged () const
		{
			m_viewChanged = true;
		}

		void testSelect (Selector& selector, SelectionTest& test)
		{
			test.BeginMesh(localToWorld());

			SelectionIntersection best;
			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).testSelect(test, best);
			}
			if (best.valid()) {
				selector.addIntersection(best);
			}
		}

		bool isSelectedComponents () const
		{
			for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				if ((*i).selectedComponents()) {
					return true;
				}
			}
			return false;
		}
		void setSelectedComponents (bool select, SelectionSystem::EComponentMode mode)
		{
			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).setSelected(mode, select);
			}
		}
		void testSelectComponents (Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode)
		{
			test.BeginMesh(localToWorld());

			switch (mode) {
			case SelectionSystem::eVertex: {
				for (VertexInstances::iterator i = m_vertexInstances.begin(); i != m_vertexInstances.end(); ++i) {
					(*i).testSelect(selector, test);
				}
			}
				break;
			case SelectionSystem::eEdge: {
				for (EdgeInstances::iterator i = m_edgeInstances.begin(); i != m_edgeInstances.end(); ++i) {
					(*i).testSelect(selector, test);
				}
			}
				break;
			case SelectionSystem::eFace: {
				if (test.getVolume().fill()) {
					for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
						(*i).testSelect(selector, test);
					}
				} else {
					for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
						(*i).testSelect_centroid(selector, test);
					}
				}
			}
				break;
			default:
				break;
			}
		}

		void selectPlanes (Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback)
		{
			test.BeginMesh(localToWorld());

			PlanePointer brushPlanes[c_brush_maxFaces];
			PlanesIterator j = brushPlanes;

			for (Brush::const_iterator i = m_brush.begin(); i != m_brush.end(); ++i) {
				*j++ = &(*i)->plane3();
			}

			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).selectPlane(selector, Line(test.getNear(), test.getFar()), brushPlanes, j, selectedPlaneCallback);
			}
		}
		void selectReversedPlanes (Selector& selector, const SelectedPlanes& selectedPlanes)
		{
			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).selectReversedPlane(selector, selectedPlanes);
			}
		}

		void transformComponents (const Matrix4& matrix)
		{
			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).transformComponents(matrix);
			}
		}
		const AABB& getSelectedComponentsBounds () const
		{
			m_aabb_component = AABB();

			for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).iterate_selected(m_aabb_component);
			}

			return m_aabb_component;
		}

		void snapComponents (float snap)
		{
			for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				(*i).snapComponents(snap);
			}
		}
		void evaluateTransform ()
		{
			Matrix4 matrix(m_transform.calculateTransform());

			if (m_transform.getType() == TRANSFORM_PRIMITIVE) {
				m_brush.transform(matrix);
			} else {
				transformComponents(matrix);
			}
		}
		void applyTransform ()
		{
			m_brush.revertTransform();
			evaluateTransform();
			m_brush.freezeTransform();
		}
		typedef MemberCaller<BrushInstance, &BrushInstance::applyTransform> ApplyTransformCaller;

		void setClipPlane (const Plane3& plane)
		{
			m_clipPlane.setPlane(m_brush, plane);
		}
};

inline BrushInstance* Instance_getBrush (scene::Instance& instance)
{
	return InstanceTypeCast<BrushInstance>::cast(instance);
}

#endif /*BRUSHINSTANCE_H_*/
