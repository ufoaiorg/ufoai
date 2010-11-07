#include "NameObserver.h"

#include "uniquenames.h"

void NameObserver::construct ()
{
	if (!empty()) {
		m_names.insert(name_read(getName()));
	}
}

void NameObserver::destroy (void)
{
	if (!empty()) {
		m_names.erase(name_read(getName()));
	}
}

//NameObserver& operator= (const NameObserver& other);

NameObserver::NameObserver (UniqueNames& names) :
	m_names(names)
{
	construct();
}
//NameObserver (const NameObserver& other) :
//	m_names(other.m_names), m_name(other.m_name)
//{
//	construct();
//}
NameObserver::~NameObserver (void)
{
	destroy();
}
bool NameObserver::empty () const
{
	return m_name.empty();
}
const std::string& NameObserver::getName () const
{
	return m_name;
}
void NameObserver::nameChanged (const std::string& name)
{
	destroy();
	m_name = name;
	construct();
}
