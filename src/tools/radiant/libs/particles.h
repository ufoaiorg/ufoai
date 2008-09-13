
#ifndef PARTICLES_H
#define PARTICLES_H

#include "archivelib.h"
#include "autoptr.h"

// the list of ufos/*.ufo files we need to work with
GSList *l_ufofiles = 0;

GSList* getUFOFileList() {
	return l_ufofiles;
}

std::list<CopiedString> g_ufoFilenames;


class ParticleDefinition {
public:
	ParticleDefinition(const char* id, const char *model, const char *image)
		: m_id(id), m_model(model), m_image(image) {
		globalOutputStream() << "particle " << id << "with: ";
		if (model)
			globalOutputStream() << "model: " << model << " ";
		if (image)
			globalOutputStream() << "image: " << image << " ";
		globalOutputStream() << "\n";
	}
	const char *m_id;
	const char *m_model;
	const char *m_image;
};

typedef std::map<CopiedString, ParticleDefinition> ParticleDefinitionMap;

ParticleDefinitionMap g_particleDefinitions;


void ParseUFOFile(Tokeniser& tokeniser, const char* filename) {
	g_ufoFilenames.push_back(filename);
	filename = g_ufoFilenames.back().c_str();
	tokeniser.nextLine();
	for (;;) {
		const char* token = tokeniser.getToken();

		if (token == 0) {
			break;
		}

		if (string_equal(token, "particle")) {
			char pID[64];
			token = tokeniser.getToken();
			if (token == 0) {
				Tokeniser_unexpectedError(tokeniser, 0, "#id-name");
				return;
			}

			strncpy(pID, token, sizeof(pID) - 1);
			pID[sizeof(pID) - 1] = '\0';

			if (!Tokeniser_parseToken(tokeniser, "{")) {
				globalOutputStream() << "ERROR: expected {.\n";
				return;
			}

			const char *model = NULL;
			const char *image = NULL;
			for (;;) {
				token = tokeniser.getToken();
				if (string_equal(token, "init")) {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalOutputStream() << "ERROR: expected {.\n";
						return;
					}
					for (;;) {
						const char* option = tokeniser.getToken();
						if (string_equal(option, "}")) {
							break;
						} else if (string_equal(option, "image")) {
							image = tokeniser.getToken();
							token = "}";
							break;
						} else if (string_equal(option, "model")) {
							model = tokeniser.getToken();
							token = "}";
							break;
						}
					}
				}
				if (string_equal(token, "}"))
					break;
			}
			if (model || image) {
				// do we already have this particle?
				if (!g_particleDefinitions.insert(ParticleDefinitionMap::value_type(pID, ParticleDefinition(pID, model, image))).second)
					globalOutputStream() << "WARNING: particle " << pID << " is already in memory, definition in " << filename << " ignored.\n";
				else
					globalOutputStream() << "adding particle " << pID << "\n";
			}
		}
	}
}

void LoadUFOFile(const char* filename) {
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (file) {
		globalOutputStream() << "Parsing ufo script file " << filename << "\n";

		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewScriptTokeniser(file->getInputStream()));
		ParseUFOFile(*tokeniser, filename);
	} else {
		globalOutputStream() << "Unable to read ufo script file " << filename << "\n";
	}
}

void addUFOFile(const char* dirstring) {
	bool found = false;

	for (GSList* tmp = l_ufofiles; tmp != 0; tmp = tmp->next) {
		if (string_equal_nocase(dirstring, (char*)tmp->data)) {
			found = true;
			break;
		}
	}

	if (!found) {
		l_ufofiles = g_slist_append(l_ufofiles, strdup(dirstring));
		StringOutputStream ufoname(256);
		ufoname << "ufos/" << dirstring;
		LoadUFOFile(ufoname.c_str());
		ufoname.clear();
	}
}

typedef FreeCaller1<const char*, addUFOFile> AddUFOFileCaller;

void Particles_Init (void)
{
	GlobalFileSystem().forEachFile("ufos/", "ufo", AddUFOFileCaller(), 0);
}

void Particles_Shutdown (void)
{
	globalOutputStream() << "Unload UFO script files\n";
	while (l_ufofiles != 0) {
		free(l_ufofiles->data);
		l_ufofiles = g_slist_remove(l_ufofiles, l_ufofiles->data);
	}
	g_ufoFilenames.clear();
	g_particleDefinitions.clear();
}

#endif
