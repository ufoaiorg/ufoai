#pragma once

#include <string>

class ScopedCommand {
private:
	const std::string _scopeLeaveCommand;
public:
	ScopedCommand (const std::string& scopeEnterCommand, const std::string& scopeLeaveCommand) :
			_scopeLeaveCommand(scopeLeaveCommand)
	{
		Cmd_ExecuteString("%s", scopeEnterCommand.c_str());
	}

	ScopedCommand (const std::string& command, const std::string& scopeEnterParameter, const std::string& scopeLeaveParameter) :
			_scopeLeaveCommand(command + " " + scopeLeaveParameter)
	{
		const std::string enter = command + " " + scopeEnterParameter;
		Cmd_ExecuteString("%s", enter.c_str());
	}

	~ScopedCommand ()
	{
		Cmd_ExecuteString("%s", _scopeLeaveCommand.c_str());
	}
};
