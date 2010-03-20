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

	void Particle::render ()
	{
	}

	std::string Particle::toString ()
	{
		return "";
	}

	scripts::IParticlePtr Particle::load (const std::string& particleID)
	{
		Parser parser("particle");
		DataBlock *data = parser.getEntryForID(particleID);
		if (data == (DataBlock*) 0)
			return IParticlePtr();

		Particle *particle = new Particle();
		BufferInputStream stream(data->getData());
		ScriptTokeniser tokeniser(stream, false);

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

#if 0
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
#ifdef NOTHING
									image
									model
									skin
									blend
									style
									thinkfade
									framefade
									size
									scale
									color
									a
									v
									s
									offset
									scroll_s
									scroll_t
									t
									dt
									rounds
									angles
									omega
									life
									tps
									lastthink
									frame
									endframe
									fps
									lastframe
									levelflags
									physics
									autohide
									stayalive
									weather
									lightcolor
									lightintensity
									lightsustain
#endif
									for (pp = pps; pp->string; pp++)
										if (baseComponentToken == pp->string)
											break;

									if (!pp->string) {
										break;
									}

									if ((pc_types[i] & PTL_ONLY_ONE_TYPE)) {
										if ((pc_types[i] & ~PTL_ONLY_ONE_TYPE) != pp->type) {
											break;
										}
									} else if (pp->type >= V_NUM_TYPES || !((1 << pp->type) & pc_types[i])) {
										break;
									}

									if (len) {
										/* get single component */
										if ((1 << pp->type) & V_VECS) {
											const int component = (baseComponentToken[len - 1] - '1');
											/* get the component we want to modify */
											if (component > 3) {
												break;
											}
											pc->type = V_FLOAT;
											/* go to component offset */
											pc->ref = -((int) pp->ofs) - component * sizeof(float);
											break;
										} else {
											break;
										}
									}

									/* set the values */
									pc->type = pp->type;
									pc->ref = -((int) pp->ofs);
									break;
								}
#endif
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
