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

namespace {
    using HandlerReturnType = int;
    template<typename T>
    using Handler = HandlerReturnType(*)(T& in, RawInput code, Logger* logger);
    //
    template<typename T>
    using UntypedKeymap = std::unordered_map<TotalInputMask, Handler<T>>;
    using std::vector;
}
enum InputTypes {
    NONE,
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    // ...
};
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
        HandlerReturnType handle(T& input, TotalInputMask btn, RawInput& code, Logger* logger=nullptr) const {
            auto it = keymap.find(btn);
            if (it == keymap.end()) return -1;
            return it->second(input, code, logger);
        }
        vector<HandlerReturnType> handleAll(T& input, RawInput& code, Logger* logger=nullptr) const {
            vector<HandlerReturnType> results(NUM_INPUTS);
            for (size_t i = 0; i < NUM_INPUTS; ++i) {
                const TotalInputMask btn = static_cast<TotalInputMask>(1 << i);
                results.push_back( handle(input, btn, code, logger) );
            }
            return results;
        }
        
};