#include "MaterialShader.h"
#include "imaterial.h"
#include "iscriplib.h"
#include "itextures.h"
#include "iscriplib.h"

#include "stream/stringstream.h"
#include "AutoPtr.h"

// constructor for empty material shader
MaterialShader::MaterialShader (const std::string& textureName) :
	_refcount(0), _fileName(textureName), _inUse(false), _isValid(false), _texture(0), _notfound(0)
{
	realise();
}

MaterialShader::MaterialShader (const std::string& fileName, const std::string& content) :
	_refcount(0), _fileName(fileName), _inUse(false), _isValid(false), _texture(0), _notfound(0)
{
	StringInputStream inputStream(content);
	AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().createSimpleTokeniser(inputStream));
	parseMaterial(*tokeniser);

	realise();
}

MaterialShader::~MaterialShader ()
{
	unrealise();
}

BlendFactor MaterialShader::parseBlendMode (const std::string& token)
{
	if (token == "GL_ONE")
		return BLEND_ONE;
	if (token == "GL_ZERO")
		return BLEND_ZERO;
	if (token == "GL_SRC_ALPHA")
		return BLEND_SRC_ALPHA;
	if (token == "GL_ONE_MINUS_SRC_ALPHA")
		return BLEND_ONE_MINUS_SRC_ALPHA;
	if (token == "GL_SRC_COLOR")
		return BLEND_SRC_COLOUR;
	if (token == "GL_DST_COLOR")
		return BLEND_DST_COLOUR;
	if (token == "GL_ONE_MINUS_SRC_COLOR")
		return BLEND_ONE_MINUS_SRC_COLOUR;
	if (token == "GL_ONE_MINUS_DST_COLOR")
		return BLEND_ONE_MINUS_DST_COLOUR;

	return BLEND_ZERO;
}

void MaterialShader::parseMaterial (Tokeniser& tokeniser)
{
	int depth = 0;
	Vector3 color(1, 1, 1);
	double alphaTest = 0.0f;
	bool terrain = false;
	GLTexture *layerTexture = 0;
	BlendFactor src = BLEND_SRC_ALPHA;
	BlendFactor dest = BLEND_ONE_MINUS_SRC_ALPHA;
	float ceilVal, floorVal;

	std::string token = tokeniser.getToken();
	while (token.length()) {
		if (token == "{") {
			depth++;
		} else if (token == "}") {
			--depth;
			if (depth == 0) {
				MapLayer layer(layerTexture, BlendFunc(src, dest), ShaderLayer::BLEND, color, alphaTest);
				if (terrain) {
					layer.setTerrain(floorVal, ceilVal);
					terrain = false;
				}
				color.set(1, 1, 1);
				addLayer(layer);
				break;
			}
		} else if (depth == 2) {
			if (token == "texture") {
				layerTexture = GlobalTexturesCache().capture(GlobalTexturePrefix_get() + tokeniser.getToken());
				if (layerTexture->texture_number == 0) {
					GlobalTexturesCache().release(layerTexture);
					layerTexture = GlobalTexturesCache().capture(GlobalTexturePrefix_get() + "tex_common/nodraw");
				}
			} else if (token == "blend") {
				src = parseBlendMode(tokeniser.getToken());
				dest = parseBlendMode(tokeniser.getToken());
			} else if (token == "lightmap") {
			} else if (token == "envmap") {
				token = tokeniser.getToken();
			} else if (token == "pulse") {
				token = tokeniser.getToken();
			} else if (token == "rotate") {
				token = tokeniser.getToken();
			} else if (token == "color") {
				const float red = string::toFloat(tokeniser.getToken());
				const float green = string::toFloat(tokeniser.getToken());
				const float blue = string::toFloat(tokeniser.getToken());
				color.set(red, green, blue);
			} else if (token == "flare") {
				token = tokeniser.getToken();
			} else if (token == "glowscale") {
				token = tokeniser.getToken();
			} else if (token == "anim" || token == "anima") {
				token = tokeniser.getToken();
				token = tokeniser.getToken();
			} else if (token == "terrain") {
				floorVal = string::toFloat(tokeniser.getToken());
				ceilVal = string::toFloat(tokeniser.getToken());
				terrain = true;
			}
		} else if (depth == 1) {
			if (token == "bump") {
				token = tokeniser.getToken();
			} else if (token == "parallax") {
				token = tokeniser.getToken();
			} else if (token == "hardness") {
				token = tokeniser.getToken();
			} else if (token == "specular") {
				token = tokeniser.getToken();
			} else if (token == "material") {
				token = tokeniser.getToken();
				// must be the same as _fileName - but without the texture dir
			}
		}
		token = tokeniser.getToken();
	}
}

// IShaders implementation -----------------
void MaterialShader::IncRef ()
{
	++_refcount;
}
void MaterialShader::DecRef ()
{
	if (--_refcount == 0) {
		delete this;
	}
}

std::size_t MaterialShader::refcount ()
{
	return _refcount;
}

GLTexture* MaterialShader::getTexture () const
{
	return _texture;
}

// get shader name
const std::string& MaterialShader::getName () const
{
	return _fileName;
}

bool MaterialShader::isInUse () const
{
	return _inUse;
}

void MaterialShader::setInUse (bool inUse)
{
	_inUse = inUse;
}

bool MaterialShader::isValid () const
{
	return _isValid;
}

void MaterialShader::setIsValid (bool isValid)
{
	_isValid = isValid;
}

// get the shader flags
int MaterialShader::getFlags () const
{
	return 0;
}

// get the transparency value
float MaterialShader::getTrans () const
{
	return 1.0f;
}

// test if it's a true shader, or a default shader created to wrap around a texture
bool MaterialShader::isDefault () const
{
	return _fileName.empty();
}

// get the alphaFunc
void MaterialShader::getAlphaFunc (MaterialShader::EAlphaFunc *func, float *ref)
{
	*func = eAlways;
	*ref = -1.0;
}

BlendFunc MaterialShader::getBlendFunc () const
{
	return BlendFunc(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);
}

// get the cull type
MaterialShader::ECull MaterialShader::getCull ()
{
	return eCullNone;
}

float MaterialShader::getPolygonOffset () const
{
	return 1.0f;
}

void MaterialShader::realise ()
{
	_texture = GlobalTexturesCache().capture(_fileName);

	if (_texture->texture_number == 0) {
		_notfound = _texture;
		_texture = GlobalTexturesCache().capture(GlobalTexturePrefix_get() + "tex_common/nodraw");
	}
}

void MaterialShader::unrealise ()
{
	GlobalTexturesCache().release(_texture);

	for (MapLayers::iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
		MapLayer& layer = *i;
		if (layer.getTexture() != 0) {
			GlobalTexturesCache().release(layer.getTexture());
		}
	}
	m_layers.clear();

	if (_notfound != 0) {
		GlobalTexturesCache().release(_notfound);
	}

	_texture = 0;
	_notfound = 0;
}

bool MaterialShader::isLayerValid (const MapLayer& layer) const
{
	if (layer.getType() == ShaderLayer::BLEND) {
		if (layer.getTexture() == 0 || layer.getTexture()->texture_number == 0)
			return false;
	}
	return true;
}

void MaterialShader::addLayer (MapLayer &layer)
{
	if (isLayerValid(layer))
		m_layers.push_back(layer);
}

bool MaterialShader::hasLayers() const
{
	return !m_layers.empty();
}

void MaterialShader::forEachLayer (const ShaderLayerCallback& callback) const
{
	for (MapLayers::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
		callback(*i);
	}
}
