#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <string>

namespace hub
{
	struct UserInfo;
	struct CommandInfo;
	typedef bool (*CommandFunc)(std::vector<const char*> args, hub::UserInfo& userInfo, std::string& message);
	static std::map<std::string, CommandInfo*> Commands;

	constexpr size_t MESSAGE_SIZE = 8192;
	constexpr size_t ARGS_SIZE = 1024;
	constexpr size_t AUTH_SIZE = 18;
	struct CommandIn
	{
		std::string auth;
		std::string args;
		uint32_t maxMessage = MESSAGE_SIZE; // message to read from client
	};
	struct CommandOut
	{
		char code;
		std::string message;
	};

	enum class Codes
	{
		UNDEFINED_EXPECTION = -1,
		NOT_ENOUGH_ARGS = -2,
		CONNECTION_CLOSED = -3,
		INVALID_USER = -4,
	};
	constexpr uint32_t PrivilegiesCount = 4;
	static const char* Privilegies[PrivilegiesCount] = {
		"user",
		"admin",
		"superadmin",
		"owner",
	};
	struct CommandInfo
	{
		const char* RequiredPrivilegie = "user";
		uint32_t ArgsCount = 0;
		CommandFunc Func;
	};
	struct UserInfo
	{
		std::string public_token;
		std::string private_token;
		std::map<std::string, std::string> servers_privilegies;
		bool fullAccess = false;
		uint32_t id;
		std::string createdBy;
	};

	CommandInfo* GetCommand(std::string commandName);
	void RegisterCommand(std::string commandName, CommandFunc func, uint32_t argsCount = 0);
	std::string SerializeArgs(CommandIn& command);
}