/**
 * @file particles.h
 */

#ifndef PARTICLES_H
#define PARTICLES_H

#include "imagelib.h"
#include "scenelib.h"
#include "archivelib.h"
#include "moduleobservers.h"
#include "AutoPtr.h"
#include "stringio.h"
#include "ifilesystem.h"
#include "itextures.h"
#include "imodel.h"
#include "stream/stringstream.h"
#include "iradiant.h"
#include <map>
#include <list>

/** possible blend modes - see also blend_names */
typedef enum
{
	BLEND_REPLACE, BLEND_ONE_PARTICLE, BLEND_BLEND, BLEND_ADD, BLEND_FILTER, BLEND_INVFILTER,

	BLEND_LAST
} blend_t;

class ParticleDefinition
{
	private:
		std::string _id;
		std::string _modelName;
		std::string _imageName;
		int _blend;
		int _width;
		int _height;

	public:
		ParticleDefinition (const std::string& id) :
			_id(id), _modelName(""), _imageName("")
		{
			_blend = BLEND_REPLACE;
			_width = 0;
			_height = 0;
		}

		ParticleDefinition (const std::string& id, const std::string& modelName, const std::string& imageName,
				int blend, int width, int height) :
			_id(id), _modelName(modelName), _imageName(imageName)
		{
			_blend = blend;
			_width = width;
			_height = height;
		}

		/** @brief copy constructor */
		ParticleDefinition (ParticleDefinition const& copy) :
			_id(copy._id), _modelName(copy._modelName), _imageName(copy._imageName)
		{
			_blend = copy._blend;
			_width = copy._width;
			_height = copy._height;
		}

		~ParticleDefinition ()
		{
		}

		const std::string& getID () const
		{
			return _id;
		}

		const std::string& getImage () const
		{
			return _imageName;
		}

		const std::string& getModel () const
		{
			return _modelName;
		}

		int getBlendMode () const
		{
			return _blend;
		}

		int getWidth () const
		{
			return _width;
		}

		int getHeight () const
		{
			return _height;
		}

		void particleChanged (const std::string& id)
		{
			g_warning("TODO: Implement particle changing\n");
		}
		typedef MemberCaller1<ParticleDefinition, const std::string&, &ParticleDefinition::particleChanged>
				ParticleChangedCaller;
};

typedef std::map<std::string, ParticleDefinition> ParticleDefinitionMap;

extern ParticleDefinitionMap g_particleDefinitions;

void Particles_Construct (void);
void Particles_Destroy (void);

#endif
