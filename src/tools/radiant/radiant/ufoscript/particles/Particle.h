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
#include "../common/ScriptValues.h"

#define		MAX_PTLDEFS		256
#define		MAX_PTLCMDS		(MAX_PTLDEFS * 32)
#define		MAX_PCMD_DATA	(MAX_PTLCMDS * 8)
#define		MAX_STACK_DEPTH	8
#define		MAX_STACK_DATA	512

#define RADR(x)		((x < 0) ? (byte*)p - x : (byte*) pcmdData + x)
#define RSTACK		-0xFFF0
#define F(x)		(1<<x)
#define	V_VECS		(F(V_FLOAT) | F(V_POS) | F(V_VECTOR) | F(V_COLOR))
#define PTL_ONLY_ONE_TYPE		(1<<31)
#define V_UNTYPED   0x7FFF

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

		enum
		{
			V_FLOAT, V_POS, V_COLOR, V_STRING, V_INT, V_BOOL, V_NUM_TYPES, V_VECTOR
		};

		static const char * vt_names[V_NUM_TYPES] = {"float", "pos", "color", "string", "int", "bool"};

		typedef enum pc_s
		{
			PC_END,

			PC_PUSH,
			PC_POP,
			PC_KPOP,
			PC_ADD,
			PC_SUB,
			PC_MUL,
			PC_DIV,
			PC_SIN,
			PC_COS,
			PC_TAN,
			PC_RAND,
			PC_CRAND,
			PC_V2,
			PC_V3,
			PC_V4,

			PC_KILL,
			PC_SPAWN,
			PC_NSPAWN,
			PC_CHILD,

			PC_NUM_PTLCMDS
		} pc_t;

		/** @brief particle commands - see pc_t */
		static const char *pc_strings[PC_NUM_PTLCMDS] = { "end",

		"push", "pop", "kpop", "add", "sub", "mul", "div", "sin", "cos", "tan", "rand", "crand", "v2", "v3", "v4",

		"kill", "spawn", "nspawn", "child" };

		/** @brief particle commands parameter and types */
		static const int pc_types[PC_NUM_PTLCMDS] = { 0,

		V_UNTYPED, V_UNTYPED, V_UNTYPED, V_VECS, V_VECS, V_VECS, V_VECS, PTL_ONLY_ONE_TYPE | V_FLOAT, PTL_ONLY_ONE_TYPE
				| V_FLOAT, PTL_ONLY_ONE_TYPE | V_FLOAT, V_VECS, V_VECS, 0, 0, 0,

		0, PTL_ONLY_ONE_TYPE | V_STRING, PTL_ONLY_ONE_TYPE | V_STRING, PTL_ONLY_ONE_TYPE | V_STRING };

		/** used e.g. in our parsers */
		typedef struct value_s
		{
				const char *string;
				const int type;
				const size_t ofs;
		} value_t;

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

	struct ParticleData
	{
			time_t msec;
			time_t frametime;

			model::IModelPtr modelPtr;
			qtexture_t *imagePtr;

			std::string model;
			std::string image;

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
	};

	class Particle
	{
		private:

			ParticleData _data;

		public:
			Particle ();

			virtual ~Particle ();

			void render ();

			std::string toString ();

	};
	scripts::IParticlePtr loadParticle (const std::string& particleID);
}

#endif /* PARTICLE_H_ */
