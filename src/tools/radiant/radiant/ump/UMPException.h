#pragma once

namespace map
{
	namespace ump
	{
		class UMPException
		{
			private:
				std::string _msg;
			public:
				UMPException (const std::string& msg) :
					_msg(msg)
				{
				}

				const std::string& getMessage () const
				{
					return _msg;
				}
		};
	}
}
