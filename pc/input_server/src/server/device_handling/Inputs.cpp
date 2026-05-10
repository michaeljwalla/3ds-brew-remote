#include "server/device_handling/Inputs.h"
#include <iostream>
#include <stdexcept>


//InputObject
InputObject::InputObject(ObjectID id, std::string_view name, InputTypes type):
    id{id},
    name{name},
    mapping{nullptr},
    os{}
{}

const InputObject::ObjectName& InputObject::getName() const {
    return this->name;
}
InputObject::ObjectID InputObject::getID() const {
    return this->id;
}
bool InputObject::is_initialized() const {
    return this->initialized;
}

std::ostream& operator<<(std::ostream& os, const InputObject& i) {
    os << "InputObject '" << i.name << "' [" << i.id << "]";
    return os;
}
Logger& operator<<(Logger& log, const InputObject& i) {
    log << "InputObject '" << i.name << "' [" << i.id << "]";
    return log;
}

//InputController
InputObject::ObjectID InputController::counter = 0;

bool InputController::emplace( uptr&& i ) {
    return inputs.try_emplace( i->id, std::move(i) ).second;
}

InputController::InputController(): inputs{}, buckets{}, logger{nullptr} {
    for (auto i = 0; i < NUM_INPUTS; i++) {
        buckets.emplace( static_cast<TotalInputMask>(1 << i), Bucket{} );
    }
    return;
}
InputController::InputController(Logger* logger): inputs{}, buckets{}, logger{logger} {
    for (auto i = 0; i < NUM_INPUTS; i++) {
        buckets.emplace( static_cast<TotalInputMask>(1 << i), Bucket{} );
    }
    return;
}

InputController::InputController(InputController&& other) noexcept:
    inputs{ std::move(other.inputs) },
    buckets{ std::move(other.buckets) },
    logger{ std::exchange(other.logger, nullptr) }
{}
InputController& InputController::operator=(InputController&& other) noexcept {
    if (this == &other) return *this;
    this->inputs = std::move(other.inputs);
    this->buckets = std::move(other.buckets);
    this->logger = std::exchange( other.logger, nullptr );
    return *this;
}
void InputController::set_logger(Logger* logger) {
    this->logger = logger;
    return;
}
void InputController::regenerate_buckets() {
    for (auto& [mask, b]: buckets) {
        b.objects.clear();
        b.ids.clear();
        b.results.clear();
    }
    for (auto& [id, ptr]: inputs) {
        add_to_buckets(id);
    }
}
void InputController::remove_from_buckets(ObjectID id) {
    for (auto& [mask, b]: buckets) {
        auto oit = b.objects.begin();
        auto iit = b.ids.begin();
        while (oit != b.objects.end()) {
            if ((*oit)->id == id) {
                oit = b.objects.erase(oit);
                iit = b.ids.erase(iit);
            } else {
                ++oit;
                ++iit;
            }
        }
        b.results.resize(b.objects.size());
    }
}
void InputController::add_to_buckets(ObjectID id) {
    auto it = inputs.find(id);
    if (it == inputs.end()) return;
    auto* obj = it->second.get();
    auto* mapping = obj->mapping;
    if (!mapping) return;
    const auto& km = mapping->getKeymap();
    for (size_t i = 0; i < NUM_INPUTS; ++i) {
        const TotalInputMask btn = static_cast<TotalInputMask>(1 << i);
        auto kit = km.find(btn);
        if (kit == km.end() || kit->second == nullptr) continue;
        auto& b = buckets[btn];
        b.objects.push_back(obj);
        b.ids.push_back(obj->id);
        b.results.resize(b.objects.size());
    }
}

InputController::FireResult InputController::fire(TotalInputMask btn, RawInput& code) const {
    auto it = buckets.find(btn);
    if (it == buckets.end()) return { {}, {} };
    const auto& b = it->second;
    const size_t n = b.objects.size();
    size_t i = 0;
    for (auto* obj: b.objects) {
        // bucket invariant (add_to_buckets): obj->mapping non-null and has a non-null handler for btn
        b.results[i++] = obj->mapping->handle(*obj, btn, code, this->logger);
    }
    return {
        std::span<const ObjectID>(b.ids.data(), n),
        std::span<const HandlerReturnType>(b.results.data(), n)
    };
}

Logger* InputController::get_logger() {
    return this->logger;
}
void InputController::remove( ObjectID id ) {
    remove_from_buckets(id);
    inputs.erase( id );
}
bool InputController::has( ObjectID id ) {
    return inputs.find( id ) != inputs.end();
}
void InputController::initialize( ObjectID id ) {
    auto it = inputs.find(id);
    if (it == inputs.end()) {
        throw std::out_of_range("InputController::initialize: ObjectID " + std::to_string(id) + " not present");
    }
    auto& obj = *it->second;
    if (obj.initialized) return; //ignore reinit
    obj.init();
    obj.initialized = true;
    add_to_buckets(id); //mapping is now set, so re-evaluate bucket membership
}

void InputController::adopt(InputController&& other) {
    if (this == &other) return;
    inputs.merge(other.inputs);
    regenerate_buckets();
    other.inputs.clear();
    return;
}

size_t InputController::size() const {
    return inputs.size();
}

std::vector<InputObject*> InputController::get_objects() const {
    std::vector<InputObject*> out;
    out.reserve( inputs.size() );
    for (auto& [id, ptr]: inputs) {
        out.push_back( ptr.get() );
    }
    return out;
}


std::ostream& operator<<(std::ostream& os, const InputController& i) {
    os << "InputController [" << i.size() << "]";
    return os;
}
Logger& operator<<(Logger& log, const InputController& i) {
    log << "InputController [" << i.size() << "]";
    return log;
}
