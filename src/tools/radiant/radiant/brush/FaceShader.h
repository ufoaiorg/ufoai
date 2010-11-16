#ifndef FACESHADER_H_
#define FACESHADER_H_

#include <string>
#include "debugging/debugging.h"
#include "container/container.h"
#include "moduleobserver.h"
#include "irender.h"
#include "shaderlib.h"

#include "ContentsFlagsValue.h"

class FaceShaderObserver
{
	public:
		virtual ~FaceShaderObserver ()
		{
		}
		virtual void realiseShader () = 0;
		virtual void unrealiseShader () = 0;
};

// ----------------------------------------------------------------------------

// greebo: Two Visitor classes to realise/unrealise a FaceShader

class FaceShaderObserverRealise
{
	public:
		void operator() (FaceShaderObserver& observer) const
		{
			observer.realiseShader();
		}
};

class FaceShaderObserverUnrealise
{
	public:
		void operator() (FaceShaderObserver& observer) const
		{
			observer.unrealiseShader();
		}
};

// ----------------------------------------------------------------------------

typedef ReferencePair<FaceShaderObserver> FaceShaderObserverPair;

inline void brush_check_shader (const std::string& name)
{
	if (!shader_valid(name)) {
		globalErrorStream() << "brush face has invalid texture name: '" << name << "'\n";
	}
}

class FaceShader: public ModuleObserver
{
	public:
		class SavedState
		{
			public:
				std::string m_shader;
				ContentsFlagsValue m_flags;

				SavedState (const FaceShader& faceShader)
				{
					m_shader = faceShader.getShader();
					m_flags = faceShader.m_flags;
				}

				void exportState (FaceShader& faceShader) const
				{
					faceShader.setShader(m_shader);
					faceShader.setFlags(m_flags);
				}
		};

		std::string m_shader;
		Shader* m_state;
		ContentsFlagsValue m_flags;
		FaceShaderObserverPair m_observers;
		bool m_instanced;
		bool m_realised;

		FaceShader (const std::string& shader, const ContentsFlagsValue& flags = ContentsFlagsValue(0, 0, 0, false)) :
			m_shader(shader), m_state(0), m_flags(flags), m_instanced(false), m_realised(false)
		{
			captureShader();
		}
		~FaceShader ()
		{
			releaseShader();
		}
		// copy-construction not supported
		FaceShader (const FaceShader& other);

		void instanceAttach ()
		{
			m_instanced = true;
			m_state->incrementUsed();
		}
		void instanceDetach ()
		{
			m_state->decrementUsed();
			m_instanced = false;
		}

		void captureShader ()
		{
			ASSERT_MESSAGE(m_state == 0, "shader cannot be captured");
			brush_check_shader(m_shader);
			m_state = GlobalShaderCache().capture(m_shader);
			m_state->attach(*this);
		}
		void releaseShader ()
		{
			ASSERT_MESSAGE(m_state != 0, "shader cannot be released");
			m_state->detach(*this);
			GlobalShaderCache().release(m_shader);
			m_state = 0;
		}

		void realise ()
		{
			ASSERT_MESSAGE(!m_realised, "FaceTexdef::realise: already realised");
			m_realised = true;
			m_observers.forEach(FaceShaderObserverRealise());
		}
		void unrealise ()
		{
			ASSERT_MESSAGE(m_realised, "FaceTexdef::unrealise: already unrealised");
			m_observers.forEach(FaceShaderObserverUnrealise());
			m_realised = false;
		}

		void attach (FaceShaderObserver& observer)
		{
			m_observers.attach(observer);
			if (m_realised) {
				observer.realiseShader();
			}
		}

		void detach (FaceShaderObserver& observer)
		{
			if (m_realised) {
				observer.unrealiseShader();
			}
			m_observers.detach(observer);
		}

		const std::string& getShader () const
		{
			return m_shader;
		}
		void setShader (const std::string& name)
		{
			if (m_instanced) {
				m_state->decrementUsed();
			}
			releaseShader();
			m_shader = name;
			captureShader();
			if (m_instanced) {
				m_state->incrementUsed();
			}
		}
		ContentsFlagsValue getFlags () const
		{
			ASSERT_MESSAGE(m_realised, "FaceShader::getFlags: flags not valid when unrealised");
			if (!m_flags.m_specified) {
				return ContentsFlagsValue(m_state->getTexture().surfaceFlags, m_state->getTexture().contentFlags,
						m_state->getTexture().value, true);
			}
			return m_flags;
		}
		void setFlags (const ContentsFlagsValue& flags)
		{
			ASSERT_MESSAGE(m_realised, "FaceShader::setFlags: flags not valid when unrealised");
			m_flags.assignMasked(flags);
		}

		Shader* state () const
		{
			return m_state;
		}

		std::size_t width () const
		{
			if (m_realised) {
				return m_state->getTexture().width;
			}
			return 1;
		}
		std::size_t height () const
		{
			if (m_realised) {
				return m_state->getTexture().height;
			}
			return 1;
		}
		unsigned int shaderFlags () const
		{
			if (m_realised) {
				return m_state->getFlags();
			}
			return 0;
		}
}; // class FaceShader

#endif /*FACESHADER_H_*/
