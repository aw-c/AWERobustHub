#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include "../shared/messages.hpp"
#include <kissnet.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// If you want to create your own Client/API like this shit, 
// You should send a request to TCP socket hub's server
// With next data:
// String with max data's size (1024 + 18 + max command length) bytes
// "<private token> <command name> [... args through space]"
// In DATAGET section you can receive:
// First 1 byte status code of your operation in Integer
// Next 4 bytes string's legth (max string length is 1024 bytes)
// Next (string's length) bytes is text message of error or success with description. You can not receive this section by some reasons.
// Also, you can doesn't specify command name in your string to check server's status.

bool TryGetGateway(std::string& endpoint)
{
    fs::path path("endpoint.ini");

    if (!fs::exists(path))
        return false;

    std::ifstream file(path);
    std::getline(file, endpoint);

    file.close();

    return true;
}

int main(int argc, const char** argv)
{
    std::string server_endpoint;
    if (!TryGetGateway(server_endpoint))
    {
        printf("File endpoint.ini doesn't exists\n");
        return 1;
    }
    if (argc == 1)
    {
        printf("You have to enter private_token and command. If you haven't private_token write 0\n");
        return 1;
    }
    int startReadingArgsFromKey = 1;
    uint32_t messageSize = 1024;
    if (std::strcmp(argv[1], "-msg") == 0)
    {
        startReadingArgsFromKey = 3;
        messageSize = std::atoi(argv[2]);
    }
    uint32_t argslen = std::strlen(argv[startReadingArgsFromKey+1]+1);
    std::string formed_args = argv[startReadingArgsFromKey];

    hub::CommandIn command{.auth = argv[startReadingArgsFromKey], .args = formed_args, .maxMessage = messageSize };

    kissnet::tcp_socket cl(kissnet::endpoint(server_endpoint.c_str()));
    auto s = cl.connect(1000);

    if (s == kissnet::socket_status::valid)
    {
        std::string serialized = hub::SerializeArgs(command);
        cl.send((std::byte*)serialized.c_str(), serialized.size());
    }
    else
    {
        printf("Connection cannot be established\n");
        return 1;
    }
}