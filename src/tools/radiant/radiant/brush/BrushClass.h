#ifndef BRUSH_BRUSH_H_
#define BRUSH_BRUSH_H_

#include "scenelib.h"
#include "cullable.h"
#include "editable.h"
#include "nameable.h"

#include "container/array.h"

#include "Face.h"
#include "SelectableComponents.h"
#include "RenderableWireFrame.h"

extern bool g_brush_texturelock_enabled;

inline double quantiseInteger (double f)
{
	return float_to_integer(f);
}

inline double quantiseFloating (double f)
{
	return float_snapped(f, 1.f / (1 << 16));
}

/// \brief Returns true if 'self' takes priority when building brush b-rep.
inline bool plane3_inside (const Plane3& self, const Plane3& other)
{
	if (vector3_equal_epsilon(self.normal(), other.normal(), 0.001)) {
		return self.dist() < other.dist();
	}
	return true;
}

/// \brief Returns the unique-id of the edge adjacent to \p faceVertex in the edge-pair for the set of \p faces.
inline FaceVertexId next_edge (const Faces& faces, FaceVertexId faceVertex)
{
	std::size_t adjacent_face = faces[faceVertex.getFace()]->getWinding()[faceVertex.getVertex()].adjacent;
	std::size_t adjacent_vertex = Winding_FindAdjacent(faces[adjacent_face]->getWinding(), faceVertex.getFace());

	ASSERT_MESSAGE(adjacent_vertex != c_brush_maxFaces, "connectivity data invalid");
	if (adjacent_vertex == c_brush_maxFaces) {
		return faceVertex;
	}

	return FaceVertexId(adjacent_face, adjacent_vertex);
}

/// \brief Returns the unique-id of the vertex adjacent to \p faceVertex in the vertex-ring for the set of \p faces.
inline FaceVertexId next_vertex (const Faces& faces, FaceVertexId faceVertex)
{
	FaceVertexId nextEdge = next_edge(faces, faceVertex);
	return FaceVertexId(nextEdge.getFace(), Winding_next(faces[nextEdge.getFace()]->getWinding(), nextEdge.getVertex()));
}

typedef std::size_t faceIndex_t;

struct EdgeFaces
{
		faceIndex_t first;
		faceIndex_t second;

		EdgeFaces () :
			first(c_brush_maxFaces), second(c_brush_maxFaces)
		{
		}
		EdgeFaces (const faceIndex_t _first, const faceIndex_t _second) :
			first(_first), second(_second)
		{
		}
};

class BrushObserver
{
	public:
		virtual ~BrushObserver ()
		{
		}
		virtual void reserve (std::size_t size) = 0;
		virtual void clear () = 0;
		virtual void push_back (Face& face) = 0;
		virtual void pop_back () = 0;
		virtual void erase (std::size_t index) = 0;
		virtual void connectivityChanged () = 0;

		virtual void edge_clear () = 0;
		virtual void edge_push_back (SelectableEdge& edge) = 0;

		virtual void vertex_clear () = 0;
		virtual void vertex_push_back (SelectableVertex& vertex) = 0;

		virtual void DEBUG_verify () const = 0;
};

class BrushVisitor
{
	public:
		virtual ~BrushVisitor ()
		{
		}
		virtual void visit (Face& face) const = 0;
};

class Brush: public TransformNode,
		public Bounded,
		public Cullable,
		public Snappable,
		public Undoable,
		public FaceObserver,
		public Nameable
{
	private:
		scene::Node* m_node;
		typedef UniqueSet<BrushObserver*> Observers;
		Observers m_observers;
		UndoObserver* m_undoable_observer;
		MapFile* m_map;

		// state
		Faces m_faces;

		// cached data compiled from state
		Array<PointVertex> m_faceCentroidPoints;
		RenderablePointArray m_render_faces;

		Array<PointVertex> m_uniqueVertexPoints;
		typedef std::vector<SelectableVertex> SelectableVertices;
		SelectableVertices m_select_vertices;
		RenderablePointArray m_render_vertices;

		Array<PointVertex> m_uniqueEdgePoints;
		typedef std::vector<SelectableEdge> SelectableEdges;
		SelectableEdges m_select_edges;
		RenderablePointArray m_render_edges;

		Array<EdgeRenderIndices> m_edge_indices;
		Array<EdgeFaces> m_edge_faces;

		AABB m_aabb_local;

		Callback m_evaluateTransform;
		Callback m_boundsChanged;

		mutable bool m_planeChanged; // b-rep evaluation required
		mutable bool m_transformChanged; // transform evaluation required

	public:
		STRING_CONSTANT(Name, "Brush");

		Callback m_lightsChanged;

		// static data
		static Shader* m_state_point;

		static double m_maxWorldCoord;

		Brush (scene::Node& node, const Callback& evaluateTransform, const Callback& boundsChanged) :
			m_node(&node), m_undoable_observer(0), m_map(0), m_render_faces(m_faceCentroidPoints, GL_POINTS),
					m_render_vertices(m_uniqueVertexPoints, GL_POINTS), m_render_edges(m_uniqueEdgePoints, GL_POINTS),
					m_evaluateTransform(evaluateTransform), m_boundsChanged(boundsChanged), m_planeChanged(false),
					m_transformChanged(false)
		{
			planeChanged();
		}
		Brush (const Brush& other, scene::Node& node, const Callback& evaluateTransform, const Callback& boundsChanged) :
			m_node(&node), m_undoable_observer(0), m_map(0), m_render_faces(m_faceCentroidPoints, GL_POINTS),
					m_render_vertices(m_uniqueVertexPoints, GL_POINTS), m_render_edges(m_uniqueEdgePoints, GL_POINTS),
					m_evaluateTransform(evaluateTransform), m_boundsChanged(boundsChanged), m_planeChanged(false),
					m_transformChanged(false)
		{
			copy(other);
		}
		Brush (const Brush& other) :
			TransformNode(other), Bounded(other), Cullable(other), Snappable(other), Undoable(other), FaceObserver(
					other), Nameable(other), m_node(0), m_undoable_observer(0), m_map(0),
					m_render_faces(m_faceCentroidPoints, GL_POINTS),
					m_render_vertices(m_uniqueVertexPoints, GL_POINTS), m_render_edges(m_uniqueEdgePoints, GL_POINTS),
					m_planeChanged(false), m_transformChanged(false)
		{
			copy(other);
		}
		~Brush ()
		{
			ASSERT_MESSAGE(m_observers.empty(), "Brush::~Brush: observers still attached");
		}

		// assignment not supported
		Brush& operator= (const Brush& other);

		void attach (BrushObserver& observer)
		{
			for (Faces::iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				observer.push_back(*(*i));
			}

			for (SelectableEdges::iterator i = m_select_edges.begin(); i != m_select_edges.end(); ++i) {
				observer.edge_push_back(*i);
			}

			for (SelectableVertices::iterator i = m_select_vertices.begin(); i != m_select_vertices.end(); ++i) {
				observer.vertex_push_back(*i);
			}

			m_observers.insert(&observer);
		}
		void detach (BrushObserver& observer)
		{
			m_observers.erase(&observer);
		}

		void forEachFace (const BrushVisitor& visitor) const
		{
			for (Faces::const_iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				visitor.visit(*(*i));
			}
		}

		void forEachFace_instanceAttach (MapFile* map) const
		{
			for (Faces::const_iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				(*i)->instanceAttach(map);
			}
		}
		void forEachFace_instanceDetach (MapFile* map) const
		{
			for (Faces::const_iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				(*i)->instanceDetach(map);
			}
		}

		InstanceCounter m_instanceCounter;
		void instanceAttach (const scene::Path& path)
		{
			if (++m_instanceCounter.m_count == 1) {
				m_map = path_find_mapfile(path.begin(), path.end());
				m_undoable_observer = GlobalUndoSystem().observer(this);
				forEachFace_instanceAttach(m_map);
			} else {
				ASSERT_MESSAGE(path_find_mapfile(path.begin(), path.end()) == m_map, "node is instanced across more than one file");
			}
		}
		void instanceDetach (const scene::Path& path)
		{
			if (--m_instanceCounter.m_count == 0) {
				forEachFace_instanceDetach(m_map);
				m_map = 0;
				m_undoable_observer = 0;
				GlobalUndoSystem().release(this);
			}
		}

		// nameable
		std::string name () const
		{
			return "brush";
		}
		void attach (const NameCallback& callback)
		{
		}
		void detach (const NameCallback& callback)
		{
		}

		// observer
		void planeChanged ()
		{
			m_planeChanged = true;
			aabbChanged();
			m_lightsChanged();
		}
		void shaderChanged ()
		{
			planeChanged();
		}

		void evaluateBRep () const
		{
			if (m_planeChanged) {
				m_planeChanged = false;
				const_cast<Brush*> (this)->buildBRep();
			}
		}

		void transformChanged ()
		{
			m_transformChanged = true;
			planeChanged();
		}
		typedef MemberCaller<Brush, &Brush::transformChanged> TransformChangedCaller;

		void evaluateTransform ()
		{
			if (m_transformChanged) {
				m_transformChanged = false;
				revertTransform();
				m_evaluateTransform();
			}
		}
		const Matrix4& localToParent () const
		{
			return Matrix4::getIdentity();
		}
		void aabbChanged ()
		{
			m_boundsChanged();
		}
		const AABB& localAABB () const
		{
			evaluateBRep();
			return m_aabb_local;
		}

		VolumeIntersectionValue intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const
		{
			return test.TestAABB(m_aabb_local, localToWorld);
		}

		void renderComponents (SelectionSystem::EComponentMode mode, Renderer& renderer, const VolumeTest& volume,
				const Matrix4& localToWorld) const
		{
			switch (mode) {
			case SelectionSystem::eVertex:
				renderer.addRenderable(m_render_vertices, localToWorld);
				break;
			case SelectionSystem::eEdge:
				renderer.addRenderable(m_render_edges, localToWorld);
				break;
			case SelectionSystem::eFace:
				renderer.addRenderable(m_render_faces, localToWorld);
				break;
			default:
				break;
			}
		}

		void transform (const Matrix4& matrix)
		{
			bool mirror = matrix4_handedness(matrix) == MATRIX4_LEFTHANDED;

			for (Faces::iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				(*i)->transform(matrix, mirror);
			}
		}
		void snapto (float snap)
		{
			for (Faces::iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				(*i)->snapto(snap);
			}
		}
		void revertTransform ()
		{
			for (Faces::iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				(*i)->revertTransform();
			}
		}
		void freezeTransform ()
		{
			for (Faces::iterator i = m_faces.begin(); i != m_faces.end(); ++i) {
				(*i)->freezeTransform();
			}
		}

		/// \brief Returns the absolute index of the \p faceVertex.
		std::size_t absoluteIndex (FaceVertexId faceVertex)
		{
			std::size_t index = 0;
			for (std::size_t i = 0; i < faceVertex.getFace(); ++i) {
				index += m_faces[i]->getWinding().numpoints;
			}
			return index + faceVertex.getVertex();
		}

		void appendFaces (const Faces& other)
		{
			clear();
			for (Faces::const_iterator i = other.begin(); i != other.end(); ++i) {
				push_back(*i);
			}
		}

		/// \brief The undo memento for a brush stores only the list of face references - the faces are not copied.
		class BrushUndoMemento: public UndoMemento
		{
			public:
				BrushUndoMemento (const Faces& faces) :
					m_faces(faces)
				{
				}

				Faces m_faces;
		};

		void undoSave ()
		{
			if (m_map != 0) {
				m_map->changed();
			}
			if (m_undoable_observer != 0) {
				m_undoable_observer->save(this);
			}
		}

		UndoMemento* exportState () const
		{
			return new BrushUndoMemento(m_faces);
		}

		void importState (const UndoMemento* state)
		{
			undoSave();
			appendFaces(static_cast<const BrushUndoMemento*> (state)->m_faces);
			planeChanged();

			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->DEBUG_verify();
			}
		}

		bool isDetail ()
		{
			return !m_faces.empty() && m_faces.front()->isDetail();
		}

		/// \brief Appends a copy of \p face to the end of the face list.
		Face* addFace (const Face& face)
		{
			if (m_faces.size() == c_brush_maxFaces) {
				return 0;
			}
			undoSave();
			push_back(FaceSmartPointer(new Face(face, this)));
			m_faces.back()->setDetail(isDetail());
			planeChanged();
			return m_faces.back();
		}

		/// \brief Appends a new face constructed from the parameters to the end of the face list.
		Face* addPlane (const Vector3& p0, const Vector3& p1, const Vector3& p2, const std::string& shader,
				const TextureProjection& projection)
		{
			if (m_faces.size() == c_brush_maxFaces) {
				return 0;
			}
			undoSave();
			push_back(FaceSmartPointer(new Face(p0, p1, p2, shader, projection, this)));
			m_faces.back()->setDetail(isDetail());
			planeChanged();
			return m_faces.back();
		}

		/**
		 * The assumption is that surfaceflags are the same for all faces
		 */
		ContentsFlagsValue getFlags() {
			if (m_faces.empty())
				return ContentsFlagsValue();
			return m_faces.back()->GetFlags();
		}

		static void constructStatic ()
		{
			Face::m_quantise = quantiseFloating;

			m_state_point = GlobalShaderCache().capture("$POINT");
		}
		static void destroyStatic ()
		{
			GlobalShaderCache().release("$POINT");
		}

		std::size_t DEBUG_size ()
		{
			return m_faces.size();
		}

		typedef Faces::const_iterator const_iterator;

		const_iterator begin () const
		{
			return m_faces.begin();
		}
		const_iterator end () const
		{
			return m_faces.end();
		}

		Face* back ()
		{
			return m_faces.back();
		}
		const Face* back () const
		{
			return m_faces.back();
		}
		/**
		 * Reserve space in the planes vector
		 * @param count The amount of planes to reserve
		 */
		void reserve (std::size_t count)
		{
			m_faces.reserve(count);
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->reserve(count);
			}
		}
		void push_back (Faces::value_type face)
		{
			m_faces.push_back(face);
			if (m_instanceCounter.m_count != 0) {
				m_faces.back()->instanceAttach(m_map);
			}
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->push_back(*face);
				(*i)->DEBUG_verify();
			}
		}
		void pop_back ()
		{
			if (m_instanceCounter.m_count != 0) {
				m_faces.back()->instanceDetach(m_map);
			}
			m_faces.pop_back();
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->pop_back();
				(*i)->DEBUG_verify();
			}
		}
		void erase (std::size_t index)
		{
			if (m_instanceCounter.m_count != 0) {
				m_faces[index]->instanceDetach(m_map);
			}
			m_faces.erase(m_faces.begin() + index);
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->erase(index);
				(*i)->DEBUG_verify();
			}
		}
		void connectivityChanged ()
		{
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->connectivityChanged();
			}
		}

		/**
		 * Clears the planes vector
		 */
		void clear ()
		{
			undoSave();
			if (m_instanceCounter.m_count != 0) {
				forEachFace_instanceDetach(m_map);
			}
			m_faces.clear();
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->clear();
				(*i)->DEBUG_verify();
			}
		}
		std::size_t size () const
		{
			return m_faces.size();
		}
		bool empty () const
		{
			return m_faces.empty();
		}

		/// \brief Returns true if any face of the brush contributes to the final B-Rep.
		bool hasContributingFaces () const
		{
			for (const_iterator i = begin(); i != end(); ++i) {
				if ((*i)->contributes()) {
					return true;
				}
			}
			return false;
		}

		/// \brief Removes faces that do not contribute to the brush. This is useful for cleaning up after CSG operations on the brush.
		/// Note: removal of empty faces is not performed during direct brush manipulations, because it would make a manipulation irreversible if it created an empty face.
		void removeEmptyFaces ()
		{
			evaluateBRep();

			std::size_t i = 0;
			while (i < m_faces.size()) {
				if (!m_faces[i]->contributes()) {
					erase(i);
					planeChanged();
				} else {
					++i;
				}
			}
		}

		/// \brief Constructs \p winding from the intersection of \p plane with the other planes of the brush.
		void windingForClipPlane (Winding& winding, const Plane3& plane) const
		{
			FixedWinding buffer[2];
			bool swap = false;

			// get a poly that covers an effectively infinite area
			Winding_createInfinite(buffer[swap], plane, m_maxWorldCoord + 1);

			// chop the poly by all of the other faces
			{
				for (std::size_t i = 0; i < m_faces.size(); ++i) {
					const Face& clip = *m_faces[i];

					if (clip.plane3() == plane || !clip.plane3().isValid() || !plane_unique(i)
							|| plane == -clip.plane3()) {
						continue;
					}

					buffer[!swap].clear();

					{
						// flip the plane, because we want to keep the back side
						Plane3 clipPlane(-clip.plane3().normal(), -clip.plane3().dist());
						Winding_Clip(buffer[swap], plane, clipPlane, i, buffer[!swap]);
					}

					swap = !swap;
				}
			}

			Winding_forFixedWinding(winding, buffer[swap]);
		}

		void update_wireframe (RenderableWireframe& wire, const bool* faces_visible) const
		{
			wire.m_faceVertex.resize(m_edge_indices.size());
			wire.m_vertices = m_uniqueVertexPoints.data();
			wire.m_size = 0;
			for (std::size_t i = 0; i < m_edge_faces.size(); ++i) {
				if (faces_visible[m_edge_faces[i].first] || faces_visible[m_edge_faces[i].second]) {
					wire.m_faceVertex[wire.m_size++] = m_edge_indices[i];
				}
			}
		}

		void update_faces_wireframe (Array<PointVertex>& wire, const bool* faces_visible) const
		{
			std::size_t count = 0;
			for (std::size_t i = 0; i < m_faceCentroidPoints.size(); ++i) {
				if (faces_visible[i]) {
					++count;
				}
			}

			wire.resize(count);
			Array<PointVertex>::iterator p = wire.begin();
			for (std::size_t i = 0; i < m_faceCentroidPoints.size(); ++i) {
				if (faces_visible[i]) {
					*p++ = m_faceCentroidPoints[i];
				}
			}
		}

		/// \brief Makes this brush a deep-copy of the \p other.
		void copy (const Brush& other)
		{
			for (Faces::const_iterator i = other.m_faces.begin(); i != other.m_faces.end(); ++i) {
				addFace(*(*i));
			}
			planeChanged();
		}

	private:
		void edge_push_back (FaceVertexId faceVertex)
		{
			m_select_edges.push_back(SelectableEdge(m_faces, faceVertex));
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->edge_push_back(m_select_edges.back());
			}
		}
		void edge_clear ()
		{
			m_select_edges.clear();
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->edge_clear();
			}
		}
		void vertex_push_back (FaceVertexId faceVertex)
		{
			m_select_vertices.push_back(SelectableVertex(m_faces, faceVertex));
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->vertex_push_back(m_select_vertices.back());
			}
		}
		void vertex_clear ()
		{
			m_select_vertices.clear();
			for (Observers::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
				(*i)->vertex_clear();
			}
		}

		/// \brief Returns true if the face identified by \p index is preceded by another plane that takes priority over it.
		bool plane_unique (std::size_t index) const
		{
			// duplicate plane
			for (std::size_t i = 0; i < m_faces.size(); ++i) {
				if (index != i && !plane3_inside(m_faces[index]->plane3(), m_faces[i]->plane3())) {
					return false;
				}
			}
			return true;
		}

		/// \brief Removes edges that are smaller than the tolerance used when generating brush windings.
		void removeDegenerateEdges ()
		{
			for (std::size_t i = 0; i < m_faces.size(); ++i) {
				Winding& winding = m_faces[i]->getWinding();
				for (Winding::iterator j = winding.begin(); j != winding.end();) {
					std::size_t index = std::distance(winding.begin(), j);
					std::size_t next = Winding_next(winding, index);
					if (Edge_isDegenerate(winding[index].vertex, winding[next].vertex)) {
						Winding& other = m_faces[winding[index].adjacent]->getWinding();
						std::size_t adjacent = Winding_FindAdjacent(other, i);
						if (adjacent != c_brush_maxFaces) {
							other.erase(other.begin() + adjacent);
						}
						winding.erase(j);
					} else {
						++j;
					}
				}
			}
		}

		/// \brief Invalidates faces that have only two vertices in their winding, while preserving edge-connectivity information.
		void removeDegenerateFaces ()
		{
			// save adjacency info for degenerate faces
			for (std::size_t i = 0; i < m_faces.size(); ++i) {
				Winding& degen = m_faces[i]->getWinding();

				if (degen.numpoints == 2) {
					// this is an "edge" face, where the plane touches the edge of the brush
					{
						Winding& winding = m_faces[degen[0].adjacent]->getWinding();
						std::size_t index = Winding_FindAdjacent(winding, i);
						if (index != c_brush_maxFaces) {
							winding[index].adjacent = degen[1].adjacent;
						}
					}
					{
						Winding& winding = m_faces[degen[1].adjacent]->getWinding();
						std::size_t index = Winding_FindAdjacent(winding, i);
						if (index != c_brush_maxFaces) {
							winding[index].adjacent = degen[0].adjacent;
						}
					}

					degen.resize(0);
				}
			}
		}

		/// \brief Removes edges that have the same adjacent-face as their immediate neighbour.
		void removeDuplicateEdges ()
		{
			// verify face connectivity graph
			for (std::size_t i = 0; i < m_faces.size(); ++i) {
				Winding& winding = m_faces[i]->getWinding();
				for (std::size_t j = 0; j != winding.numpoints;) {
					std::size_t next = Winding_next(winding, j);
					if (winding[j].adjacent == winding[next].adjacent) {
						winding.erase(winding.begin() + next);
					} else {
						++j;
					}
				}
			}
		}

		/// \brief Removes edges that do not have a matching pair in their adjacent-face.
		void verifyConnectivityGraph ()
		{
			// verify face connectivity graph
			for (std::size_t i = 0; i < m_faces.size(); ++i) {
				Winding& winding = m_faces[i]->getWinding();
				for (Winding::iterator j = winding.begin(); j != winding.end();) {
					// remove unidirectional graph edges
					if ((*j).adjacent == c_brush_maxFaces || Winding_FindAdjacent(m_faces[(*j).adjacent]->getWinding(),
							i) == c_brush_maxFaces) {
						winding.erase(j);
					} else {
						++j;
					}
				}
			}
		}

		/// \brief Returns true if the brush is a finite volume. A brush without a finite volume extends past the maximum world bounds and is not valid.
		bool isBounded ()
		{
			for (const_iterator i = begin(); i != end(); ++i) {
				if (!(*i)->is_bounded()) {
					return false;
				}
			}
			return true;
		}

		/// \brief Constructs the polygon windings for each face of the brush. Also updates the brush bounding-box and face texture-coordinates.
		bool buildWindings ()
		{
			m_aabb_local = AABB();

			for (std::size_t i = 0; i < m_faces.size(); ++i) {
				Face& f = *m_faces[i];

				if (!f.plane3().isValid() || !plane_unique(i)) {
					f.getWinding().resize(0);
				} else {
					windingForClipPlane(f.getWinding(), f.plane3());

					// update brush bounds
					const Winding& winding = f.getWinding();
					for (Winding::const_iterator i = winding.begin(); i != winding.end(); ++i) {
						aabb_extend_by_point_safe(m_aabb_local, (*i).vertex);
					}

					// update texture coordinates
					f.EmitTextureCoordinates();
				}
			}

			const bool degenerate = !isBounded();

			if (!degenerate) {
				// clean up connectivity information.
				// these cleanups must be applied in a specific order.
				removeDegenerateEdges();
				removeDegenerateFaces();
				removeDuplicateEdges();
				verifyConnectivityGraph();
			}

			return degenerate;
		}

		/// \brief Constructs the face windings and updates anything that depends on them.
		void buildBRep ();
}; // class Brush

#endif /*BRUSH_BRUSH_H_*/
