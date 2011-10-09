#ifndef FACEINSTANCE_H_
#define FACEINSTANCE_H_

#include "math/Plane3.h"
#include "math/line.h"
#include "render.h"
#include "selectionlib.h"

#include "SelectableComponents.h"
#include "VertexSelection.h"
#include "Face.h"

typedef const Plane3* PlanePointer;
typedef PlanePointer* PlanesIterator;

class FaceInstance
{
	private:
		Face* m_face;
		ObservedSelectable m_selectable;
		ObservedSelectable m_selectableVertices;
		ObservedSelectable m_selectableEdges;
		SelectionChangeCallback m_selectionChanged;

		VertexSelection m_vertexSelection;
		VertexSelection m_edgeSelection;

	public:
		FaceInstance (Face& face, const SelectionChangeCallback& observer);
		FaceInstance (const FaceInstance& other);
		FaceInstance& operator= (const FaceInstance& other);
		Face& getFace ();
		const Face& getFace () const;
		Face* getFacePtr () const;
		void selectedChanged (const Selectable& selectable);
		typedef MemberCaller1<FaceInstance, const Selectable&, &FaceInstance::selectedChanged> SelectedChangedCaller;

		bool selectedVertices () const;
		bool selectedEdges () const;
		bool isSelected () const;
		void invertSelected();

		bool selectedComponents () const;
		bool selectedComponents (SelectionSystem::EComponentMode mode) const;
		void setSelected (SelectionSystem::EComponentMode mode, bool select);

		template<typename Functor>
		void SelectedVertices_foreach (Functor functor) const;

		template<typename Functor>
		void SelectedEdges_foreach (Functor functor) const;

		template<typename Functor>
		void SelectedFaces_foreach (Functor functor) const;

		template<typename Functor>
		void SelectedComponents_foreach (Functor functor) const;

		void iterate_selected (AABB& aabb) const;

		class RenderablePointVectorPushBack
		{
				RenderablePointVector& m_points;
			public:
				RenderablePointVectorPushBack (RenderablePointVector& points) :
					m_points(points)
				{
				}
				void operator() (const Vector3& point) const
				{
					const Colour4b colour_selected(0, 0, 255, 255);
					m_points.push_back(PointVertex(point, colour_selected));
				}
		};
		void iterate_selected (RenderablePointVector& points) const;

		bool intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const;

		void render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;

		void testSelect (SelectionTest& test, SelectionIntersection& best);
		void testSelect (Selector& selector, SelectionTest& test);
		void testSelect_centroid (Selector& selector, SelectionTest& test);

		void selectPlane (Selector& selector, const Line& line, PlanesIterator first, PlanesIterator last,
				const PlaneCallback& selectedPlaneCallback);
		void selectReversedPlane (Selector& selector, const SelectedPlanes& selectedPlanes);

		void transformComponents (const Matrix4& matrix);

		void snapto (float snap);

		void snapComponents (float snap);
		void update_move_planepts_vertex (std::size_t index);
		void update_move_planepts_vertex2 (std::size_t index, std::size_t other);
		void update_selection_vertex ();
		void select_vertex (std::size_t index, bool select);
		bool selected_vertex (std::size_t index) const;

		void update_move_planepts_edge (std::size_t index);
		void update_selection_edge ();
		void select_edge (std::size_t index, bool select);
		bool selected_edge (std::size_t index) const;
		const Vector3& centroid () const;
		void connectivityChanged ();
};

typedef std::vector<FaceInstance> FaceInstances;
typedef SelectionList<FaceInstance> FaceInstancesList;

class FaceInstanceSet
{
	public:
		FaceInstancesList m_faceInstances;

		/** @brief Insert a new face instance into the selected faces list */
		void insert (FaceInstance& faceInstance)
		{
			m_faceInstances.append(faceInstance);
		}
		/** @brief Removes a face instance from the selected faces list */
		void erase (FaceInstance& faceInstance)
		{
			m_faceInstances.erase(faceInstance);
		}

		template<typename Functor>
		void foreach (Functor functor)
		{
			for (FaceInstancesList::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
				functor(*(*i));
			}
		}

		/** @brief Wipes the list of selected faces */
		bool empty () const
		{
			return m_faceInstances.empty();
		}

		/** @return The last entry of all the selected faces */
		FaceInstance& last () const
		{
			return m_faceInstances.back();
		}

		std::size_t size() const {
			return m_faceInstances.size();
		}
};

extern FaceInstanceSet g_SelectedFaceInstances;

#endif /*FACEINSTANCE_H_*/
