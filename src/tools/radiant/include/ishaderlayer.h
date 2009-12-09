#ifndef ISHADERLAYER_H_
#define ISHADERLAYER_H_

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
		virtual qtexture_t* getTexture () const = 0;

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
};

#endif /* ISHADERLAYER_H_ */
