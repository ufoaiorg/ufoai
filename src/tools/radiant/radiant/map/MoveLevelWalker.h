#ifndef MOVELEVELWALKER_H_
#define MOVELEVELWALKER_H_

#include "scenelib.h"

namespace map {

class BrushMoveLevel
{
	private:

		bool _up;

	public:

		BrushMoveLevel (bool up) :
			_up(up)
		{
		}

		void operator() (Face& face) const
		{
			ContentsFlagsValue flags;
			face.GetFlags(flags);

			int levels = (flags.m_contentFlags >> 8) & 255;
			if (_up) {
				levels <<= 1;
				if (!levels)
					levels = 1;
			} else {
				const int newLevel = levels >> 1;
				if (newLevel != (newLevel & 255))
					return;
				levels >>= 1;
			}

			levels &= 255;
			levels <<= 8;

			flags.m_contentFlags &= levels;
			flags.m_contentFlags |= levels;

			face.SetFlags(flags);
		}
};

class MoveLevelWalker: public scene::Graph::Walker
{
	private:

		bool _up;

	public:

		MoveLevelWalker (bool up) :
			_up(up)
		{
		}

		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (path.top().get().visible()) {
				if (Node_isBrush(path.top())) {
					Brush* brush = Node_getBrush(path.top());
					if (brush != 0) {
						Brush_forEachFace(*brush, BrushMoveLevel(_up));
					}
				} else if (Node_isEntity(path.top())) {
					Entity* entity = Node_getEntity(path.top());
					if (entity != 0) {
						std::string name = entity->getKeyValue("classname");
						// TODO: get this info from entities.ufo
						if (name == "func_rotating" || name == "func_door" || name == "func_breakable" || name
								== "misc_item" || name == "misc_mission" || name == "misc_mission_alien" || name
								== "misc_model" || name == "misc_sound" || name == "misc_particle") {
							const std::string spawnflags = entity->getKeyValue("spawnflags");
							if (!spawnflags.empty()) {
								int levels = string::toInt(spawnflags) & 255;
								if (_up) {
									levels <<= 1;
								} else {
									levels >>= 1;
								}
								if (levels == 0)
									levels = 1;
								entity->setKeyValue("spawnflags", string::toString(levels & 255));
							}
						}
					}
				}
			}
			return true;
		}
};

} // namespace map

#endif /*MOVELEVELWALKER_H_*/
