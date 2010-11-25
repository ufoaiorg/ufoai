#include "iarchive.h"

#include "os/path.h"

class FileListVisitor: public Archive::Visitor
{
		GSList*& m_matches;
		const std::string& m_directory;
		const std::string& m_extension;

		void pathlist_prepend_unique (GSList*& pathlist, char* path)
		{
			if (g_slist_find_custom(pathlist, path, (GCompareFunc) path_compare) == 0) {
				pathlist = g_slist_prepend(pathlist, path);
			} else {
				g_free(path);
			}
		}

	public:
		FileListVisitor (GSList*& matches, const std::string& directory, const std::string& extension) :
			m_matches(matches), m_directory(directory), m_extension(extension)
		{
		}
		void visit (const std::string& name)
		{
			const std::string subname = os::makeRelative(name, m_directory);
			if (subname != name) {
				if (extension_equal(os::getExtension(subname), m_extension))
					pathlist_prepend_unique(m_matches, g_strdup(subname.c_str()));
			}
		}
};
