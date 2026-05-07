

#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <string>
#include <cstdint>
#include <system_error>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <poll.h>
#include <dirent.h>

static void checked_ioctl(int fd, unsigned long req, const char* name) {
    if (ioctl(fd, req) < 0)
        throw std::system_error(errno, std::generic_category(), name);
}

static void checked_ioctl(int fd, unsigned long req, int arg, const char* name) {
    if (ioctl(fd, req, arg) < 0)
        throw std::system_error(errno, std::generic_category(), name);
}

static void emit(int fd, int type, int code, int val) {
    struct input_event ev{};
    ev.type  = type;
    ev.code  = code;
    ev.value = val;
    if (write(fd, &ev, sizeof(ev)) != sizeof(ev))
        throw std::system_error(errno, std::generic_category(), "write input_event");
}

// Returns the eventN name for a given sysfs input name (e.g. "input15" -> "event15").
// Looks in /sys/class/input/<sysname>/ for a subdirectory starting with "event".
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

// Waits for /dev/input/<node> to appear. Sets up inotify before the stat check
// to avoid the race where it appears between check and watch.
static void waitForDevNode(const std::string& node, int timeout_ms = 3000) {
    int ifd = inotify_init1(IN_CLOEXEC);
    if (ifd < 0)
        throw std::system_error(errno, std::generic_category(), "inotify_init1");

    inotify_add_watch(ifd, "/dev/input", IN_CREATE);

    // Check if it already exists (race-free: watch is registered first)
    std::string path = "/dev/input/" + node;
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) {
        close(ifd);
        return;
    }

    // Read inotify events until our node appears or we time out
    struct pollfd pfd{ ifd, POLLIN, 0 };
    char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
    while (poll(&pfd, 1, timeout_ms) > 0) {
        ssize_t n = read(ifd, buf, sizeof(buf));
        for (ssize_t i = 0; i < n;) {
            auto* ev = reinterpret_cast<struct inotify_event*>(buf + i);
            if (ev->len && ev->name == node) {
                close(ifd);
                return;
            }
            i += sizeof(struct inotify_event) + ev->len;
        }
    }

    close(ifd);
    throw std::runtime_error("timed out waiting for " + path);
}

class UinputDevice {
    int fd = -1;

    void waitReady() {
        // UI_GET_SYSNAME succeeds only once the kernel input device is live.
        char sysname[64]{};
        if (ioctl(fd, UI_GET_SYSNAME(sizeof(sysname)), sysname) < 0)
            throw std::system_error(errno, std::generic_category(), "UI_GET_SYSNAME");

        // Kernel device is registered; now wait for udev to create /dev/input/eventN
        // so libinput/the compositor can actually see the device.
        std::string eventname = findEventName(sysname);
        if (eventname.empty())
            throw std::runtime_error(std::string("no event node found under /sys/class/input/") + sysname);

        waitForDevNode(eventname);
        sleep(1);
    }

public:
    UinputDevice() {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0)
            throw std::system_error(errno, std::generic_category(),
                                    "open /dev/uinput (need root or input group)");
    }

    ~UinputDevice() {
        if (fd >= 0) {
            ioctl(fd, UI_DEV_DESTROY);
            close(fd);
        }
    }

    UinputDevice(const UinputDevice&)            = delete;
    UinputDevice& operator=(const UinputDevice&) = delete;

    // ---- Setup ----

    void enableEventType(int type) { checked_ioctl(fd, UI_SET_EVBIT,  type, "UI_SET_EVBIT");  }
    void enableKey(int key)        { checked_ioctl(fd, UI_SET_KEYBIT, key,  "UI_SET_KEYBIT"); }
    void enableRelAxis(int axis)   { checked_ioctl(fd, UI_SET_RELBIT, axis, "UI_SET_RELBIT"); }
    void enableAbsAxis(int axis)   { checked_ioctl(fd, UI_SET_ABSBIT, axis, "UI_SET_ABSBIT"); }

    void create(const std::string& name, uint16_t vendor = 0x1234, uint16_t product = 0x5678) {
        struct uinput_setup usetup{};
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor  = vendor;
        usetup.id.product = product;
        strncpy(usetup.name, name.c_str(), UINPUT_MAX_NAME_SIZE - 1);

        
        if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0)
            throw std::system_error(errno, std::generic_category(), "UI_DEV_SETUP");
        checked_ioctl(fd, UI_DEV_CREATE, "UI_DEV_CREATE");
        waitReady();
    }

    // ---- Emit helpers ----

    void syn()                     { emit(fd, EV_SYN, SYN_REPORT, 0); }
    void keyDown(int key)          { emit(fd, EV_KEY, key, 1); syn(); }
    void keyUp(int key)            { emit(fd, EV_KEY, key, 0); syn(); }
    void keyPress(int key)         { keyDown(key); keyUp(key); }
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
