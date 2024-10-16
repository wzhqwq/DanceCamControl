#include "framework.h"
#include "Osc.h"

#include <iostream>

using namespace std;

Osc::Osc(int recv_from_port, int send_to_port, function<void(OSCPP::Server::Message const&)> handler)
	: m_handler(handler)
{
	m_udp = make_unique<OscUdp>(recv_from_port, send_to_port);
}

void Osc::handlePacket(OSCPP::Server::Packet const& packet)
{
    if (packet.isBundle()) {
        // Convert to bundle
        OSCPP::Server::Bundle bundle(packet);

        // Get packet stream
        OSCPP::Server::PacketStream packets(bundle.packets());

        // Iterate over all the packets and call handlePacket recursively.
        // Cuidado: Might lead to stack overflow!
        while (!packets.atEnd()) {
            handlePacket(packets.next());
        }
    }
    else {
        // Convert to message
        OSCPP::Server::Message msg(packet);
		m_handler(msg);
    }
}

void Osc::recvPacket()
{
	size_t size = m_udp->recv(m_recvBuffer.data(), m_recvBuffer.size());
    if (size == 0) return;
    handlePacket(OSCPP::Server::Packet(m_recvBuffer.data(), size));
}

void Osc::sendPacket(OSCPP::Client::Packet const& packet)
{
	m_udp->send(packet.data(), packet.size());
}

void Osc::sendBool(const char* address, bool value)
{
    array<char, kMaxPacketSize> buffer;
    OSCPP::Client::Packet packet(buffer.data(), buffer.size());
    packet
        //.openBundle(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count())
        .openMessage(address, 1)
        .boolean(value)
        .closeMessage();
		//.closeBundle();
	sendPacket(packet);
}
void Osc::sendInt(const char* address, int value)
{
	array<char, kMaxPacketSize> buffer;
	OSCPP::Client::Packet packet(buffer.data(), buffer.size());
    packet
        //.openBundle(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count())
        .openMessage(address, 1)
        .int32(value)
        .closeMessage();
		//.closeBundle();
    sendPacket(packet);
}
void Osc::sendFloat(const char* address, float value)
{
    array<char, kMaxPacketSize> buffer;
    OSCPP::Client::Packet packet(buffer.data(), buffer.size());
    packet
        //.openBundle(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count())
        .openMessage(address, 1)
        .float32(value)
        .closeMessage();
        //.closeBundle();
    sendPacket(packet);
}

template <typename T>
void Osc::SetParameter(const char* parameter, T value)
{
	throw runtime_error("Unsupported type");
}
template <>
void Osc::SetParameter<bool>(const char* parameter, bool value)
{
    sendBool(format("/avatar/parameters/{}", parameter).c_str(), value);
}
template <>
void Osc::SetParameter<int>(const char* parameter, int value)
{
	sendInt(format("/avatar/parameters/{}", parameter).c_str(), value);
}
template <>
void Osc::SetParameter<float>(const char* parameter, float value)
{
	sendFloat(format("/avatar/parameters/{}", parameter).c_str(), value);
}

void Osc::Start()
{
	m_thread = thread(&Osc::ListernThread, this);
}
void Osc::ListernThread()
{
	while (!m_terminated) {
		recvPacket();
	}
}
void Osc::Terminate()
{
	m_terminated = true;
	m_thread.join();
}
