#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstdint>

// Helper to emit a single event
static void emit(int fd, int type, int code, int val) {
    struct input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = val;
    write(fd, &ev, sizeof(ev));
}

class UinputDevice {
    int fd = -1;
public:
    UinputDevice() {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) throw std::runtime_error("open /dev/uinput failed (need root or input group)");
    }
    ~UinputDevice() {
        if (fd >= 0) {
            ioctl(fd, UI_DEV_DESTROY);
            close(fd);
        }
    }

    // ---- Setup ----

    void enableEventType(int type)           { ioctl(fd, UI_SET_EVBIT,  type); }
    void enableKey(int key)                  { ioctl(fd, UI_SET_KEYBIT, key);  }
    void enableRelAxis(int axis)             { ioctl(fd, UI_SET_RELBIT, axis); }
    void enableAbsAxis(int axis)             { ioctl(fd, UI_SET_ABSBIT, axis); }

    void create(const std::string& name, uint16_t vendor = 0x1234, uint16_t product = 0x5678) {
        struct uinput_setup usetup{};
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor  = vendor;
        usetup.id.product = product;
        strncpy(usetup.name, name.c_str(), UINPUT_MAX_NAME_SIZE - 1);
        ioctl(fd, UI_DEV_SETUP, &usetup);
        ioctl(fd, UI_DEV_CREATE);
        sleep(1); // let kernel register the device
    }

    // ---- Emit helpers ----

    void syn()                              { emit(fd, EV_SYN, SYN_REPORT, 0); }
    void keyDown(int key)                   { emit(fd, EV_KEY, key, 1); syn(); }
    void keyUp(int key)                     { emit(fd, EV_KEY, key, 0); syn(); }
    void keyPress(int key)                  { keyDown(key); keyUp(key); }
    void mouseMove(int dx, int dy) {
        emit(fd, EV_REL, REL_X, dx);
        emit(fd, EV_REL, REL_Y, dy);
        syn();
    }
    void mouseClick(int btn) {
        emit(fd, EV_KEY, btn, 1); syn();
        emit(fd, EV_KEY, btn, 0); syn();
    }
};

// ---- Example usage ----

int main() {
    // --- Virtual Mouse ---
    UinputDevice mouse;
    mouse.enableEventType(EV_KEY);
    mouse.enableKey(BTN_LEFT);
    mouse.enableKey(BTN_RIGHT);
    mouse.enableEventType(EV_REL);
    mouse.enableRelAxis(REL_X);
    mouse.enableRelAxis(REL_Y);
    mouse.create("Virtual Mouse");

    mouse.mouseMove(100, 50);
    mouse.mouseClick(BTN_LEFT);

    // --- Virtual Keyboard ---
    UinputDevice kbd;
    kbd.enableEventType(EV_KEY);
    kbd.enableKey(KEY_H);
    kbd.enableKey(KEY_I);
    kbd.enableKey(KEY_ENTER);
    kbd.create("Virtual Keyboard");

    kbd.keyPress(KEY_H);
    kbd.keyPress(KEY_I);
    kbd.keyPress(KEY_ENTER);

    return 0;
}