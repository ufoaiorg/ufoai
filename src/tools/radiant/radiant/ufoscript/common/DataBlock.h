#ifndef DATABLOCK_H_
#define DATABLOCK_H_

#include <string>

namespace scripts
{
	/**
	 * UFO Script data blocks that contains additional information like
	 * the filename the the line number in that file.
	 */
	class DataBlock
	{

		private:

			std::string _data;
			std::string _filename;
			std::size_t _lineNumber;
			std::string _id;

		public:
			/**
			 * Constructor that initializes a data block element with its filename and its linenumber
			 * @param filename The filename the datablock is stored in
			 * @param lineNumber The linenumber the datablock can be found at in the given file
			 * @param id unique name of datablock (usually the keyword between the section name and the opening parenthesis)
			 */
			DataBlock (const std::string& filename, const std::size_t lineNumber, const std::string& id);

			virtual ~DataBlock ();

			/**
			 * @param data The data of the block
			 */
			void setData (const std::string& data);

			/**
			 * @return The data in this block
			 */
			const std::string& getData () const;

			const std::string& getID() const;

			const std::string& getFilename() const;

			std::size_t getLineNumber() const;
	};
}

#endif /* DATABLOCK_H_ */
