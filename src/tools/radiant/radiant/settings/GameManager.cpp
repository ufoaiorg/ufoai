#include "GameManager.h"
#include "GameDescription.h"
#include "iregistry.h"
#include "radiant_i18n.h"
#include "../environment.h"
#include "gtkutil/dialog.h"
#include <libxml/parser.h>

namespace ui {

/**
 * @brief Loads the game description file
 */
void GameManager::initialise ()
{
	std::string strGameFilename = GlobalRegistry().get(RKEY_APP_PATH) + "game.xml";

	xmlDocPtr pDoc = xmlParseFile(strGameFilename.c_str());
	if (pDoc) {
		_currentGameDescription = new GameDescription(pDoc, strGameFilename);
		// Import this information into the registry
		//GlobalRegistry().importFromFile(strGameFilename, "");
		xmlFreeDoc(pDoc);
	} else {
		gtkutil::errorDialog(_("XML parser failed to parse game.xml"));
	}
}

GameManager::~GameManager ()
{
	if (_currentGameDescription)
		delete _currentGameDescription;
}

GameDescription* GameManager::getGameDescription ()
{
	return _currentGameDescription;
}

GameManager& GameManager::Instance ()
{
	static GameManager _instance;
	return _instance;
}

} // namespace ui
