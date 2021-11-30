#include "common.h"

#include <fcntl.h>
#include <unistd.h>

int copyFile(int in, int out) {
    enum {BUFFER_SIZE = 4096};
    char buffer[BUFFER_SIZE] = {0};
    ssize_t num_read = 0;
    while ((num_read = read(in, buffer, BUFFER_SIZE)) > 0) {
        ssize_t total_write = 0;
        while (total_write < num_read) {
            ssize_t num_write = write(out, buffer + total_write, num_read - total_write);
            if (num_write < 0) {
                return num_write;
            }
            total_write += num_write;
        }
    }
    return num_read;
}
