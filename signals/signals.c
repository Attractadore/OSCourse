#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#ifdef DEBUG 
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while(0)
#endif

unsigned short buffer_size;
unsigned char buffer[USHRT_MAX];

unsigned char* active_data;
size_t active_bit;

bool server_ready;

void recieveWaitHandler(int signum) {
    server_ready = true;
}

void recieve0Handler(int signum) {
    DEBUG_PRINT("Client: bit %zu\n", active_bit);
    DEBUG_PRINT("Client: recieved 0\n");
    size_t write_byte = active_bit / CHAR_BIT;   
    size_t write_bit = active_bit % CHAR_BIT;
    active_data[write_byte] &= (UCHAR_MAX ^ (1u << write_bit));
    active_bit++;
    server_ready = true;
}

void recieve1Handler(int signum) {
    DEBUG_PRINT("Client: bit %zu\n", active_bit);
    DEBUG_PRINT("Client: recieved 1\n");
    size_t write_byte = active_bit / CHAR_BIT;   
    size_t write_bit = active_bit % CHAR_BIT;
    active_data[write_byte] |= (1u << write_bit);
    active_bit++;
    server_ready = true;
}

int activateRecieveWaitHandlers() {
    struct sigaction act = {
        .sa_handler = recieveWaitHandler,
    };
    DEBUG_PRINT("Client: set wait handler for SIGUSR1\n");
    sigaction(SIGUSR1, &act, NULL);
    return 0;
}

int activateRecieveHandlers() {
    struct sigaction act0 = {
        .sa_handler = recieve0Handler,
    };
    struct sigaction act1 = {
        .sa_handler = recieve1Handler,
    };
    DEBUG_PRINT("Client: set read bit handler for SIGUSR1\n");
    sigaction(SIGUSR1, &act0, NULL);
    DEBUG_PRINT("Client: set read bit handler for SIGUSR2\n");
    sigaction(SIGUSR2, &act1, NULL);
    return 0;
}

// TODO: handle server dying
int waitForServer(const sigset_t* mask) {
    DEBUG_PRINT("Client: wait for server\n");
    sigset_t old_mask;
    sigprocmask(SIG_BLOCK, mask, &old_mask);
    while (!server_ready) {
        DEBUG_PRINT("Client: suspend...\n");
        sigsuspend(&old_mask);
    }
    server_ready = false;
    sigprocmask(SIG_UNBLOCK, mask, NULL);
    DEBUG_PRINT("Client: server ready\n");
    return 0;
}

int waitForServerReady() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    return waitForServer(&mask);
}

int waitForServerSend() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    return waitForServer(&mask);
}

int recieveN(pid_t cpid, size_t n_bits) {
    DEBUG_PRINT("Client: recieve %zu bits\n", n_bits);
    active_bit = 0;
    while (active_bit < n_bits) {
        errno = 0;
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
    buffer_size = 0;
    active_data = (unsigned char*) &buffer_size;
    int rc = recieveN(cpid, USHRT_WIDTH);
    DEBUG_PRINT("Client: recieved buffer size %d\n", buffer_size);
    return rc;
}

int recieveBuffer(pid_t cpid) {
    DEBUG_PRINT("Client: recieve buffer\n");
    active_data = buffer;
    int rc = recieveN(cpid, buffer_size * CHAR_BIT);
    DEBUG_PRINT("Client: recieved buffer\n");
    return rc;
}

int mainRecieveLoop(pid_t cpid) {
    do {
        if (waitForServerReady()) {
            return -1;
        }
    
        if (activateRecieveHandlers()) {
            return -1;
        }
        if (recieveBufferSize(cpid)) {
            return -1;
        }
        if (recieveBuffer(cpid)) {
            return -1;
        }
        if (activateRecieveWaitHandlers()) {
            return -1;
        }
        DEBUG_PRINT("Client: send confirmation to server\n");
        if (kill(cpid, SIGUSR1)) {
            return -1;
        }

        ssize_t num_written =
            TEMP_FAILURE_RETRY(write(STDOUT_FILENO, buffer, buffer_size));
        DEBUG_PRINT("Client: wrote %zd bytes\n", num_written);
        if (num_written < 0) {
            return -1;
        }
    } while (buffer_size);

    return 0;
}

bool client_ready;

void sendWaitHandler(int signum) {
    client_ready = true;
}

int activateSendHandlers() {
    struct sigaction act = {
        .sa_handler = sendWaitHandler,
    };
    DEBUG_PRINT("Server: set wait handler for SIGUSR1");
    sigaction(SIGUSR1, &act, NULL);
    return 0;
}

// TODO: handle client dying
int waitForClient() {
    DEBUG_PRINT("Server: wait for client\n");
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    while (!client_ready) {
        DEBUG_PRINT("Server: suspend...\n");
        sigsuspend(&old_mask);
    }
    client_ready = false;
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    DEBUG_PRINT("Server: client ready\n");
    return 0;
}

int sendN(pid_t ppid, size_t n_bits) {
    DEBUG_PRINT("Server: send %zu bits\n", n_bits);
    for (active_bit = 0; active_bit < n_bits; active_bit++) {
        if (waitForClient()) {
            return -1;
        }
        size_t read_byte = active_bit / CHAR_BIT;
        size_t read_bit = active_bit % CHAR_BIT;
        char bit = active_data[read_byte] & (1 << read_bit);
        DEBUG_PRINT("Server: bit %zu\n", active_bit);
        DEBUG_PRINT("Server: send %d\n", bit);
        if (kill(ppid, bit ? SIGUSR2: SIGUSR1)) {
            return -1;
        }
    }

    return 0;
}

int sendBufferSize(pid_t ppid) {
    DEBUG_PRINT("Server: send buffer size %u\n", buffer_size);
    active_data = (unsigned char*) &buffer_size;
    int rc = sendN(ppid, SHRT_WIDTH);
    DEBUG_PRINT("Server: sent buffer size\n");
    return rc;
}

int sendBuffer(pid_t ppid) {
    DEBUG_PRINT("Server: send buffer\n");
    active_data = buffer;
    int rc = sendN(ppid, buffer_size * CHAR_BIT);
    DEBUG_PRINT("Server: sent buffer\n");
    return rc;
}

int mainSendLoop(pid_t ppid, int fd) {
    do {
        ssize_t num_read =
            TEMP_FAILURE_RETRY(read(fd, buffer, sizeof(buffer)));
        DEBUG_PRINT("Server: read %zd bytes\n", num_read);
        if (num_read < 0) {
            return -1;
        }
        buffer_size = num_read;

        DEBUG_PRINT("Server: send ready status to client\n");
        if (kill(ppid, SIGUSR1)) {
            return -1;
        }
        if (sendBufferSize(ppid)) {
            return -1;
        }
        if (sendBuffer(ppid)) {
            return -1;
        }
        DEBUG_PRINT("Server: wait for client confirmation\n");
        if (waitForClient()) {
            return -1;
        }
    } while(buffer_size);

    return 0;
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return -1;
    }

    const char* file_name = argv[1];
    int fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open send file");
        return -1;
    }

    if (activateRecieveWaitHandlers() < 0) {
        perror("Failed to set signal handlers");
        return -1;
    }
    pid_t cpid = fork();
    if (cpid < 0) {
        perror("Failed to fork");
        return -1;
    }
    
    if (cpid) {
        close(fd);
        return mainRecieveLoop(cpid);
    }
    else {
        if (activateSendHandlers()) {
            perror("Failed to set signal handlers");
            return -1;
        }
        // TODO: getppid is a race condition
        return mainSendLoop(getppid(), fd);
    }
}
