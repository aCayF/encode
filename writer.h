/*
 * writer.h
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#ifndef _WRITER_H
#define _WRITER_H

#include <xdc/std.h>

#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Pause.h>
#include <ti/sdo/dmai/Rendezvous.h>

/* Environment passed when creating the thread */
typedef struct WriterEnv {
    Rendezvous_Handle hRendezvousInit;
    Rendezvous_Handle hRendezvousCleanup;
    Pause_Handle      hPauseProcess;
    Fifo_Handle       hOutFifo;
    Fifo_Handle       hInFifo;
    Char             *videoFile;
    Int32             outBufSize;
    Int32             outsBufSize;
} WriterEnv;

/* Thread function prototype */
extern Void *writerThrFxn(Void *arg);

#endif /* _WRITER_H */
