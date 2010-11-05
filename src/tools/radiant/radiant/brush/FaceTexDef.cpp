#include "FaceTexDef.h"

// Constructor
FaceTexdef::FaceTexdef(FaceShader& shader, const TextureProjection& projection, bool projectionInitialised) :
	m_shader(shader), m_projection(projection)
{
	m_shader.attach(*this);
}

// Destructor
FaceTexdef::~FaceTexdef() {
	m_shader.detach(*this);
}

void FaceTexdef::realiseShader() {
}

void FaceTexdef::unrealiseShader() {
}

void FaceTexdef::setTexdef(const TextureProjection& projection) {
	m_projection.m_texdef = projection.m_texdef;
}

void FaceTexdef::shift(float s, float t) {
	ASSERT_MESSAGE(m_projection.m_texdef.isSane(), "FaceTexdef::shift: bad texdef");
	m_projection.m_texdef.shift(s, t);
}

void FaceTexdef::scale(float s, float t) {
	m_projection.m_texdef.scale(s, t);
}

void FaceTexdef::rotate(float angle) {
	m_projection.m_texdef.rotate(angle);
}

void FaceTexdef::fit(const Vector3& normal, const Winding& winding, float s_repeat, float t_repeat) {
	m_projection.fitTexture(m_shader.width(), m_shader.height(), normal, winding, s_repeat, t_repeat);
}

void FaceTexdef::flipTexture(unsigned int flipAxis) {
	m_projection.flipTexture(flipAxis);
}

void FaceTexdef::emitTextureCoordinates(Winding& winding, const Vector3& normal, const Matrix4& localToWorld) {
	m_projection.emitTextureCoordinates(m_shader.width(), m_shader.height(), winding, normal,
						localToWorld);
}

void FaceTexdef::transform(const Plane3& plane, const Matrix4& matrix) {
	m_projection.transformLocked(m_shader.width(), m_shader.height(), plane, matrix);
}

TextureProjection FaceTexdef::normalised() const {
	return TextureProjection(m_projection.m_texdef);
}
