#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <poll.h>
#include <dirent.h>
#include <libevdev/libevdev-uinput.h>


#include "server/device_handling/Inputs.h"
#include "server/device_handling/mappings.h"


/*
    void keyDown(int key)          { emit(EV_KEY, key, 1); syn(); }
    void keyUp(int key)            { emit(EV_KEY, key, 0); syn(); }
    void keyPress(int key)         { keyDown(key); keyUp(key); }
    void mouseMove(int dx, int dy) {
        emit(EV_REL, REL_X, dx);
        emit(EV_REL, REL_Y, dy);
        syn();
    }
    void mouseClick(int btn) {
        emit(EV_KEY, btn, 1); syn();
        emit(EV_KEY, btn, 0); syn();
    }
};
*/

// ---- Example usage ----
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