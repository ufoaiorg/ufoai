#include "UMPFile.h"

#include "stream/textfilestream.h"
#include "iradiant.h"
#include "ifilesystem.h"
#include "iarchive.h"
#include <sstream>
#include "AutoPtr.h"
#include "UMPException.h"
#include "string/string.h"
#include "os/path.h"
#include "../map/map.h"

namespace map
{
	namespace ump
	{
		UMPFile::UMPFile (const std::string& fileName, const std::string& base) throw (UMPException) :
			_fileName(fileName), _base(base)
		{
			if (_fileName.empty())
				throw UMPException("fileName is empty");
		}

		UMPFile::~UMPFile ()
		{
		}

		void UMPFile::addTile (UMPTile& tile)
		{
			_umpTiles.push_back(tile);
		}

		UMPTile* UMPFile::getTileForMap (const std::string& map)
		{
			if (map.find(getBase()) == std::string::npos)
				return (UMPTile*) 0;
			const std::string tileName = getTileName(map);
			for (UMPTileIterator i = _umpTiles.begin(); i != _umpTiles.end(); ++i) {
				UMPTile& tile = *i;
				if (tile.getTileName(getBase()) == tileName)
					return &tile;
			}
			return (UMPTile*) 0;
		}

		const std::string& UMPFile::getFileName () const
		{
			return _fileName;
		}

		const std::string& UMPFile::getBase () const
		{
			return _base;
		}

		bool UMPFile::save ()
		{
			TextFileOutputStream file(map::getMapsPath() + _fileName);
			if (!file.failed()) {
				std::stringstream os;

				os << "base " << _base << std::endl;

				for (UMPTileIterator i = _umpTiles.begin(); i != _umpTiles.end(); ++i) {
					os << (*i).toString(_base);
				}
				for (UMPAssemblyIterator i = _umpAssenblies.begin(); i != _umpAssenblies.end(); ++i) {
					os << (*i).toString();
				}

				const std::string umpString = os.str();
				std::size_t written = file.write(umpString);
				if (written == umpString.length())
					return true;

				return false;
			}
			return false;
		}

		void UMPFile::parseTile (Tokeniser &tokeniser) throw (UMPException)
		{
			std::string name = tokeniser.getToken();
			if (name.empty())
				throw UMPException("Unexpected tile format, expected tile name");
			std::string token = tokeniser.getToken();
			if (token != "{")
				throw UMPException("Unexpected tile format, expected {, found " + token);

			std::string width = tokeniser.getToken();
			if (width.empty())
				throw UMPException("Unexpected tile format, expected tile width");

			std::string height = tokeniser.getToken();
			if (width.empty())
				throw UMPException("Unexpected tile format, expected tile height");

			UMPTile tile(name, string::toInt(width), string::toInt(height));

			for (;;) {
				token = tokeniser.getToken();
				if (token.empty() || token == "}")
					break;
				// TODO: fill the data
			}

			addTile(tile);
		}

		void UMPFile::parse (Tokeniser &tokeniser)
		{
			std::string token = tokeniser.getToken();
			while (token.length()) {
				if (token == "base") {
					_base = tokeniser.getToken();
					if (_base.empty()) {
						globalErrorStream() << _fileName << ": base without parameter given\n";
						return;
					}
				} else if (token == "tile") {
					try {
						parseTile(tokeniser);
					} catch (UMPException &e) {
						globalErrorStream() << _fileName << ": " << e.getMessage() << "\n";
						return;
					}
				}
				token = tokeniser.getToken();
			}
		}

		bool UMPFile::load ()
		{
			AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(map::getMapsPath() + _fileName));
			if (file) {
				AutoPtr<Tokeniser> reader(GlobalScriptLibrary().createScriptTokeniser(file->getInputStream()));
				parse(*reader);
				return true;
			}
			return false;
		}

		const std::string UMPFile::getMapName (const std::string& tile) const
		{
			// a '+' indicates that this is a short name
			if (tile.substr(0, 1) == "+") {
				return getBase() + tile.substr(1);
			}
			return tile;
		}

		const std::string UMPFile::getTileName (const std::string& map) const
		{
			const std::string relativeMapPath = GlobalFileSystem().getRelative(map);
			const std::string baseMapName = os::stripExtension(relativeMapPath);
			// remove the maps/ part but keep any other subdir (below maps/) because it
			// can be part of a rma theme
			std::string relativeToRMASubdir;
			if (baseMapName.find("maps/") != std::string::npos)
				relativeToRMASubdir = baseMapName.substr(5);
			else
				relativeToRMASubdir = baseMapName;

			const size_t size = getBase().length();
			if (size > 0 && relativeToRMASubdir.length() > size)
				return std::string("+") + relativeToRMASubdir.substr(size);
			return relativeToRMASubdir;
		}
	}
}
