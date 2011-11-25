#include "XMLRegistry.h"		// The Abstract Base Class

#include <iostream>
#include <stdexcept>

#include "stringio.h"
#include "stream/stringstream.h"

#include "../environment.h"

#include "os/file.h"
#include "os/path.h"

#include "version.h"
#include "string/string.h"
#include "gtkutil/IConv.h"

XMLRegistry::XMLRegistry() :
	_topLevelNode("uforadiant"),
	_standardTree(_topLevelNode),
	_userTree(_topLevelNode),
	_queryCounter(0),
	_settingsPath("")
{}

XMLRegistry::~XMLRegistry() {
	globalOutputStream() << "XMLRegistry Shutdown: " << _queryCounter << " queries processed.\n";

	// Save the user tree to the settings path, this contains all
	// settings that have been modified during runtime
	if (get(RKEY_SKIP_REGISTRY_SAVE).empty()) {
		// Replace the version tag and set it to the current DarkRadiant version
		deleteXPath("user//version");
		set("user/version", RADIANT_VERSION);

		// Export the user-defined filter definitions to a separate file
		exportToFile("user/ui/filtersystem/filters", _settingsPath + "filters.xml");
		deleteXPath("user/ui/filtersystem/filters");

		// Export the colour schemes and remove them from the registry
		exportToFile("user/ui/colourschemes", _settingsPath + "colours.xml");
		deleteXPath("user/ui/colourschemes");

		// Export the input definitions into the user's settings folder and remove them as well
		exportToFile("user/ui/input", _settingsPath + "input.xml");
		deleteXPath("user/ui/input");

		// Delete all nodes marked as "transient", they are NOT exported into the user's xml file
		deleteXPath("user/*[@transient='1']");

		// Remove any remaining upgradePaths (from older registry files)
		deleteXPath("user/upgradePaths");
		// Remove legacy <interface> node
		deleteXPath("user/ui/interface");

		// Save the remaining /darkradiant/user tree to user.xml so that the current settings are preserved
		exportToFile("user", _settingsPath + "user.xml");
	}
}

xml::NodeList XMLRegistry::findXPath(const std::string& path) {
	// Query the user tree first
	xml::NodeList results = _userTree.findXPath(path);
	xml::NodeList stdResults = _standardTree.findXPath(path);

	// Append the stdResults to the results
	for (std::size_t i = 0; i < stdResults.size(); i++) {
		results.push_back(stdResults[i]);
	}

	_queryCounter++;

	return results;
}


void XMLRegistry::dump() const {
	std::cout << "User Tree:" << std::endl;
	_userTree.dump();
	std::cout << "Default Tree:" << std::endl;
	_standardTree.dump();
}

void XMLRegistry::exportToFile(const std::string& key, const std::string& filename) {
	// Only the usertree should be exported, so pass the call to this tree
	_userTree.exportToFile(key, filename);
}

void XMLRegistry::addKeyObserver(RegistryKeyObserver* observer, const std::string& observedKey) {
	_keyObservers.insert( std::make_pair(observedKey, observer) );
}

// Removes an observer watching the <observedKey> from the internal list of observers.
void XMLRegistry::removeKeyObserver(RegistryKeyObserver* observer) {
	// Traverse through the keyObserverMap and try to find the specified observer
	for (KeyObserverMap::iterator i = _keyObservers.begin(); i != _keyObservers.end();
	/* in-loop increment */) {
		if (i->second == observer) {
			_keyObservers.erase(i++);
		}
		else {
			++i;
		}
	}
}

bool XMLRegistry::keyExists(const std::string& key) {
	// Pass the query on to findXPath which queries the subtrees
	xml::NodeList result = findXPath(key);
	return (result.size() > 0);
}

void XMLRegistry::deleteXPath(const std::string& path) {
	// Add the toplevel node to the path if required
	xml::NodeList nodeList = findXPath(path);

	for (std::size_t i = 0; i < nodeList.size(); i++) {
		// unlink and delete the node
		nodeList[i].erase();
	}
}

xml::Node XMLRegistry::createKeyWithName(const std::string& path,
		const std::string& key, const std::string& name)
{
	// The key will be created in the user tree (the default tree is read-only)
	return _userTree.createKeyWithName(path, key, name);
}

xml::Node XMLRegistry::createKey(const std::string& key) {
	return _userTree.createKey(key);
}

void XMLRegistry::setAttribute(const std::string& path,
	const std::string& attrName, const std::string& attrValue)
{
	_userTree.setAttribute(path, attrName, attrValue);
}

std::string XMLRegistry::getAttribute(const std::string& path,
		const std::string& attrName)
{
	// Pass the query to the findXPath method, which queries the user tree first
	xml::NodeList nodeList = findXPath(path);

	if (nodeList.empty())
	{
		return "";
	}

	return nodeList[0].getAttributeValue(attrName);
}

std::string XMLRegistry::get(const std::string& key) {
	// Pass the query to the findXPath method, which queries the user tree first
	xml::NodeList nodeList = findXPath(key);

	// Does it even exist?
	// It may well be the case that this returns two or more nodes that match the key criteria
	// This function always uses the first one, as the user tree should override the default tree
	if (!nodeList.empty())
	{
		// Convert the UTF-8 string back to locale and return
		return gtkutil::IConv::localeFromUTF8(nodeList[0].getAttributeValue("value"));
	}
	else {
		//globalOutputStream() << "XMLRegistry: GET: Key " << fullKey.c_str() << " not found, returning empty string!\n";
		return "";
	}
}

float XMLRegistry::getFloat(const std::string& key) {
	// Load the key and convert to float
	return string::toFloat(get(key));
}

void XMLRegistry::setFloat(const std::string& key, const float value) {
	// Pass the call to set() to do the rest
	set(key, string::toString(value));
}

int XMLRegistry::getInt(const std::string& key) {
	// Load the key and convert to int
	return string::toInt(get(key));
}

void XMLRegistry::setInt(const std::string& key, const int value) {
	// Pass the call to set() to do the rest
	set(key, string::toString(value));
}

bool XMLRegistry::getBool(const std::string& key)
{
	std::string value = get(key);

	return !value.empty() && value != "0";
}

void XMLRegistry::setBool(const std::string& key, const bool value)
{
	set(key, value ? "1" : "0");
}

void XMLRegistry::set(const std::string& key, const std::string& value) {
	// Create or set the value in the user tree, the default tree stays untouched
	// Convert the string to UTF-8 before storing it into the RegistryTree
	_userTree.set(key, gtkutil::IConv::localeToUTF8(value));

	// Notify the observers
	notifyKeyObservers(key, value);
}

void XMLRegistry::import(const std::string& importFilePath, const std::string& parentKey, Tree tree) {
	switch (tree) {
		case treeUser:
			_userTree.importFromFile(importFilePath, parentKey);
			break;
		case treeStandard:
			_standardTree.importFromFile(importFilePath, parentKey);
			break;
	}
}

void XMLRegistry::notifyKeyObservers(const std::string& changedKey, const std::string& newVal)
{
	for (KeyObserverMap::iterator it = _keyObservers.find(changedKey);
		 it != _keyObservers.upper_bound(changedKey) && it != _keyObservers.end();
		 ++it)
	{
		RegistryKeyObserver* keyObserver = it->second;

		if (keyObserver != NULL) {
			keyObserver->keyChanged(changedKey, newVal);
		}
	}
}

void XMLRegistry::init() {
	_settingsPath = GlobalRegistry().get(RKEY_SETTINGS_PATH);
	const std::string basePath = GlobalRegistry().get(RKEY_APP_PATH);

	try {
		// Load all of the required XML files
		import(basePath + "user.xml", "", Registry::treeStandard);
		import(basePath + "colours.xml", "user/ui", Registry::treeStandard);
		import(basePath + "input.xml", "user/ui", Registry::treeStandard);
		import(basePath + "menu.xml", "user/ui", Registry::treeStandard);
		import(basePath + "game.xml", "", Registry::treeStandard);
	}
	catch (std::runtime_error& e) {
		std::cerr << "XML registry population failed:\n\n" << e.what() << "\n";
	}

	// Load user preferences, these overwrite any values that have defined before
	// The called method also checks for any upgrades that have to be performed
	const std::string userSettingsFile = _settingsPath + "user.xml";
	if (file_exists(userSettingsFile)) {
		import(userSettingsFile, "", Registry::treeUser);
	}
	else
	{
		globalOutputStream() << "XMLRegistry: no user.xml in " << userSettingsFile << "\n";
	}

	const std::string userColoursFile = _settingsPath + "colours.xml";
	if (file_exists(userColoursFile)) {
		import(userColoursFile, "user/ui", Registry::treeUser);
	}

	const std::string userInputFile = _settingsPath + "input.xml";
	if (file_exists(userInputFile)) {
		import(userInputFile, "user/ui", Registry::treeUser);
	}

	const std::string userFilterFile = _settingsPath + "filters.xml";
	if (file_exists(userFilterFile)) {
		import(userFilterFile, "user/ui/filtersystem", Registry::treeUser);
	}
}

void XMLRegistry::reset()
{
	deleteXPath("user");
	init();
}
