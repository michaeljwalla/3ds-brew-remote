
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <poll.h>
#include <dirent.h>

//return fd input referrer
int os_spawn() {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        throw std::system_error(errno, std::generic_category(),
            "open /dev/uinput (need root or input group)");
    return fd;
}

void os_close(int fd) {
    if (fd < 0) return;

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return;
}

int os_ioctl(int fd, unsigned long req) {
    return ioctl(fd, req);
}