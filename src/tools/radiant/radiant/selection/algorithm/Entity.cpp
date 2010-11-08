#include "Entity.h"

#include "radiant_i18n.h"
#include "selectionlib.h"
#include "../../mainframe.h"
#include "iregistry.h"
#include "itextstream.h"
#include "entitylib.h"
#include "gtkutil/dialog.h"

#include "../../entity.h"

namespace selection {
	namespace algorithm {

void connectSelectedEntities()
{
	if (GlobalSelectionSystem().countSelected() == 2)
	{
		GlobalEntityCreator().connectEntities(
			GlobalSelectionSystem().penultimateSelected().path(),	// source
			GlobalSelectionSystem().ultimateSelected().path()		// target
		);
	}
	else
	{
		gtkutil::errorDialog(_("Exactly two entities must be selected for this operation."));
	}
}

	} // namespace algorithm
} // namespace selection
