/*
 * video.c
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#include <xdc/std.h>

#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Pause.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Venc1.h>

#include "video.h"
#include "../demo.h"

#ifndef YUV_420SP
#define YUV_420SP 256
#endif 

/* Number of buffers in the pipe to the capture thread */
/* Note: It needs to match capture.c pipe size */
#define VIDEO_PIPE_SIZE           3

/******************************************************************************
 * videoThrFxn
 ******************************************************************************/
Void *videoThrFxn(Void *arg)
{
    VideoEnv               *envp                = (VideoEnv *) arg;
    Void                   *status              = THREAD_SUCCESS;
    VIDENC1_Params          defaultParams       = Venc1_Params_DEFAULT;
    VIDENC1_DynamicParams   defaultDynParams    = Venc1_DynamicParams_DEFAULT;
    BufferGfx_Attrs         gfxAttrs            = BufferGfx_Attrs_DEFAULT;
    Venc1_Handle            hVe1                = NULL;
    Engine_Handle           hEngine             = NULL;
    BufTab_Handle           hBufTab             = NULL;
    Int                     frameCnt            = 0;
    Buffer_Handle           hCapBuf, hDstBuf;
    VIDENC1_Params         *params;
    VIDENC1_DynamicParams  *dynParams;
    Int                     fifoRet;
    Int                     bufIdx;
    ColorSpace_Type       colorSpace = ColorSpace_YUV420PSEMI;
    Bool                    localBufferAlloc = TRUE;

    /* Open the codec engine */
    hEngine = Engine_open(envp->engineName, NULL, NULL);

    if (hEngine == NULL) {
        ERR("Failed to open codec engine %s\n", envp->engineName);
        cleanup(THREAD_FAILURE);
    }

    /* In case of 720P resolution the video buffer will be allocated
       by capture thread. */
    if((envp->imageWidth == VideoStd_720P_WIDTH) && 
        (envp->imageHeight == VideoStd_720P_HEIGHT)) {
        localBufferAlloc = FALSE;
    } 
    
    /* Use supplied params if any, otherwise use defaults */
    params = envp->params ? envp->params : &defaultParams;
    dynParams = envp->dynParams ? envp->dynParams : &defaultDynParams;

    /* Set up codec parameters */
    params->maxWidth          = envp->imageWidth;
    params->maxHeight         = envp->imageHeight;
    params->encodingPreset    = XDM_HIGH_SPEED;
    if (colorSpace ==  ColorSpace_YUV420PSEMI) { 
        params->inputChromaFormat = XDM_YUV_420SP;
    } else {
        params->inputChromaFormat = XDM_YUV_422ILE;
    }
    params->reconChromaFormat = XDM_YUV_420SP;
    params->maxFrameRate      = envp->videoFrameRate;

    /* Set up codec parameters depending on bit rate */
    if (envp->videoBitRate < 0) {
        /* Variable bit rate */
        params->rateControlPreset = IVIDEO_NONE;

        /*
         * If variable bit rate use a bogus bit rate value (> 0)
         * since it will be ignored.
         */
        params->maxBitRate        = 0;
    }
    else {
        /* Constant bit rate */
        params->rateControlPreset = IVIDEO_STORAGE;
        params->maxBitRate        = envp->videoBitRate;
    }

    dynParams->targetBitRate   = params->maxBitRate;
    dynParams->inputWidth      = params->maxWidth;
    dynParams->inputHeight     = params->maxHeight;    
    dynParams->refFrameRate    = params->maxFrameRate;
    dynParams->targetFrameRate = params->maxFrameRate;
    dynParams->interFrameInterval = 0;
    
    /* Create the video encoder */
    hVe1 = Venc1_create(hEngine, envp->videoEncoder, params, dynParams);

    if (hVe1 == NULL) {
        ERR("Failed to create video encoder: %s\n", envp->videoEncoder);
        cleanup(THREAD_FAILURE);
    }

    /* Store the output buffer size in the environment */
    envp->outBufSize = Venc1_getOutBufSize(hVe1);

    /* Signal that the codec is created and output buffer size available */
    Rendezvous_meet(envp->hRendezvousWriter);
    if (localBufferAlloc == TRUE) {
        gfxAttrs.colorSpace = colorSpace;
        gfxAttrs.dim.width  = envp->imageWidth;
        gfxAttrs.dim.height = envp->imageHeight;
        gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(gfxAttrs.dim.width,
                                                           gfxAttrs.colorSpace);
        /*
         * Ask the codec how much input data it needs and create a table of
         * buffers with this size.
         */
        hBufTab = BufTab_create(VIDEO_PIPE_SIZE, Venc1_getInBufSize(hVe1),
                                BufferGfx_getBufferAttrs(&gfxAttrs));

        if (hBufTab == NULL) {
            ERR("Failed to allocate contiguous buffers\n");
            cleanup(THREAD_FAILURE);
        }

        /* Send buffers to the capture thread to be ready for main loop */
        for (bufIdx = 0; bufIdx < VIDEO_PIPE_SIZE; bufIdx++) {
            if (Fifo_put(envp->hCaptureInFifo,
                         BufTab_getBuf(hBufTab, bufIdx)) < 0) {
                ERR("Failed to send buffer to display thread\n");
                cleanup(THREAD_FAILURE);
            }
        }
    } else {
        /* Send buffers to the capture thread to be ready for main loop */
        for (bufIdx = 0; bufIdx < VIDEO_PIPE_SIZE; bufIdx++) {
            if ((Fifo_get(envp->hCaptureOutFifo, &hCapBuf)) < 0) {
                ERR("Failed to get buffer from capture thread\n");
                cleanup(THREAD_FAILURE);
            }
            if (fifoRet < 0) {
                ERR("Failed to get buffer from video thread\n");
                cleanup(THREAD_FAILURE);
            }

            /* Did the capture thread flush the fifo? */
            if (fifoRet == Dmai_EFLUSH) {
                cleanup(THREAD_SUCCESS);
            }
            /* Return buffer to capture thread */
            if (Fifo_put(envp->hCaptureInFifo, hCapBuf) < 0) {
                ERR("Failed to send buffer to display thread\n");
                cleanup(THREAD_FAILURE);
            }
        }
    }
    /* Signal that initialization is done and wait for other threads */
    Rendezvous_meet(envp->hRendezvousInit);
    
    while (!gblGetQuit()) {
        /* Pause processing? */
        Pause_test(envp->hPauseProcess);

        /* Get a buffer to encode from the capture thread */
        fifoRet = Fifo_get(envp->hCaptureOutFifo, &hCapBuf);

        if (fifoRet < 0) {
            ERR("Failed to get buffer from video thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Did the capture thread flush the fifo? */
        if (fifoRet == Dmai_EFLUSH) {
            cleanup(THREAD_SUCCESS);
        }

        /* Get a buffer to encode to from the writer thread */
        fifoRet = Fifo_get(envp->hWriterOutFifo, &hDstBuf);

        if (fifoRet < 0) {
            ERR("Failed to get buffer from writer thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Did the writer thread flush the fifo? */
        if (fifoRet == Dmai_EFLUSH) {
            cleanup(THREAD_SUCCESS);
        }

        /* Make sure the whole buffer is used for input */
        BufferGfx_resetDimensions(hCapBuf);

        /* Decode the video buffer */
        if (Venc1_process(hVe1, hCapBuf, hDstBuf) < 0) {
            ERR("Failed to encode video buffer\n");
            cleanup(THREAD_FAILURE);
        }
        /* Send encoded buffer to writer thread for filesystem output */
        if (Fifo_put(envp->hWriterInFifo, hDstBuf) < 0) {
            ERR("Failed to send buffer to writer thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Return buffer to capture thread */
        if (Fifo_put(envp->hCaptureInFifo, hCapBuf) < 0) {
            ERR("Failed to send buffer to display thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Increment statistics for the user interface */
        gblIncVideoBytesProcessed(Buffer_getNumBytesUsed(hDstBuf));

        frameCnt++;
    }

cleanup:
    /* Make sure the other threads aren't waiting for us */
    Rendezvous_force(envp->hRendezvousInit);
    Rendezvous_force(envp->hRendezvousWriter);
    Pause_off(envp->hPauseProcess);
    Fifo_flush(envp->hWriterInFifo);
    Fifo_flush(envp->hCaptureInFifo);

    /* Make sure the other threads aren't waiting for init to complete */
    Rendezvous_meet(envp->hRendezvousCleanup);

    /* Clean up the thread before exiting */
    if (hBufTab) {
        BufTab_delete(hBufTab);
    }

    if (hVe1) {
        Venc1_delete(hVe1);
    }

    if (hEngine) {
        Engine_close(hEngine);
    }

    return status;
}

