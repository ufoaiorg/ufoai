#include "GameDialog.h"
#include "GameDescription.h"
#include "iregistry.h"
#include "radiant_i18n.h"
#include "../environment.h"
#include "gtkutil/dialog.h"
#include <libxml/parser.h>

namespace ui {

GtkWindow* CGameDialog::BuildDialog ()
{
	return NULL;
}

/**
 * @brief Loads the game description file
 */
void CGameDialog::initialise ()
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

CGameDialog::~CGameDialog ()
{
	if (_currentGameDescription)
		delete _currentGameDescription;
}

void CGameDialog::setGameDescription (GameDescription* newGameDescription)
{
	_currentGameDescription = newGameDescription;
}

GameDescription* CGameDialog::getGameDescription ()
{
	return _currentGameDescription;
}

CGameDialog& CGameDialog::Instance ()
{
	static CGameDialog _instance;
	return _instance;
}

} // namespace ui
