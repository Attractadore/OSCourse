// This program sends data to a client
#include "common.h"

#include <errno.h>
#include <iso646.h>
#include <limits.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [file_name]\n", argv[0]);
        return -1;
    }

    const char* file_name = argv[1];
    DEBUG_PRINT("Try to open %s\n", file_name);
    int in = open(file_name, O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        perror("Failed to open input file");
        return -1;
    }
    DEBUG_PRINT("Opened %s\n", file_name);

    DEBUG_PRINT("Try to open sync fifo %s\n", SYNC_FIFO_NAME);
    if (mkfifo(SYNC_FIFO_NAME, 0600) && errno != EEXIST) {
        perror("Failed to create sync fifo");
        return -1;
    }
    int sync = open(SYNC_FIFO_NAME, O_RDWR | O_CLOEXEC);
    if (sync < 0) {
        perror("Failed to open sync fifo");
        return -1;
    }
    DEBUG_PRINT("Opened %s\n", SYNC_FIFO_NAME);

    DEBUG_PRINT("Try to read send fifo name\n");
    char fifo_name[PIPE_BUF];
    ssize_t num_read = read(sync, fifo_name, PIPE_BUF);
    if (num_read != PIPE_BUF) {
        DEBUG_PRINT("Read %d bytes instead of %d\n", num_read, PIPE_BUF);
        perror("Failed to read send fifo name from sync fifo");
        return -1;
    }
    DEBUG_PRINT("Send fifo name is %s\n", fifo_name);

    DEBUG_PRINT("Try to open send fifo\n");
    int out = open(fifo_name, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (out < 0) {
        if (errno == ENXIO) {
            DEBUG_PRINT("Reader has died, restart\n");
            execvp(argv[0], argv);
            perror("Failed to restart");
        }
        else {
            perror("Failed to open send fifo");
        }
        return -1;
    }
    if (fcntl(out, F_SETFL, 0) == -1) {
        perror("Failed to fcntl() send fifo");
        return -1;
    }
    DEBUG_PRINT("Opened send fifo\n");

    DEBUG_PRINT("Begin sending file\n");
    if (copyFile(in, out)) {
        perror("Failed to send file");
        return -1;
    }
    DEBUG_PRINT("File sent\n");
}
