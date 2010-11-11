#ifndef MAPCOMPILEEXCEPTION_H_
#define MAPCOMPILEEXCEPTION_H_

#include <stdexcept>
#include <string>

// Exception to indicate failure while using the mapcompiler

class MapCompileException: public std::runtime_error {
public:
	// Constructor. Must initialise the parent.
	MapCompileException(const std::string& what) :
		std::runtime_error(what) {
	}
};

#endif /*MAPCOMPILEEXCEPTION_H_*/
