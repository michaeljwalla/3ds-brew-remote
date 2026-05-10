//for statics
#include "mappings.h"
#include "static_mappings.h"
#include "server/protocol.h"
#include "static_inputs.h"
#include "logger.h"
#include <utility>

using namespace mappingtypes;
namespace {

    using IO = InputObject;
    using Datagram = const RawInput&;
    using pair = std::pair<InputTypes, InputMap<IO>>;
}


std::unordered_map<InputTypes, InputMap<InputObject>> available {
    pair(InputTypes::NONE, {}),
    // pair(InputTypes::MOUSE, {}),
    pair(InputTypes::MOUSE, { "LOGGING", {
        {TotalInputMask::A, [](IO* in, Datagram code, Logger* log) {
            *log << "A pressed!";
            return 0;
        }}
    }}),
};

InputMap<InputObject>& get_mapping(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error(std::string("no mapping for type ") + getInputTypeName(type));
    return it->second;
}