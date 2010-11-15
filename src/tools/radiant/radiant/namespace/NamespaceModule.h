#ifndef NAMESPACEMODULE_H_
#define NAMESPACEMODULE_H_

#include "inamespace.h"

#include "generic/constant.h"

class NamespaceAPI
{
		INamespace* _namespace;
	public:
		typedef INamespace Type;
		STRING_CONSTANT(Name, "*");

		NamespaceAPI ();

		~NamespaceAPI ();

		INamespace* getTable ();
};

#endif /* NAMESPACEAPI_H_ */
