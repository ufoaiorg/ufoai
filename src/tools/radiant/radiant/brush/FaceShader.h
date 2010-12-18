#ifndef FACESHADER_H_
#define FACESHADER_H_

#include <string>
#include "container/container.h"
#include "moduleobserver.h"
#include "irender.h"

class FaceShaderObserver
{
	public:
		virtual ~FaceShaderObserver ()
		{
		}
		virtual void realiseShader () = 0;
		virtual void unrealiseShader () = 0;

		virtual void shaderChanged () {};
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

typedef ReferencePair<FaceShaderObserver> FaceShaderObserverPair;

#include "ContentsFlagsValue.h"

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

		FaceShader (const std::string& shader, const ContentsFlagsValue& flags = ContentsFlagsValue(0, 0, 0, false));
		~FaceShader ();
		// copy-construction not supported
		FaceShader (const FaceShader& other);

		void instanceAttach ();
		void instanceDetach ();

		void captureShader ();
		void releaseShader ();

		void realise ();
		void unrealise ();

		void attach (FaceShaderObserver& observer);
		void detach (FaceShaderObserver& observer);

		const std::string& getShader () const;
		void setShader (const std::string& name);
		const ContentsFlagsValue& getFlags () const;
		void setFlags (const ContentsFlagsValue& flags);

		Shader* state () const;

		std::size_t width () const;
		std::size_t height () const;
		unsigned int shaderFlags () const;
}; // class FaceShader

#endif /*FACESHADER_H_*/
