#include "UMPAssembly.h"

namespace map
{
	namespace ump
	{
		UMPAssembly::UMPAssembly ()
		{
		}

		UMPAssembly::~UMPAssembly ()
		{
		}

		const std::string UMPAssembly::toString () const
		{
			std::stringstream os;

			// header
			os << "assembly " << _name << " {" << std::endl;

			// size
			os << "size\t\"" << _width << " " << _height << "\"" << std::endl;

			// grid
			if (_gridWidth > 0 && _gridHeight > 0)
				os << "grid\t\"" << _gridWidth << " " << _gridHeight << "\"" << std::endl;

			// tiles
			for (TileAndAmountConstIterator i = _tiles.begin(); i != _tiles.end(); ++i) {
				os << (*i).toString();
			}

			os << "}" << std::endl;
			return os.str();
		}
	}
}
