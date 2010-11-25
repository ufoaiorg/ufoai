/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "DirectoryArchive.h"

#include "AutoPtr.h"
#include "idatastream.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "stream/filestream.h"
#include "stream/textfilestream.h"
#include "string/string.h"
#include "os/path.h"
#include "os/file.h"
#include "os/dir.h"
#include "archivelib.h"

#include "UnixPath.h"

DirectoryArchive::DirectoryArchive (const std::string& root) :
	m_root(root)
{
}

ArchiveFile* DirectoryArchive::openFile (const std::string& name)
{
	UnixPath path(m_root);
	path.push_filename(name);
	AutoPtr<DirectoryArchiveFile> file(new DirectoryArchiveFile(name, path));
	if (!file->failed()) {
		return file.release();
	}
	return 0;
}

ArchiveTextFile* DirectoryArchive::openTextFile (const std::string& name)
{
	UnixPath path(m_root);
	path.push_filename(name);
	AutoPtr<DirectoryArchiveTextFile> file(new DirectoryArchiveTextFile(name, path));
	if (!file->failed()) {
		return file.release();
	}

	AutoPtr<DirectoryArchiveTextFile> absfile(new DirectoryArchiveTextFile(name, name));
	if (!absfile->failed()) {
		return absfile.release();
	}
	return 0;
}

bool DirectoryArchive::containsFile (const std::string& name)
{
	UnixPath path(m_root);
	path.push_filename(name);
	return file_readable(path);
}

void DirectoryArchive::forEachFile (VisitorFunc visitor, const std::string& root)
{
	std::vector<Directory*> dirs;
	UnixPath path(m_root);
	path.push(root);
	dirs.push_back(directory_open(path));

	while (!dirs.empty() && directory_good(dirs.back())) {
		const char* name = directory_read_and_increment(dirs.back());

		if (name == 0) {
			directory_close(dirs.back());
			dirs.pop_back();
			path.pop();
		} else if (!string_equal(name, ".") && !string_equal(name, "..")) {
			path.push_filename(name);

			bool is_directory = file_is_directory(path);

			if (!is_directory)
				visitor.file(os::makeRelative(path, m_root));

			path.pop();

			if (is_directory) {
				path.push(name);

				if (!visitor.directory(os::makeRelative(path, m_root), dirs.size()))
					dirs.push_back(directory_open(path));
				else
					path.pop();
			}
		}
	}
}

Archive* OpenDirArchive (const std::string& name)
{
	return new DirectoryArchive(name);
}
