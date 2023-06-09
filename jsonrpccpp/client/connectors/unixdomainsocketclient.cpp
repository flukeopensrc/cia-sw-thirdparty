/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    unixdomainsocketclient.cpp
 * @date    11.05.2015
 * @author  Alexandre Poirot <alexandre.poirot@legrand.fr>
 * @license See attached LICENSE.txt
 ************************************************************************/

#include "unixdomainsocketclient.h"
#include <string>
#include <string.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <iostream>

#define BUFFER_SIZE 64
#define PATH_MAX 108
#ifndef DELIMITER_CHAR
#define DELIMITER_CHAR char(0x0A)
#endif //DELIMITER_CHAR

using namespace jsonrpc;
using namespace std;

	UnixDomainSocketClient::UnixDomainSocketClient(const std::string& path)
: path(path)
{

	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0)
	{
        throw JsonRpcException(Errors::ERROR_CLIENT_CONNECTOR, "Could not create unix domain socket");
	}

	memset(&address, 0, sizeof(sockaddr_un));

    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, PATH_MAX, "%s", this->path.c_str());

	if(connect(socket_fd, (struct sockaddr *) &address,  sizeof(sockaddr_un)) != 0)
	{
		throw JsonRpcException(Errors::ERROR_CLIENT_CONNECTOR, "Could not connect to: " + this->path);
	}

}

UnixDomainSocketClient::~UnixDomainSocketClient()
{
    close (socket_fd);
}

void UnixDomainSocketClient::SendRPCMessage(const std::string& message, std::string& result) throw (JsonRpcException)
{
	int nbytes;
	char buffer[BUFFER_SIZE];

	bool fullyWritten = false;
	string toSend = message;
	do
	{
		ssize_t byteWritten = write(socket_fd, toSend.c_str(), toSend.size());
		if(static_cast<size_t>(byteWritten) < toSend.size())
		{
			int len = toSend.size() - byteWritten;
			toSend = toSend.substr(byteWritten + sizeof(char), len);
		}
		else
			fullyWritten = true;
	} while(!fullyWritten);

	do
	{
		nbytes = read(socket_fd, buffer, BUFFER_SIZE);
		string tmp;
		tmp.append(buffer, nbytes);
		result.append(buffer,nbytes);

	} while(result.find(DELIMITER_CHAR) == string::npos);
}
