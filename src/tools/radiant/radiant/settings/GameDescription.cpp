#include "GameDescription.h"

#include "gtkutil/dialog.h"
#include "string/string.h"
#include "radiant_i18n.h"

#include <libxml/parser.h>

inline const char* xmlAttr_getName (xmlAttrPtr attr)
{
	return reinterpret_cast<const char*> (attr->name);
}

inline const char* xmlAttr_getValue (xmlAttrPtr attr)
{
	return reinterpret_cast<const char*> (attr->children->content);
}

GameDescription::GameDescription (xmlDocPtr pDoc, const std::string& gameFile) :
	emptyString("")
{
	// read the user-friendly game name
	xmlNodePtr pNode = pDoc->children;

	while (!string_equal((const char*)pNode->name, "game") && pNode != 0)
		pNode = pNode->next;

	if (!pNode)
		gtkutil::errorDialog(_("Didn't find 'game' node in game.xml file\n"));

	for (xmlAttrPtr attr = pNode->properties; attr != 0; attr = attr->next) {
		m_gameDescription.insert(GameDescriptionMap::value_type(xmlAttr_getName(attr), xmlAttr_getValue(attr)));
	}

	mGameFile = gameFile;
}

const std::string& GameDescription::getKeyValue (const std::string& key) const
{
	GameDescriptionMap::const_iterator i = m_gameDescription.find(key);
	if (i != m_gameDescription.end()) {
		return (*i).second;
	}
	return GameDescription::emptyString;
}

const std::string& GameDescription::getRequiredKeyValue (const std::string& key) const
{
	GameDescriptionMap::const_iterator i = m_gameDescription.find(key);
	if (i != m_gameDescription.end()) {
		return (*i).second;
	}
	gtkutil::errorDialog(string::format(_("Didn't find attribute '%s' in game node\n"), key.c_str()));
	return emptyString;
}
