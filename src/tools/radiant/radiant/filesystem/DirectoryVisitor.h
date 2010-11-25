#ifndef DIRECTORYVISITOR_H_
#define DIRECTORYVISITOR_H_

#include "ifilesystem.h"
#include "iarchive.h"

#include "os/path.h"

#include <set>
#include <string>

class DirectoryVisitor: public Archive::Visitor
{
		// The VirtualFileSystem::Visitor to call for each located file
		const FileNameCallback& _callback;

		// Set of already-visited files
		std::set<std::string>& _visitedDirs;

		// Directory to search within
		std::string _directory;

	public:

		// Constructor
		DirectoryVisitor (const FileNameCallback& callback, const std::string& dir, std::set<std::string>& visitedDirs) :
			_callback(callback), _visitedDirs(visitedDirs), _directory(dir)
		{
		}

		// Required visit function
		void visit (const std::string& name)
		{
			// Cut off the base directory prefix
			std::string subname = os::makeRelative(name, _directory);

			if (subname == name)
				return;

			int lastCharIndex = subname.size() - 1;
			if (subname[lastCharIndex] == '/')
				subname.erase(lastCharIndex);

			if (_visitedDirs.find(subname) != _visitedDirs.end()) {
				return; // already visited
			}

			// Suitable file, call the callback and add to visited file set
			_callback(subname);

			_visitedDirs.insert(subname);
		}
};

#endif /*DIRECTORYVISITOR_H_*/
