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
    CEMU_GAMEPAD,
    CEMU_RELAY_3DS
    // ...
};
inline const char* getInputTypeName(InputTypes t) {
    switch (t) {
        case NONE: return "NONE";
        case LOGGING: return "LOGGING";
        case KEYBOARD: return "KEYBOARD";
        case MOUSE: return "MOUSE";
        case GAMEPAD: return "GAMEPAD";
        case CEMU_GAMEPAD: return "CEMU_GAMEPAD";
        case CEMU_RELAY_3DS: return "CEMU_RELAY_3DS";

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

        // Map1 | Map2 w/ map2 priority on dupes
        InputMap extend(const std::string& new_name, Keymap overrides) const {
            Keymap merged = this->keymap;   // copy (member is const Keymap)
            for (auto& [k, v] : overrides) merged[k] = v;   // right wins
            return InputMap(new_name, std::move(merged));
        }

};