#include <iostream>
#include "server.h"

#include <libevdev/libevdev-uinput.h>


#include "server/device_handling/Inputs.h"
#include "server/device_handling/mappings.h"

using std::cout, std::endl, std::cin, std::string;
namespace {
    using ObjectID = InputObject::ObjectID;
}
int main() {
    InputController controller;
    ObjectID mId = controller.spawn<InputMouse>("My Virtual Mouse");
    InputMouse* mouse = controller.get_static<InputMouse>(mId);
    
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
//     cout << "Input IP [127.0.0.1]: ";
//     std::string in;
//     std::getline(cin, in);

//     if (in.empty()) in = "127.0.0.1";
    
//     run_client( { in } );
// }