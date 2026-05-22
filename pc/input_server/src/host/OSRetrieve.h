#pragma once
#include <cstdint>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <string>

class OSWrapper {
    libevdev* dev {nullptr};
    libevdev_uinput* udev {nullptr};

    void waitReady();
    bool queued = false;

    public:
    OSWrapper() = default;
    OSWrapper(const OSWrapper&) = delete;
    OSWrapper& operator=(const OSWrapper&) = delete;
    OSWrapper(OSWrapper&&) = default;
    OSWrapper& operator=(OSWrapper&&) = default;
    ~OSWrapper();
    ///
    void spawn();
    void close();

    void setName(const std::string& name);
    void setIds(uint16_t vendor, uint16_t product);
    void setVersion(uint16_t version);

    void enableKey(int code);
    void enableRelAxis(int axis);
    void enableAbsAxis(int axis, const input_absinfo& info);
    void sync(bool force=false);
    void emit(int type, int code, int value);

    //yields
    void create(const std::string& name, uint16_t vendor=0x1234, uint16_t product=0x5678, uint16_t version=0x1);
};