#include "ishader.h"
#include "ishaderlayer.h"

#include "texturelib.h"
#include "shaderlib.h"

class Tokeniser;

class MapLayer: public ShaderLayer
{
	private:
		GLTexture* m_texture;
		BlendFunc m_blendFunc;
		double m_alphaTest;
		ShaderLayer::Type m_type;
		Vector3 m_color;
		float _height;
		float _ceilVal;
		float _floorVal;
		bool _terrain;
		float _polygonOffset;
	public:
		MapLayer (GLTexture* texture, BlendFunc blendFunc, ShaderLayer::Type type, Vector3& color, double alphaTest) :
			m_texture(texture), m_blendFunc(blendFunc), m_alphaTest(alphaTest), m_type(type), m_color(color),
					_height(0), _ceilVal(0), _floorVal(0), _terrain(false), _polygonOffset(0.0)
		{
		}

		Type getType () const
		{
			return m_type;
		}

		Vector3 getColour () const
		{
			return m_color;
		}

		GLTexture* getTexture () const
		{
			return m_texture;
		}

		BlendFunc getBlendFunc () const
		{
			return m_blendFunc;
		}

		double getAlphaTest () const
		{
			return m_alphaTest;
		}

		void setTerrain (float floorVal, float ceilVal)
		{
			_floorVal = floorVal;
			_ceilVal = ceilVal;
			_height = _ceilVal - _floorVal;
			_terrain = true;
		}

		bool isTerrain ()
		{
			return _terrain;
		}

		float getTerrainAlpha (float z) const
		{
			if (z < _floorVal)
				return 0.0;
			else if (z > _ceilVal)
				return 1.0;
			else
				return (z - _floorVal) / _height;
		}

		float getPolygonOffset () const
		{
			return _polygonOffset;
		}

		void setPolygonOffset (float polygonOffset)
		{
			_polygonOffset = polygonOffset;
		}
};

class MaterialShader: public IShader
{
	private:

		std::size_t _refcount;

		std::string _fileName;

		bool _inUse;

		bool _isValid;

		GLTexture* _texture;

		GLTexture* _notfound;

		typedef std::vector<MapLayer> MapLayers;
		MapLayers m_layers;

		BlendFactor parseBlendMode (const std::string& token);
		void parseMaterial (Tokeniser& tokenizer);

		bool searchLicense () const;

	public:

		MaterialShader (const std::string& textureName);
		MaterialShader (const std::string& fileName, const std::string& content);

		virtual ~MaterialShader ();

		// IShaders implementation -----------------
		void IncRef ();
		void DecRef ();

		std::size_t refcount ();

		// get the texture pointer Radiant uses to represent this shader object
		GLTexture* getTexture () const;

		// get shader name
		const std::string& getName () const;

		bool isInUse () const;

		void setInUse (bool inUse);

		bool isValid () const;

		void setIsValid (bool bIsValid);

		// get the shader flags
		int getFlags () const;

		// get the transparency value
		float getTrans () const;

		// test if it's a true shader, or a default shader created to wrap around a texture
		bool isDefault () const;

		// get the alphaFunc
		void getAlphaFunc (EAlphaFunc *func, float *ref);

		BlendFunc getBlendFunc () const;

		// get the cull type
		ECull getCull ();

		void realise ();

		void unrealise ();

		void addLayer (MapLayer &layer);

		bool isLayerValid (const MapLayer& layer) const;

		void forEachLayer (const ShaderLayerCallback& callback) const;

		bool hasLayers() const;

		float getPolygonOffset () const;
};
