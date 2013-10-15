/**************************
 *  File name: shm.h
 *  Descraption: 
 *      used to share memory between app
 *
 *  Created by zcy
 *  06-13-2010
 **************************/

#ifndef _SHM_H_
#define _SHM_H_

/**************************
 *  Headers
 **************************/

/**************************
 *  Structs
 **************************/
typedef struct
{
    int shmid;
    int semid;
    unsigned int shmSize;
    unsigned int *realSize;
    unsigned int shmPoint;
    unsigned int *shmCon;
} SHM_ST;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/**************************
 *  Functions
 **************************/

SHM_ST *createShm(const char* pathName, size_t shmSize);
void deleteShm(SHM_ST *shmPtr);
unsigned int writeShm(SHM_ST *shmPtr, char *datPtr, unsigned int datSize);
unsigned int readShm(SHM_ST *shmPtr, char *datPtr, unsigned int size);

#endif

