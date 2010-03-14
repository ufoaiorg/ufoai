#ifndef PARTICLE_H_
#define PARTICLE_H_

#include <memory>
#include <string>
#include "imodel.h"
#include "texturelib.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <time.h>

#define		MAX_PTLDEFS		256
#define		MAX_PTLCMDS		(MAX_PTLDEFS * 32)
#define		MAX_PCMD_DATA	(MAX_PTLCMDS * 8)
#define		MAX_STACK_DEPTH	8
#define		MAX_STACK_DATA	512

namespace scripts
{
	class Particle;
	class ParticleCommand;
	class ParticleDefinition;

	// Smart pointer typedef
	typedef std::auto_ptr<scripts::Particle> IParticlePtr;
	typedef std::auto_ptr<scripts::ParticleCommand> IParticleCommandPtr;
	typedef std::auto_ptr<scripts::ParticleDefinition> IParticleDefinitionPtr;

	class ParticleCommand
	{
		public:
			unsigned char cmd;
			unsigned char type;
			int ref;
	};

	class ParticleDefinition
	{
		public:
			std::string name;
			IParticleCommandPtr init;
			IParticleCommandPtr run;
			IParticleCommandPtr think;
			IParticleCommandPtr round;
			IParticleCommandPtr physics;
	};

	namespace
	{

		static IParticleDefinitionPtr ptlDef[MAX_PTLDEFS];
		static IParticleCommandPtr ptlCmd[MAX_PTLCMDS];

#if 0
		static int numPtlDefs;
		static int numPtlCmds;

		static unsigned char pcmdData[MAX_PCMD_DATA];
		static unsigned char *pcmdPos;

		static unsigned char cmdStack[MAX_STACK_DATA];
		static void *stackPtr[MAX_STACK_DEPTH];
		static unsigned char stackType[MAX_STACK_DEPTH];
#endif
	}

	class Particle
	{
		private:

			time_t msec;
			time_t frametime;

			model::IModelPtr model;
			qtexture_t *image;

			int blend;
			int style;
			Vector2 size;
			Vector3 scale;
			Vector4 color;
			Vector3 s; /**< current position */
			Vector3 origin; /**< start position - set initial s position to get this value */
			Vector3 offset;
			Vector3 angles;
			Vector3 lightColor;
			float lightIntensity;
			float lightSustain;
			int levelFlags;

			int skin; /**< model skin to use for this particle */

			IParticlePtr children; /**< list of children */
			IParticlePtr next; /**< next peer in list */
			IParticlePtr parent; /**< pointer to parent */

			ParticleDefinition ctrl;
			int startTime;
			int frame, endFrame;
			float fps; /**< how many frames per second (animate) */
			float lastFrame; /**< time (in seconds) when the think function was last executed (perhaps this can be used to delay or speed up particle actions). */
			float tps; /**< think per second - call the think function tps times each second, the first call at 1/tps seconds */
			float lastThink;
			unsigned char thinkFade, frameFade;
			float t; /**< time that the particle has been active already */
			float dt; /**< time increment for rendering this particle (delta time) */
			float life; /**< specifies how long a particle will be active (seconds) */
			int rounds; /**< specifies how many rounds a particle will be active */
			int roundsCnt;
			float scrollS;
			float scrollT;
			Vector3 a; /**< acceleration vector */
			Vector3 v; /**< velocity vector */
			Vector3 omega; /**< the rotation vector for the particle (newAngles = oldAngles + frametime * omega) */
			bool physics; /**< basic physics */
			bool autohide; /**< only draw the particle if the current position is
			 * not higher than the current level (useful for weather
			 * particles) */
			bool stayalive; /**< used for physics particles that hit the ground */
			bool weather; /**< used to identify weather particles (can be switched
			 * off via cvar cl_particleweather) */

		public:
			Particle ();

			virtual ~Particle ();

			void render ();

			std::string toString ();

			static scripts::IParticlePtr load (const std::string& particleID);
	};
}

#endif /* PARTICLE_H_ */
