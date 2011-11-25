#include "OpenGLShader.h"
#include "OpenGLShaderPass.h"
#include "OpenGLStateLess.h"
#include "OpenGLStateManager.h"

#include "ishadersystem.h"
#include "irender.h"
#include "ishader.h"

#include "string/string.h"
#include "../../plugin.h"

OpenGLShader::OpenGLShader (render::OpenGLStateManager& glStateManager) :
	m_shader(0), m_used(0), _glStateManager(glStateManager)
{
}

OpenGLShader::~OpenGLShader ()
{
}

void OpenGLShader::destroy ()
{
	if (m_shader) {
		m_shader->DecRef();
	}
	m_shader = 0;

	for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
		delete *i;
	}
	m_passes.clear();
}

void OpenGLShader::addRenderable (const OpenGLRenderable& renderable, const Matrix4& modelview)
{
	for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
		(*i)->addRenderable(renderable, modelview);
	}
}

void OpenGLShader::incrementUsed ()
{
	if (++m_used == 1 && m_shader != 0) {
		m_shader->setInUse(true);
	}
}

void OpenGLShader::decrementUsed ()
{
	if (--m_used == 0 && m_shader != 0) {
		m_shader->setInUse(false);
	}
}

bool OpenGLShader::realised () const
{
	return m_shader != 0;
}

void OpenGLShader::attach (ModuleObserver& observer)
{
	if (realised()) {
		observer.realise();
	}
	m_observers.attach(observer);
}

void OpenGLShader::detach (ModuleObserver& observer)
{
	if (realised()) {
		observer.unrealise();
	}
	m_observers.detach(observer);
}

void OpenGLShader::realise (const std::string& name)
{
	if (!name.empty())
		construct(name);

	if (m_used != 0 && m_shader != 0) {
		m_shader->setInUse(true);
	}

	for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
		_glStateManager.insertSortedState(render::OpenGLStates::value_type(&(*i)->getState(), *i));
	}

	m_observers.realise();
}

void OpenGLShader::unrealise ()
{
	m_observers.unrealise();

	for (Passes::iterator i = m_passes.begin(); i != m_passes.end(); ++i) {
		_glStateManager.eraseSortedState(&(*i)->getState());
	}

	destroy();
}

GLTexture& OpenGLShader::getTexture () const
{
	ASSERT_NOTNULL(m_shader);
	return *m_shader->getTexture();
}

unsigned int OpenGLShader::getFlags () const
{
	ASSERT_NOTNULL(m_shader);
	return m_shader->getFlags();
}

IShader& OpenGLShader::getShader () const
{
	ASSERT_NOTNULL(m_shader);
	return *m_shader;
}

OpenGLState& OpenGLShader::appendDefaultPass ()
{
	m_passes.push_back(new OpenGLShaderPass);
	OpenGLState& state = m_passes.back()->getState();
	state.constructDefault();
	return state;
}

GLenum OpenGLShader::convertBlendFactor (BlendFactor factor)
{
	switch (factor) {
	case BLEND_ZERO:
		return GL_ZERO;
	case BLEND_ONE:
		return GL_ONE;
	case BLEND_SRC_COLOUR:
		return GL_SRC_COLOR;
	case BLEND_ONE_MINUS_SRC_COLOUR:
		return GL_ONE_MINUS_SRC_COLOR;
	case BLEND_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case BLEND_ONE_MINUS_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case BLEND_DST_COLOUR:
		return GL_DST_COLOR;
	case BLEND_ONE_MINUS_DST_COLOUR:
		return GL_ONE_MINUS_DST_COLOR;
	case BLEND_DST_ALPHA:
		return GL_DST_ALPHA;
	case BLEND_ONE_MINUS_DST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case BLEND_SRC_ALPHA_SATURATE:
		return GL_SRC_ALPHA_SATURATE;
	}
	return GL_ZERO;
}

// Append a blend (non-interaction) layer
void OpenGLShader::appendBlendLayer (const ShaderLayer& layer)
{
	GLTexture* layerTex = layer.getTexture();

	OpenGLState& state = appendDefaultPass();
	state.m_state = RENDER_FILL | RENDER_BLEND | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_COLOURCHANGE | RENDER_TEXTURE_2D;
	state.m_depthfunc = GL_LEQUAL;

	// Set the texture
	state.m_texture = layerTex->texture_number;

	// Get the blend function
	BlendFunc blendFunc = layer.getBlendFunc();
	state.m_blend_src = convertBlendFactor(blendFunc.m_src);
	state.m_blend_dst = convertBlendFactor(blendFunc.m_dst);

	// Alpha-tested stages or one-over-zero blends should use the depth buffer
	if (state.m_blend_src == GL_SRC_ALPHA || state.m_blend_dst == GL_SRC_ALPHA || (state.m_blend_src == GL_ONE
			&& state.m_blend_dst == GL_ZERO)) {
		state.m_state |= RENDER_DEPTHWRITE;
	}

	// Colour modulation
	state.m_colour = Vector4(layer.getColour(), 1.0f);

	state.m_sort = OpenGLState::eSortOverlayFirst;

	// Polygon offset
	state.m_polygonOffset = layer.getPolygonOffset();
}

void OpenGLShader::visitShaderLayers (const ShaderLayer& layer)
{
	if (layer.getType() == ShaderLayer::BLEND)
		appendBlendLayer(layer);
}

/// \todo Define special-case shaders in a data file.
void OpenGLShader::construct (const std::string& name)
{
	static Vector4 highLightColour(1, 0, 0, 0.3);

	// Check the first character of the name to see if this is a special built-in shader
	switch (name[0]) {
	case '(': {
		// fill shader
		OpenGLState& state = appendDefaultPass();
		sscanf(name.c_str(), "(%g %g %g)", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2]);
		state.m_colour[3] = 1.0f;
		state.m_state = RENDER_FILL | RENDER_LIGHTING | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_COLOURWRITE
				| RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortFullbright;
		break;
	}

	case '[': {
		OpenGLState& state = appendDefaultPass();
		sscanf(name.c_str(), "[%g %g %g]", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2]);
		state.m_colour[3] = 0.5f;
		state.m_state = RENDER_FILL | RENDER_LIGHTING | RENDER_DEPTHTEST | RENDER_CULLFACE | RENDER_COLOURWRITE
				| RENDER_DEPTHWRITE | RENDER_BLEND;
		state.m_sort = OpenGLState::eSortTranslucent;
		break;
	}

	case '<': {
		// wireframe shader
		OpenGLState& state = appendDefaultPass();
		sscanf(name.c_str(), "<%g %g %g>", &state.m_colour[0], &state.m_colour[1], &state.m_colour[2]);
		state.m_colour[3] = 1;
		state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortFullbright;
		state.m_depthfunc = GL_LESS;
		state.m_linewidth = 1;
		state.m_pointsize = 1;
		break;
	}

	case '$': {
		OpenGLState& state = appendDefaultPass();

		if (name == "$POINT") {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst;
			state.m_pointsize = 4;
		} else if (name == "$SELPOINT") {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortControlFirst + 1;
			state.m_pointsize = 4;
		} else if (name == "$PIVOT") {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHTEST | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_linewidth = 2;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHTEST | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_linewidth = 2;
			hiddenLine.m_depthfunc = GL_GREATER;
		} else if (name == "$WIREFRAME") {
			state.m_state = RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFullbright;
		} else if (name == "$CAM_HIGHLIGHT") {
			state.m_colour = highLightColour;
			state.m_state = RENDER_FILL | RENDER_DEPTHTEST | RENDER_CULLFACE
					| RENDER_BLEND | RENDER_COLOURWRITE;
			state.m_sort = OpenGLState::eSortHighlight;
			state.m_depthfunc = GL_LEQUAL;
		} else if (name == "$CAM_OVERLAY") {
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_DEPTHWRITE
					| RENDER_OFFSETLINE;
			state.m_sort = OpenGLState::eSortOverlayFirst + 1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_colour = Vector4(0.75, 0.75, 0.75, 1);
			hiddenLine.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_OFFSETLINE
					| RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortOverlayFirst;
			hiddenLine.m_depthfunc = GL_GREATER;
			hiddenLine.m_linestipple_factor = 2;
		} else if (name == "$XY_OVERLAY") {
			Vector3 colorSelBrushes = ColourSchemes().getColourVector3("selected_brush");
			state.m_colour = Vector4(colorSelBrushes, 1);
			state.m_state = RENDER_COLOURWRITE | RENDER_LINESTIPPLE;
			state.m_sort = OpenGLState::eSortOverlayFirst;
			state.m_linewidth = 2;
			state.m_linestipple_factor = 3;
		} else if (name == "$DEBUG_CLIPPED") {
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortLast;
		} else if (name == "$Q3MAP2_LIGHT_SPHERE") {
			state.m_colour =  Vector4(.05f, .05f, .05f, 1);
			state.m_state = RENDER_CULLFACE | RENDER_DEPTHTEST | RENDER_BLEND | RENDER_FILL;
			state.m_blend_src = GL_ONE;
			state.m_blend_dst = GL_ONE;
			state.m_sort = OpenGLState::eSortTranslucent;
		} else if (name == "$WIRE_OVERLAY") {
#if 0
			state._visStack = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OVERRIDE;
			state.m_sort = OpenGLState::eSortOverlayFirst;
#else
			state.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST
					| RENDER_OVERRIDE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST
					| RENDER_OVERRIDE | RENDER_LINESTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_depthfunc = GL_GREATER;
#endif
		} else if (name == "$FLATSHADE_OVERLAY") {
			state.m_state = RENDER_CULLFACE | RENDER_LIGHTING | RENDER_SMOOTH | RENDER_SCALED | RENDER_COLOURARRAY
					| RENDER_FILL | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OVERRIDE;
			state.m_sort = OpenGLState::eSortGUI1;
			state.m_depthfunc = GL_LEQUAL;

			OpenGLState& hiddenLine = appendDefaultPass();
			hiddenLine.m_state = RENDER_CULLFACE | RENDER_LIGHTING | RENDER_SMOOTH | RENDER_SCALED | RENDER_COLOURARRAY
					| RENDER_FILL | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST | RENDER_OVERRIDE
					| RENDER_POLYGONSTIPPLE;
			hiddenLine.m_sort = OpenGLState::eSortGUI0;
			hiddenLine.m_depthfunc = GL_GREATER;
		} else if (name == "$CLIPPER_OVERLAY") {
			Vector3 colorClipper = ColourSchemes().getColourVector3("clipper");
			state.m_colour = Vector4(colorClipper, 1);
			state.m_state = RENDER_CULLFACE | RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_FILL
					| RENDER_POLYGONSTIPPLE;
			state.m_sort = OpenGLState::eSortOverlayFirst;
		} else {
			// default to something recognisable.. =)
			ERROR_MESSAGE("hardcoded renderstate not found");
			state.m_colour = Vector4(1, 0, 1, 1);
			state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
			state.m_sort = OpenGLState::eSortFirst;
		}
		break;
	} // case '$'

	default: {
		// This is not a hard-coded shader, construct from the shader system
		constructNormalShader(name);
	}

	} // switch
}

bool OpenGLShader::isTransparent (const std::string& name)
{
	return string::contains(name, "tex_common/");
}

void OpenGLShader::constructNormalShader (const std::string& name)
{
	// construction from IShader
	m_shader = GlobalShaderSystem().getShaderForName(name);

	OpenGLState& state = appendDefaultPass();

	state.m_texture = m_shader->getTexture()->texture_number;

	state.m_state = RENDER_FILL | RENDER_TEXTURE_2D | RENDER_DEPTHTEST | RENDER_COLOURWRITE | RENDER_LIGHTING
			| RENDER_SMOOTH;
	state.m_state |= RENDER_CULLFACE;
	if ((m_shader->getFlags() & QER_ALPHATEST) != 0) {
		state.m_state |= RENDER_ALPHATEST;
		state.m_colour[3] = 0.25;
		IShader::EAlphaFunc alphafunc;
		m_shader->getAlphaFunc(&alphafunc, &state.m_alpharef);
		switch (alphafunc) {
		case IShader::eAlways:
			state.m_alphafunc = GL_ALWAYS;
		case IShader::eEqual:
			state.m_alphafunc = GL_EQUAL;
		case IShader::eLess:
			state.m_alphafunc = GL_LESS;
		case IShader::eGreater:
			state.m_alphafunc = GL_GREATER;
		case IShader::eLEqual:
			state.m_alphafunc = GL_LEQUAL;
		case IShader::eGEqual:
			state.m_alphafunc = GL_GEQUAL;
		}
	}

	state.m_colour = Vector4(m_shader->getTexture()->color, 1.0);

	if (isTransparent(m_shader->getName())) {
		state.m_state |= RENDER_BLEND;
		state.m_state |= RENDER_DEPTHWRITE;
		state.m_colour = Vector4(1.0, 1.0, 1.0, 0.4);
		state.m_sort = OpenGLState::eSortTranslucent;
	} else if ((m_shader->getFlags() & QER_TRANS) != 0) {
		state.m_state |= RENDER_BLEND;
		state.m_colour[3] = m_shader->getTrans();
		BlendFunc blendFunc = m_shader->getBlendFunc();
		state.m_blend_src = convertBlendFactor(blendFunc.m_src);
		state.m_blend_dst = convertBlendFactor(blendFunc.m_dst);
		if (state.m_blend_src == GL_SRC_ALPHA || state.m_blend_dst == GL_SRC_ALPHA) {
			state.m_state |= RENDER_DEPTHWRITE;
		}
		state.m_sort = OpenGLState::eSortTranslucent;
	} else if (m_shader->getTexture()->hasAlpha) {
		state.m_state |= RENDER_BLEND;
		state.m_state |= RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortTranslucent;
	} else {
		state.m_state |= RENDER_DEPTHWRITE;
		state.m_sort = OpenGLState::eSortFullbright;
	}

	m_shader->forEachLayer(ShaderLayerVisitor(*this));
}
