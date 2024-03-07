#include "messages.hpp"

namespace hub
{
	CommandInfo* GetCommand(std::string commandName)
	{
		auto it = Commands.find(commandName);
		if (it == Commands.end())
			return nullptr;
		return it->second;
	}
	void RegisterCommand(std::string commandName, CommandFunc func, uint32_t argsCount)
	{
		CommandInfo* commandInfo = new CommandInfo();
		commandInfo->Func = func;
		commandInfo->ArgsCount = argsCount;
		Commands.insert({ commandName, commandInfo });
	}
	std::string SerializeArgs(CommandIn& command)
	{
		return std::string(command.auth) + '\0' + command.args;
	}
}