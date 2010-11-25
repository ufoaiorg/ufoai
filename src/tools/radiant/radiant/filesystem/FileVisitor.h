#ifndef FILEVISITOR_H_
#define FILEVISITOR_H_

#include "ifilesystem.h"
#include "iarchive.h"

#include "os/path.h"

#include <set>
#include <string>

class FileVisitor: public Archive::Visitor
{
		// The VirtualFileSystem::Visitor to call for each located file
		const FileNameCallback& _callback;

		// Set of already-visited files
		std::set<std::string>& _visitedFiles;

		// Directory to search within
		std::string _directory;

		// Extension to match
		std::string _extension;

		// The length of the directory name
		std::size_t _dirPrefixLength;

		bool _visitAll;

		std::size_t _extLength;

	public:

		// Constructor
		FileVisitor (const FileNameCallback& callback, const std::string& dir, const std::string& ext, std::set<
				std::string>& visitedFiles) :
			_callback(callback), _visitedFiles(visitedFiles), _directory(dir), _extension(ext), _dirPrefixLength(
					_directory.length()), _visitAll(_extension == "*"), _extLength(_extension.length())
		{
		}

		// Required visit function
		void visit (const std::string& name)
		{
			// Cut off the base directory prefix
			std::string subname = name.substr(_dirPrefixLength);

			// Check for matching file extension
			if (!_visitAll) {
				// The dot must be at the right position
				if (subname.length() <= _extLength || subname[subname.length() - _extLength - 1] != '.') {
					return;
				}

				// And the extension must match
				std::string ext = subname.substr(subname.length() - _extLength);
#ifdef WIN32
				// Treat extensions case-insensitively in Windows
				ext = string::toLower(ext);
#endif
				if (ext != _extension) {
					return; // extension mismatch
				}
			}

			if (_visitedFiles.find(subname) != _visitedFiles.end()) {
				return; // already visited
			}

			// Suitable file, call the callback and add to visited file set
			_callback(subname);

			_visitedFiles.insert(subname);
		}
};

#endif /*FILEVISITOR_H_*/
