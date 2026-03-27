#include <functional>
#include <string_view>
#include <variant>
#include <string>
#include <iostream>

class LoggerProxy {
public:
    using LogArg = std::variant<int, float, bool, std::string, std::string_view, const char*, 
                               std::ostream& (*)(std::ostream&)>;
    using LogFunc = std::function<void(const LogArg&)>;
private:
    LogFunc _callback;
    
    static void default_log(const LogArg& arg) { //defaults to cout << ... functionality
        std::visit(
            [](const auto& v) { std::cout << v; },
            arg
        );
        return;
    }
public:
    static LoggerProxy& singleton() {
        static LoggerProxy inst;
        return inst;
    }
    LoggerProxy() : _callback(default_log) {}
    explicit LoggerProxy(const LogFunc callback): _callback(callback) {}

    LoggerProxy& operator<<(const LogArg& arg) {
        _callback(arg);
        return *this;
    }
    void set_logger(const LogFunc callback) {
        this->_callback = callback;
    }
};
