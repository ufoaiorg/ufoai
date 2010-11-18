#ifndef GAMEDESCRIPTION_H_
#define GAMEDESCRIPTION_H_

#include <map>
#include <string>
#include "xmlutil/Document.h"

/*!
 holds game specific configuration stuff
 such as base names, engine names, some game specific features to activate in the various modules
 it is not strictly a prefs thing since the user is not supposed to edit that (unless he is hacking
 support for a new game)

 what we do now is fully generate the information for this during the setup. We might want to
 generate a piece that just says "the game pack is there", but put the rest of the config somwhere
 else (i.e. not generated, copied over during setup .. for instance in the game tools directory)
 */
class GameDescription
{
	private:
		typedef std::map<std::string, std::string> GameDescriptionMap;

	public:
		std::string mGameFile; ///< the .game file that describes this game
		const std::string emptyString;
		GameDescriptionMap m_gameDescription;

		const std::string& getKeyValue (const std::string& key) const;
		const std::string& getRequiredKeyValue (const std::string& key) const;

		GameDescription (xmlDocPtr pDoc, const std::string &GameFile);
};

#endif /*GAMEDESCRIPTION_H_*/
