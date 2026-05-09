//wrapper for os calls, deciding to change to libevdev...
#include <cassert>
#include <unistd.h>
#include <system_error>

#include "OSRetrieve.h"



OSWrapper::~OSWrapper() {
    close();
}
void OSWrapper::spawn() {
    dev = libevdev_new();
    assert(dev && "libevdev_new failed");
}
void OSWrapper::close() {
    if (udev) libevdev_uinput_destroy(udev);
    if (dev)  libevdev_free(dev);
    dev = nullptr;
    udev = nullptr;
}
void OSWrapper::setName(const std::string& name) {
    assert(dev && "device not spawned");
    libevdev_set_name(dev, name.c_str());
}
void OSWrapper::setIds(uint16_t vendor, uint16_t product) {
    assert(dev && "device not spawned");
    libevdev_set_id_bustype(dev, BUS_USB);
    libevdev_set_id_vendor(dev, vendor);
    libevdev_set_id_product(dev, product);
}
void OSWrapper::toggleState(int state, bool enabled) {
    assert(dev && "device not spawned");
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, state, nullptr);
}
void OSWrapper::sync() {
    assert(udev && "device not created");
    int rc = libevdev_uinput_write_event(udev, EV_SYN, SYN_REPORT, 0);
    if (rc < 0)        throw std::system_error(-rc, std::generic_category(), "libevdev_uinput_write_event");
    return;
}
void OSWrapper::emit(int type, int code, int value) {
    assert(udev && "device not created");
    int rc = libevdev_uinput_write_event(udev, type, code, value);
    if (rc < 0)        throw std::system_error(-rc, std::generic_category(), "libevdev_uinput_write_event");
    return;
}

void OSWrapper::waitReady() { sleep(1); } //TODO utilize a non arbitrary way to await readiness;
void OSWrapper::create(const std::string& name, uint16_t vendor, uint16_t product) {
    setName(name);
    setIds(vendor, product);

    // LIBEVDEV_UINPUT_OPEN_MANAGED: libevdev opens/closes /dev/uinput itself
    int rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &udev);
    if (rc < 0)
        throw std::system_error(-rc, std::generic_category(), "libevdev_uinput_create_from_device");

    waitReady();
}