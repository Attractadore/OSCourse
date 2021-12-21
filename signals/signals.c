#include <fcntl.h>
#include <iso646.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifdef DEBUG 
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while(0)
#endif

struct {
unsigned short buffer_size;
unsigned char buffer[USHRT_MAX];
} data;

unsigned char* active_data = (unsigned char*) &data;

size_t active_bit = USHRT_WIDTH;

int fd;
pid_t ppid;

void childDeathHandler(int signum) {
    exit(-1);
}

bool server_done;

void recieve0Handler(int signum) {
    DEBUG_PRINT("Client: bit %zu\n", active_bit);
    DEBUG_PRINT("Client: recieved 0\n");
    size_t write_byte = active_bit / CHAR_BIT;   
    size_t write_bit = active_bit % CHAR_BIT;
    active_data[write_byte] &= (UCHAR_MAX ^ (1u << write_bit));
    active_bit++;
    server_done = true;
}

void recieve1Handler(int signum) {
    DEBUG_PRINT("Client: bit %zu\n", active_bit);
    DEBUG_PRINT("Client: recieved 1\n");
    size_t write_byte = active_bit / CHAR_BIT;   
    size_t write_bit = active_bit % CHAR_BIT;
    active_data[write_byte] |= (1u << write_bit);
    active_bit++;
    server_done = true;
}

void sendHandler(int signum) {
    if (active_bit == USHRT_WIDTH + data.buffer_size * CHAR_BIT) {
        ssize_t num_read = read(fd, data.buffer, sizeof(data.buffer));
        DEBUG_PRINT("Server: read %zd bytes\n", num_read);
        if (num_read < 0) {
            exit(-1);
        }
        data.buffer_size = num_read;
        active_bit = 0;
    }

    size_t read_byte = active_bit / CHAR_BIT;
    size_t read_bit = active_bit % CHAR_BIT;
    char bit = active_data[read_byte] & (1 << read_bit);
    DEBUG_PRINT("Server: bit %zu\n", active_bit);
    DEBUG_PRINT("Server: send %d\n", bit);
    active_bit++;
    if (kill(ppid, bit ? SIGUSR2: SIGUSR1)) {
        exit(-1);
    }

    if (active_bit == USHRT_WIDTH and !data.buffer_size) {
        exit(0);
    }
}

int activateChildDeathHandler() {
    struct sigaction act = {
        .sa_handler = childDeathHandler,
        .sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT,
    };
    DEBUG_PRINT("Client: set exit handler for child death\n");
    sigaction(SIGCHLD, &act, NULL);
    return 0;
}

int activateRecieveHandlers() {
    struct sigaction act0 = {
        .sa_handler = recieve0Handler,
        .sa_flags = SA_RESTART,
    };
    struct sigaction act1 = {
        .sa_handler = recieve1Handler,
        .sa_flags = SA_RESTART,
    };
    DEBUG_PRINT("Client: set read bit handler for SIGUSR1\n");
    sigaction(SIGUSR1, &act0, NULL);
    DEBUG_PRINT("Client: set read bit handler for SIGUSR2\n");
    sigaction(SIGUSR2, &act1, NULL);
    return 0;
}

int activateSendHandler() {
    struct sigaction act = {
        .sa_handler = sendHandler,
        .sa_flags = SA_RESTART,
    };
    DEBUG_PRINT("Server: set recieve handler\n");
    sigaction(SIGUSR1, &act, NULL);
    return 0;
}

int waitForServerSend() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    DEBUG_PRINT("Client: wait for server\n");
    sigset_t old_mask;
    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    while (!server_done) {
        DEBUG_PRINT("Client: suspend...\n");
        sigsuspend(&old_mask);
    }
    server_done = false;
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    DEBUG_PRINT("Client: server done\n");
    return 0;
}

int recieveN(pid_t cpid, size_t n_bits) {
    DEBUG_PRINT("Client: recieve %zu bits\n", n_bits);
    size_t end = active_bit + n_bits;
    while (active_bit < end) {
        if (kill(cpid, SIGUSR1)) {
            return -1;
        }
        if (waitForServerSend()) {
            return -1;
        }
    }
    return 0;
}

int recieveBufferSize(pid_t cpid) {
    DEBUG_PRINT("Client: recieve buffer size\n");
    int rc = recieveN(cpid, USHRT_WIDTH);
    DEBUG_PRINT("Client: recieved buffer size %d\n", data.buffer_size);
    return rc;
}

int recieveBuffer(pid_t cpid) {
    DEBUG_PRINT("Client: recieve buffer\n");
    int rc = recieveN(cpid, data.buffer_size * CHAR_BIT);
    DEBUG_PRINT("Client: recieved buffer\n");
    return rc;
}

int mainRecieveLoop(pid_t cpid) {
    do {
        active_bit = 0;
        if (recieveBufferSize(cpid)) {
            return -1;
        }
        if (recieveBuffer(cpid)) {
            return -1;
        }

        size_t total_written = 0;
        while(total_written < data.buffer_size) {
            ssize_t num_written =
                write(STDOUT_FILENO, data.buffer + total_written, data.buffer_size - total_written);
            DEBUG_PRINT("Client: wrote %zd bytes\n", num_written);
            if (num_written < 0) {
                return -1;
            }
            total_written += num_written;
        }
    } while (data.buffer_size);

    return 0;
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return -1;
    }

    const char* file_name = argv[1];
    fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open send file");
        return -1;
    }

    if (activateSendHandler()) {
        perror("Server: failed to set send handler");
        return -1;
    }
    ppid = getpid();
    pid_t cpid = fork();
    if (cpid < 0) {
        perror("Client: failed to fork");
        return -1;
    }
    
    if (cpid) {
        close(fd);
        if (activateChildDeathHandler() < 0) {
            perror("Client: failed to set server death signal handler");
            return -1;
        }
        if (activateRecieveHandlers()) {
            perror("Client: failed to set recieve signal handlers");
            return -1;
        }
        return mainRecieveLoop(cpid);
    }
    else {
        if (prctl(PR_SET_PDEATHSIG, SIGTERM)) {
            perror("Server: failed to set client death signal\n");
            return -1;
        }
        if (getppid() != ppid) {
            fprintf(stderr, "Server: client has already died\n");
            return -1;
        }
        while (true) {}
    }
}
