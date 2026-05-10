//for statics
#include "mappings.h"
#include "static_mappings.h"

//internal
namespace {
    static std::unordered_map<InputTypes, InputMap<InputObject>> available = {
        { InputTypes::NONE, {} }
    }; //todo att get() and then revise host_environment to use it
}

InputMap<InputObject>& get_mapping(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error(std::string("no mapping for type ") + getInputTypeName(type));
    return it->second;
}
