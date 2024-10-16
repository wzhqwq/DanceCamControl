#pragma once
#include <oscpp/server.hpp>
#include <oscpp/print.hpp>
#include "OscUdp.h"
#include <thread>
#include <functional>

const size_t kMaxPacketSize = 8192;

class Osc
{
public:
	Osc(int recv_from_port, int send_to_port, std::function<void(OSCPP::Server::Message const&)> handler);
	~Osc() {
		Terminate();
	}
	void Start();
	void Terminate();
	void ListernThread();

	template <typename T>
	void SetParameter(const char* parameter, T value);

private:
	void handlePacket(OSCPP::Server::Packet const& packet);
	void recvPacket();
	void sendPacket(OSCPP::Client::Packet const& packet);
	void sendBool(const char* address, bool value);
	void sendInt(const char* address, int value);
	void sendFloat(const char* address, float value);

private:
	std::thread m_thread;
	std::unique_ptr<OscUdp> m_udp;
	std::function<void(OSCPP::Server::Message const&)> m_handler;
	std::atomic<bool> m_terminated = false;

	std::array<char, kMaxPacketSize> m_recvBuffer;
};
