#ifndef SCENEGRAPHMODULE_H_
#define SCENEGRAPHMODULE_H_

#include "generic/constant.h"
#include "CompiledGraph.h"

class SceneGraphAPI
{
		CompiledGraph* _scenegraph;
	public:
		typedef scene::Graph Type;
		STRING_CONSTANT(Name, "*");

		// Constructor
		SceneGraphAPI ();
		~SceneGraphAPI ();

		// Accessor method to retrieve the contained CompiledGraph instance.
		scene::Graph* getTable ();
};

// Accessor method to retrieve the treemodel from the CompiledGraph instance
GraphTreeModel* scene_graph_get_tree_model ();

#endif /*SCENEGRAPHMODULE_H_*/
