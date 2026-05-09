#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <string>
#include <stdexcept>
#include <system_error>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <poll.h>
#include <dirent.h>
#include <libevdev/libevdev-uinput.h>
static std::string findEventName(const char* sysname) {
    std::string dir = std::string("/sys/class/input/") + sysname;
    DIR* d = opendir(dir.c_str());
    if (!d) return {};
    std::string found;
    for (struct dirent* e; (e = readdir(d));) {
        if (strncmp(e->d_name, "event", 5) == 0) {
            found = e->d_name;
            break;
        }
    }
    closedir(d);
    return found;
}

static void waitForDevNode(const std::string& node, int timeout_ms = 3000) {
    int ifd = inotify_init1(IN_CLOEXEC);
    if (ifd < 0)
        throw std::system_error(errno, std::generic_category(), "inotify_init1");

    inotify_add_watch(ifd, "/dev/input", IN_CREATE);

    std::string path = "/dev/input/" + node;
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) { close(ifd); return; }

    struct pollfd pfd{ ifd, POLLIN, 0 };
    char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
    while (poll(&pfd, 1, timeout_ms) > 0) {
        ssize_t n = read(ifd, buf, sizeof(buf));
        for (ssize_t i = 0; i < n;) {
            auto* ev = reinterpret_cast<struct inotify_event*>(buf + i);
            if (ev->len && ev->name == node) { close(ifd); return; }
            i += sizeof(struct inotify_event) + ev->len;
        }
    }
    close(ifd);
    throw std::runtime_error("timed out waiting for " + path);
}

// ---- UinputDevice ----

class UinputDevice {
    libevdev*        dev  = nullptr;
    libevdev_uinput* udev = nullptr;

    void waitReady() {
        const char* sysname = libevdev_uinput_get_syspath(udev);
        if (!sysname)
            throw std::runtime_error("libevdev_uinput_get_syspath returned null");

        // syspath is e.g. /sys/devices/virtual/input/input15
        // we need just the last component: "input15"
        const char* base = strrchr(sysname, '/');
        base = base ? base + 1 : sysname;

        std::string eventname = findEventName(base);
        if (eventname.empty())
            throw std::runtime_error(std::string("no event node under /sys/class/input/") + base);

        waitForDevNode(eventname);
        sleep(1);
    }

public:
    UinputDevice() {
        dev = libevdev_new();
        if (!dev)
            throw std::runtime_error("libevdev_new failed");
    }

    ~UinputDevice() {
        if (udev) libevdev_uinput_destroy(udev);
        if (dev)  libevdev_free(dev);
    }

    UinputDevice(const UinputDevice&)            = delete;
    UinputDevice& operator=(const UinputDevice&) = delete;

    // ---- Setup ----

    void setName(const std::string& name) { libevdev_set_name(dev, name.c_str()); }
    void setIds(uint16_t vendor, uint16_t product) {
        libevdev_set_id_bustype(dev, BUS_USB);
        libevdev_set_id_vendor(dev, vendor);
        libevdev_set_id_product(dev, product);
    }

    void enableKey(int key) {
        libevdev_enable_event_type(dev, EV_KEY);
        libevdev_enable_event_code(dev, EV_KEY, key, nullptr);
    }
    void enableRelAxis(int axis) {
        libevdev_enable_event_type(dev, EV_REL);
        libevdev_enable_event_code(dev, EV_REL, axis, nullptr);
    }
    void enableAbsAxis(int axis, const input_absinfo& info) {
        libevdev_enable_event_type(dev, EV_ABS);
        libevdev_enable_event_code(dev, EV_ABS, axis, &info);
    }

    // Call after all enable*() calls
    void create(const std::string& name, uint16_t vendor = 0x1234, uint16_t product = 0x5678) {
        setName(name);
        setIds(vendor, product);

        // LIBEVDEV_UINPUT_OPEN_MANAGED: libevdev opens/closes /dev/uinput itself
        int rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &udev);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category(), "libevdev_uinput_create_from_device");

        waitReady();
    }

    // ---- Emit ----

    void emit(int type, int code, int value) {
        int rc = libevdev_uinput_write_event(udev, type, code, value);
        if (rc < 0)
            throw std::system_error(-rc, std::generic_category(), "libevdev_uinput_write_event");
    }

    void syn()                     { emit(EV_SYN, SYN_REPORT, 0); }
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

// ---- Example usage ----

int main() {
    UinputDevice mouse;
    mouse.enableKey(BTN_LEFT);
    mouse.enableKey(BTN_RIGHT);
    mouse.enableRelAxis(REL_X);
    mouse.enableRelAxis(REL_Y);
    mouse.create("Virtual Mouse");

    mouse.mouseMove(100, 50);
    mouse.mouseClick(BTN_LEFT);

    UinputDevice kbd;
    kbd.enableKey(KEY_H);
    kbd.enableKey(KEY_I);
    kbd.enableKey(KEY_ENTER);
    kbd.create("Virtual Keyboard");

    kbd.keyPress(KEY_H);
    kbd.keyPress(KEY_I);
    kbd.keyPress(KEY_ENTER);

    return 0;
}