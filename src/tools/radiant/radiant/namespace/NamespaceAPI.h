#ifndef NAMESPACEAPI_H_
#define NAMESPACEAPI_H_

#include "inamespace.h"

#include "generic/constant.h"

class NamespaceAPI
{
		Namespace* m_namespace;
	public:
		typedef Namespace Type;
		STRING_CONSTANT(Name, "*");

		NamespaceAPI ();

		Namespace* getTable ();
};

#endif /* NAMESPACEAPI_H_ */
