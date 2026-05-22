#pragma once
#include <array>
#include <list>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>
#include <string>

#include "logger.h"
#include "InputMap.h"
#include "host/OSRetrieve.h"

#include <concepts>

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
        virtual void init() = 0;
        bool initialized = false; //flipped by InputController; subclass init() does not touch
    protected:
        const ObjectID id;
        const ObjectName name;
        InputMap<InputObject>* mapping;
        struct OSWrapper os;
        InputObject(ObjectID id, std::string_view name, InputTypes type = InputTypes::NONE);


    public:
        // must use input controller
        InputObject() = delete;
        InputObject(const InputObject&) = delete;
        InputObject(InputObject&&) = delete;
        InputObject& operator=(const InputObject&) = delete;
        InputObject& operator=(InputObject&&) = delete;

        virtual ~InputObject() = default;
        const ObjectName& getName() const;
        ObjectID getID() const;
        bool is_initialized() const;

    public:
        friend class InputController;
        friend class InputMap<InputObject>;
        friend std::ostream& operator<<(std::ostream&, const InputObject&);
        friend Logger& operator<<(Logger&, const InputObject&);
};

namespace {
    template <typename T>
    concept isInputObject = std::derived_from<T, InputObject>;
}
//

//not thread safe
class InputController {
    public:
        using ObjectID = InputObject::ObjectID;
        using ObjectName = InputObject::ObjectName;
        using uptr = std::unique_ptr<InputObject>;
        // parallel spans into the bucket's preallocated storage; valid until the
        // next bucket mutation (create/remove/adopt). length = number of objects
        // bound to this btn.
        using FireResult = std::pair<std::span<const ObjectID>, std::span<const HandlerReturnType>>;
    private:
        struct Bucket {
            std::list<InputObject*> objects;
            std::vector<ObjectID> ids;                       // parallel to objects, in iteration order
            mutable std::vector<HandlerReturnType> results;  // pre-sized, written in fire()
        };

        std::unordered_map<ObjectID, uptr> inputs;
        // TotalInputMask is linear [0, NUM_INPUTS); indexed directly.
        std::array<Bucket, NUM_INPUTS> buckets;

        static ObjectID counter;
        Logger* logger; //non owning

        //
        bool emplace( uptr&& i );

        //factory
        template<isInputObject I>
        uptr _create(bool init);
        template<isInputObject I>
        uptr _create(const ObjectName& name, bool init);

        //buckets used so fire() fires only what has handles defined for some TotalInputMask
        //the tradeoff would be faster fire() and the extra change-overhead is fine since changing
        //inputs is not expected to occur during "live use"
        void regenerate_buckets();
        void remove_from_buckets(ObjectID id);
        void add_to_buckets(ObjectID id);
        //
        public:
        InputController();
        InputController(Logger* logger);
        ~InputController() = default;

        //no copy
        InputController(const InputController&) = delete;
        InputController& operator=(const InputController&) = delete;

        //move operations
        InputController(InputController&& other) noexcept;
        InputController& operator=(InputController&& other) noexcept;
        //
        
        Logger* get_logger();
        void set_logger(Logger* logger);
        void remove( ObjectID id );
        bool has( ObjectID id );
        //run init() on a deferred-init object; throws if id absent, no-op if already initialized
        void initialize( ObjectID id );

        template<isInputObject I>
        ObjectID create(bool init);
        template<isInputObject I>
        ObjectID create(const ObjectName& name, bool init);

        void override_mapping(ObjectID id, InputMap<InputObject>& type);

        //hold by ObjectID and use get()...
        //dynamic casted
        template<isInputObject I>
        I* get( ObjectID id );
        template<isInputObject I>
        I* get_static( ObjectID id );
        
        //take others
        void adopt(InputController&& other);

        size_t size() const;

        //iterate through each InputObject with a handler for the TotalInputMask
        FireResult fire(TotalInputMask btn, RawInput code) const;
        //hold by ObjectID and use get()
        std::vector<InputObject*> get_objects() const;

    friend Logger& operator<<(Logger&, InputController&);
    friend std::ostream& operator<<(std::ostream&, InputController&);
};

std::ostream& operator<<(std::ostream& os, const InputObject& i);
Logger& operator<<(Logger& log, const InputObject& i);

std::ostream& operator<<(std::ostream& os, const InputController& i);
Logger& operator<<(Logger& log, const InputController& i);



// templated functions below

//factory
template<isInputObject I>
InputController::uptr InputController::_create(bool init) {
    auto id = counter++;
    auto ptr = uptr(new I(id, std::to_string(id)));
    if (init) { ptr->init(); ptr->initialized = true; }
    return ptr;
}

//factory
template<isInputObject I>
InputController::uptr InputController::_create(const ObjectName& name, bool init) {
    auto id = counter++;
    auto ptr = uptr(new I(id, name));
    if (init) { ptr->init(); ptr->initialized = true; }
    return ptr;
}

template<isInputObject I>
InputObject::ObjectID InputController::create(bool init) {
    uptr i = _create<I>(init);
    auto id = i->id;
    emplace( std::move(i) );
    add_to_buckets(id);
    return id;
} 
template<isInputObject I>
InputObject::ObjectID InputController::create(const ObjectName& name, bool init)
{
    uptr i = _create<I>(name, init);
    auto id = i->id;
    emplace( std::move(i) );
    add_to_buckets(id);
    return id;
}

template<isInputObject I>
I* InputController::get( ObjectID id ) {
    auto it = inputs.find( id );
    if (it == inputs.end()) return nullptr;
    return dynamic_cast<I*>(it->second.get());
}

//faster than get() but dont lose the type!
template<isInputObject I>
I* InputController::get_static( ObjectID id ) {
    auto it = inputs.find( id );
    if (it == inputs.end()) return nullptr;
    return static_cast<I*>(it->second.get());
}