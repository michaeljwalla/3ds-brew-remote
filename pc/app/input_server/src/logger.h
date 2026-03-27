#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <variant>
#include <string>
#include <iostream>

//this unifies how you can log stuff. passes info through << and thats about it for now.
//set any callback but you may have to redefine operator<< for specific types for complex tasks

class Logger {
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
static Logger singleton()  {
    static ThreadSafeLogger inst;
    return inst;
}