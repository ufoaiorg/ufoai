#ifndef ISHADERLAYER_H_
#define ISHADERLAYER_H_

#include "generic/callback.h"

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
		BlendFunc (const BlendFunc& func) :
			m_src(func.m_src), m_dst(func.m_dst) {
		}
		BlendFunc (BlendFactor src, BlendFactor dst) :
			m_src(src), m_dst(dst)
		{
		}
		BlendFactor m_src;
		BlendFactor m_dst;
};

/**
 * \brief
 * A single layer of a material shader.
 *
 * Each shader layer contains an image texture, a blend mode (e.g. add,
 * modulate) and various other data.
 */
class ShaderLayer
{
	public:

		/**
		 * \brief
		 * Enumeration of layer types.
		 */
		enum Type
		{
			DIFFUSE, BLEND
		};

		/**
		 * \brief
		 * Destructor
		 */
		virtual ~ShaderLayer ()
		{
		}

		/**
		 * \brief
		 * Return the layer type.
		 */
		virtual Type getType () const = 0;

		/**
		 * \brief
		 * Return the Texture object corresponding to this layer (may be NULL).
		 */
		virtual GLTexture* getTexture () const = 0;

		/**
		 * \brief
		 * Return the GL blend function for this layer.
		 *
		 * Only layers of type BLEND use a BlendFunc. Layers of type DIFFUSE, BUMP
		 * and SPECULAR do not use blend functions.
		 */
		virtual BlendFunc getBlendFunc () const = 0;

		/**
		 * \brief
		 * Multiplicative layer colour (set with "red 0.6", "green 0.2" etc)
		 */
		virtual Vector3 getColour () const = 0;

		/**
		 * \brief
		 * Get the alpha test value for this layer.
		 *
		 * \return
		 * The alpha test value, between 0.0 and 1.0 if it is set. If no alpha test
		 * value is set, -1 will be returned.
		 */
		virtual double getAlphaTest () const = 0;

		/**
		 * \brief
		 * Return a polygon offset if one is defined. The default is 0.
		 */
		virtual float getPolygonOffset () const = 0;
};

typedef Callback1<const ShaderLayer&> ShaderLayerCallback;

#endif /* ISHADERLAYER_H_ */
