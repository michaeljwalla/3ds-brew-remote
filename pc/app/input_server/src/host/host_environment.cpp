#include "host_environment.h"
#include "../server/logger.h"
#include <memory>
#include <unordered_map>

namespace {
    static Logger& logger = Logger::singleton();
    static std::unordered_map<std::string, std::unique_ptr<InputObject>> inputs;

}

// caller never owns object. use remove_device() to delete.
InputObject* spawn_device() {
    auto device = std::make_unique<InputObject>();
    logger << "Spawned new device '" << *device << "'";

    InputObject* raw = device.get();
    register_device( std::move(device) );
    return raw;
}

bool register_device(std::unique_ptr<InputObject> i) {
    InputObject& raw = *i;
    auto name = raw.getName();
    auto [_, success] = inputs.try_emplace(name, std::move(i));

    if (!success)
        logger << LoggerState::WARN << raw << " already exists.";
    else
        logger << LoggerState::GOOD << raw << " registered.";

    return success;
}

// non owning pointers to all tracked devices.
std::vector<InputObject*> get_devices() {
    std::vector<InputObject*> out;
    out.reserve( inputs.size() );
    for (auto& [name, ptr]: inputs) {
        out.push_back( ptr.get() );
    }
    return out;
}