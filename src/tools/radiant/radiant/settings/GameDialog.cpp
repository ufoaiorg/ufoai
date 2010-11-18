#include "GameDialog.h"
#include "GameDescription.h"
#include "iregistry.h"
#include "radiant_i18n.h"
#include "../environment.h"
#include "gtkutil/dialog.h"
#include <libxml/parser.h>

GameDescription *g_pGameDescription; ///< shortcut to g_GamesDialog.m_pCurrentDescription

GtkWindow* CGameDialog::BuildDialog ()
{
	return NULL;
}

void CGameDialog::Reset ()
{
}

/**
 * @brief Loads the game description file
 */
void CGameDialog::Init ()
{
	std::string strGameFilename = GlobalRegistry().get(RKEY_APP_PATH) + "game.xml";

	xmlDocPtr pDoc = xmlParseFile(strGameFilename.c_str());
	if (pDoc) {
		g_pGameDescription = new GameDescription(pDoc, strGameFilename);
		// Import this information into the registry
		//GlobalRegistry().importFromFile(strGameFilename, "");
		xmlFreeDoc(pDoc);
	} else {
		gtkutil::errorDialog(_("XML parser failed to parse game.xml"));
	}
}

CGameDialog::~CGameDialog ()
{
	delete g_pGameDescription;
}

CGameDialog g_GamesDialog;
