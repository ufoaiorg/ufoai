#ifndef TEXTUREPROJECTION_H_
#define TEXTUREPROJECTION_H_

#include "itextstream.h"

/* greebo: A texture projection contains the texture definition
 * as well as the brush primitive texture definition.
 */
class TextureProjection
{
	public:
		TexDef m_texdef;
		Vector3 m_basis_s;
		Vector3 m_basis_t;

		// Constructor
		TextureProjection ()
		{
		}

		// Copy Constructor
		TextureProjection (const TexDef& texdef, const Vector3& basis_s, const Vector3& basis_t) :
			m_texdef(texdef), m_basis_s(basis_s), m_basis_t(basis_t)
		{
		}

		/* greebo: Uses the transformation matrix <transform> to set the internal texture
		 * definitions. Checks the matrix for validity and passes it on to
		 * the according internal texture definitions (TexDef or BPTexDef)
		 */
		void setTransform (float width, float height, const Matrix4& transform)
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
		Matrix4 getTransform (float width, float height) const
		{
			return m_texdef.getTransform(width, height);
		}

		// Normalise projection for a given texture width and height.
		void normalise (float width, float height)
		{
			m_texdef.normalise(width, height);
		}

};

#endif /*TEXTUREPROJECTION_H_*/
