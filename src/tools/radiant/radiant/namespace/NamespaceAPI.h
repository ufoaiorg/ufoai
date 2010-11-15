#ifndef NAMESPACEAPI_H_
#define NAMESPACEAPI_H_

#include "inamespace.h"

#include "generic/constant.h"

class NamespaceAPI
{
		INamespace* m_namespace;
	public:
		typedef INamespace Type;
		STRING_CONSTANT(Name, "*");

		NamespaceAPI ();

		INamespace* getTable ();
};

#endif /* NAMESPACEAPI_H_ */
