#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include "shm.h"

#include <ti/sdo/dmai/Dmai.h>

#define SEM_READ 0
#define SEM_WRITE 1

#define MODULE_NAME   "Shm"

/******************************************************************************
 * P  V Functions
 ******************************************************************************/
static int p(int semid, unsigned short semnum)
{
    int ret;
    struct sembuf sops = {semnum, -1, IPC_NOWAIT};
	//Dmai_dbg1("semid=%d\n",semid);
    ret = semop(semid, &sops, 1);
	if (ret == -1)
	{
		if (errno != EAGAIN)
            return -1;
        else
            return EAGAIN;
	}
    return 0;
}

static void v(int semid, unsigned short semnum)
{
	int ret;
    struct sembuf sops = {semnum, 1, 0};
	//Dmai_dbg1("semid=%d\n",semid);
    ret = semop(semid, &sops, 1);
	if (ret < 0)
	{
		Dmai_err2("RET %d, error %d\n", ret, errno);
	}
}

/******************************************************************************
 * createSem Function
 ******************************************************************************/
static int createSem(key_t key)
{
    int semid=0;
    union semun arg;

    /* create semaphore set, first for read, second for write */
    if((semid = semget(key , 2, IPC_CREAT|IPC_EXCL|0666)) == -1)
    {
        if(errno == EEXIST)
        {
            errno = 0;
            semid = semget(key , 2, IPC_CREAT|0666);
            return semid;
        }
        else
        {
            Dmai_err0("semget semid failed!\n");
            return semid;
        }
    }

    arg.val = 0;
    if ((semctl(semid, SEM_READ, SETVAL, arg)) == -1)
    {
        Dmai_err0("init semaphore error\n");
        semctl(semid, SEM_READ, IPC_RMID);
        return -1;
    }
    arg.val = 1;
    if ((semctl(semid, SEM_WRITE, SETVAL, arg)) == -1)
    {
        Dmai_err0("init semaphore error\n");
        semctl(semid, SEM_READ, IPC_RMID);
        semctl(semid, SEM_WRITE, IPC_RMID);
        return -1;
    }

    return semid;
    
}

/******************************************************************************
 * deleteSem Function
 ******************************************************************************/
static void deleteSem(int semid)
{
    if(semid == 0)
        return;
    semctl(semid, 0, IPC_RMID);
}

/******************************************************************************
 * createShm Function
 ******************************************************************************/
SHM_ST *createShm(const char* pathName, size_t shmSize)
{
    key_t key;

    /* malloc mm for SHM_ST  */
    SHM_ST *shm_st = (SHM_ST *)malloc(sizeof(SHM_ST));
    if(shm_st == 0)
    {
        Dmai_err0("Malloc SHM_ST failed!\n");
        return 0;
    }
    memset(shm_st, 0, sizeof(SHM_ST));
    shm_st->shmSize = shmSize;

    /* create semaphore for sh mm */
    key = ftok(pathName, 0);
    if((shm_st->semid = createSem(key)) == -1)
    {
        Dmai_err0("create semaphore error\n");
        goto create_failed;
    }

    /* create shm */
    if((shm_st->shmid = shmget(key, shmSize+(sizeof(unsigned int)), IPC_CREAT|0666)) == -1)
    {
        Dmai_err0("create shm error\n");
        goto create_failed;
    }

    if((int)(shm_st->realSize= (unsigned int *)shmat(shm_st->shmid,0,0)) == -1)
    {
        Dmai_err0("attach shm error\n");
        goto create_failed;
    }

    shm_st->shmCon = shm_st->realSize+1;
    shm_st->shmPoint = 0;

    return shm_st;
    
create_failed:
    if (shm_st->shmid != 0)
        shmctl(shm_st->shmid, IPC_RMID, 0);

    if (shm_st->semid != 0)
        deleteSem(shm_st->semid);

    if (shm_st != 0)
        free(shm_st);
    
    return 0;
}

/******************************************************************************
 * deleteShm Function
 ******************************************************************************/
void deleteShm(SHM_ST *shmPtr)
{
    if(shmPtr == 0)
        return;
    shmdt(shmPtr->realSize);
    shmctl(shmPtr->shmid, 0, IPC_RMID);
    deleteSem(shmPtr->semid);
    free(shmPtr);
}

/******************************************************************************
 * writeShm Function
 ******************************************************************************/
unsigned int writeShm(SHM_ST *shmPtr, char *datPtr, unsigned int datSize)
{
    int ret;
    /* get the semaphore */
    ret = p(shmPtr->semid, SEM_WRITE);
    
    if(ret == 0)
    {
      /* write shm */
      if(datSize < shmPtr->shmSize)
      {
         memcpy(shmPtr->shmCon, datPtr, datSize);
         *shmPtr->realSize = datSize;
      }
      else
      {
         memcpy(shmPtr->shmCon, datPtr, shmPtr->shmSize);
         *shmPtr->realSize = shmPtr->shmSize;
      }
  
      /* release semaphore */
      v(shmPtr->semid, SEM_READ);
      Dmai_dbg0("writeShm: write 1 frame\n");
    
      return (*shmPtr->realSize);
    }
    else if(ret == EAGAIN) 
    {
      Dmai_dbg0("Can't get semaphore right now\n");
      return 0;
    }
    else 
    {
      Dmai_err0("Failed to get semaphore\n");
      return -1;
    }
}

