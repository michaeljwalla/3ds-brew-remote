#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <mutex>
#include <ostream>
#include <shared_mutex>
#include <string_view>
#include <variant>
#include <string>
#include <iostream>

namespace {
    #define is_type(v, T) (std::is_same_v<std::decay_t<decltype(v)>, T>) //bool

    template<typename... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };

    // deduction guide (not needed in ver >= 20)
    template<typename... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;
}


//this unifies how you can log stuff. passes info through << and thats about it for now.
//set any callback but you may have to redefine operator<< for specific types for complex tasks

//optional state param
struct LoggerState {
    size_t value;
    explicit LoggerState(size_t value): value(value) {}

    static const LoggerState END;
    friend const std::ostream& operator<<(std::ostream& out, LoggerState& obj); //technically unncecessary friend
};
inline const LoggerState LoggerState::END {std::numeric_limits<size_t>::max()};
inline std::ostream& operator<<(std::ostream& out, const LoggerState& obj) {
    out << "[[ LoggerState: " << obj.value << " ]]";
    return out;
}


//some overload help
class Logger {
public:
    using OstreamManip = std::ostream& (*)(std::ostream&);
    using IosBaseManip = std::ios_base& (*)(std::ios_base&);
    using LogArg = std::variant <
        int, long, float, bool,
        unsigned int, unsigned long,
        std::string, std::string_view, const char*, 
        OstreamManip, IosBaseManip, LoggerState // in case user doesnt predefine an overload
    >;
    using LogFunc = std::function<void(const LogArg&)>;

    //to convert 
    static OstreamManip manip(OstreamManip f) { return f; }
    static IosBaseManip manip(IosBaseManip f) { return f; }
protected:
    LogFunc _callback;

private:
    //just to track what the current output is
    static std::pair<std::array<std::ostream*,2>, size_t> default_log_state_redirects;
    // close me
    static void default_log(const LogArg& arg) { //defaults to cout << ... functionality
        std::visit(
            overloaded{ //state 0 = cout, 1 = cerr
                [](const LoggerState& v) {
                    size_t idx = v.value >= default_log_state_redirects.first.size() ? 0 : v.value;
                    default_log_state_redirects.second = idx;
                },
                [](const auto& v) {
                    //get the stream to output to
                    std::ostream* const out = default_log_state_redirects.first[ default_log_state_redirects.second ];
                    //
                    *out << v;
                    return;
                },
            },
            arg
        );
        return;
    }
public:
    static Logger& singleton();
    Logger() : _callback(default_log) {}
    explicit Logger(const LogFunc callback): _callback(callback) {}

    virtual Logger& operator<<(const LogArg& arg) {
        _callback(arg);
        return *this;
    }
    virtual void set_logger(const LogFunc callback) {
        this->_callback = callback;
    }
    virtual ~Logger() = default;
};
inline std::pair<std::array<std::ostream*,2>, size_t> Logger::default_log_state_redirects = { { &std::cout, &std::cerr }, 0};

class ThreadSafeLogger : public Logger {
    mutable std::shared_mutex _mutex; //default constructed
public:
    ThreadSafeLogger& operator<<(const LogArg& arg) override {
        std::shared_lock lock(_mutex);
        Logger::operator<<(arg);
        return *this;
    }
    void set_logger(const LogFunc callback) override {
        std::unique_lock lock(_mutex);
        Logger::set_logger(callback);
    }
};

//define singleton
inline Logger& Logger::singleton()  {
    static ThreadSafeLogger inst;
    return inst;
}