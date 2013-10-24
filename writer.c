/*
 * writer.c
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/Time.h>

#include "writer.h"
#include "shm.h"
#include "../demo.h"

#define MODULE_NAME     "Writer Thread"

/* Number of buffers in writer pipe */
#define NUM_WRITER_BUFS         1

#define SHM_DIR2 "/shm/video/v2"

/******************************************************************************
 * writerThrFxn
 ******************************************************************************/
Void *writerThrFxn(Void *arg)
{
    WriterEnv          *envp            = (WriterEnv *) arg;
    Void               *status          = THREAD_SUCCESS;
    FILE               *outFile         = NULL;
    Buffer_Attrs        bAttrs          = Buffer_Attrs_DEFAULT;
    BufTab_Handle       hBufTab         = NULL;
    BufTab_Handle       hsBufTab        = NULL;
    Buffer_Handle       hOutBuf, hsOutBuf;
    Int                 fifoRet;
    Int                 bufIdx;
    Int                 frameCnt        = 0;
    SHM_ST	           *shm_pns         = NULL;
    Char                pathname[40]    = {"\0"};
    Char                filename[25]    = {"\0"};

    /* Open the output video file */
    strcpy(pathname,"/mnt/mmc/video/\0");
    Time_getStr(filename);
    strcat(pathname,filename);
    strcat(pathname,".264");
    outFile = fopen(pathname, "w");
    Dmai_dbg1("pathname is %s\n",pathname);

    if (outFile == NULL) {
        ERR("Failed to open %s for writing\n", envp->videoFile);
        cleanup(THREAD_FAILURE);
    }

    /* Create a share memory for tansporting data to upper layer */
	shm_pns = createShm(SHM_DIR2, envp->outsBufSize);
    Dmai_dbg1("bufsize is %d\n",envp->outsBufSize);

    if (shm_pns == NULL) {
        ERR("Failed to create share memory\n");
        cleanup(THREAD_FAILURE);
    }

    /*
     * Create a table of buffers for communicating buffers to
     * and from the video thread.
     */
    hBufTab = BufTab_create(NUM_WRITER_BUFS, envp->outBufSize, &bAttrs);

    if (hBufTab == NULL) {
        ERR("Failed to allocate contiguous buffers\n");
        cleanup(THREAD_FAILURE);
    }

    /*
     * Create a table of buffers for communicating resized buffers to
     * and from the video thread.
     */
    hsBufTab = BufTab_create(NUM_WRITER_BUFS, envp->outsBufSize, &bAttrs);

    if (hsBufTab == NULL) {
        ERR("Failed to allocate contiguous buffers\n");
        cleanup(THREAD_FAILURE);
    }

    /* Send all buffers to the video thread to be filled with encoded data */
    for (bufIdx = 0; bufIdx < NUM_WRITER_BUFS; bufIdx++) {
        if (Fifo_put(envp->hOutFifo, BufTab_getBuf(hBufTab, bufIdx)) < 0) {
            ERR("Failed to send buffer to video thread\n");
            cleanup(THREAD_FAILURE);
        }

        if (Fifo_put(envp->hOutFifo, BufTab_getBuf(hsBufTab, bufIdx)) < 0) {
            ERR("Failed to send buffer to display thread\n");
            cleanup(THREAD_FAILURE);
        }
    }

    /* Signal that initialization is done and wait for other threads */
    Rendezvous_meet(envp->hRendezvousInit);

    while (TRUE) {
        /* Get an encoded buffer from the video thread */
        fifoRet = Fifo_get(envp->hInFifo, &hOutBuf);

        if (fifoRet < 0) {
            ERR("Failed to get buffer from video thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Did the video thread flush the fifo? */
        if (fifoRet == Dmai_EFLUSH) {
            cleanup(THREAD_SUCCESS);
        }

        /* Get an encoded resized buffer from the video thread */
        fifoRet = Fifo_get(envp->hInFifo, &hsOutBuf);

        if (fifoRet < 0) {
            ERR("Failed to get resized buffer from video thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Did the video thread flush the fifo? */
        if (fifoRet == Dmai_EFLUSH) {
            cleanup(THREAD_SUCCESS);
        }

        /* Store the encoded resize frame to shm */
        writeShm(shm_pns, (char *) Buffer_getUserPtr(hsOutBuf),
                 (unsigned int) Buffer_getNumBytesUsed(hsOutBuf));
        /* Store the encoded frame to disk */
        if (Buffer_getNumBytesUsed(hOutBuf)) {
            if (fwrite(Buffer_getUserPtr(hOutBuf),
                       Buffer_getNumBytesUsed(hOutBuf), 1, outFile) != 1) {
                ERR("Error writing the encoded data to video file\n");
                cleanup(THREAD_FAILURE);
            }
        } else {
            printf("Warning, writer received 0 byte encoded frame\n");
        }

        /* Return buffer to video thread */
        if (Fifo_put(envp->hOutFifo, hOutBuf) < 0) {
            ERR("Failed to send buffer to video thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Return resized buffer to video thread */
        if (Fifo_put(envp->hOutFifo, hsOutBuf) < 0) {
            ERR("Failed to send buffer to video thread\n");
            cleanup(THREAD_FAILURE);
        }

        frameCnt++;
    }

cleanup:
    /* Make sure the other threads aren't waiting for us */
    Rendezvous_force(envp->hRendezvousInit);
    Pause_off(envp->hPauseProcess);
    Fifo_flush(envp->hOutFifo);

    /* Meet up with other threads before cleaning up */
    Rendezvous_meet(envp->hRendezvousCleanup);

    /* Clean up the thread before exiting */
    if (outFile) {
        fclose(outFile);
    }

    if (hBufTab) {
        BufTab_delete(hBufTab);
    }
	
    if (hsBufTab) {
        BufTab_delete(hsBufTab);
    }

	if (shm_pn) {
		deleteShm(shm_pn);
	}

	if (shm_pns) {
		deleteShm(shm_pns);
	}

    return status;
}
