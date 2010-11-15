#ifndef BASICNAMESPACE_H_
#define BASICNAMESPACE_H_

#include "NameObserver.h"
#include <map>
#include "inamespace.h"

class BasicNamespace: public INamespace
{
		typedef std::map<NameCallback, NameObserver> Names;
		Names m_names;
		UniqueNames m_uniqueNames;
	public:
		~BasicNamespace (void);
		void mergeNames (const BasicNamespace& other) const;
		void attach (const NameCallback& setName, const NameCallbackCallback& attachObserver);
		void detach (const NameCallback& setName, const NameCallbackCallback& detachObserver);
		void makeUnique (const std::string& name, const NameCallback& setName) const;
};
extern BasicNamespace g_defaultNamespace;
extern BasicNamespace g_cloneNamespace;
#endif /* BASICNAMESPACE_H_ */
