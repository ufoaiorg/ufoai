#ifndef TRAVERSE_
#define TRAVERSE_

#include "scenelib.h"

namespace map {

/** greebo: Traverses the entire scenegraph (used as entry point/TraverseFunc for map saving)
 */
void Map_Traverse (scene::Node& root, const scene::Traversable::Walker& walker);

/** greebo: Traverses only the selected items
 */
void Map_Traverse_Selected (scene::Node& root, const scene::Traversable::Walker& walker);

} // namespace map

#endif /*TRAVERSE_*/
