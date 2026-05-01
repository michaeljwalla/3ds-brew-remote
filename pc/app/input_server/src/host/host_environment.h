#pragma once
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string>
#include "../server/logger.h"

/*
    Use InputController to spawn/fetch InputObjects
    If 'tracking' an object externally, use the ObjectID and InputController.get()
    to fetch (in case it deletes).
*/


class InputObject{
    public:
        using ObjectID = uint32_t; //if you have over 2 bil input devices wyd
        using ObjectName = std::string;
    private:
        const ObjectID id;
        const ObjectName name;
        //non-const unknown object*(?) = nullptr (?);
        InputObject(ObjectID id, std::string_view name): id{id}, name{name} {}

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
    os << "InputObject '" << i.name << "' [" << i.id << "]";
    return os;
}
inline Logger& operator<<(Logger& log, const InputObject& i) {
    log << "InputObject '" << i.name << "' [" << i.id << "]";
    return log;
};
//

//not thread safe
class InputController {
    public:
        using ObjectID = InputObject::ObjectID;
        using ObjectName = InputObject::ObjectName;
        using uptr = std::unique_ptr<InputObject>;
    private:
        std::unordered_map<ObjectID, uptr> inputs;
        static ObjectID counter;
        Logger* logger; //non owning

        //factory
        uptr create() {
            auto id = counter++;
            return uptr(new InputObject(id, std::to_string(id)));
        }
        uptr create(const ObjectName& name) {
            auto id = counter++;
            return uptr(new InputObject(id, name));
        }
        bool emplace( uptr&& i ) {
            return inputs.try_emplace( i->id, std::move(i) ).second;
        }
        public:
        InputController(): inputs{},  logger{nullptr} {}
        InputController(Logger* logger): inputs{}, logger{logger} {}
        ~InputController() = default;

        //no copy
        InputController(const InputController&) = delete;
        InputController& operator=(const InputController&) = delete;

        //move operations
        InputController(InputController&& other) noexcept:
            inputs{ std::move(other.inputs) },
            logger{ std::exchange(other.logger, nullptr) }
        {}
        InputController& operator=(InputController&& other) noexcept {
            if (this == &other) return *this;
            this->inputs = std::move(other.inputs);
            this->logger = std::exchange( other.logger, nullptr );
            return *this;
        }
        //

        ObjectID spawn() {
            uptr i = create();
            auto id = i->id;
            emplace( std::move(i) );
            return id;
        }
        ObjectID spawn(const ObjectName& name) {
            uptr i = create(name);
            auto id = i->id;
            emplace( std::move(i) );
            return id;
        }

        void remove( ObjectID id ) {
            inputs.erase( id );
        }
        bool has( ObjectID id ) {
            return inputs.find( id ) != inputs.end(); 
        }

        //hold by ObjectID and use get()
        InputObject* get( ObjectID id ) {
            auto it = inputs.find( id );
            if (it == inputs.end()) return nullptr;
            return it->second.get();
        }
        //take others
        void adopt(InputController&& other) {
            if (this == &other) return;
            inputs.merge(other.inputs);
            other.inputs.clear();
            return;
        }

        size_t size() const {
            return inputs.size();
        }

        //hold by ObjectID and use get()
        std::vector<InputObject*> get_objects() const {
            std::vector<InputObject*> out;
            out.reserve( inputs.size() );
            for (auto& [id, ptr]: inputs) {
                out.push_back( ptr.get() );
            }
            return out;
        }
    friend Logger& operator<<(Logger&, InputController&);
    friend std::ostream& operator<<(std::ostream&, InputController&);
};
inline InputObject::ObjectID InputController::counter = 0;
inline std::ostream& operator<<(std::ostream& os, const InputController& i) {
    os << "InputController [" << i.size() << "]";
    return os;
}
inline Logger& operator<<(Logger& log, const InputController& i) {
    log << "InputController [" << i.size() << "]";
    return log;
};