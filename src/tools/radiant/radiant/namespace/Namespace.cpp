#include "Namespace.h"

#include "debugging/debugging.h"
#include <list>

void Namespace::attach (const NameCallback& setName, const NameCallbackCallback& attachObserver)
{
	std::pair<Names::iterator, bool> result = m_names.insert(Names::value_type(setName, m_uniqueNames));
	ASSERT_MESSAGE(result.second, "cannot attach name");
	attachObserver(NameObserver::NameChangedCaller((*result.first).second));
}

void Namespace::detach (const NameCallback& setName, const NameCallbackCallback& detachObserver)
{
	Names::iterator i = m_names.find(setName);
	ASSERT_MESSAGE(i != m_names.end(), "cannot detach name");
	detachObserver(NameObserver::NameChangedCaller((*i).second));
	m_names.erase(i);
}

void Namespace::makeUnique (const std::string& name, const NameCallback& setName) const
{
	char buffer[1024];
	name_write(buffer, m_uniqueNames.make_unique(name_read(name)));
	setName(buffer);
}

void Namespace::mergeNames (const Namespace& other) const
{
	typedef std::list<NameCallback> SetNameCallbacks;
	typedef std::map<std::string, SetNameCallbacks> NameGroups;
	NameGroups groups;

	UniqueNames uniqueNames(other.m_uniqueNames);

	for (Names::const_iterator i = m_names.begin(); i != m_names.end(); ++i) {
		groups[(*i).second.getName()].push_back((*i).first);
	}

	for (NameGroups::iterator i = groups.begin(); i != groups.end(); ++i) {
		name_t uniqueName(uniqueNames.make_unique(name_read((*i).first)));
		uniqueNames.insert(uniqueName);

		char buffer[1024];
		name_write(buffer, uniqueName);

		SetNameCallbacks& setNameCallbacks = (*i).second;

		for (SetNameCallbacks::const_iterator j = setNameCallbacks.begin(); j != setNameCallbacks.end(); ++j) {
			(*j)(buffer);
		}
	}
}

Namespace g_defaultNamespace;
Namespace g_cloneNamespace;
