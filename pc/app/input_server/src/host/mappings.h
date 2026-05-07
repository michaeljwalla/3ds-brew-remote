#include <functional>
#include <string>
#include <unordered_map>
#include <utility>


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