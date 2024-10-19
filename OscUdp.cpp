#include "framework.h"
#include "OscUdp.h"

OscUdp::OscUdp(int listenPort, int destPort) {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		throw std::runtime_error("WSAStartup failed");
	}
	if ((clientFd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		throw std::runtime_error("socket failed");
    }
	if ((serverFd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		throw std::runtime_error("socket failed");
	}
    
	memset(&listenAddr, 0, sizeof(listenAddr));
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_port = htons(listenPort);
	listenAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	memset(&destAddr, 0, sizeof(destAddr));
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(destPort);
	destAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(serverFd, (sockaddr*)&listenAddr, sizeof(listenAddr)) != 0) {
		throw std::runtime_error(std::format("bind failed {}", WSAGetLastError()));
	}
}

size_t OscUdp::send(const void* buffer, size_t size)
{
	size_t ret = sendto(clientFd, (const char*)buffer, (int)size, 0, (sockaddr*)&destAddr, sizeof(destAddr));
	if (ret == SOCKET_ERROR) {
		throw std::runtime_error(std::format("sendto failed {}", WSAGetLastError()));
	}
	return ret;
}

size_t OscUdp::recv(void* buffer, size_t size)
{
	size_t ret = recvfrom(serverFd, (char*)buffer, (int)size, 0, nullptr, nullptr);
	if (ret == SOCKET_ERROR) {
		throw std::runtime_error(std::format("recvfrom failed {}", WSAGetLastError()));
	}
	return ret;
}
