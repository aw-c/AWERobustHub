#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#define SQLITECPP_COMPILE_DLL
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/SQLiteCppExport.h>
#include <filesystem>
#include <kissnet.hpp>
#include <json/json.h>
#include "../shared/messages.hpp"
#include <numeric>

//#define TEST_BAN
//#define TEST_ARGS
//#define TEST_JSON

namespace fs = std::filesystem;
namespace sql = SQLite;

sql::Database* WorkingDatabase;
std::map<std::string, hub::UserInfo*> CachedUsers;
Json::Reader JsonReader;

const std::string BaseStatement = R"(
	CREATE TABLE IF NOT EXISTS "users" (
		"id" INTEGER,
		"public_token" TEXT,
		"private_token" TEXT,
		"roles" TEXT,
		"full_access" BOOL,
		"created_by" TEXT
	);

	CREATE TABLE IF NOT EXISTS "minges" (
		"ckey" TEXT,
		"guid" TEXT,
		"bans" TEXT,
		"expired_bans" TEXT
	);

	CREATE TABLE IF NOT EXISTS "servers" (
		"name" TEXT
	);
)";

//bool CanUseSpecificCommand(std::string server_name, hub::UserInfo& userInfo)
//{
//	if (userInfo.fullAccess)
//		return true;
//
//	auto it = userInfo.servers_privilegies.find(server_name);
//	if (it != userInfo.servers_privilegies.end())
//		if (it->second == server_name)
//		return true;
//	return false;
//}

std::pair<bool, std::string> HasUserGroup(std::string server_name, std::string required_group, hub::UserInfo& userInfo)
{
	if (userInfo.fullAccess)
		return {true, "all"};

	if (required_group == "user")
		return {true, "all"};

	auto it = userInfo.servers_privilegies.find(server_name);
	if (it != userInfo.servers_privilegies.end())
		if (it->first == server_name)
			if (it->second == required_group || (required_group == "admin" && it->second == "superadmin"))
				return {true, it->second};
	return { false, "" };
}

/*
*	STRUCT `info`
*	{
*		["ss220"] = 122131312,
*		["corvax"] = 234324,
*	}
* 
*/

std::vector<std::string> GetServers()
{
	std::vector<std::string> servers;
	sql::Statement query(*WorkingDatabase,"SELECT * FROM `servers`");
	while (query.executeStep())
	{
		servers.push_back(query.getColumn(0).getString());
	}
	return servers;
}

bool IsServerExists(std::string server_name)
{
	auto servers = GetServers();
	auto p = std::find(servers.begin(), servers.end(), server_name);

	if (p != servers.end())
		return true;
	return false;
}

const char* GETUSER_STATEMENT = "SELECT * FROM `users` WHERE (ckey = ?, guid = ??)";
const char* BAN_STATEMENT = "INSERT INTO `minges` (`name`, `guid`, `bans`, `expired_bans`) VALUES (?, ??, ???, ????);";

bool Ban(std::vector<const char*> args, hub::UserInfo& userInfo, std::string& message)
{
	const char* server_name = args[0];
	auto bsAccess = HasUserGroup(server_name, "admin", userInfo);
	if (!IsServerExists(server_name))
		return false;

	if (bsAccess.first)
	{
		sql::Statement query(*WorkingDatabase, GETUSER_STATEMENT);

		query.bind(0, args[1]);
		query.bind(1, args[2]);

		query.executeStep();

		if (!query.hasRow())
		{
			Json::Value json;
			json[server_name] = 0;

			query = sql::Statement(*WorkingDatabase, BAN_STATEMENT);
			query.bind(1, args[1]);
			query.bind(2, args[2]);
			query.bind(3, json.asString().c_str());
			query.bind(4, "[]");

			return true;
		}
		//json[]
		return true;
	}

	message = "";
	return false;
}

bool AddServer(std::vector<std::string> args, hub::UserInfo& userInfo, std::string& message)
{
	return true;
}

bool UnBan(std::vector<std::string> args, hub::UserInfo& userInfo, std::string& message)
{
	return true;
}

void RegisterCommands()
{
	hub::RegisterCommand("ban", Ban, 4); // server, ckey, uniqueId, reason
	hub::RegisterCommand("unban", Ban, 3); // ckey, server, reason
	hub::RegisterCommand("unbanall", Ban, 2); // ckey, reason
	hub::RegisterCommand("setaccess", Ban, 2); // public token, server
	hub::RegisterCommand("createprivatetoken", Ban, 2); // public token, server
}

void PrepareDB(fs::path file)
{
	WorkingDatabase = new sql::Database(file.string(), sql::OPEN_READWRITE | sql::OPEN_CREATE);
	WorkingDatabase->exec(BaseStatement);
}

void TestBan()
{
	std::string expt;
	std::vector<const char*> args { "corvax", "alanwake" };
	hub::UserInfo info;
	info.fullAccess = true;
	info.public_token = "surxnavi";
	Ban(args, info, expt);
}

std::map<std::string, std::string> PrepareServerRoles(std::string& sServersRoles)
{
	Json::Value json;
	JsonReader.parse(sServersRoles, json);
	std::map<std::string, std::string> mServerRoles;

	for (Json::String v : json.getMemberNames())
		mServerRoles.insert(std::make_pair(v, json[v].asString()));

	return mServerRoles;
}

hub::UserInfo* FindUser(std::string& private_token)
{
	auto it = CachedUsers.find(private_token);

	if (it != CachedUsers.end())
		if (it->second != nullptr)
			return it->second;

	hub::UserInfo* info;

	sql::Statement query(*WorkingDatabase, "SELECT * from `users` WHERE (private_token = ?)");
	query.bind(1, private_token);

	if (!query.executeStep())
		return nullptr;

	std::string server_roles = query.getColumn(3).getString();

	info = new hub::UserInfo();
	info->private_token = private_token;

	info->id = query.getColumn(0).getInt();
	info->public_token = query.getColumn(1).getString();
	info->servers_privilegies = PrepareServerRoles(server_roles);
	info->fullAccess = query.getColumn(4).getInt();
	info->createdBy = query.getColumn(5).getString();


	return info;
}

const char* CreateUserLabel(hub::UserInfo& info)
{
	std::stringstream ss;
	ss << info.public_token << "[" << 0 << "]" << std::endl;

	return ss.str().c_str();
}

bool isTerminator(char& symb)
{
	return symb == '\0' || symb == ' ';
}

void DeleteStrs(std::vector<const char*> strs)
{
	for (auto v : strs)
		delete[] v;
}

std::vector<const char*> DeserializeArgs(const char* str, uint32_t& realLength)
{
	std::vector<const char*> args;
	uint32_t prevStrStart = 0;
	uint32_t currentOffset = 0;

	while (currentOffset < realLength)
	{
		char term = str[currentOffset];
		if (isTerminator(term))
		{
			uint32_t str_len = currentOffset - prevStrStart;
			const char* new_str = new char[currentOffset - prevStrStart];
			new_str = std::strncpy((char*)new_str, str + prevStrStart, str_len);

			args.push_back(new_str);
			prevStrStart = currentOffset + 1;
		}
		currentOffset++;
	}

	return args;
}

void TestDeserializeArgs()
{
	const char* pseudoArgs = "tdrtdfg ban 0 1223 dsfsdfdsf";
	uint32_t len = std::strlen(pseudoArgs) + 1;
	for (auto str : DeserializeArgs(pseudoArgs, len))
	{
		printf("%s\n", str);
	}
}

void ProccessArgs(hub::UserInfo& user, std::vector<const char*> args, std::string& message)
{
	for (auto v : args)
		printf("%s\n", v);
}

struct kissnet_tcp_override
{
	kissnet::tcp_socket client;
	explicit kissnet_tcp_override(kissnet::tcp_socket& server)
	{
		client = server.accept();

		std::thread thr([=] {this->ProcessConnection(); });
		thr.detach();
	}
	void ProcessConnection()
	{
		uint32_t length = client.bytes_available();

		printf("received %d bytes\n", length);

		char* str = new char[length];
		client.recv((std::byte*)str, length);

		std::vector<const char*> args = DeserializeArgs(str, length);
		std::string private_token = args[0];

		hub::UserInfo* user = FindUser(private_token);

		std::string message;

		if (user != nullptr)
		{
			printf("Accepted connection from %s %s", client.get_recv_endpoint().address.c_str(), CreateUserLabel(*user));
			ProccessArgs(*user, args, message);
		}

		printf("Everything okay\n");

		delete[] str;
		DeleteStrs(args);

		client.close();

		delete this;
	}
};

int main(int argc, char** argv)
{

//#ifdef TEST_JSON
//	printf("test json\n");
//	std::string test_json(R"({"ss220": "admin", "corvax": "owner"})");
//	PrepareServerRoles(test_json);
//#endif
//#ifdef TEST_ARGS
//	TestDeserializeArgs();
//	return 0;
//#endif

    if (argc == 3)
    {
        const char* name = argv[1];
        uint16_t port = std::atoi(argv[2]);
        printf("Starting hub. Name: %s Port: %d\n", name, port);
        printf("Trying to find %s.db\n", name);
        fs::path file = fs::path(std::string(std::getenv("LOCALAPPDATA")) + "\\AWERobustHub\\" + name + ".db");
		fs::create_directories(file.parent_path());

		PrepareDB(file);

		printf("Starting listening socket\n");

#ifdef TEST_BAN
		printf("test adding ban\n");
		TestBan();
#endif

		kissnet::tcp_socket server(kissnet::endpoint("127.0.0.1", port));
		server.bind();
		server.listen();

		while (true)
		{
			new kissnet_tcp_override(server);
		}

        return 0;
    }
    printf("Unable to start hub. You should enter name, port\n");
    return 1;
}