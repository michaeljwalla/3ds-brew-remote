#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <variant>
#include <string>
#include <iostream>

//this unifies how you can log stuff. passes info through << and thats about it for now.
//set any callback but you may have to redefine operator<< for specific types for complex tasks

class LoggerProxy {
public:
    using LogArg = std::variant<int, float, bool, std::string, std::string_view, const char*, 
                               std::ostream& (*)(std::ostream&)>;
    using LogFunc = std::function<void(const LogArg&)>;

protected:
    LogFunc _callback;

private:
    
    static void default_log(const LogArg& arg) { //defaults to cout << ... functionality
        std::visit(
            [](const auto& v) { std::cout << v; },
            arg
        );
        return;
    }
public:
    static LoggerProxy& singleton();
    LoggerProxy() : _callback(default_log) {}
    explicit LoggerProxy(const LogFunc callback): _callback(callback) {}

    virtual LoggerProxy& operator<<(const LogArg& arg) {
        _callback(arg);
        return *this;
    }
    virtual void set_logger(const LogFunc callback) {
        this->_callback = callback;
    }
    virtual ~LoggerProxy() = default;
};

class ThreadSafeLoggerProxy : public LoggerProxy {
    mutable std::shared_mutex _mutex; //default constructed
public:
    ThreadSafeLoggerProxy& operator<<(const LogArg& arg) override {
        std::shared_lock lock(_mutex);
        LoggerProxy::operator<<(arg);
        return *this;
    }
    void set_logger(const LogFunc callback) override {
        std::unique_lock lock(_mutex);
        LoggerProxy::set_logger(callback);
    }
};

//define singleton
static LoggerProxy singleton()  {
    static ThreadSafeLoggerProxy inst;
    return inst;
}