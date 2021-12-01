#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

enum {
    // There is a server in this block if this is one
    BLOCK_SERVER_SEM_ID,
    // There is a client in this block if this is one
    BLOCK_CLIENT_SEM_ID,

    // Number of buffers that can be read from
    BLOCK_FULL_SEM_ID,
    // Number of buffers that can be written to
    BLOCK_EMPTY_SEM_ID,

    BLOCK_NUM_SEMS,
};

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                (Linux-specific) */
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))

static const char* tok_file = "/tmp/sem_tok_file";
static int createTokFile() {
    int fd = open(tok_file, O_CREAT | O_WRONLY, 0600);
    if (fd == -1) {
        return -1;
    }
    close(fd);
    return 0;
}


static key_t getShmKey() {
    if (createTokFile()) {
        return -1;
    }
    return ftok(tok_file, 1);
}

SharedMemory* getSharedMemory() {
    DEBUG_PRINT("Open shm\n");
    key_t shm_key = getShmKey();
    if (shm_key == -1) {
        perror("Failed to get shm key");
        return NULL;
    }
    int shm = shmget(shm_key, sizeof(SharedMemory), IPC_CREAT | 0600);
    if (shm == -1) {
        perror("Failed to open shm");
        return NULL;
    }
    DEBUG_PRINT("Attach shm\n");
    errno = 0;
    void* ptr = shmat(shm, NULL, 0);
    if (errno) {
        perror("Failed to attach shm");
        return NULL;
    }
    DEBUG_PRINT("\n");
    return ptr;
}

static key_t getSemSetKey() {
    if (createTokFile()) {
        return -1;
    }
    return ftok(tok_file, 2);
}

int getSemaphoreSet() {
    DEBUG_PRINT("Open sem set\n");
    key_t sem_key = getSemSetKey();
    if (sem_key == -1) {
        perror("Failed to get sem set key\n");
        return -1;
    }
    int sem = semget(sem_key, BLOCK_NUM_SEMS, IPC_CREAT | 0600);
    if (sem == -1) {
        perror("Failed to open sem set\n");
        return -1;
    }
    DEBUG_PRINT("\n");
    return sem;
}

int acquireServer(int sem_set) {
    struct sembuf ops[] = {
        {
            .sem_num = BLOCK_SERVER_SEM_ID,
            .sem_op = 0,
        },
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = 0,
        },
        {
            .sem_num = BLOCK_SERVER_SEM_ID,
            .sem_op = 1,
            .sem_flg = SEM_UNDO,
        },
    };
    DEBUG_PRINT("Acquire from server\n");
    int res = semop(sem_set, ops, ARRAY_SIZE(ops));
    if (res == -1) {
        perror("Failed to acquire from server");
        return -1;
    }

    union semun v;
    v.val = 0;
    errno = 0;
    DEBUG_PRINT("Set full sem to %d\n", v.val);
    semctl(sem_set, BLOCK_FULL_SEM_ID, SETVAL, v);
    if (errno) {
        perror("Failed to init sem");
        return -1;
    }

    v.val = 1;
    errno = 0;
    DEBUG_PRINT("Set empty sem to %d\n", v.val);
    semctl(sem_set, BLOCK_EMPTY_SEM_ID, SETVAL, v);
    if (errno) {
        perror("Failed to init sem");
        return -1;
    }

    DEBUG_PRINT("Server sem value: %d\n", semctl(sem_set, BLOCK_SERVER_SEM_ID, GETVAL));
    DEBUG_PRINT("Client sem value: %d\n", semctl(sem_set, BLOCK_CLIENT_SEM_ID, GETVAL));
    DEBUG_PRINT("Full sem value: %d\n", semctl(sem_set, BLOCK_FULL_SEM_ID, GETVAL));
    DEBUG_PRINT("Empty sem value: %d\n", semctl(sem_set, BLOCK_EMPTY_SEM_ID, GETVAL));
    DEBUG_PRINT("\n");

    return 0;
}

int acquireClient(int sem_set) {
    DEBUG_PRINT("Acquire from client\n");
    struct sembuf ops[] = {
        {
            .sem_num = BLOCK_SERVER_SEM_ID,
            .sem_op = -1,
        },
        {
            .sem_num = BLOCK_SERVER_SEM_ID,
            .sem_op = 0,
        },
        {
            .sem_num = BLOCK_SERVER_SEM_ID,
            .sem_op = 1,
        },
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = 0,
        },
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = 1,
            .sem_flg = SEM_UNDO,
        },
    };
    int res = semop(sem_set, ops, ARRAY_SIZE(ops));
    if (res == -1) {
        perror("Failed to acquire from client");
        return -1;
    }

    DEBUG_PRINT("Server sem value: %d\n", semctl(sem_set, BLOCK_SERVER_SEM_ID, GETVAL));
    DEBUG_PRINT("Client sem value: %d\n", semctl(sem_set, BLOCK_CLIENT_SEM_ID, GETVAL));
    DEBUG_PRINT("Full sem value: %d\n", semctl(sem_set, BLOCK_FULL_SEM_ID, GETVAL));
    DEBUG_PRINT("Empty sem value: %d\n", semctl(sem_set, BLOCK_EMPTY_SEM_ID, GETVAL));
    DEBUG_PRINT("\n");

    return 0;
}

int waitForClient(int sem_set) {
    struct sembuf wait_ops[] = {
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = -1,
        },
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = 0,
        },
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = 1,
        },
    };
    return semop(sem_set, wait_ops, ARRAY_SIZE(wait_ops));
}

int copyIntoSharedMemory(int sem_set, SharedMemory* shm, int fd) {
    DEBUG_PRINT("Wait for client\n");
    int res = waitForClient(sem_set);
    if (res == -1) {
        perror("Failed to wait for client");
        return -1;
    }

    DEBUG_PRINT("Begin copying to shm\n");
    while (true) {
        DEBUG_PRINT("Wait for client to read\n");
        struct sembuf wait_ops[] = {
            {
                .sem_num = BLOCK_CLIENT_SEM_ID,
                .sem_op = -1,
                .sem_flg = IPC_NOWAIT,
            },
            {
                .sem_num = BLOCK_CLIENT_SEM_ID,
                .sem_op = 0,
                .sem_flg = IPC_NOWAIT,
            },
            {
                .sem_num = BLOCK_CLIENT_SEM_ID,
                .sem_op = 1,
            },
            {
                .sem_num = BLOCK_EMPTY_SEM_ID,
                .sem_op = -1,
            },

        };
        errno = 0;
        semop(sem_set, wait_ops, ARRAY_SIZE(wait_ops));
        if (errno) {
            if (errno == EAGAIN) {
                fprintf(stderr, "Client has died\n");
            }
            else {
                perror("Failed to wait for client\n");
            }
            return -1;
        }

        DEBUG_PRINT("Read chunk\n");
        shm->buffer.size = read(fd, shm->buffer.buffer, sizeof(shm->buffer.buffer));
        if (shm->buffer.size < 0) {
            perror("Failed to read from fd");
            return -1;
        }
        DEBUG_PRINT("Read %zd bytes\n", shm->buffer.size);

        DEBUG_PRINT("Wake up client\n");
        struct sembuf write_op = {
            .sem_num = BLOCK_FULL_SEM_ID,
            .sem_op = 1,
        };
        errno = 0;
        semop(sem_set, &write_op, 1);
        if (errno) {
            perror("Failed to wake up client\n");
            return -1;
        }

        if (!shm->buffer.size) {
            DEBUG_PRINT("EOF\n");
            return 0;
        }
    }
}

int copyFromSharedMemory(int sem_set, const volatile SharedMemory* shm, int fd) {
    DEBUG_PRINT("Begin copying from shm\n");
    while (true) {
        DEBUG_PRINT("Wait for server to write\n");
        struct sembuf wait_ops[] = {
            {
                .sem_num = BLOCK_SERVER_SEM_ID,
                .sem_op = -1,
                .sem_flg = IPC_NOWAIT,
            },
            {
                .sem_num = BLOCK_SERVER_SEM_ID,
                .sem_op = 0,
                .sem_flg = IPC_NOWAIT,
            },
            {
                .sem_num = BLOCK_SERVER_SEM_ID,
                .sem_op = 1,
            },
            {
                .sem_num = BLOCK_FULL_SEM_ID,
                .sem_op = -1,
            },
        };
        errno = 0;
        semop(sem_set, wait_ops, ARRAY_SIZE(wait_ops));
        if (errno) {
            if (errno == EAGAIN) {
                fprintf(stderr, "Server has died\n");
                return -1;
            }
            else {
                perror("Failed to wait for server\n");
                return -1;
            }
        }
        
        if (shm->buffer.size > 0) {
            DEBUG_PRINT("Write chunk\n");
            ssize_t total_num_written = 0;
            while (total_num_written < shm->buffer.size) {
                ssize_t num_written = write(fd, shm->buffer.buffer, shm->buffer.size);
                if (num_written < 0) {
                    perror("Failed to write to fd");
                    return -1;
                }
                DEBUG_PRINT("Wrote %zd bytes\n", num_written);
                total_num_written += num_written;
            }
        } else {
            DEBUG_PRINT("EOF\n");
            return 0;
        }

        DEBUG_PRINT("Wake up server\n");
        struct sembuf read_op = {
            .sem_num = BLOCK_EMPTY_SEM_ID,
            .sem_op = 1,
        };
        errno = 0;
        semop(sem_set, &read_op, 1);
        if (errno) {
            perror("Failed to wake up server");
            return -1;
        }
    }
}
