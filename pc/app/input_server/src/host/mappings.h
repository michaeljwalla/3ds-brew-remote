#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

enum InputTypes {
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    // ...
};
class InputMap;
class InputMap {
    using HandlerReturnType = int;
    using Handler = HandlerReturnType(*)(int fd, int code);
    //
    const std::string name;
    const std::unordered_map<int, Handler> keymap;


    public:
        InputMap(const std::string name, std::unordered_map<int, Handler>&& keymap):
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

static std::unordered_map<InputTypes, const InputMap> available = {

}; //todo att get() and then revise host_environment to use it

inline const InputMap& getMapping(InputTypes type) {
    auto it = available.find(type);
    if (it == available.end()) throw std::runtime_error("no mapping for type " + std::to_string(type));
    return it->second;
}
