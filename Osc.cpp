#include "framework.h"
#include "Osc.h"

#include <oscpp/server.hpp>
#include <oscpp/print.hpp>
#include <iostream>

void handlePacket(const OSCPP::Server::Packet& packet)
{
    if (packet.isBundle()) {
        // Convert to bundle
        OSCPP::Server::Bundle bundle(packet);

        // Print the time
        std::cout << "#bundle " << bundle.time() << std::endl;

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

        // Get argument stream
        OSCPP::Server::ArgStream args(msg.args());

        // Directly compare message address to string with operator==.
        // For handling larger address spaces you could use e.g. a
        // dispatch table based on std::unordered_map.
        if (msg == "/s_new") {
            const char* name = args.string();
            const int32_t id = args.int32();
            std::cout << "/s_new" << " "
                << name << " "
                << id << " ";
            // Get the params array as an ArgStream
            OSCPP::Server::ArgStream params(args.array());
            while (!params.atEnd()) {
                const char* param = params.string();
                const float value = params.float32();
                std::cout << param << ":" << value << " ";
            }
            std::cout << std::endl;
        }
        else if (msg == "/n_set") {
            const int32_t id = args.int32();
            const char* key = args.string();
            // Numeric arguments are converted automatically
            // to float32 (e.g. from int32).
            const float value = args.float32();
            std::cout << "/n_set" << " "
                << id << " "
                << key << " "
                << value << std::endl;
        }
        else {
            // Simply print unknown messages
            std::cout << "Unknown message: " << msg << std::endl;
        }
    }
}