#ifndef UMPASSEMBLY_H_
#define UMPASSEMBLY_H_

#include <string>
#include <sstream>
#include <vector>

namespace map
{
	namespace ump
	{
		/** Data structure containing both a tile name and the min/max amount for this assembly */
		struct TileAndAmount
		{
			private:

				std::string _tileName;

				int _min;

				int _max;

				bool _fix;

			public:
				// Constructor
				TileAndAmount (const std::string& tileName, const int min, const int max, bool fix = false) :
					_tileName(tileName), _min(min), _max(max), _fix(fix)
				{
				}

				const std::string toString () const
				{
					std::stringstream os;
					os << _tileName << "\t" << "\"" << _min << " " << _max << "\"" << std::endl;
					return os.str();
				}

				const std::string getTileName () const
				{
					return _tileName;
				}
				int getMinAmount () const
				{
					return _min;
				}
				int getMaxAmount () const
				{
					return _max;
				}
				bool isFix () const
				{
					return _fix;
				}
		};

		class UMPAssembly
		{
			private:
				/** the grid size of the assembly - useful for optimizing stuff if every
				 * tile in the ump has the same boundaries.
				 */
				int _gridWidth;
				int _gridHeight;

				/** the size of the assembly - this must match with the sizes of the tiles
				 * used in this assembly
				 */
				int _width;
				int _height;

				/** the title of the assembly that is show when the map starts
				 */
				std::string _title;

				/** The name of the assembly
				 */
				std::string _name;

				std::vector<TileAndAmount> _tiles;
				typedef std::vector<TileAndAmount>::iterator TileAndAmountIterator;
				typedef std::vector<TileAndAmount>::const_iterator TileAndAmountConstIterator;

			public:
				UMPAssembly ();

				virtual ~UMPAssembly ();

				const std::string toString () const;
		};
	}
}

#endif /* UMPASSEMBLY_H_ */
