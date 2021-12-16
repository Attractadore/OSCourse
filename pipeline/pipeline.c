#include <errno.h>
#include <fcntl.h>
#include <iso646.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

int copyFile(int in, int out) {
    enum {BUFFER_SIZE = 4096};
    char buffer[BUFFER_SIZE] = {0};
    ssize_t num_read = 0;
    do {
        num_read = read(in, buffer, BUFFER_SIZE);
        DEBUG_PRINT("Child: read %zd from fd %d\n", num_read, in);
        ssize_t total_write = 0;
        while (total_write < num_read) {
            ssize_t num_write = write(out, buffer + total_write, num_read - total_write);
            DEBUG_PRINT("Child: write %zd to fd %d\n", num_write, out);
            if (num_write < 0) {
                perror("Child: write failed");
                return num_write;
            }
            total_write += num_write;
        }
        DEBUG_PRINT("Child: copy %zd from fd %d to fd %d\n", num_read, in, out);
    }
    while (num_read > 0);
    if (num_read < 0) {
        perror("Child: read failed");
        sleep(10);
    }
    return num_read;
}

void runChild(int read_fd, int write_fd) {
    int rc = copyFile(read_fd, write_fd);
    DEBUG_PRINT("Child: close fds %d and %d\n", read_fd, write_fd);
    exit(rc);
}

int spawnChildren(unsigned n, int file_fd, int** write_fds_ptr, int** read_fds_ptr) {
    enum {
        READ_END = 0,
        WRITE_END = 1,
    };

    int* write_fds = calloc(n - 1, sizeof(*write_fds));          
    if (!write_fds) {
        return -1;
    }
    int* read_fds = calloc(n - 1, sizeof(*read_fds));          
    if (!read_fds) {
        return -1;
    }
    for (unsigned i = 0; i < n; i++) {
        int to_child_fildes[2];
        int from_child_fildes[2];
        if (i > 0) {
            if (pipe(to_child_fildes) < 0) {
                perror("Failed to create to child pipe");
                return -1;
            }
            DEBUG_PRINT("Open to pipe for child %u with READ_END %d and WRITE_END %d\n",
                i, to_child_fildes[READ_END], to_child_fildes[WRITE_END]
            );
        }
        if (i < n - 1) {
            if (pipe(from_child_fildes) < 0) {
                perror("Failed from create to child pipe");
                return -1;
            }
            DEBUG_PRINT("Open from pipe for child %u with READ_END %d and WRITE_END %d\n",
                i, from_child_fildes[READ_END], from_child_fildes[WRITE_END]
            );
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Failed to creat child");
            return -1;
        }
        if (!pid) {
            for (unsigned j = 0; j < i; j++) {
                if (close(read_fds[j]) < 0) {
                    perror("Pipe fd close failed");
                    return -1;
                }
            }
            for (unsigned j = 1; j < i; j++) {
                if (close(write_fds[j - 1]) < 0) {
                    perror("Pipe fd close failed");
                    return -1;
                }
            }
            free(write_fds);
            free(read_fds);
            int read_fd = to_child_fildes[READ_END];
            int write_fd = from_child_fildes[WRITE_END];
            if (i == 0) {
                read_fd = file_fd;
            }
            else {
                if (close(to_child_fildes[WRITE_END]) < 0) {
                    perror("Pipe fd close failed");
                    return -1;
                }
            }
            if (i == n - 1) {
                write_fd = STDOUT_FILENO;
            }
            else {
                if (close(from_child_fildes[READ_END]) < 0) {
                    perror("Pipe fd close failed");
                    return -1;
                }
            }
            runChild(read_fd, write_fd);
        }
        else {
            DEBUG_PRINT("Spawn child %d\n", pid);
            int write_fd = to_child_fildes[WRITE_END];
            int read_fd = from_child_fildes[READ_END];
            if (i > 0) {
                if (close(to_child_fildes[READ_END]) < 0) {
                    perror("Pipe fd close failed");
                    return -1;
                }
                if (fcntl(write_fd, F_SETFL, O_NONBLOCK) < 0) {
                    perror("Failed to fcntl write fd");
                    return -1;
                }
                write_fds[i - 1] = write_fd;
            }
            if (i < n - 1) {
                if (close(from_child_fildes[WRITE_END]) < 0) {
                    perror("Pipe fd close failed");
                    return -1;
                }
                if (fcntl(read_fd, F_SETFL, O_NONBLOCK) < 0) {
                    perror("Failed to fcntl read fd");
                    return -1;
                }
                read_fds[i] = read_fd;
            }
        }
    }

    *write_fds_ptr = write_fds;
    *read_fds_ptr = read_fds;

    return 0;
}

size_t bufferSize(unsigned n, unsigned i) {
    size_t size = 3 * 3 * 3 * 3;
    long long d = n - i;
    if (d > 0) {
        while (d) {
            size *= 3;
            d--;
        }
    }
    else if (d < 0) {
        while (d) {
            size /= 3;
            d++;
        }
    }
    if (size > (1 << 17)) {
        size = (1 << 17);
    }
    return size;
}

int allocateBuffers(unsigned n, char*** buffers_ptr, size_t** sizes_ptr, char** buffer_ptr) {
    char** buffers = calloc(n - 1, sizeof(*buffers));
    if (!buffers) {
        return -1;
    }
    size_t* sizes = calloc(n - 1, sizeof(*sizes));
    if (!sizes) {
        return -1;
    }
    size_t buffer_size = 0;
    for (unsigned i = 0; i < n - 1; i++) {
        sizes[i] = bufferSize(n, i);
        buffer_size += sizes[i];
    }
    char* buffer = calloc(buffer_size, sizeof(*buffer));
    if (!buffer) {
        return -1;
    }
    char* offset_buffer = buffer;
    for (unsigned i = 0; i < n - 1; i++) {
        buffers[i] = offset_buffer;
        offset_buffer += sizes[i];
    }
    *buffers_ptr = buffers;
    *sizes_ptr = sizes;
    *buffer_ptr = buffer;
    return 0;
}

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s n file\n", argv[0]);
        return -1;
    }
    char* end = NULL;
    long long n = strtoll(argv[1], &end, 10);
    if (*end or n <= 0) {
        fprintf(stderr, "N must be an non-negative integer\n");
        return -1;
    }
    if (errno == ERANGE or n > UINT_MAX) {
        fprintf(stderr, "N is too large. Use a smaller value\n");
        return -1;
    }
    const char* file_name = argv[2];
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        return -1;
    }

    int* write_fds = NULL;
    int* read_fds = NULL;
    DEBUG_PRINT("Spawn children\n");
    if (spawnChildren(n, file_fd, &write_fds, &read_fds) < 0) {
        fprintf(stderr, "Failed to spawn all children\n");
        return -1;
    }

    size_t num_buffers = n - 1;
    char** buffers = NULL;
    size_t* buffer_sizes = NULL;
    char* allocation = NULL;
    DEBUG_PRINT("Allocate buffers\n");
    if (allocateBuffers(n, &buffers, &buffer_sizes, &allocation) < 0) {
        fprintf(stderr, "Malloc failed\n");
        return -1;
    }

    size_t* num_available = calloc(num_buffers, sizeof(*num_available));
    if (!num_available) {
        fprintf(stderr, "Malloc failed\n");
        return -1;
    }
    char** offset_buffers = calloc(num_buffers, sizeof(*offset_buffers));
    if (!num_available) {
        fprintf(stderr, "Malloc failed\n");
        return -1;
    }

    DEBUG_PRINT("Setup poll structures\n");
    size_t num_fds = 2 * num_buffers;
    struct pollfd* fds = calloc(num_fds, sizeof(*fds));
    for (unsigned i = 0; i < num_buffers; i++) {
        struct pollfd* read_fd = &fds[2 * i];
        struct pollfd* write_fd = &fds[2 * i + 1];
        read_fd->fd = read_fds[i];
        read_fd->events = POLLIN;
        write_fd->fd = write_fds[i];
        write_fd->events = POLLOUT;
    }
    while(true) {
        DEBUG_PRINT("Poll\n");
        if (poll(fds, num_fds, -1) < 0) {
            return -1;
        }

        for (unsigned i = 0; i < num_buffers; i++) {
            DEBUG_PRINT("Process buffer %u\n", i);

            struct pollfd* read_fd = &fds[2 * i];
            struct pollfd* write_fd = &fds[2 * i + 1];
            bool can_read = read_fd->revents & POLLIN;
            bool can_write = write_fd->revents & POLLOUT;
            bool can_finish =
                (read_fd->revents & POLLHUP and !can_read) or
                (write_fd->revents & POLLERR);
            if (!num_available[i] and can_read) {
                DEBUG_PRINT("Child %u can write\n", i);
                ssize_t num_read = read(read_fd->fd, buffers[i], buffer_sizes[i]);
                DEBUG_PRINT("Child %u wrote %zd to fd %d\n", i, num_read, read_fd->fd);
                if (num_read <= 0) {
                    can_finish = true;
                    if (num_read < 0) {
                        perror("Read failed");
                        return -1;
                    }
                }
                else {
                    num_available[i] = num_read;
                    offset_buffers[i] = buffers[i];
                }
            }
            if (num_available[i] and can_write) {
                DEBUG_PRINT("Child %u can read\n", i + 1);
                ssize_t num_written = write(write_fd->fd, offset_buffers[i], num_available[i]);
                DEBUG_PRINT("Child %u read %zd from fd %d\n", i + 1, num_written, write_fd->fd);
                if (num_written <= 0) {
                    can_finish = true;
                    if (num_written < 0) {
                        perror("Write failed");
                        return -1;
                    }
                }
                else {
                    num_available[i] -= num_written;
                }
            }
            if (can_finish) {
                DEBUG_PRINT("Close fds %d and %d\n", read_fd->fd, write_fd->fd);
                close(read_fd->fd);
                close(write_fd->fd);
                if (i == num_buffers - 1) {
                    return 0;
                }
            }
        }
    }
}
