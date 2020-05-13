

#ifndef __SHMMAP_H__
#define __SHMMAP_H__

#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <cstring>

template<class T>
//Create shared memory segment
T* shm_map(int shm_key, int * shm_id) {
    int shm_fd = shmget(shm_key, sizeof(T), IPC_CREAT | 0666);
    if(shm_fd < 0) {
        std::cerr << "shmget failed: " << strerror(errno) << std::endl;
        return nullptr;
    }
    void * shm = shmat(shm_fd, NULL, 0);
    if(shm == (void*) -1) {
        std::cerr << "shmat failed: " << strerror(errno) << std::endl;
        return nullptr;
    }
    *shm_id = shm_fd;
    return (T*) shm;
}

//Delete shared memory segment
void shm_del(int shm_id) {
    int success = shmctl(shm_id, IPC_RMID, 0);
    if(success < 0) {
        std::cerr << "shmctl failed: " << strerror(errno) << std::endl;
    }
}

#endif /* #ifndef __SHMMAP_H__ */