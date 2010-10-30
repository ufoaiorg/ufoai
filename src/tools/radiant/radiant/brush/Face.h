#ifndef FACE_H_
#define FACE_H_

#include "irender.h"
#include "iundo.h"
#include "mapfile.h"
#include "selectable.h"

#include "math/Vector3.h"
#include "math/matrix.h"
#include "generic/referencecounted.h"

#include "FaceTexDef.h"
#include "FaceShader.h"
#include "PlanePoints.h"
#include "FacePlane.h"

const double GRID_MIN = 0.125;

inline double quantiseInteger (double f)
{
	return float_to_integer(f);
}

inline double quantiseFloating (double f)
{
	return float_snapped(f, 1.f / (1 << 16));
}

typedef double (*QuantiseFunc) (double f);

class Face;

void Brush_addTextureChangedCallback (const SignalHandler& callback);
void Brush_textureChanged ();

extern bool g_brush_texturelock_enabled;

class FaceObserver
{
	public:
		virtual ~FaceObserver ()
		{
		}
		virtual void planeChanged () = 0;
		virtual void connectivityChanged () = 0;
		virtual void shaderChanged () = 0;
		virtual void evaluateTransform () = 0;
};

class Face: public OpenGLRenderable, public Undoable, public FaceShaderObserver
{
		std::size_t m_refcount;

		class SavedState: public UndoMemento
		{
			public:
				FacePlane::SavedState m_planeState;
				FaceTexdef::SavedState m_texdefState;
				FaceShader::SavedState m_shaderState;

				SavedState (const Face& face) :
					m_planeState(face.getPlane()), m_texdefState(face.getTexdef()), m_shaderState(face.getShader())
				{
				}

				void exportState (Face& face) const
				{
					m_planeState.exportState(face.getPlane());
					m_shaderState.exportState(face.getShader());
					m_texdefState.exportState(face.getTexdef());
				}
		};

	public:
		static QuantiseFunc m_quantise;

		PlanePoints m_move_planepts;
		PlanePoints m_move_planeptsTransformed;
	private:
		FacePlane m_plane;
		FacePlane m_planeTransformed;
		FaceShader m_shader;
		FaceTexdef m_texdef;
		TextureProjection m_texdefTransformed;

		Winding m_winding;
		Vector3 m_centroid;

		FaceObserver* m_observer;
		UndoObserver* m_undoable_observer;
		MapFile* m_map;

		// assignment not supported
		Face& operator= (const Face& other);
		// copy-construction not supported
		Face (const Face& other);

	public:

		Face (FaceObserver* observer) :
			m_refcount(0), m_shader(GlobalTexturePrefix_get()), m_texdef(m_shader, TextureProjection(), false),
					m_observer(observer), m_undoable_observer(0), m_map(0)
		{
			m_shader.attach(*this);
			m_plane.copy(Vector3(0, 0, 0), Vector3(64, 0, 0), Vector3(0, 64, 0));
			planeChanged();
		}
		Face (const Vector3& p0, const Vector3& p1, const Vector3& p2, const std::string& shader,
				const TextureProjection& projection, FaceObserver* observer) :
			m_refcount(0), m_shader(shader), m_texdef(m_shader, projection), m_observer(observer), m_undoable_observer(
					0), m_map(0)
		{
			m_shader.attach(*this);
			m_plane.copy(p0, p1, p2);
			planeChanged();
		}
		Face (const Face& other, FaceObserver* observer) :
			m_refcount(0), m_shader(other.m_shader.getShader(), other.m_shader.m_flags), m_texdef(m_shader,
					other.getTexdef().normalised()), m_observer(observer), m_undoable_observer(0), m_map(0)
		{
			m_shader.attach(*this);
			m_plane.copy(other.m_plane);
			planepts_assign(m_move_planepts, other.m_move_planepts);
			planeChanged();
		}
		~Face ()
		{
			m_shader.detach(*this);
		}

		void planeChanged ()
		{
			revertTransform();
			m_observer->planeChanged();
		}

		void realiseShader ()
		{
			m_observer->shaderChanged();
		}
		void unrealiseShader ()
		{
		}

		void instanceAttach (MapFile* map)
		{
			m_shader.instanceAttach();
			m_map = map;
			m_undoable_observer = GlobalUndoSystem().observer(this);
		}
		void instanceDetach (MapFile* map)
		{
			m_undoable_observer = 0;
			GlobalUndoSystem().release(this);
			m_map = 0;
			m_shader.instanceDetach();
		}

		void render (RenderStateFlags state) const
		{
			Winding_Draw(m_winding, m_planeTransformed.plane3().normal(), state);
		}

		void undoSave ()
		{
			if (m_map != 0) {
				m_map->changed();
			}
			if (m_undoable_observer != 0) {
				m_undoable_observer->save(this);
			}
		}

		// undoable
		UndoMemento* exportState () const
		{
			return new SavedState(*this);
		}
		void importState (const UndoMemento* data)
		{
			undoSave();

			static_cast<const SavedState*> (data)->exportState(*this);

			planeChanged();
			m_observer->connectivityChanged();
			texdefChanged();
			m_observer->shaderChanged();
		}

		void IncRef ()
		{
			++m_refcount;
		}
		void DecRef ()
		{
			if (--m_refcount == 0)
				delete this;
		}

		void flipWinding ()
		{
			m_plane.reverse();
			planeChanged();
		}

		bool intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			return volume.TestPlane(Plane3(plane3().normal(), -plane3().dist()), localToWorld);
		}

		void render (Renderer& renderer, const Matrix4& localToWorld) const
		{
			// Submit this face to the Renderer only if its shader is not filtered
			if (GlobalFilterSystem().isVisible("texture", m_shader.getShader())) {
				if (GlobalFilterSystem().isVisible("surfaceflags", m_shader.getFlags().m_surfaceFlags)
						&& GlobalFilterSystem().isVisible("contentflags", m_shader.getFlags().m_contentFlags)) {
					renderer.SetState(m_shader.state(), Renderer::eFullMaterials);
					renderer.addRenderable(*this, localToWorld);
				}
			}
		}

		void transform (const Matrix4& matrix, bool mirror)
		{
			if (g_brush_texturelock_enabled) {
				m_texdefTransformed.transformLocked(m_shader.width(), m_shader.height(), m_plane.plane3(), matrix);
			}

			m_planeTransformed.transform(matrix, mirror);
			m_observer->planeChanged();
		}

		void assign_planepts (const PlanePoints planepts)
		{
			m_planeTransformed.copy(planepts[0], planepts[1], planepts[2]);
			m_observer->planeChanged();
		}

		/// \brief Reverts the transformable state of the brush to identity.
		void revertTransform ()
		{
			m_planeTransformed = m_plane;
			planepts_assign(m_move_planeptsTransformed, m_move_planepts);
			m_texdefTransformed = m_texdef.m_projection;
		}
		void freezeTransform ()
		{
			undoSave();
			m_plane = m_planeTransformed;
			planepts_assign(m_move_planepts, m_move_planeptsTransformed);
			m_texdef.m_projection = m_texdefTransformed;
		}

		void update_move_planepts_vertex (std::size_t index, PlanePoints planePoints)
		{
			std::size_t numpoints = getWinding().numpoints;
			ASSERT_MESSAGE(index < numpoints, "update_move_planepts_vertex: invalid index");

			std::size_t opposite = Winding_Opposite(getWinding(), index);
			std::size_t adjacent = Winding_wrap(getWinding(), opposite + numpoints - 1);
			planePoints[0] = getWinding()[opposite].vertex;
			planePoints[1] = getWinding()[index].vertex;
			planePoints[2] = getWinding()[adjacent].vertex;
			// winding points are very inaccurate, so they must be quantised before using them to generate the face-plane
			planepts_quantise(planePoints, GRID_MIN);
		}

		void snapto (float snap)
		{
			if (contributes()) {
				PlanePoints planePoints;
				update_move_planepts_vertex(0, planePoints);
				vector3_snap(planePoints[0], snap);
				vector3_snap(planePoints[1], snap);
				vector3_snap(planePoints[2], snap);
				assign_planepts(planePoints);
				freezeTransform();

				SceneChangeNotify();
				if (!m_plane.plane3().isValid()) {
					g_warning("Invalid plane after snap to grid\n");
				}
			}
		}

		void testSelect (SelectionTest& test, SelectionIntersection& best)
		{
			Winding_testSelect(m_winding, test, best);
		}

		void testSelect_centroid (SelectionTest& test, SelectionIntersection& best)
		{
			test.TestPoint(m_centroid, best);
		}

		void shaderChanged ()
		{
			EmitTextureCoordinates();
			Brush_textureChanged();
			m_observer->shaderChanged();
			planeChanged();
			SceneChangeNotify();
		}

		const std::string& GetShader () const
		{
			return m_shader.getShader();
		}
		void SetShader (const std::string& name)
		{
			undoSave();
			m_shader.setShader(name);
			shaderChanged();
		}

		void revertTexdef ()
		{
			m_texdefTransformed = m_texdef.m_projection;
		}
		void texdefChanged ()
		{
			revertTexdef();
			EmitTextureCoordinates();
			Brush_textureChanged();
		}

		void GetTexdef (TextureProjection& projection) const
		{
			projection = m_texdef.normalised();
		}
		void SetTexdef (const TextureProjection& projection)
		{
			undoSave();
			m_texdef.setTexdef(projection);
			texdefChanged();
		}

		/**
		 * @brief Get the content and surface flags from a given face
		 * @note Also generates a diff bitmask of the flag variable given to store
		 * the flag values in and the current face flags
		 * @param[in,out] flags The content and surface flag container
		 * @sa ContentsFlagsValue_assignMasked
		 */
		void GetFlags (ContentsFlagsValue& flags) const
		{
			ContentsFlagsValue oldFlags = flags;
			flags = m_shader.getFlags();

			// rescue the mark dirty value and old dirty flags
			flags.m_markDirty = oldFlags.m_markDirty;
			flags.m_surfaceFlagsDirty = oldFlags.m_surfaceFlagsDirty;
			flags.m_contentFlagsDirty = oldFlags.m_contentFlagsDirty;

			if (flags.m_markDirty) {
				// Figure out which buttons are inconsistent
				flags.m_contentFlagsDirty |= (oldFlags.m_contentFlags ^ flags.m_contentFlags);
				flags.m_surfaceFlagsDirty |= (oldFlags.m_surfaceFlags ^ flags.m_surfaceFlags);
			}
			flags.m_markDirty = 1;
			// preserve dirty state, don't mark dirty if we only select one face / first face (value in old flags is 0, own value could differ)
			if (oldFlags.m_valueDirty || ((flags.m_value != oldFlags.m_value) && !oldFlags.m_firstValue)) {
				flags.m_valueDirty = true;
			}
			flags.m_firstValue = false;
		}

		ContentsFlagsValue GetFlags () const
		{
			return m_shader.getFlags();
		}

		/** @sa ContentsFlagsValue_assignMasked */
		void SetFlags (const ContentsFlagsValue& flags)
		{
			undoSave();
			m_shader.setFlags(flags);
			m_observer->shaderChanged();
		}

		void ShiftTexdef (float s, float t)
		{
			undoSave();
			m_texdef.shift(s, t);
			texdefChanged();
		}

		void ScaleTexdef (float s, float t)
		{
			undoSave();
			m_texdef.scale(s, t);
			texdefChanged();
		}

		void RotateTexdef (float angle)
		{
			undoSave();
			m_texdef.rotate(angle);
			texdefChanged();
		}

		void FitTexture (float s_repeat, float t_repeat)
		{
			undoSave();
			m_texdef.fit(m_plane.plane3().normal(), m_winding, s_repeat, t_repeat);
			texdefChanged();
		}

		void EmitTextureCoordinates ()
		{
			m_texdefTransformed.emitTextureCoordinates(m_shader.width(), m_shader.height(), m_winding,
					plane3().normal(), Matrix4::getIdentity());
		}

		const Vector3& centroid () const
		{
			return m_centroid;
		}

		void construct_centroid ()
		{
			Winding_Centroid(m_winding, plane3(), m_centroid);
		}

		const Winding& getWinding () const
		{
			return m_winding;
		}
		Winding& getWinding ()
		{
			return m_winding;
		}

		const Plane3& plane3 () const
		{
			m_observer->evaluateTransform();
			return m_planeTransformed.plane3();
		}
		FacePlane& getPlane ()
		{
			return m_plane;
		}
		const FacePlane& getPlane () const
		{
			return m_plane;
		}
		FaceTexdef& getTexdef ()
		{
			return m_texdef;
		}
		const FaceTexdef& getTexdef () const
		{
			return m_texdef;
		}
		FaceShader& getShader ()
		{
			return m_shader;
		}
		const FaceShader& getShader () const
		{
			return m_shader;
		}

		bool isDetail () const
		{
			return (m_shader.m_flags.m_contentFlags & BRUSH_DETAIL_MASK) != 0;
		}
		void setDetail (bool detail)
		{
			undoSave();
			if (detail && !isDetail()) {
				m_shader.m_flags.m_contentFlags |= BRUSH_DETAIL_MASK;
			} else if (!detail && isDetail()) {
				m_shader.m_flags.m_contentFlags &= ~BRUSH_DETAIL_MASK;
			}
			m_observer->shaderChanged();
		}

		bool contributes () const
		{
			return m_winding.numpoints > 2;
		}
		bool is_bounded () const
		{
			for (Winding::const_iterator i = m_winding.begin(); i != m_winding.end(); ++i) {
				if ((*i).adjacent == c_brush_maxFaces) {
					return false;
				}
			}
			return true;
		}
}; // class Face

#endif /*FACE_H_*/
