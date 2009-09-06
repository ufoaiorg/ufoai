/**
 * @file particles.h
 */

#ifndef PARTICLES_H
#define PARTICLES_H

#include "imagelib.h"
#include "scenelib.h"
#include "archivelib.h"
#include "moduleobservers.h"
#include "autoptr.h"
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
		bool m_copy;
		void loadParticleTexture (void)
		{
			if (m_image)
				return;

			if (m_imageName) {
				StringOutputStream name(256);
				name << "pics/" << m_imageName;
				m_copy = false;
				m_image = GlobalTexturesCache().capture(name.c_str());
				if (!m_image)
					g_warning("Particle image: pics/%s wasn't found\n", m_imageName);
			}
		}

		void freeParticleTexture (void)
		{
			if (m_image && !m_copy) {
				GlobalTexturesCache().release(m_image);
				m_image = (qtexture_t *) 0;
			}
		}
	public:
		ParticleDefinition (const char *id) :
			m_copy(false), m_modelName((char *) 0), m_imageName((char *) 0), m_image((qtexture_t *) 0), m_model(
					(NodeSmartReference*) 0)
		{
			m_id = strdup(id);
			m_blend = BLEND_REPLACE;
			m_width = 0;
			m_height = 0;
		}

		ParticleDefinition (const char* id, const char *modelName, const char *imageName, int blend, int width,
				int height) :
			m_copy(false), m_image((qtexture_t *) 0), m_model((NodeSmartReference*) 0)
		{
			m_id = strdup(id);
			m_blend = blend;
			m_width = width;
			m_height = height;

			if (imageName != (char *) 0)
				m_imageName = strdup(imageName);
			else
				m_imageName = (char *) 0;

			if (modelName != (char *) 0)
				m_modelName = strdup(modelName);
			else
				m_modelName = (char *) 0;

			loadParticleTexture();
		}

		/** @brief copy constructor */
		ParticleDefinition (ParticleDefinition const& copy) :
			m_copy(true), m_image(copy.m_image), m_model(copy.m_model)
		{
			m_id = strdup(copy.m_id);
			m_blend = copy.m_blend;
			m_width = copy.m_width;
			m_height = copy.m_height;

			if (copy.m_imageName != (char *) 0)
				m_imageName = strdup(copy.m_imageName);
			else
				m_imageName = (char *) 0;

			if (copy.m_modelName != (char *) 0)
				m_modelName = strdup(copy.m_modelName);
			else
				m_modelName = (char *) 0;
		}

		~ParticleDefinition ()
		{
			freeParticleTexture();
			if (m_model != (NodeSmartReference *) 0)
				delete m_model;
			if (m_id != (char *) 0)
				free(m_id);
			if (m_modelName != (char *) 0)
				free(m_modelName);
			if (m_imageName != (char *) 0)
				free(m_imageName);
		}

		char *m_id;
		char *m_modelName;
		char *m_imageName;
		qtexture_t *m_image;
		int m_blend;
		int m_width;
		int m_height;
		NodeSmartReference *m_model;

		void loadModel (ModelLoader *loader, ArchiveFile &file)
		{
			m_model = new NodeSmartReference(loader->loadModel(file));
			m_model->get().m_isRoot = true;
		}

		void loadTextureForCopy ()
		{
			if (!m_copy)
				return;

			/* this reference was not captured */
			m_image = (qtexture_t *) 0;
			loadParticleTexture();
		}

		void particleChanged (const char* id)
		{
			g_warning("TODO: Implement particle changing\n");
		}
		typedef MemberCaller1<ParticleDefinition, const char*, &ParticleDefinition::particleChanged>
				ParticleChangedCaller;
};

typedef std::map<CopiedString, ParticleDefinition> ParticleDefinitionMap;

extern ParticleDefinitionMap g_particleDefinitions;

void Particles_Construct (void);
void Particles_Destroy (void);

#endif
