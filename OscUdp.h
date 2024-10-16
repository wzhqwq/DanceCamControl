#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <cstdint>

class OscUdp
{
public:
	OscUdp(int listenPort, int destPort);
	~OscUdp()
	{
		closesocket(clientFd);
		closesocket(serverFd);
		WSACleanup();
	}

    size_t send(const void* buffer, size_t size);

    size_t recv(void* buffer, size_t size);

private:
    int clientFd;
	int serverFd;
	sockaddr_in listenAddr;
	sockaddr_in destAddr;
};

