/**
 * @file particles.cpp
 */

/*
 Copyright (C) 2002-2010 UFO: Alien Invasion.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "particles.h"
#include <stdlib.h>

// the list of ufos/*.ufo files we need to work with
static GSList *l_ufofiles = 0;
static std::list<std::string> g_ufoFilenames;

ParticleDefinitionMap g_particleDefinitions;

static const char * const blend_names[] = { "replace", "one", "blend", "add", "filter", "invfilter" };

static int GetBlendMode (const std::string& token)
{
	int i;

	for (i = 0; i < BLEND_LAST; i++) {
		if (token == blend_names[i]) {
			return i;
		}
	}
	return BLEND_REPLACE;
}

static void ParseParticleScriptFile (Tokeniser& tokeniser, const std::string& filename)
{
	for (;;) {
		std::string token = tokeniser.getToken();

		if (token.empty())
			break;

		if (token == "particle") {
			int kill = 0;
			const std::string particleID = tokeniser.getToken();
			if (particleID.empty()) {
				Tokeniser_unexpectedError(tokeniser, 0, "#id-name");
				return;
			}

			if (!Tokeniser_parseToken(tokeniser, "{")) {
				globalErrorStream() << "ERROR: expected {. parsing file " << filename << " particle: " << particleID
						<< "\n";
				return;
			}

			std::string model = "";
			std::string image = "";
			int blend = BLEND_REPLACE;
			int width = 0;
			int height = 0;
			for (;;) {
				token = tokeniser.getToken();
				if (token == "init") {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalErrorStream() << "ERROR: expected { in init.\n";
						return;
					}
					for (;;) {
						const std::string option = tokeniser.getToken();
						if (option == "}") {
							break;
						} else if (option == "image") {
							image = tokeniser.getToken();
						} else if (option == "model") {
							model = tokeniser.getToken();
						} else if (option == "blend") {
							blend = GetBlendMode(tokeniser.getToken());
						} else if (option == "size") {
							const std::string sizeVector2 = tokeniser.getToken();
							char size[64];
							char *seperator;
							strncpy(size, sizeVector2.c_str(), sizeof(size));
							seperator = size;
							while (seperator != '\0') {
								seperator++;
								if (*seperator == ' ') {
									*seperator = '\0';
									width = atoi(size);
									height = atoi(++seperator);
									break;
								}
							}
						}
					}
				} else if (token == "think") {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalErrorStream() << "ERROR: expected { in think.\n";
						return;
					}
					for (;;) {
						const std::string option = tokeniser.getToken();
						if (option == "}") {
							break;
						} else if (option == "kill") {
							kill = 1;
							break;
						}
					}
				} else if (token == "run") {
					if (!Tokeniser_parseToken(tokeniser, "{")) {
						globalErrorStream() << "ERROR: expected { in run.\n";
						return;
					}
					for (;;) {
						const std::string option = tokeniser.getToken();
						if (option == "}") {
							break;
						} else if (option == "kill") {
							kill = 1;
							break;
						}
					}
				}
				if (token == "}")
					break;
			}
			if (!kill && (!model.empty() || !image.empty())) {
				// do we already have this particle?
				ParticleDefinition *particleDefinition = new ParticleDefinition(particleID, model, image, blend, width,
						height);
				if (!g_particleDefinitions.insert(ParticleDefinitionMap::value_type(particleID, *particleDefinition)).second)
					g_warning("Particle '%s' is already in memory, definition in '%s' ignored.\n", particleID.c_str(),
							filename.c_str());
				delete particleDefinition;
			}
		}
	}
}

void LoadUFOFile (const std::string& filename)
{
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (file) {
		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewScriptTokeniser(file->getInputStream()));
		g_ufoFilenames.push_back(filename);
		ParseParticleScriptFile(*tokeniser, filename);
	} else {
		g_warning("Unable to read ufo script file '%s'\n", filename.c_str());
	}
}

void addUFOFile (const std::string& filename)
{
	bool found = false;

	for (GSList* tmp = l_ufofiles; tmp != 0; tmp = tmp->next) {
		if (string_equal_nocase(filename, (const char*) tmp->data)) {
			found = true;
			break;
		}
	}

	if (!found) {
		l_ufofiles = g_slist_append(l_ufofiles, strdup(filename.c_str()));
		LoadUFOFile("ufos/" + filename);
	}
}

typedef FreeCaller1<const std::string&, addUFOFile> AddUFOFileCaller;

void Particles_Construct (void)
{
	GlobalFileSystem().forEachFile("ufos/", "ufo", AddUFOFileCaller(), 0);
}

void Particles_Destroy (void)
{
	while (l_ufofiles != 0) {
		g_free(l_ufofiles->data);
		l_ufofiles = g_slist_remove(l_ufofiles, l_ufofiles->data);
	}
	g_ufoFilenames.clear();
	g_particleDefinitions.clear();
}
