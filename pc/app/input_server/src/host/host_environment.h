#pragma once
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string>
#include "../server/logger.h"

class InputObject{
    public:
        using ObjectID = uint32_t; //if you have over 2 bil input devices wyd
        using ObjectName = std::string;
    private:
        const ObjectID id;
        const ObjectName name;
        //non-const unknown object*(?) = nullptr (?);
        InputObject(ObjectID id, std::string_view name): id{id}, name{name} {}

        //migrate (explicit to reconstruct with InputObject& to steal)
    public:
        // must use input controller
        InputObject() = delete;
        InputObject(const InputObject&) = delete;
        InputObject(InputObject&&) = delete;
        InputObject& operator=(const InputObject&) = delete;
        InputObject& operator=(InputObject&&) = delete;

        const ObjectName& getName() const {
            return this->name;
        }
        ObjectID getID() const {
            return this->id;
        }
    
    friend class InputController;
    friend std::ostream& operator<<(std::ostream&, const InputObject&);
    friend Logger& operator<<(Logger&, const InputObject&);
};

inline std::ostream& operator<<(std::ostream& os, const InputObject& i) {
    os << "InputObject " << i.name;
    return os;
}
inline Logger& operator<<(Logger& log, const InputObject& i) {
    log << "InputObject " << i.name;
    return log;
};
//

class InputController {
    public:
        using ObjectID = InputObject::ObjectID;
        using ObjectName = InputObject::ObjectName;
        using uptr = std::unique_ptr<InputObject>;
    private:
        std::unordered_map<ObjectID, uptr> inputs;
        ObjectID counter;
        Logger* logger; //non owning

        //factory
        uptr create(ObjectID id, std::string_view name) {
            return uptr(new InputObject(id, name));
        }
    public:
        InputController(): inputs{}, counter{0}, logger{nullptr} {}
        InputController(Logger* logger): inputs{}, counter{0}, logger{logger} {}
        ~InputController() = default;

        //no copy
        InputController(const InputController&) = delete;
        InputController& operator=(const InputController&) = delete;

        //move operations
        InputController(InputController&& other) noexcept:
            inputs{ std::move(other.inputs) },
            counter{ std::exchange(other.counter, 0) },
            logger{ std::exchange(other.logger, nullptr) }
            {}
        InputController& operator=(InputController&& other) noexcept {
            if (this == &other) return *this;
            this->inputs = std::move(other.inputs);
            this->counter =  std::exchange(other.counter, 0);
            this->logger = std::exchange( other.logger, nullptr );
            return *this;
        }
        //

        ObjectID spawn() {
            auto id = counter++;
            inputs.emplace(id, create(id, std::to_string(id)));
            return id;
        }
        ObjectID spawn(std::string_view name) {
            auto id = counter++;
            inputs.emplace(id, create(id, name));
            return id;
        }
        
        //
        bool include(uptr&& in) {
            //migrate() here
            return false;
        }

        size_t size() const {
            return inputs.size();
        }

        std::vector<InputObject*> get_objects() const {
            std::vector<InputObject*> out;
            out.reserve( inputs.size() );
            for (auto& [id, ptr]: inputs) {
                out.push_back( ptr.get() );
            }
            return out;
        }
        
};