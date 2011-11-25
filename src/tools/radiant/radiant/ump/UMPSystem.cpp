#include "UMPSystem.h"

#include "radiant_i18n.h"

#include "ifilesystem.h"
#include "iradiant.h"
#include "ieventmanager.h"

#include "gtkutil/dialog.h"
#include "os/path.h"

#include "UMPFile.h"
#include "UMPTile.h"
#include "../ui/umpeditor/UMPEditor.h"
#include "../map/map.h"

UMPSystem::UMPCollector::UMPCollector (std::set<std::string> &list) :
	_list(list)
{
	GlobalFileSystem().forEachFile("maps/", "*", makeCallback1(*this), 0);
	std::size_t size = _list.size();
	globalOutputStream() << "Found " << string::toString(size) << " ump files\n";
}

// Functor operator needed for the forEachFile() call
void UMPSystem::UMPCollector::operator() (const std::string& file)
{
	std::string extension = os::getExtension(file);

	if (extension == "ump")
		_list.insert(file);
}

void UMPSystem::editUMPDefinition ()
{
	const std::string umpFileName = getUMPFilename(GlobalMap().getName());
	if (umpFileName.empty()) {
		gtkutil::infoDialog(_("Could not find the map in any ump file"));
		return;
	}
	ui::UMPEditor editor(map::getMapsPath() + umpFileName);
	editor.show();
}

/**
 * @return A vector with ump filesnames
 */
const std::set<std::string> UMPSystem::getFiles () const
{
	return _umpFiles;
}

void UMPSystem::init ()
{
	UMPCollector collector(_umpFiles);
	// TODO: Register observer for base directory

	GlobalEventManager().addCommand("EditUMPDefinition", MemberCaller<IUMPSystem, &IUMPSystem::editUMPDefinition> (GlobalUMPSystem()));
}

/**
 * @return The ump filename for the given map
 */
std::string UMPSystem::getUMPFilename (const std::string& map)
{
	if (map.empty())
		return "";

	UMPFileMap::const_iterator i = _umpFileMap.find(map);
	if (i != _umpFileMap.end())
		return i->second;

	for (UMPFilesIterator i = _umpFiles.begin(); i != _umpFiles.end(); ++i) {
		try {
			map::ump::UMPFile umpFile(*i);
			if (!umpFile.load()) {
				globalErrorStream() << "Could not load the ump file " << (*i) << "\n";
				continue;
			}

			map::ump::UMPTile* tile = umpFile.getTileForMap(map);
			if (tile) {
				_umpFileMap[map] = *i;
				return *i;
			}
		} catch (map::ump::UMPException& e) {
			globalErrorStream() << e.getMessage() << "\n";
			gtkutil::errorDialog(e.getMessage());
		}
	}
	// not found in any ump file
	_umpFileMap[map] = "";
	return "";
}
