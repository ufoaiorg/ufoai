#include "GameManager.h"
#include "GameDescription.h"
#include "iregistry.h"
#include "radiant_i18n.h"
#include "../ui/prefdialog/EnginePathDialog.h"
#include "os/file.h"
#include "os/path.h"
#include "gtkutil/messagebox.h"

#include <libxml/parser.h>

namespace ui {

bool GameManager::settingsValid () const
{
	if (file_exists(_enginePath)) {
		return true; // all settings there
	}

	// Engine path doesn't exist
	return false;
}

/**
 * @brief Loads the game description file
 */
void GameManager::initialise ()
{
#ifdef PKGDATADIR
	_enginePath = PKGDATADIR;
#endif

	// Check loop, continue, till the user specifies a valid setting
	while (!settingsValid()) {
		// Engine path doesn't exist, ask the user
		ui::PathsDialog dialog;

		if (!settingsValid()) {
			std::string msg(_("<b>Warning:</b>\n"));

			if (!file_exists(_enginePath)) {
				msg += _("Engine path does not exist.");
				msg += "\n";
			}

			msg += _("Do you want to correct these settings?");

			EMessageBoxReturn ret = gtk_MessageBox(NULL, msg, _("Invalid Settings"), eMB_YESNO);
			if (ret == eIDNO) {
				break;
			}
		}
	}
}

const std::string& GameManager::getKeyValue (const std::string& key) const
{
	if (_currentGameDescription)
		return _currentGameDescription->getKeyValue(key);
	return _emptyString;
}

const std::string& GameManager::getEnginePath () const
{
	if (_enginePath.empty())
		return _emptyString;
	return _cleanedEnginePath;
}

GameManager::GameManager () :
	_currentGameDescription(0), _enginePath(GlobalRegistry().get(RKEY_ENGINE_PATH)), _cleanedEnginePath(
			DirectoryCleaned(_enginePath)), _emptyString("")
{
	GlobalRegistry().addKeyObserver(this, RKEY_ENGINE_PATH);

	// greebo: Register this class in the preference system so that the constructPreferencePage() gets called.
	GlobalPreferenceSystem().addConstructor(this);

	// TODO Remove this and read the game.xml data from the xmlregistry, too
	std::string path = DirectoryCleaned(_enginePath);
	std::string strGameFilename = Environment::Instance().getAppPath() + "game.xml";

	xmlDocPtr pDoc = xmlParseFile(strGameFilename.c_str());
	if (pDoc) {
		_currentGameDescription = new GameDescription(pDoc, strGameFilename);
		// Import this information into the registry
		//GlobalRegistry().importFromFile(strGameFilename, "");
		xmlFreeDoc(pDoc);
	} else {
		gtkutil::errorDialog(_("XML parser failed to parse game.xml"));
	}

	initialise();
}

GameManager::~GameManager ()
{
	if (_currentGameDescription)
		delete _currentGameDescription;
}

// Callback when a registry key is changed
void GameManager::keyChanged (const std::string& key, const std::string& val)
{
	const std::string path = GlobalRegistry().get(RKEY_ENGINE_PATH);

	if (_enginePath != path) {
		_enginePathObservers.unrealise();
		_enginePath = path;
		_cleanedEnginePath = DirectoryCleaned(_enginePath);
		_enginePathObservers.realise();
	}
}

void GameManager::constructPreferencePage (PreferenceGroup& group)
{
	// Add a page to the given group
	PreferencesPage* page(group.createPage(_("Path"), _("Path Preferences")));

	// Add the sliders for the movement and angle speed and connect them to the observer
	page->appendPathEntry(GlobalGameManager().getKeyValue("name") + " " + std::string(_("Installation Path")), RKEY_ENGINE_PATH, true);
}

void GameManager::init ()
{
	_enginePathObservers.realise();
}

void GameManager::destroy ()
{
	_enginePathObservers.unrealise();
}

} // namespace ui
