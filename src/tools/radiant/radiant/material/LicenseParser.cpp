#include "LicenseParser.h"
#include "iscriplib.h"
#include "iregistry.h"
#include "iarchive.h"
#include "ifilesystem.h"

#include "string/string.h"
#include "os/path.h"
#include "AutoPtr.h"

#include "../environment.h"

void LicenseParser::parseLicensesFile (Tokeniser& tokeniser, const std::string& filename)
{
	for (;;) {
		std::string token = tokeniser.getToken();
		if (token.empty())
			break;
		if (tokeniser.getLine() > 1) {
			tokeniser.ungetToken();
			break;
		}
	}
	std::size_t lastLine = 1;
	for (;;) {
		std::string token = tokeniser.getToken();
		if (token.empty())
			break;

		if (string::contains(token, "base/textures/") && lastLine != tokeniser.getLine()) {
			_licensesMap[os::stripExtension(token.substr(5))] = true;
			lastLine = tokeniser.getLine();
		}
	}
}

LicenseParser::LicenseParser ()
{
}

void LicenseParser::clearLicenses ()
{
	_licensesMap.clear();
}

bool LicenseParser::isValid (const std::string& filename) const
{
	// Look up candidate in the map and return true if found
	LicensesMap::const_iterator it = _licensesMap.find(filename);
	if (it != _licensesMap.end())
		return true;

	return false;
}

void LicenseParser::openLicenseFile (const std::string& filename)
{
	clearLicenses();

	std::string fullpath;
	if (os::isAbsolute(filename)) {
		fullpath = filename;
	} else {
		const std::string& appPath = GlobalRegistry().get(RKEY_APP_PATH);
		fullpath = appPath + filename;
	}

	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(fullpath));
	if (file) {
		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().createScriptTokeniser(file->getInputStream()));
		parseLicensesFile(*tokeniser, fullpath);
	}
}

LicenseParser::~LicenseParser ()
{
	clearLicenses();
}
