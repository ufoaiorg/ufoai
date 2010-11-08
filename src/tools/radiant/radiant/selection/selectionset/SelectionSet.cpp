#include "SelectionSet.h"

#include "iscenegraph.h"
#include "scenelib.h"
#include "signal/isignal.h"

namespace selection
{

SelectionSet::~SelectionSet()
{
	GlobalSceneGraph().removeEraseObserver(this);
}

SelectionSet::SelectionSet(const std::string& name) :
	_name(name)
{
	GlobalSceneGraph().addEraseObserver(this);
}

const std::string& SelectionSet::getName()
{
	return _name;
}

bool SelectionSet::empty()
{
	return _nodes.empty();
}

void SelectionSet::clear()
{
	_nodes.clear();
}

void SelectionSet::onErase(scene::Instance *instance) {
	if (empty())
		return;

	NodeSet::iterator i = _nodes.find(instance);
	if (i != _nodes.end())
		_nodes.erase(i);
}

void SelectionSet::select()
{
	for (NodeSet::iterator i = _nodes.begin(); i != _nodes.end(); ++i)
	{
		scene::Instance* instance = *i;

#if 0
		// TODO: mattn - fix this
		// skip deleted nodes
		if (instance == NULL)
			continue;
#endif

		// skip invisible, non-instantiated nodes
		if (!instance->visible())
			continue;

		Selectable* selectable = Instance_getSelectable(*instance);
		if (selectable)
			selectable->setSelected(true);
	}
}

void SelectionSet::deselect()
{
	for (NodeSet::iterator i = _nodes.begin(); i != _nodes.end(); ++i)
	{
		scene::Instance* instance = *i;

#if 0
		// TODO: mattn - fix this
		// skip deleted nodes
		if (instance == NULL)
			continue;
#endif

		// skip invisible, non-instantiated nodes
		if (!instance->visible())
			continue;

		Selectable* selectable = Instance_getSelectable(*instance);
		if (selectable)
			selectable->setSelected(false);
	}
}

void SelectionSet::addNode(scene::Instance* instance)
{
	_nodes.insert(instance);
}

void SelectionSet::assignFromCurrentScene()
{
	clear();

	class Walker :
		public SelectionSystem::Visitor
	{
	private:
		SelectionSet& _set;

	public:
		Walker(SelectionSet& set) :
			_set(set)
		{}

		void visit(scene::Instance& instance) const
		{
			_set.addNode(&instance);
		}

	} _walker(*this);

	GlobalSelectionSystem().foreachSelected(_walker);
}

} // namespace
