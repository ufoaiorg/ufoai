#ifndef DEFLATEDARCHIVETEXTFILE_H_
#define DEFLATEDARCHIVETEXTFILE_H_

#include "iarchive.h"
#include "archivelib.h"
#include "DeflatedInputStream.h"

/**
 * ArchiveFile stored in a ZIP in DEFLATE format.
 */
class DeflatedArchiveTextFile: public ArchiveTextFile
{
		std::string m_name;
		std::size_t m_size;
		std::size_t m_filesize;
		FileInputStream m_istream;
		SubFileInputStream m_substream;
		DeflatedInputStream m_zipstream;
		BinaryToTextInputStream<DeflatedInputStream> m_textStream;
	public:
		typedef FileInputStream::size_type size_type;
		typedef FileInputStream::position_type position_type;

		DeflatedArchiveTextFile (const std::string& name, const std::string& archiveName, position_type position,
				size_type stream_size, size_type filesize) :
			m_name(name), m_size(stream_size), m_filesize(filesize), m_istream(archiveName), m_substream(m_istream,
					position, stream_size), m_zipstream(m_substream), m_textStream(m_zipstream)
		{
		}

		/** @brief returns the uncompressed size of the file */
		std::size_t size ()
		{
			return m_filesize;
		}
		const std::string getName () const
		{
			return m_name;
		}
		TextInputStream& getInputStream ()
		{
			return m_textStream;
		}
};

#endif /* DEFLATEDARCHIVETEXTFILE_H_ */
