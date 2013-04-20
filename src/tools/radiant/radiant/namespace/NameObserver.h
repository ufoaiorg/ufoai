#pragma once

#include "uniquenames.h"
#include "generic/callback.h"

class NameObserver
{
		UniqueNames& m_names;
		std::string m_name;
		void construct (void);
		void destroy (void);
		bool empty (void) const;

		NameObserver& operator= (const NameObserver& other);
	public:
		NameObserver (UniqueNames& names);
		~NameObserver (void);

		void nameChanged (const std::string& name);
		typedef MemberCaller1<NameObserver, const std::string&, &NameObserver::nameChanged> NameChangedCaller;

		const std::string& getName () const;
};
