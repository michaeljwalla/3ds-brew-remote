#include "server/device_handling/Inputs.h"
#include <iostream>


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

InputController::InputController(): inputs{}, logger{nullptr} {}
InputController::InputController(Logger* logger): inputs{}, logger{logger} {}

InputController::InputController(InputController&& other) noexcept:
    inputs{ std::move(other.inputs) },
    logger{ std::exchange(other.logger, nullptr) }
{}
InputController& InputController::operator=(InputController&& other) noexcept {
    if (this == &other) return *this;
    this->inputs = std::move(other.inputs);
    this->logger = std::exchange( other.logger, nullptr );
    return *this;
}

void InputController::remove( ObjectID id ) {
    inputs.erase( id );
}
bool InputController::has( ObjectID id ) {
    return inputs.find( id ) != inputs.end();
}

void InputController::adopt(InputController&& other) {
    if (this == &other) return;
    inputs.merge(other.inputs);
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

//InputMouse
InputMouse::InputMouse(ObjectID id, std::string_view name):
    InputObject(id, name, InputTypes::MOUSE),
    pos()
{}

void InputMouse::init() {
    os.spawn();
    os.enableKey(BTN_LEFT);
    os.enableKey(BTN_RIGHT);
    os.enableRelAxis(REL_X);
    os.enableRelAxis(REL_Y);
    os.create(name);
    return;
}
void InputMouse::button_click(int btn) {
    os.emit(EV_KEY, btn, 1); os.sync();
    os.emit(EV_KEY, btn, 0); os.sync();
}
void InputMouse::button_down(int btn) {
    os.emit(EV_KEY, btn, 1); os.sync();
}
void InputMouse::button_up(int btn) {
    os.emit(EV_KEY, btn, 0); os.sync();
}
void InputMouse::move(InputMouse::coordinate delta) {
    pos += delta;
    os.emit(EV_REL, REL_X, delta.x);
    os.emit(EV_REL, REL_Y, delta.y);
    os.sync();
}
void InputMouse::set_pos(InputMouse::coordinate newPos) {
    move(newPos - pos);
}

InputMouse::coordinate InputMouse::get_pos() const{
    return pos;
}