#ifndef MAPDEF_H_
#define MAPDEF_H_

namespace scripts
{
	class MapDef
	{
		public:
			virtual ~MapDef ();

			MapDef ();

			/**
			 * Will try to add the current loaded map to the ufo script file with its own mapdef entry.
			 * In case the current loaded map is part of a ump the ump will be added.
			 * @return @c true if the map was added to the script, @c false if it wasn't possible or the
			 * map is already included in the script
			 */
			bool add ();
	};
}

#endif /* MAPDEF_H_ */
