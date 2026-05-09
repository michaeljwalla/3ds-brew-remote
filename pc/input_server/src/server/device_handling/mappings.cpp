//for statics
#include "mappings.h"
#include "Inputs.h"

//internal
namespace {
    static std::unordered_map<InputTypes, InputMap<InputObject>> available = {
        { InputTypes::NONE, {} }
    }; //todo att get() and then revise host_environment to use it
}

template<typename T>
inline InputMap<T>& getMapper(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error("no mapping for type " + std::to_string(type));
    return it->second;
}
