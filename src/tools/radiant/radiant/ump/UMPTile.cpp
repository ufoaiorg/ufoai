#include "UMPTile.h"
#include <sstream>

namespace map
{
	namespace ump
	{
		UMPTile::UMPTile (std::string& mapName, int width, int height) :
			_width(width), _height(height), _mapName(mapName)
		{
		}

		UMPTile::~UMPTile ()
		{
		}

		const std::string UMPTile::getTileName (const std::string& base) const
		{
			const size_t pos = _mapName.find(base);
			if (pos != std::string::npos)
				return "+" + _mapName.substr(pos);
			return _mapName;
		}

		std::string UMPTile::toString (const std::string& base)
		{
			std::stringstream os;

			os << "tile " << getTileName(base) << " {" << std::endl;

			os << _width << " " << _height << std::endl << std::endl;

			for (int j = 0; j < _height; j++) {
				for (int i = 0; i < _width; i++) {
					// TODO: Fill the tile data
					os << "0 ";
				}
				os << std::endl;
			}

			os << "}" << std::endl;

			return os.str();
		}
	}
}
