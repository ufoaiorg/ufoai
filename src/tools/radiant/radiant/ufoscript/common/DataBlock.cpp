#include "DataBlock.h"

namespace scripts
{
	DataBlock::DataBlock (const std::string& filename, const std::size_t lineNumber, const std::string& id) :
		_data(""), _filename(filename), _lineNumber(lineNumber), _id(id)
	{
	}

	DataBlock::~DataBlock ()
	{
	}

	void DataBlock::setData (const std::string& data)
	{
		_data = data;
	}

	const std::string& DataBlock::getData () const
	{
		return _data;
	}

	const std::string& DataBlock::getID () const
	{
		return _id;
	}

	const std::string& DataBlock::getFilename () const
	{
		return _filename;
	}

	std::size_t DataBlock::getLineNumber () const
	{
		return _lineNumber;
	}
}
