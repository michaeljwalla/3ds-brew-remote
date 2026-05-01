#include "host_environment.h"
#include <string_view>
#include <unordered_map>
#include "../server/logger.h"
#include "linux/uinput.h"
#include <string>

namespace {
    using entry_input = std::pair<std::string_view, InputObject&>;
    //
    static Logger& logger = Logger::singleton();
    static std::unordered_map<std::string_view, InputObject> inputs;
    static size_t object_counter;

}

class InputObject{
    private:
        std::string name;

    public:
    InputObject(): name{ static_cast<char>(object_counter++) } {
        return;
    }
    std::string_view getName() const {
        return this->name;
    }
};

InputObject spawn_device() {
    InputObject x;

    

    return x;
}

bool register_device(const InputObject &i) {
    std::string_view name = i.getName();
    if (inputs.find(name) != inputs.end()) {
        logger << LoggerState::WARN << name << "registered but already exists.";
        return false;
    }
    inputs[i.getName()] = i;

    logger << LoggerState::GOOD << name << " registered.";

    return true;
}