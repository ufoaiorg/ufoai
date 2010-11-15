#ifndef BASICNAMESPACE_H_
#define BASICNAMESPACE_H_

#include "NameObserver.h"
#include <map>
#include "inamespace.h"

class Namespace: public INamespace
{
		typedef std::map<NameCallback, NameObserver> Names;
		Names m_names;
		UniqueNames m_uniqueNames;
	public:
		void mergeNames (const Namespace& other) const;
		void attach (const NameCallback& setName, const NameCallbackCallback& attachObserver);
		void detach (const NameCallback& setName, const NameCallbackCallback& detachObserver);
		void makeUnique (const std::string& name, const NameCallback& setName) const;
};
extern Namespace g_defaultNamespace;
extern Namespace g_cloneNamespace;
#endif /* BASICNAMESPACE_H_ */
