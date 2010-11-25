#include "iarchive.h"

#include "os/path.h"

class DirectoryListVisitor: public Archive::Visitor
{
		GSList*& m_matches;
		const std::string& m_directory;

		void pathlist_prepend_unique (GSList*& pathlist, char* path)
		{
			if (g_slist_find_custom(pathlist, path, (GCompareFunc) path_compare) == 0) {
				pathlist = g_slist_prepend(pathlist, path);
			} else {
				g_free(path);
			}
		}

	public:
		DirectoryListVisitor (GSList*& matches, const std::string& directory) :
			m_matches(matches), m_directory(directory)
		{
		}
		void visit (const std::string& name)
		{
			const char* subname = path_make_relative(name, m_directory);
			if (subname != name.c_str()) {
				if (subname[0] == '/')
					++subname;
				char* dir = g_strdup(subname);
				char* last_char = dir + strlen(dir);
				if (last_char != dir && *(--last_char) == '/')
					*last_char = '\0';
				pathlist_prepend_unique(m_matches, dir);
			}
		}
};
