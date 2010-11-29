#ifndef OPENGLSHADER_H_
#define OPENGLSHADER_H_

#include "irender.h"
#include "iglrender.h"
#include "ishader.h"
#include "moduleobservers.h"

#include <list>

class OpenGLShaderPass;
namespace render { class OpenGLStateManager; }

class OpenGLShader: public Shader {
	private:
		typedef std::list<OpenGLShaderPass*> Passes;
		Passes m_passes;
		IShader* m_shader;
		std::size_t m_used;
		ModuleObservers m_observers;

		// The state manager we will be inserting/removing OpenGL states from (this
		// will be the OpenGLRenderSystem).
		render::OpenGLStateManager& _glStateManager;

	public:
		OpenGLShader(render::OpenGLStateManager& glStateManager);

		~OpenGLShader();

		void destroy();
		void addRenderable(const OpenGLRenderable& renderable, const Matrix4& modelview);
		void incrementUsed();
		void decrementUsed();
		bool realised() const;
		void attach(ModuleObserver& observer);
		void detach(ModuleObserver& observer);
		void realise(const std::string& name);
		void unrealise();
		GLTexture& getTexture() const;
		unsigned int getFlags() const;
		IShader& getShader() const;
		OpenGLState& appendDefaultPass();

		GLenum convertBlendFactor(BlendFactor factor);

		// Append a blend (non-interaction) layer
		void appendBlendLayer(const ShaderLayer& layer);

		void visitShaderLayers(const ShaderLayer& layer);

		typedef MemberCaller1<OpenGLShader, const ShaderLayer&, &OpenGLShader::visitShaderLayers>
				ShaderLayerVisitor;

		/// \todo Define special-case shaders in a data file.
		void construct(const std::string& name);

		// return true if it's a common texture (tex_common)
		bool isTransparent(const std::string& name);

		void constructNormalShader(const std::string& name);
};

#endif /* OPENGLSHADER_H_ */
