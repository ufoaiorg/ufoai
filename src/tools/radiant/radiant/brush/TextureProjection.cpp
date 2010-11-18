#include "TextureProjection.h"
#include "BrushModule.h"
#include "winding.h"
#include "math/aabb.h"

TextureProjection::TextureProjection ()
{
}

TextureProjection::TextureProjection (const TexDef& texdef) :
	m_texdef(texdef)
{
}

void TextureProjection::constructDefault ()
{
	float scale = GlobalRegistry().getFloat("user/ui/textures/defaultTextureScale");

	m_texdef._scale[0] = scale;
	m_texdef._scale[1] = scale;
	m_texdef._rotate = 0;
	m_texdef._shift[0] = 0;
	m_texdef._shift[1] = 0;
}

/* greebo: Uses the transformation matrix <transform> to set the internal texture
 * definitions. Checks the matrix for validity and passes it on to
 * the according internal texture definitions (TexDef or BPTexDef)
 */
void TextureProjection::setTransform (float width, float height, const Matrix4& transform)
{
	// Check the matrix for validity
	if ((transform[0] != 0 || transform[4] != 0) && (transform[1] != 0 || transform[5] != 0)) {
		// Decide which TexDef to use
		m_texdef = TexDef(width, height, transform);
	} else {
		globalErrorStream() << "invalid texture matrix\n";
	}
}

/* greebo: Returns the transformation matrix from the
 * texture definitions members.
 */
Matrix4 TextureProjection::getTransform (float width, float height) const
{
	return m_texdef.getTransform(width, height);
}

void TextureProjection::shift(float s, float t) {
	m_texdef.shift(s, t);
}

void TextureProjection::scale(float s, float t) {
	m_texdef.scale(s, t);
}

void TextureProjection::rotate(float angle) {
	m_texdef.rotate(angle);
}

// Normalise projection for a given texture width and height.
void TextureProjection::normalise (float width, float height)
{
	m_texdef.normalise(width, height);
}

void TextureProjection::fitTexture (std::size_t width, std::size_t height, const Vector3& normal, const Winding& w,
		float s_repeat, float t_repeat)
{
	if (w.size() < 3) {
		return;
	}

	Matrix4 st2tex = m_texdef.getTransform((float) width, (float) height);

	// the current texture transform
	Matrix4 local2tex = st2tex;
	{
		Matrix4 xyz2st;
		basisForNormal(normal, xyz2st);
		matrix4_multiply_by_matrix4(local2tex, xyz2st);
	}

	// the bounds of the current texture transform
	AABB bounds;
	for (Winding::const_iterator i = w.begin(); i != w.end(); ++i) {
		Vector3 texcoord = matrix4_transformed_point(local2tex, (*i).vertex);
		aabb_extend_by_point_safe(bounds, texcoord);
	}
	bounds.origin.z() = 0;
	bounds.extents.z() = 1;

	// the bounds of a perfectly fitted texture transform
	AABB perfect(Vector3(s_repeat * 0.5, t_repeat * 0.5, 0), Vector3(s_repeat * 0.5, t_repeat * 0.5, 1));

	// the difference between the current texture transform and the perfectly fitted transform
	Matrix4 matrix(Matrix4::getTranslation(bounds.origin - perfect.origin));
	matrix4_pivoted_scale_by_vec3(matrix, bounds.extents / perfect.extents, perfect.origin);
	matrix4_affine_invert(matrix);

	// apply the difference to the current texture transform
	matrix4_premultiply_by_matrix4(st2tex, matrix);

	setTransform((float) width, (float) height, st2tex);
	m_texdef.normalise((float) width, (float) height);
}

void TextureProjection::flipTexture(unsigned int flipAxis) {
	// Check for x flip (x-component not zero)
	if (flipAxis == 0) {
		// Invert the x scale and rotate 180 degree
		m_texdef._scale[0] *= -1;
		m_texdef._rotate -= 180;
	}
	// Check for y flip (y-component not zero)
	else if (flipAxis == 1) {
		// Invert the y scale and rotate 180 degree
		m_texdef._scale[1] *= -1;
		m_texdef._rotate -= 180;
	}
}

void TextureProjection::transformLocked (std::size_t width, std::size_t height, const Plane3& plane,
		const Matrix4& identity2transformed)
{

	Vector3 normalTransformed(matrix4_transformed_direction(identity2transformed, plane.normal()));

	// identity: identity space
	// transformed: transformation
	// stIdentity: base st projection space before transformation
	// stTransformed: base st projection space after transformation
	// stOriginal: original texdef space

	// stTransformed2stOriginal = stTransformed -> transformed -> identity -> stIdentity -> stOriginal

	Matrix4 identity2stIdentity;
	basisForNormal(plane.normal(), identity2stIdentity);

	Matrix4 transformed2stTransformed;
	basisForNormal(normalTransformed, transformed2stTransformed);

	Matrix4 stTransformed2identity(matrix4_affine_inverse(matrix4_multiplied_by_matrix4(transformed2stTransformed,
			identity2transformed)));

	Vector3 originalProjectionAxis(matrix4_affine_inverse(identity2stIdentity).z().getVector3());

	Vector3 transformedProjectionAxis(stTransformed2identity.z().getVector3());

	Matrix4 stIdentity2stOriginal = m_texdef.getTransform((float) width, (float) height);
	Matrix4 identity2stOriginal(matrix4_multiplied_by_matrix4(stIdentity2stOriginal, identity2stIdentity));

	double dot = originalProjectionAxis.dot(transformedProjectionAxis);
	if (dot == 0) {
		// The projection axis chosen for the transformed normal is at 90 degrees
		// to the transformed projection axis chosen for the original normal.
		// This happens when the projection axis is ambiguous - e.g. for the plane
		// 'X == Y' the projection axis could be either X or Y.

		Matrix4 identityCorrected = matrix4_reflection_for_plane45(plane, originalProjectionAxis,
				transformedProjectionAxis);

		identity2stOriginal = matrix4_multiplied_by_matrix4(identity2stOriginal, identityCorrected);
	}

	Matrix4 stTransformed2stOriginal = matrix4_multiplied_by_matrix4(identity2stOriginal, stTransformed2identity);

	setTransform((float) width, (float) height, stTransformed2stOriginal);
	m_texdef.normalise((float) width, (float) height);
}

/*!
 \brief Construct a transform from XYZ space to ST space (3d to 2d).
 This will be one of three axis-aligned spaces, depending on the surface normal.
 NOTE: could also be done by swapping values.
 */
void TextureProjection::basisForNormal (const Vector3& normal, Matrix4& basis)
{
	Normal_GetTransform(normal, basis);
}

void TextureProjection::emitTextureCoordinates (std::size_t width, std::size_t height, Winding& w,
		const Vector3& normal, const Matrix4& localToWorld)
{
	if (w.size() < 3) {
		return;
	}

	Matrix4 local2tex = m_texdef.getTransform((float) width, (float) height);

	{
		Matrix4 xyz2st;
		// we don't care if it's not normalised...
		basisForNormal(matrix4_transformed_direction(localToWorld, normal), xyz2st);
		matrix4_multiply_by_matrix4(local2tex, xyz2st);
	}

	Vector3 tangent(local2tex.getTransposed().x().getVector3().getNormalised());
	Vector3 bitangent(local2tex.getTransposed().y().getVector3().getNormalised());

	matrix4_multiply_by_matrix4(local2tex, localToWorld);

	for (Winding::iterator i = w.begin(); i != w.end(); ++i) {
		Vector3 texcoord = matrix4_transformed_point(local2tex, (*i).vertex);
		(*i).texcoord[0] = texcoord[0];
		(*i).texcoord[1] = texcoord[1];

		(*i).tangent = tangent;
		(*i).bitangent = bitangent;
	}
}
