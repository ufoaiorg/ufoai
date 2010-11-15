#ifndef MAPROOTNODE_H_
#define MAPROOTNODE_H_

#include "nameable.h"
#include "traverselib.h"
#include "selectionlib.h"
#include "UndoFileChangeTracker.h"
#include "scenelib.h"
#include "instancelib.h"

namespace map {

/** greebo: This is the root node of the map, it gets inserted as
 * 			the top node into the scenegraph. Each entity node is
 * 			inserted as child node to this.
 *
 * Note:	Inserting a child node to this MapRoot automatically
 * 			triggers an instantiation of this child node.
 *
 * 			The contained InstanceSet functions as Traversable::Observer
 * 			and instantiates the node as soon as it gets notified about it.
 */
class RootNode :
	public scene::Node,
	public scene::Instantiable,
	public Nameable,
	public TransformNode,
	public MapFile,
	public scene::Traversable
{
	IdentityTransform m_transform;
	TraversableNodeSet m_traverse;
	InstanceSet m_instances;

	// The actual name of the map
	std::string _name;

	UndoFileChangeTracker m_changeTracker;
public:
	void insert (Node& node);
	void erase (Node& node);
	void traverse (const Walker& walker);
	bool empty () const;

	// TransformNode implementation
	const Matrix4& localToParent() const;

	// Nameable implementation
	std::string name() const;

	// MapFile implementation
	void save();
	bool saved() const;
	void changed();
	void setChangedCallback(const Callback& changed);
	std::size_t changes() const;

	RootNode (const std::string& name);
	~RootNode ();

	InstanceCounter m_instanceCounter;
	void instanceAttach (const scene::Path& path);
	void instanceDetach (const scene::Path& path);

	scene::Node& clone () const;

	scene::Instance* create (const scene::Path& path, scene::Instance* parent);
	void forEachInstance (const scene::Instantiable::Visitor& visitor);
	void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
	scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);
};

} // namespace map

inline NodeSmartReference NewMapRoot (const std::string& name)
{
	return NodeSmartReference(*(new map::RootNode(name)));
}

#endif /*MAPROOTNODE_H_*/
