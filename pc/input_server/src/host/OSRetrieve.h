#pragma once

int os_spawn();
void os_close(int fd);
int os_ioctl(int fd, unsigned long req);