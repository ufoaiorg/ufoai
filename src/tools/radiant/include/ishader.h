#ifndef ISHADER_H_
#define ISHADER_H_

#include "texturelib.h"

typedef unsigned char BlendFactor;
const BlendFactor BLEND_ZERO = 0;
const BlendFactor BLEND_ONE = 1;
const BlendFactor BLEND_SRC_COLOUR = 2;
const BlendFactor BLEND_ONE_MINUS_SRC_COLOUR = 3;
const BlendFactor BLEND_SRC_ALPHA = 4;
const BlendFactor BLEND_ONE_MINUS_SRC_ALPHA = 5;
const BlendFactor BLEND_DST_COLOUR = 6;
const BlendFactor BLEND_ONE_MINUS_DST_COLOUR = 7;
const BlendFactor BLEND_DST_ALPHA = 8;
const BlendFactor BLEND_ONE_MINUS_DST_ALPHA = 9;
const BlendFactor BLEND_SRC_ALPHA_SATURATE = 10;

class BlendFunc
{
	public:
		BlendFunc (BlendFactor src, BlendFactor dst) :
			m_src(src), m_dst(dst)
		{
		}
		BlendFactor m_src;
		BlendFactor m_dst;
};

class IShader
{
	public:

		enum EAlphaFunc
		{
			eAlways, eEqual, eLess, eGreater, eLEqual, eGEqual,
		};

		enum ECull
		{
			eCullNone, eCullBack,
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
		 * get/set the qtexture_t* Radiant uses to represent this shader object
		 */
		virtual qtexture_t* getTexture () const = 0;

		/**
		 * get shader name
		 */
		virtual const char* getName () const = 0;

		virtual bool IsInUse () const = 0;

		virtual void SetInUse (bool bInUse) = 0;

		virtual bool IsValid () const = 0;

		virtual void SetIsValid (bool bIsValid) = 0;

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
		virtual bool IsDefault () const = 0;

		/**
		 * get the alphaFunc
		 */
		virtual void getAlphaFunc (EAlphaFunc *func, float *ref) = 0;

		virtual BlendFunc getBlendFunc () const = 0;

		/**
		 * get the cull type
		 */
		virtual ECull getCull () = 0;
};

#endif /* ISHADER_H_ */
