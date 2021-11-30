#include "common.h"

#include <assert.h>
#include <errno.h>
#include <iso646.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

enum {
    // There is a server in this block if this is one
    BLOCK_SERVER_SEM_ID = 0,
    // There is a client in this block if this is one
    BLOCK_CLIENT_SEM_ID = 1,
    // A client can connect to this block if this is one
    BLOCK_BEGIN_SEM_ID = 2,
    // Number of buffers that can be read from
    BLOCK_FULL_SEM_ID = 3,
    // Number of buffers that can be written to
    BLOCK_EMPTY_SEM_ID = 4,

    BLOCK_NUM_SEMS,
};

static size_t getMaxBlocks() {
    return 10;
}

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                (Linux-specific) */
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))

static key_t getShmKey() {
    return ftok(__FILE__, 1);
}

static size_t getSharedMemorySize() {
    return sizeof(SharedMemory) + sizeof(TransferBuffer[32000]);
}

SharedMemory* getSharedMemory() {
    DEBUG_PRINT("Get shm key\n");
    key_t shm_key = getShmKey();
    DEBUG_PRINT("Shm key is %d\n", shm_key);
    if (shm_key == -1) {
        perror("Failed to get shm key");
        return NULL;
    }
    DEBUG_PRINT("Open shm\n");
    int shm = shmget(shm_key, getSharedMemorySize(), IPC_CREAT | 0600);
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

TransferBuffer* getBlockBuffer(SharedMemory* shm, int i) {
    return &shm->buffers[i];
}

#include <limits.h>

static key_t getBlockSemKey(int i) {
    assert("TODO: enable creating keys when i more than 8 bits" && 2 + i <= UCHAR_MAX);
    return ftok(__FILE__, 2 + i);
}

int getBlockSemaphore(int i) {
    key_t block_sem_key = getBlockSemKey(i);
    if (block_sem_key == -1) {
        return -1;
    }
    return semget(block_sem_key, BLOCK_NUM_SEMS, IPC_CREAT | 0600);
}

static int selectBlockImpl(struct sembuf* ops, size_t nops) {
    size_t max_blocks = getMaxBlocks();
    for (int i = 0; i < max_blocks; i++) {
        DEBUG_PRINT("Get sem set for block %d\n", i);
        int sem = getBlockSemaphore(i);       
        if (sem == -1) {
            fprintf(stderr, "Failed to get sem set for block %d\n", i);
            perror("");
        }
        int res = semop(sem, ops, nops);
        if (!res) {
            return i;
        }
    }
    assert(!"TODO: handle case when no block is found");
    return -1;
}

int selectServerBlock() {
    struct sembuf ops[] = {
        {
            .sem_num = BLOCK_SERVER_SEM_ID,
            .sem_op = 0,
            .sem_flg = IPC_NOWAIT,
        },
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = 0,
            .sem_flg = IPC_NOWAIT,
        },
        {
            .sem_num = BLOCK_SERVER_SEM_ID,
            .sem_op = 1,
            .sem_flg = IPC_NOWAIT | SEM_UNDO,
        },
        {
            .sem_num = BLOCK_BEGIN_SEM_ID,
            .sem_op = 1,
            .sem_flg = IPC_NOWAIT | SEM_UNDO,
        },
    };
    DEBUG_PRINT("Select server block\n");
    int i = selectBlockImpl(ops, ARRAY_SIZE(ops));
    if (i == -1) {
        return -1;
    }
    DEBUG_PRINT("Selected block is %d\n", i);

    int sem_set = getBlockSemaphore(i);
    if (sem_set == -1) {
        return -1;
    }
    DEBUG_PRINT("Block sem set is %d\n", sem_set);

    union semun v;
    v.val = 0;
    errno = 0;
    DEBUG_PRINT("Set full sem to %d\n", v.val);
    semctl(sem_set, BLOCK_FULL_SEM_ID, SETVAL, v);
    if (errno) {
        return -1;
    }

    v.val = 1;
    errno = 0;
    DEBUG_PRINT("Set empty sem to %d\n", v.val);
    semctl(sem_set, BLOCK_EMPTY_SEM_ID, SETVAL, v);
    if (errno) {
        return -1;
    }

    DEBUG_PRINT("Server sem value: %d\n", semctl(sem_set, BLOCK_SERVER_SEM_ID, GETVAL));
    DEBUG_PRINT("Client sem value: %d\n", semctl(sem_set, BLOCK_CLIENT_SEM_ID, GETVAL));
    DEBUG_PRINT("Begin sem value: %d\n", semctl(sem_set, BLOCK_BEGIN_SEM_ID, GETVAL));
    DEBUG_PRINT("Full sem value: %d\n", semctl(sem_set, BLOCK_FULL_SEM_ID, GETVAL));
    DEBUG_PRINT("Empty sem value: %d\n", semctl(sem_set, BLOCK_EMPTY_SEM_ID, GETVAL));
    DEBUG_PRINT("\n");

    return i;
}

int selectClientBlock() {
    struct sembuf ops[] = {
        {
            .sem_num = BLOCK_CLIENT_SEM_ID,
            .sem_op = 1,
            .sem_flg = IPC_NOWAIT | SEM_UNDO,
        },
        {
            .sem_num = BLOCK_BEGIN_SEM_ID,
            .sem_op = -1,
            .sem_flg = IPC_NOWAIT,
        },
    };
    int i = -1;
    selectBlockImpl(ops, ARRAY_SIZE(ops));

    int sem_set = getBlockSemaphore(i);
    DEBUG_PRINT("Server sem value: %d\n", semctl(sem_set, BLOCK_SERVER_SEM_ID, GETVAL));
    DEBUG_PRINT("Client sem value: %d\n", semctl(sem_set, BLOCK_CLIENT_SEM_ID, GETVAL));
    DEBUG_PRINT("Begin sem value: %d\n", semctl(sem_set, BLOCK_BEGIN_SEM_ID, GETVAL));
    DEBUG_PRINT("Full sem value: %d\n", semctl(sem_set, BLOCK_FULL_SEM_ID, GETVAL));
    DEBUG_PRINT("Empty sem value: %d\n", semctl(sem_set, BLOCK_EMPTY_SEM_ID, GETVAL));
    DEBUG_PRINT("\n");

    return i;
}

static const struct timespec timeout = {
    .tv_sec = 60,
};

void copyIntoSharedMemory(int sem_set, TransferBuffer* buffer, int fd) {
    DEBUG_PRINT("Begin copying to shm\n");
    DEBUG_PRINT("Sem set is %d\n", sem_set);

    while (true) {
        DEBUG_PRINT("Wait for client to read\n");
        DEBUG_PRINT("Empty sem value: %d\n", semctl(sem_set, BLOCK_EMPTY_SEM_ID, GETVAL));
        struct sembuf wait_op = {
            .sem_num = BLOCK_EMPTY_SEM_ID,
            .sem_op = -1,
        };
        errno = 0;
        semtimedop(sem_set, &wait_op, 1, &timeout);
        if (errno == EAGAIN) {
            fprintf(stderr, "Client has died\n");
            return;
        }

        DEBUG_PRINT("Read chunk\n");
        buffer->size = read(fd, buffer->buffer, sizeof(buffer->buffer));

        DEBUG_PRINT("Wake up client\n");
        DEBUG_PRINT("Full sem value: %d\n", semctl(sem_set, BLOCK_FULL_SEM_ID, GETVAL));
        struct sembuf write_op = {
            .sem_num = BLOCK_FULL_SEM_ID,
            .sem_op = 1,
        };
        semop(sem_set, &write_op, 1);

        if (buffer->size <= 0) {
            DEBUG_PRINT("EOF\n");
            return;
        }
    }
}

void copyFromSharedMemory(int sem_set, const volatile TransferBuffer* buffer, int fd) {
    DEBUG_PRINT("Begin copying from shm\n");
    DEBUG_PRINT("Sem set is %d\n", sem_set);

    while (true) {
        DEBUG_PRINT("Wait for server to write\n");
        DEBUG_PRINT("Full sem value: %d\n", semctl(sem_set, BLOCK_FULL_SEM_ID, GETVAL));
        struct sembuf wait_op = {
            .sem_num = BLOCK_FULL_SEM_ID,
            .sem_op = -1,
        };
        errno = 0;
        semtimedop(sem_set, &wait_op, 1, &timeout);
        if (errno == EAGAIN) {
            fprintf(stderr, "Server has died\n");
            return;
        }
        
        if (buffer->size > 0) {
            DEBUG_PRINT("Write chunk\n");
            write(fd, buffer->buffer, buffer->size);
        } else {
            DEBUG_PRINT("EOF\n");
            return;
        }

        DEBUG_PRINT("Wake up server\n");
        DEBUG_PRINT("Empty sem value: %d\n", semctl(sem_set, BLOCK_EMPTY_SEM_ID, GETVAL));
        struct sembuf read_op = {
            .sem_num = BLOCK_EMPTY_SEM_ID,
            .sem_op = 1,
        };
        semop(sem_set, &read_op, 1);
    }
}
