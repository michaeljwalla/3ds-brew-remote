// assumption is that Handler(...) on some fd already has the proper
// uinput flags set up to work. to throw / ignore is TBD
#pragma once

#include <cassert>
#include <string>
#include <unordered_map>
#include <utility>
#include "server/protocol.h"
#include <vector>
#include "logger.h"

namespace mappingtypes {
    using HandlerReturnType = int;
    using Datagram = const RawInput&;
    template<typename T>
    struct HandlerParams {
        T* parent;
        Datagram code;
        Logger* log = nullptr;
    };
    template<typename T>
    using Handler = HandlerReturnType(*)(HandlerParams<T> info);
    template<typename T>
    using UntypedKeymap = std::unordered_map<TotalInputMask, Handler<T>>;
}
using namespace mappingtypes;
enum InputTypes {
    NONE,
    LOGGING,
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    // ...
};
inline const char* getInputTypeName(InputTypes t) {
    switch (t) {
        case NONE: return "NONE";
        case LOGGING: return "LOGGING";
        case KEYBOARD: return "KEYBOARD";
        case MOUSE: return "MOUSE";
        case GAMEPAD: return "GAMEPAD";
        
        default:    return "Unknown";
    }
}
template<typename T>
class InputMap { //immutable
    //
    public:
        using Keymap = UntypedKeymap<T>;
        
    private:
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
        HandlerReturnType handle(TotalInputMask btn, HandlerParams<T> data) const {
            auto it = keymap.find(btn);
            if (it == keymap.end() || it->second == nullptr) return -1;
            return it->second(data);
        }
        std::vector<HandlerReturnType> handleAll(HandlerParams<T> data) const {
            std::vector<HandlerReturnType> results;
            results.reserve(NUM_INPUTS);
            for (size_t i = 0; i < NUM_INPUTS; ++i) {
                results.push_back( handle(static_cast<TotalInputMask>(i), data) );
            }
            return results;
        }
        
};