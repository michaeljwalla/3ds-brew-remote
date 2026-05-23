#include <iostream>
#include "logger.h"
#include "server.h"

#include <libevdev/libevdev-uinput.h>
#include <unistd.h>


// #include "server/device_handling/Inputs.h"
#include "server/device_handling/statics/Mappings.h"
#include "server/device_handling/statics/Objects.h"

namespace {
    using namespace LoggerCommons;
    using LS = LoggerState;

    using std::cin;
} 
namespace {
    using ObjectID = InputObject::ObjectID;
}

InputController mouse(Logger& log) {
    InputController controller(&log);
    ObjectID mId = controller.create<InputMouse>("My Virtual Mouse", true);
    InputMouse* mouse = controller.get_static<InputMouse>(mId);
    //controller.override_mapping(mId, get_mapping(InputTypes::LOGGING));
    
    log << LS::GOOD << *mouse << endl;
    return std::move(controller);
}
InputController gamepad(Logger& log) {
    InputController controller(&log);
    controller.create<InputGamepad>("My Virtual Gamepad", true);

    return controller;
}
int main() {
    using LogArg = Logger::LogArg;
    using LogFunc = Logger::LogFunc;

    //dropall
    Logger log{[](const LogArg& arg) { //defaults to cout << ... functionality
        std::visit( overloaded { //state 1 = cerr, others [0, 1) U (1, inf] are cout
            [](const LoggerState& v) {
                return;
            },
            [](const auto& v) {
                return;
            },
        },
        arg);
        return;
    }};
    // Logger& log = Logger::singleton();
    
    auto controller {
        gamepad(log)
    };
    //

    log << "Input IP [127.0.0.1]: ";
    std::string in;
    std::getline(cin, in);

    if (in.empty()) in = "127.0.0.1";
    
    run_client( { in }, controller );
    // mouse.enableKey(BTN_LEFT);
    // mouse.enableKey(BTN_RIGHT);
    // mouse.enableRelAxis(REL_X);
    // mouse.enableRelAxis(REL_Y);
    // mouse.create("Virtual Mouse");

    // mouse.mouseMove(100, 50);
    // mouse.mouseClick(BTN_LEFT);

    // UinputDevice kbd;
    // kbd.enableKey(KEY_H);
    // kbd.enableKey(KEY_I);
    // kbd.enableKey(KEY_ENTER);
    // kbd.create("Virtual Keyboard");

    // kbd.keyPress(KEY_H);
    // kbd.keyPress(KEY_I);
    // kbd.keyPress(KEY_ENTER);

    return 0;
}

// int main() {
//     
// }