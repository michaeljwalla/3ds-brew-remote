#include <cassert>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include "../server/protocol.h"


namespace {
    using HandlerReturnType = int;
    using Handler = HandlerReturnType(*)(int fd, RawInput code);
    //
    using Keymap = std::unordered_map<ButtonMask, Handler>;
    using std::vector;
}
enum InputTypes {
    NONE,
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    // ...
};
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
        const Keymap& getKeymap() const { return keymap; }
        //
        HandlerReturnType handle(int fd, ButtonMask btn, RawInput& code) const {
            auto it = keymap.find(btn);
            if (it == keymap.end()) return -1;
            return it->second(fd, code);
        }
        vector<HandlerReturnType> handleAll(int fd, RawInput& code) const {
            vector<HandlerReturnType> results(NUM_INPUTS);
            for (size_t i = 0; i < NUM_INPUTS; ++i) {
                const ButtonMask btn = static_cast<ButtonMask>(1 << i);
                results.push_back(handle(fd, btn, code));
            }
            return results;
        }
        
};

//internal
namespace {
    static std::unordered_map<InputTypes, InputMap> available = {
        { InputTypes::NONE, {} }
    }; //todo att get() and then revise host_environment to use it
}
inline InputMap& getMapper(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error("no mapping for type " + std::to_string(type));
    return it->second;
}
