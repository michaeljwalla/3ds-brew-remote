#pragma once
#include <cstdint>
#include <string>

struct Endpoint {
    static constexpr uint16_t DEFAULT_3DS_PORT = 6000;
    static constexpr uint16_t DEFAULT_PC_PORT  = 6001;

    std::string IP_Address;
    uint16_t Port_3DS = DEFAULT_3DS_PORT;
    uint16_t Port_PC  = DEFAULT_PC_PORT;
};

void run_client(const Endpoint& ep);