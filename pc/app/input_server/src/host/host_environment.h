#pragma once
#include <memory>
#include <vector>
#include <string>
#include "../server/logger.h"

class InputObject{
    private:
        static size_t object_counter;
        const size_t id;
        const std::string name;
    public:
    InputObject(): id(object_counter++), name{static_cast<char>(id)} {
        return;
    }
    InputObject(std::string_view name): id(object_counter++), name{name} {

    }
    const std::string& getName() const {
        return this->name;
    }
    friend std::ostream& operator<<(std::ostream&, const InputObject&);
    friend Logger& operator<<(Logger&, const InputObject&);
};
inline std::ostream& operator<<(std::ostream& os, const InputObject& i) {
    os << "InputObject " << i.name;
    return os;
}
inline Logger& operator<<(Logger& log, const InputObject& i) {
    log << "InputObject " << i.name;
    return log;
}

InputObject* spawn_device();
bool register_device( std::unique_ptr<InputObject> );
std::vector<InputObject*> get_devices();

