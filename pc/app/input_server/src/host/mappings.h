#include <cassert>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>



namespace {
    using HandlerReturnType = int;
    using Handler = HandlerReturnType(*)(int fd, int code);
    //
    using Keymap = std::unordered_map<int, Handler>;
}
enum InputTypes {
    NONE,
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    // ...
};
class InputMap;
class InputMap { //immutable
    //
    const std::string name;
    const Keymap keymap;

    public:
        InputMap(): name{"NONE"}, keymap{} {}
        InputMap(const std::string name, Keymap&& keymap):
            name{name},
            keymap{std::move(keymap)} {}
        const std::string& getName() const { return name; }
        const std::unordered_map<int, Handler>& getKeymap() const { return keymap; }
        //
        HandlerReturnType handle(int fd, int code) const {
            auto it = keymap.find(code);
            if (it == keymap.end()) return -1;
            return it->second(fd, code);
        }
        
};


//internal
static std::unordered_map<InputTypes, InputMap> available = {
    { InputTypes::NONE, {} }
}; //todo att get() and then revise host_environment to use it

inline InputMap& getMapper(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error("no mapping for type " + std::to_string(type));
    return it->second;
}
