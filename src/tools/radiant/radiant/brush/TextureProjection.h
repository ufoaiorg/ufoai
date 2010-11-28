#ifndef TEXTUREPROJECTION_H_
#define TEXTUREPROJECTION_H_

#include "itextstream.h"
#include "TexDef.h"

class Winding;

/* greebo: A texture projection contains the texture definition
 */
class TextureProjection
{
	private:
		void basisForNormal (const Vector3& normal, Matrix4& basis);

	public:
		TexDef m_texdef;

		// Constructor
		TextureProjection ();

		// Copy Constructor
		TextureProjection (const TexDef& texdef);

		void constructDefault ();

		/* greebo: Uses the transformation matrix <transform> to set the internal texture
		 * definitions. Checks the matrix for validity and passes it on to
		 * the according internal texture definitions (TexDef or BPTexDef)
		 */
		void setTransform (float width, float height, const Matrix4& transform);

		/* greebo: Returns the transformation matrix from the
		 * texture definitions members.
		 */
		Matrix4 getTransform (float width, float height) const;

		void shift(float s, float t);
		void scale(float s, float t);
		void rotate(float angle);

		// Normalise projection for a given texture width and height.
		void normalise (float width, float height);

		void fitTexture (std::size_t width, std::size_t height, const Vector3& normal, const Winding& w,
				float s_repeat, float t_repeat);

		/** greebo: Mirrors the texture around the given axis.
		 *
		 * @param flipAxis: 0 = flip x, 1 = flip y
		 */
		void flipTexture(unsigned int flipAxis);

		void transformLocked (std::size_t width, std::size_t height, const Plane3& plane,
				const Matrix4& identity2transformed);

		void emitTextureCoordinates (std::size_t width, std::size_t height, Winding& w, const Vector3& normal,
				const Matrix4& localToWorld);
};

#endif /*TEXTUREPROJECTION_H_*/
