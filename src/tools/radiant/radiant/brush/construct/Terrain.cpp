#include "Terrain.h"

#include "radiant_i18n.h"

#include "gtkutil/dialog.h"

namespace brushconstruct
{
	const std::string Terrain::getName () const
	{
		return "brushTerrain";
	}

	/**
	 * @param brush The brush to create the terrain from
	 * @param bounds The bounds of the selected brush to create the terrain from
	 * @param sides This is ignore here
	 * @param projection
	 * @param shader The shader to use for the generated planes
	 */
	void Terrain::generate (Brush& brush, const AABB& bounds, std::size_t sides, const TextureProjection& projection,
			const std::string& shader)
	{
		gtkutil::infoDialog(_("Not yet implemented"));
		/** @todo implement me */
	}
}
