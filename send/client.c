// This program reads data sent by the server and prints it
#include "common.h"

#include <errno.h>
#include <iso646.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    DEBUG_PRINT("Try to open sync fifo\n", SYNC_FIFO_NAME);
    if (mkfifo(SYNC_FIFO_NAME, 0600) and errno != EEXIST) {
        perror("Failed to create sync fifo");
        return -1;
    }
    int sync = open(SYNC_FIFO_NAME, O_RDWR | O_CLOEXEC);
    if (sync < 0) {
        perror("Failed to open sync fifo");
        return -1;
    }
    DEBUG_PRINT("Opened %s\n", SYNC_FIFO_NAME);

    DEBUG_PRINT("Try to create send fifo\n");
    char fifo_name[PIPE_BUF] = "/tmp/send_fifo_XXXXXX";
    mktemp(fifo_name);
    if (!fifo_name[0]) {
        perror("Failed to generate send fifo name");
        return -1;
    }
    if (mkfifo(fifo_name, 0600)) {
        perror("Failed to create send fifo");
        return -1;
    }
    DEBUG_PRINT("Created send fifo %s\n", fifo_name);

    DEBUG_PRINT("Try to open send fifo\n");
    int in = open(fifo_name, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (in < 0) {
        perror("Failed to open send fifo");
        return -1;
    }
    if (fcntl(in, F_SETFL, 0) == -1) {
        perror("Failed to fcntl() send fifo");
        return -1;
    }
    DEBUG_PRINT("Opened send fifo\n");

    DEBUG_PRINT("Try to write send fifo name to sync fifo\n");
    if (write(sync, fifo_name, PIPE_BUF) != PIPE_BUF) {
        perror("Failed to write send fifo name to sync fifo");
        return -1;
    }
    DEBUG_PRINT("Wrote send fifo name\n");

    DEBUG_PRINT("Wait for writer\n");
    {
        int timeout = 120 * 1000;
        struct pollfd pfd = {
            .fd = in,
            .events = POLLIN,
        };
        int res = poll(&pfd, 1, timeout);
        if (res < 0) {
            perror("Failed to poll() send fifo");
            return -1;
        }
        if (!res) {
            DEBUG_PRINT("Timeout while waiting for writer, restart\n");
            execvp(argv[0], argv);
            perror("Failed to restart");
            return -1;
        }
    }

    DEBUG_PRINT("Begin recieving file\n");
    int out = STDOUT_FILENO;
    if (copyFile(in, out)) {
        perror("Failed to recieve file");
        return -1;
    }
    DEBUG_PRINT("File recieved\n");
}
