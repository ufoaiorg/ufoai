#ifndef ISHADER_H_
#define ISHADER_H_

#include "texturelib.h"
#include "ishaderlayer.h"

class IShader
{
	public:

		enum EAlphaFunc
		{
			eAlways, eEqual, eLess, eGreater, eLEqual, eGEqual
		};

		enum ECull
		{
			eCullNone, eCullBack
		};

		virtual ~IShader ()
		{
		}

		/**
		 * Increment the number of references to this object
		 */
		virtual void IncRef () = 0;

		/**
		 * Decrement the reference count
		 */
		virtual void DecRef () = 0;

		/**
		 * get the texture pointer Radiant uses to represent this shader object
		 */
		virtual GLTexture* getTexture () const = 0;

		/**
		 * get shader name
		 */
		virtual const std::string& getName () const = 0;

		virtual bool isInUse () const = 0;

		virtual void setInUse (bool inUse) = 0;

		virtual bool isValid () const = 0;

		virtual void setIsValid (bool isValid) = 0;

		/**
		 * get the editor flags (QER_TRANS)
		 */
		virtual int getFlags () const = 0;

		/**
		 * get the transparency value
		 */
		virtual float getTrans () const = 0;

		/**
		 * test if it's a true shader, or a default shader created to wrap around a texture
		 */
		virtual bool isDefault () const = 0;

		/**
		 * get the alphaFunc
		 */
		virtual void getAlphaFunc (EAlphaFunc *func, float *ref) = 0;

		virtual BlendFunc getBlendFunc () const = 0;

		virtual bool hasLayers() const = 0;

		virtual void forEachLayer(const ShaderLayerCallback& layer) const = 0;

		/**
		 * \brief
		 * Return a polygon offset if one is defined. The default is 0.
		 */
		float getPolygonOffset () const;

		/**
		 * get the cull type
		 */
		virtual ECull getCull () = 0;
};

#endif /* ISHADER_H_ */
