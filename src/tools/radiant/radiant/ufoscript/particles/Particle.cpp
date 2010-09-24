#include "Particle.h"
#include "../common/Parser.h"
#include "stream/memstream.h"
#include "script/scripttokeniser.h"

namespace scripts
{
	Particle::Particle ()
	{
	}

	Particle::~Particle ()
	{
	}

	/** Submit renderable geometry when rendering takes place in Solid mode. */
	void Particle::renderSolid (Renderer& renderer, const VolumeTest& volume) const
	{
	}

	/** Submit renderable geometry when rendering takes place in Wireframe mode */
	void Particle::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
	{
	}

	void Particle::renderComponents (Renderer&, const VolumeTest&)
	{
	}

	void Particle::render ()
	{
	}

	std::string Particle::toString ()
	{
		return "";
	}

	scripts::IParticlePtr loadParticle (const std::string& particleID)
	{
		Parser parser("particle");
		DataBlock *data = parser.getEntryForID(particleID);
		if (data == (DataBlock*) 0)
			return IParticlePtr();

		ScriptValues scriptValues;

		Particle *particle = new Particle();
		BufferInputStream stream(data->getData());
		ScriptTokeniser tokeniser(stream, false);

//		ScriptValue imageValue = ScriptValue("image", offsetof(scripts::ParticleData, image), V_STRING);
//		scriptValues.addScriptValue(imageValue);
//		ScriptValue modelValue = ScriptValue("model", offsetof(scripts::ParticleData, model), V_STRING);
//		scriptValues.addScriptValue(modelValue);

		for (;;) {
			std::string token = tokeniser.getToken();
			if (token.empty())
				break;

			if (token == "init" || token == "run" || token == "think" || token == "round" || token == "physics") {
				token = tokeniser.getToken();
				if (token == "{") {
					for (;;) {
						token = tokeniser.getToken();
						if (token.empty()) {
							g_warning("Invalid particle definition '%s'\n", particleID.c_str());
							break;
						} else if (token == "}") {
							break;
						}
						int i;
						for (i = 0; i < PC_NUM_PTLCMDS; i++) {
							if (token == pc_strings[i]) {
								ParticleCommand ptlCmd;
								ptlCmd.cmd = i;

								if (!pc_types[i])
									break;

								/* get parameter type */
								token = tokeniser.getToken();
								if (token.empty())
									break;

								/* operate on the top element on the stack */
								if (token[0] == '#') {
									ptlCmd.ref = RSTACK;
									if (token[1] == '.')
										ptlCmd.ref -= (token[2] - '0');
									break;
								}

								/* it's a variable reference */
								if (token[0] == '*') {
									int len;
									/* we maybe have to modify it */
									std::string baseComponentToken = token.substr(1, token.length());

									/* check for component specifier */
									len = baseComponentToken.size();
									/* it's possible to change only the second value of e.g. a vector
									 * just defined e.g. 'size.2' to modify the second value of size */
									if (len >= 2 && baseComponentToken[len - 2] == '.') {
										baseComponentToken[len - 2] = 0;
									} else
										len = 0;

									ScriptValues::ScriptValueVectorConstIterator pp;
									for (pp = scriptValues.begin(); pp != scriptValues.end(); pp++) {
										if (baseComponentToken == pp->getID()) {
											if ((pc_types[i] & PTL_ONLY_ONE_TYPE)) {
												if ((pc_types[i] & ~PTL_ONLY_ONE_TYPE) != pp->getType()) {
													break;
												}
											} else if (pp->getType() >= V_NUM_TYPES || !((1 << pp->getType())
													& pc_types[i])) {
												break;
											}

											if (len) {
												/* get single component */
												if ((1 << pp->getType()) & V_VECS) {
													const int component = (baseComponentToken[len - 1] - '1');
													/* get the component we want to modify */
													if (component > 3) {
														break;
													}
													ptlCmd.type = V_FLOAT;
													/* go to component offset */
													ptlCmd.ref = -((int) pp->getOffset()) - component * sizeof(float);
												}
											} else {
												/* set the values */
												ptlCmd.type = pp->getType();
												ptlCmd.ref = -((int) pp->getOffset());
											}
											break;
										}
									}
								}

								int j;
								/* get the type */
								if (pc_types[i] & PTL_ONLY_ONE_TYPE)
									/* extract the real type */
									j = pc_types[i] & ~PTL_ONLY_ONE_TYPE;
								else {
									for (j = 0; j < V_NUM_TYPES; j++)
										if (token == vt_names[j])
											break;

									if (j >= V_NUM_TYPES || !((1 << j) & pc_types[i]))
										break;

									/* get the value */
									token = tokeniser.getToken();
									if (token.empty())
										break;
								}

								/* set the values */
								ptlCmd.type = j;
								ptlCmd.ref = 0;//(int) (pcmdPos - pcmdData);
							}
						}
					}
				} else {
					g_warning("Invalid particle definition '%s'\n", particleID.c_str());
					break;
				}
			} else {
				g_warning("Invalid token for particle '%s' (token: '%s')\n", particleID.c_str(), token.c_str());
			}
		}

		scripts::IParticlePtr obj(particle);
		return obj;
	}
}
