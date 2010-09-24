#ifndef ROUTINGLUMP_H_
#define ROUTINGLUMP_H_

#include <list>
#include "math/Vector3.h"

namespace routing
{
	/**
	 * enum of rendering directions
	 * @todo perhaps it would be better to have an extended enum/class
	 * for direction which provides the rotate arrow, the order here is
	 * another than the one in routing data
	 */
	enum EDirection
	{
		DIR_WEST, DIR_NORTHWEST, DIR_NORTH, DIR_NORTHEAST, DIR_EAST, DIR_SOUTHEAST, DIR_SOUTH, DIR_SOUTHWEST,

		MAX_DIRECTIONS
	};

	/**
	 * @brief ++ operator for EDirection enum
	 */
	inline routing::EDirection operator++ (routing::EDirection &rs, int)
	{
		return rs = (routing::EDirection) (rs + 1);
	}

	/**
	 * @brief connectivity states
	 */
	enum EConnectionState
	{
		CON_DISABLE, CON_CROUCHABLE, CON_WALKABLE, MAX_CONNECTIONSTATES
	};

	/**
	 * @brief accessibility states
	 */
	enum EAccessState
	{
		ACC_DISABLED, ACC_CROUCH, ACC_STAND, MAX_ACCESSSTATE
	};

	// TODO: Whatever there is in the lump
	class RoutingLumpEntry
	{
		private:
			Vector3 _origin;
			int _level;
			EConnectionState _connectionStates[MAX_DIRECTIONS];
			EAccessState _accessState;
		public:
			RoutingLumpEntry (Vector3 origin, int level);
			RoutingLumpEntry (const RoutingLumpEntry &other);

			const Vector3& getOrigin (void) const {
				return _origin;
			}

			EConnectionState getConnectionState (const EDirection direction) const;
			/**
			 * @brief setter method for connection state
			 * @param direction direction to change connection state for
			 * @param connectionState connection state to set
			 */
			void setConnectionState (const EDirection direction, const EConnectionState connectionState)
			{
				_connectionStates[direction] = connectionState;
			}
			EAccessState getAccessState (void) const
			{
				return _accessState;
			}
			void setAccessState (const EAccessState accessState) {
				_accessState = accessState;
			}

			int getLevel (void) const {
				return _level;
			}
	};

	typedef std::list<RoutingLumpEntry> RoutingLumpEntries;

	class RoutingLump
	{
		private:
			RoutingLumpEntries _entries;

		public:
			RoutingLump ();

			virtual ~RoutingLump ();

			RoutingLumpEntries& getEntries ();

			/**@todo check whether this function should be private */
			void add (const RoutingLumpEntry& dataEntry);
	};
}
#endif /* ROUTINGLUMP_H_ */
