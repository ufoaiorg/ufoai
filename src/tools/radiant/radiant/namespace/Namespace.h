#ifndef BASICNAMESPACE_H_
#define BASICNAMESPACE_H_

#include "NameObserver.h"
#include <map>
#include <vector>
#include "inamespace.h"
#include "iscenegraph.h"

class Namespace: public INamespace
{
		typedef std::map<NameCallback, NameObserver> Names;
		Names m_names;
		UniqueNames m_uniqueNames;

		// This is the list populated by gatherNamespaced(), see below
		typedef std::vector<Namespaced*> NamespacedList;
		NamespacedList _cloned;

	public:
		void mergeNames (const Namespace& other) const;
		void attach (const NameCallback& setName, const NameCallbackCallback& attachObserver);
		void detach (const NameCallback& setName, const NameCallbackCallback& detachObserver);
		void makeUnique (const std::string& name, const NameCallback& setName) const;

		/** greebo: Collects all Namespaced nodes in the subgraph,
		 * 			whose starting point is defined by <root>.
		 * 			This stores all the Namespaced* objects into
		 * 			a local list, which can subsequently be used
		 * 			by mergeClonedNames().
		 */
		void gatherNamespaced(scene::Node& root);

		/** greebo: This moves all gathered Namespaced nodes into this
		 * 			Namespace, making sure that all names are properly
		 * 			made unique.
		 */
		void mergeClonedNames();
};

#endif /* BASICNAMESPACE_H_ */
