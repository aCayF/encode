/*
 * capture.h
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#ifndef _CAPTURE_H
#define _CAPTURE_H

#include <xdc/std.h>

#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Pause.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/VideoStd.h>

/* Environment passed when creating the thread */
typedef struct CaptureEnv {
    Rendezvous_Handle hRendezvousInit;
    Rendezvous_Handle hRendezvousCapStd;
    Rendezvous_Handle hRendezvousCleanup;
    Rendezvous_Handle hRendezvousPrime;
    Pause_Handle      hPauseProcess;
    Fifo_Handle       hOutFifo;
    Fifo_Handle       hInFifo;
    VideoStd_Type     videoStd;
    Int32             imageWidth;
    Int32             imageHeight;
    Int32             resizeWidth;
    Int32             resizeHeight;
    Capture_Input     videoInput;
} CaptureEnv;

/* Thread function prototype */
extern Void *captureThrFxn(Void *arg);

#endif /* _CAPTURE_H */
