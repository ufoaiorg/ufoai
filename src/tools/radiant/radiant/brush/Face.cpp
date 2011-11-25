#include "Face.h"

#include "ifilter.h"
#include "renderable.h"
#include "BrushModule.h"

#include "shaderlib.h"
#include "winding.h"
#include "cullable.h"

void Brush_textureChanged ();

Face::Face (FaceObserver* observer) :
	m_refcount(0), m_shader(GlobalTexturePrefix_get()), m_texdef(m_shader, TextureProjection(), false), m_observer(
			observer), m_undoable_observer(0), m_map(0)
{
	m_shader.attach(*this);
	m_plane.copy(Vector3(0, 0, 0), Vector3(64, 0, 0), Vector3(0, 64, 0));
	planeChanged();
}
Face::Face (const Vector3& p0, const Vector3& p1, const Vector3& p2, const std::string& shader,
		const TextureProjection& projection, FaceObserver* observer) :
	m_refcount(0), m_shader(shader), m_texdef(m_shader, projection), m_observer(observer), m_undoable_observer(0),
			m_map(0)
{
	m_shader.attach(*this);
	m_plane.copy(p0, p1, p2);
	planeChanged();
}
Face::Face (const Face& other, FaceObserver* observer) :
	m_refcount(0), m_shader(other.m_shader.getShader(), other.m_shader.m_flags), m_texdef(m_shader,
			other.getTexdef().normalised()), m_observer(observer), m_undoable_observer(0), m_map(0)
{
	m_shader.attach(*this);
	m_plane.copy(other.m_plane);
	planepts_assign(m_move_planepts, other.m_move_planepts);
	planeChanged();
}
Face::~Face ()
{
	m_shader.detach(*this);
}

void Face::planeChanged ()
{
	revertTransform();
	m_observer->planeChanged();
}

void Face::realiseShader ()
{
	m_observer->shaderChanged();
}
void Face::unrealiseShader ()
{
}

void Face::instanceAttach (MapFile* map)
{
	m_shader.instanceAttach();
	m_map = map;
	m_undoable_observer = GlobalUndoSystem().observer(this);
}
void Face::instanceDetach (MapFile* map)
{
	m_undoable_observer = 0;
	GlobalUndoSystem().release(this);
	m_map = 0;
	m_shader.instanceDetach();
}

void Face::render (RenderStateFlags state) const
{
	// TODO: This is a hack to allow to render blended surfaces
	float colour[4];
	GLboolean blendActive;
	const int flags = m_shader.m_flags.getSurfaceFlags();
	if (flags & (SURF_BLEND33 | SURF_BLEND66)) {
		glGetFloatv(GL_CURRENT_COLOR, colour);
		float oldAlpha = colour[3];
		colour[3] = 0.5;
		glColor4fv(colour);
		colour[3] = oldAlpha;
		glGetBooleanv(GL_BLEND, &blendActive);
		if (!blendActive)
			glEnable(GL_BLEND);
	}

	m_winding.draw(m_planeTransformed.plane3().normal(), state);

	if (flags & (SURF_BLEND33 | SURF_BLEND66)) {
		if (!blendActive)
			glDisable(GL_BLEND);
		glColor4fv(colour);
	}
}

void Face::undoSave ()
{
	if (m_map != 0) {
		m_map->changed();
	}
	if (m_undoable_observer != 0) {
		m_undoable_observer->save(this);
	}
}

// undoable
UndoMemento* Face::exportState () const
{
	return new SavedState(*this);
}
void Face::importState (const UndoMemento* data)
{
	undoSave();

	static_cast<const SavedState*> (data)->exportState(*this);

	planeChanged();
	m_observer->connectivityChanged();
	texdefChanged();
	m_observer->shaderChanged();
}

void Face::IncRef ()
{
	++m_refcount;
}
void Face::DecRef ()
{
	if (--m_refcount == 0)
		delete this;
}

void Face::flipWinding ()
{
	m_plane.reverse();
	planeChanged();
}

bool Face::intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const
{
	return volume.TestPlane(Plane3(plane3().normal(), -plane3().dist()), localToWorld);
}

void Face::render (Renderer& renderer, const Matrix4& localToWorld) const
{
	// Submit this face to the Renderer only if its shader is not filtered
	if (GlobalFilterSystem().isVisible(FilterRule::TYPE_TEXTURE, m_shader.getShader())) {
		if (GlobalFilterSystem().isVisible(FilterRule::TYPE_SURFACEFLAGS, m_shader.getFlags().getSurfaceFlags())
				&& GlobalFilterSystem().isVisible(FilterRule::TYPE_CONTENTFLAGS, m_shader.getFlags().getContentFlags())) {
			renderer.SetState(m_shader.state(), Renderer::eFullMaterials);
			renderer.addRenderable(*this, localToWorld);
		}
	}
}

void Face::transform (const Matrix4& matrix, bool mirror)
{
	if (GlobalBrush()->textureLockEnabled()) {
		m_texdefTransformed.transformLocked(m_shader.width(), m_shader.height(), m_plane.plane3(), matrix);
	}

	// Transform the FacePlane using the given matrix
	m_planeTransformed.transform(matrix, mirror);
	m_observer->planeChanged();
}

void Face::assign_planepts (const PlanePoints planepts)
{
	m_planeTransformed.copy(planepts[0], planepts[1], planepts[2]);
	m_observer->planeChanged();
}

/// \brief Reverts the transformable state of the brush to identity.
void Face::revertTransform ()
{
	m_planeTransformed = m_plane;
	planepts_assign(m_move_planeptsTransformed, m_move_planepts);
	m_texdefTransformed = m_texdef.m_projection;
}
void Face::freezeTransform ()
{
	undoSave();
	m_plane = m_planeTransformed;
	planepts_assign(m_move_planepts, m_move_planeptsTransformed);
	m_texdef.m_projection = m_texdefTransformed;
}

void Face::update_move_planepts_vertex (std::size_t index, PlanePoints planePoints)
{
	std::size_t numpoints = getWinding().size();
	ASSERT_MESSAGE(index < numpoints, "update_move_planepts_vertex: invalid index");

	std::size_t opposite = Winding_Opposite(getWinding(), index);
	std::size_t adjacent = getWinding().wrap(opposite + numpoints - 1);
	planePoints[0] = getWinding()[opposite].vertex;
	planePoints[1] = getWinding()[index].vertex;
	planePoints[2] = getWinding()[adjacent].vertex;
	// winding points are very inaccurate, so they must be quantised before using them to generate the face-plane
	planepts_quantise(planePoints, GRID_MIN);
}

void Face::snapto (float snap)
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

void Face::testSelect (SelectionTest& test, SelectionIntersection& best)
{
	m_winding.testSelect(test, best);
}

void Face::testSelect_centroid (SelectionTest& test, SelectionIntersection& best)
{
	test.TestPoint(m_centroid, best);
}

void Face::shaderChanged ()
{
	EmitTextureCoordinates();
	m_observer->shaderChanged();
	planeChanged();
	SceneChangeNotify();
}

const std::string& Face::GetShader () const
{
	return m_shader.getShader();
}
void Face::SetShader (const std::string& name)
{
	undoSave();
	m_shader.setShader(name);
	shaderChanged();
}

void Face::revertTexdef ()
{
	m_texdefTransformed = m_texdef.m_projection;
}
void Face::texdefChanged ()
{
	revertTexdef();
	EmitTextureCoordinates();
}

void Face::GetTexdef (TextureProjection& projection) const
{
	projection = m_texdef.normalised();
}
void Face::SetTexdef (const TextureProjection& projection)
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
void Face::GetFlags (ContentsFlagsValue& flags) const
{
	ContentsFlagsValue oldFlags = flags;
	flags = m_shader.getFlags();
	flags.mergeFlags(oldFlags);
}

ContentsFlagsValue Face::GetFlags () const
{
	return m_shader.getFlags();
}

/** @sa ContentsFlagsValue_assignMasked */
void Face::SetFlags (const ContentsFlagsValue& flags)
{
	undoSave();
	m_shader.setFlags(flags);
	m_observer->shaderChanged();
}

void Face::ShiftTexdef (float s, float t)
{
	undoSave();
	m_texdef.shift(s, t);
	texdefChanged();
}

void Face::ScaleTexdef (float s, float t)
{
	undoSave();
	m_texdef.scale(s, t);
	texdefChanged();
}

void Face::RotateTexdef (float angle)
{
	undoSave();
	m_texdef.rotate(angle);
	texdefChanged();
}

void Face::FitTexture (float s_repeat, float t_repeat)
{
	undoSave();
	m_texdef.fit(m_plane.plane3().normal(), m_winding, s_repeat, t_repeat);
	texdefChanged();
}

void Face::flipTexture (unsigned int axis)
{
	undoSave();
	m_texdef.flipTexture(axis);
	texdefChanged();
}

void Face::EmitTextureCoordinates ()
{
	m_texdefTransformed.emitTextureCoordinates(m_shader.width(), m_shader.height(), m_winding, plane3().normal(),
			Matrix4::getIdentity());
}

const Vector3& Face::centroid () const
{
	return m_centroid;
}

void Face::construct_centroid ()
{
	Winding_Centroid(m_winding, plane3(), m_centroid);
}

const Winding& Face::getWinding () const
{
	return m_winding;
}
Winding& Face::getWinding ()
{
	return m_winding;
}

const Plane3& Face::plane3 () const
{
	m_observer->evaluateTransform();
	return m_planeTransformed.plane3();
}
FacePlane& Face::getPlane ()
{
	return m_plane;
}
const FacePlane& Face::getPlane () const
{
	return m_plane;
}
FaceTexdef& Face::getTexdef ()
{
	return m_texdef;
}
const FaceTexdef& Face::getTexdef () const
{
	return m_texdef;
}
FaceShader& Face::getShader ()
{
	return m_shader;
}
const FaceShader& Face::getShader () const
{
	return m_shader;
}

bool Face::isDetail () const
{
	return m_shader.m_flags.isDetail();
}
void Face::setDetail (bool detail)
{
	undoSave();
	m_shader.m_flags.setDetail(detail);
	m_observer->shaderChanged();
}

bool Face::contributes () const
{
	return m_winding.size() > 2;
}
bool Face::is_bounded () const
{
	for (Winding::const_iterator i = m_winding.begin(); i != m_winding.end(); ++i) {
		if ((*i).adjacent == c_brush_maxFaces) {
			return false;
		}
	}
	return true;
}

QuantiseFunc Face::m_quantise;
