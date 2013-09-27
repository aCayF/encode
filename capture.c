/*
 * capture.c
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#include <xdc/std.h>
#include <string.h>

#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Pause.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Capture.h>
#include <ti/sdo/dmai/Display.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Framecopy.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Rendezvous.h>

#include "capture.h"
#include "../demo.h"

/* Buffering for the display driver */
#define NUM_DISPLAY_BUFS         3

/* Buffering for the capture driver */
#define NUM_CAPTURE_BUFS         3

/* Number of buffers in the pipe to the capture thread */
/* Note: It needs to match capture.c pipe size */
#define VIDEO_PIPE_SIZE          3

#define NUM_BUFS (NUM_CAPTURE_BUFS + NUM_DISPLAY_BUFS + VIDEO_PIPE_SIZE)

/******************************************************************************
 * captureThrFxn
 ******************************************************************************/
Void *captureThrFxn(Void *arg)
{
    CaptureEnv           *envp     = (CaptureEnv *) arg;
    Void                 *status   = THREAD_SUCCESS;
    Capture_Attrs         cAttrs   = Capture_Attrs_DM365_DEFAULT;
    Display_Attrs         dAttrs   = Display_Attrs_DM365_VID_DEFAULT;
    Framecopy_Attrs       fcAttrs  = Framecopy_Attrs_DEFAULT;
    BufferGfx_Attrs       gfxAttrs = BufferGfx_Attrs_DEFAULT;    
    Capture_Handle        hCapture = NULL;
    Display_Handle        hDisplay = NULL;
    Framecopy_Handle      hFcDisp  = NULL;
    Framecopy_Handle      hFcEnc   = NULL;
    BufTab_Handle         hBufTab  = NULL;    
    Buffer_Handle         hDstBuf, hCapBuf, hDisBuf, hBuf;
    BufferGfx_Dimensions  disDim, capDim;
    VideoStd_Type         videoStd;
    Int32                 width, height, bufSize;
    Int                   fifoRet;
    ColorSpace_Type       colorSpace = ColorSpace_YUV420PSEMI; //ColorSpace_UYVY;
    Int                   bufIdx;
    Bool                  frameCopy = TRUE;

    /* Create capture device driver instance */
    cAttrs.numBufs = NUM_CAPTURE_BUFS;
    cAttrs.videoInput = envp->videoInput;
    cAttrs.videoStd = envp->videoStd;

    if (Capture_detectVideoStd(NULL, &videoStd, &cAttrs) < 0) {
        ERR("Failed to detect video standard, video input connected?\n");
        cleanup(THREAD_FAILURE);
    }

    /* We only support D1 & 720P input */
    if (videoStd != VideoStd_D1_NTSC && videoStd != VideoStd_D1_PAL 
        && videoStd != VideoStd_720P_60) {

        ERR("Need D1/720P input to this demo\n");
        cleanup(THREAD_FAILURE);
    }

    if (envp->imageWidth > 0 && envp->imageHeight > 0) {
        if (VideoStd_getResolution(videoStd, &width, &height) < 0) {
            ERR("Failed to calculate resolution of video standard\n");
            cleanup(THREAD_FAILURE);
        }

        if (width < envp->imageWidth && height < envp->imageHeight) {
            ERR("User resolution (%ldx%ld) larger than detected (%ldx%ld)\n",
                envp->imageWidth, envp->imageHeight, width, height);
            cleanup(THREAD_FAILURE);
        }

        capDim.x          = 0;
        capDim.y          = 0;
        capDim.height     = envp->imageHeight;
        capDim.width      = envp->imageWidth;
        capDim.lineLength = BufferGfx_calcLineLength(width, colorSpace);
    } else {
        /* Calculate the dimensions of a video standard given a color space */
        if (BufferGfx_calcDimensions(videoStd, colorSpace, &capDim) < 0) {
            ERR("Failed to calculate Buffer dimensions\n");
            cleanup(THREAD_FAILURE);
        }

        envp->imageWidth  = capDim.width;
        envp->imageHeight = capDim.height;
    }
    /* If it is not component capture with 720P resolution then use framecopy as
       there is a size mismatch between capture, display and video buffers. */
    if((envp->imageWidth == VideoStd_720P_WIDTH) && 
        (envp->imageHeight == VideoStd_720P_HEIGHT)) {
        frameCopy = FALSE;
    }
    /* Calculate the dimensions of a video standard given a color space */
    envp->imageWidth  = capDim.width;
    envp->imageHeight = capDim.height;

    if(frameCopy == FALSE) {
        gfxAttrs.dim.height = capDim.height;
        gfxAttrs.dim.width = capDim.width;
        gfxAttrs.dim.lineLength = ((Int32)((BufferGfx_calcLineLength(gfxAttrs.dim.width, colorSpace)+31)/32))*32;
        gfxAttrs.dim.x = 0;
        gfxAttrs.dim.y = 0;
        if (colorSpace ==  ColorSpace_YUV420PSEMI) {
            bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 3 / 2;
        } else {
            bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 2;
        }

        /* Create a table of buffers to use with the device drivers */
        gfxAttrs.colorSpace = colorSpace;
        hBufTab = BufTab_create(NUM_BUFS, bufSize,
                                BufferGfx_getBufferAttrs(&gfxAttrs));
        if (hBufTab == NULL) {
            ERR("Failed to create buftab\n");
            cleanup(THREAD_FAILURE);
        }
    } else {
        gfxAttrs.dim = capDim;
    }
    /* Update global data for user interface */
    gblSetImageWidth(envp->imageWidth);
    gblSetImageHeight(envp->imageHeight);

    /* Report the video standard and image size back to the main thread */
    Rendezvous_meet(envp->hRendezvousCapStd);

    if(envp->videoStd == VideoStd_720P_60) {
        cAttrs.videoStd = VideoStd_720P_30;
    } else {
        cAttrs.videoStd = envp->videoStd;    
    }
    cAttrs.numBufs    = NUM_CAPTURE_BUFS;    
    cAttrs.colorSpace = colorSpace;
    cAttrs.captureDimension = &gfxAttrs.dim;
    /* Create the capture device driver instance */
    hCapture = Capture_create(hBufTab, &cAttrs);

    if (hCapture == NULL) {
        ERR("Failed to create capture device\n");
        cleanup(THREAD_FAILURE);
    }

    /* Create display device driver instance */
    dAttrs.videoStd = envp->videoStd;
    if ( (dAttrs.videoStd == VideoStd_CIF) ||
        (dAttrs.videoStd == VideoStd_SIF_NTSC) ||
        (dAttrs.videoStd == VideoStd_SIF_PAL) ||
        (dAttrs.videoStd == VideoStd_VGA) ||
        (dAttrs.videoStd == VideoStd_D1_NTSC) ||        
        (dAttrs.videoStd == VideoStd_D1_PAL) ) {
        dAttrs.videoOutput = Display_Output_COMPOSITE; //plc
    } else {
        dAttrs.videoOutput = Display_Output_COMPONENT;
    }    
    dAttrs.numBufs    = NUM_DISPLAY_BUFS;
    dAttrs.colorSpace = colorSpace;
    hDisplay = Display_create(hBufTab, &dAttrs);

    if (hDisplay == NULL) {
        ERR("Failed to create display device\n");
        cleanup(THREAD_FAILURE);
    }
    if (frameCopy == TRUE) {
        /* Get a buffer from the video thread */
        fifoRet = Fifo_get(envp->hInFifo, &hDstBuf);

        if (fifoRet < 0) {
            ERR("Failed to get buffer from video thread\n");
            cleanup(THREAD_FAILURE);
        }

        /* Did the video thread flush the fifo? */
        if (fifoRet == Dmai_EFLUSH) {
            cleanup(THREAD_SUCCESS);
        }

        /* Create frame copy module for display buffer */
        fcAttrs.accel = TRUE;
        hFcDisp = Framecopy_create(&fcAttrs);

        if (hFcDisp == NULL) {
            ERR("Failed to create frame copy job\n");
            cleanup(THREAD_FAILURE);
        }

        /* Configure frame copy jobs */
        if (Framecopy_config(hFcDisp,
                             BufTab_getBuf(Capture_getBufTab(hCapture), 0),
                             BufTab_getBuf(Display_getBufTab(hDisplay), 0)) < 0) {
            ERR("Failed to configure frame copy job\n");
            cleanup(THREAD_FAILURE);
        }

        /* Create frame copy module for encode buffer */
        hFcEnc = Framecopy_create(&fcAttrs);

        if (hFcEnc == NULL) {
            ERR("Failed to create frame copy job\n");
            cleanup(THREAD_FAILURE);
        }

        if (Framecopy_config(hFcEnc,
                             BufTab_getBuf(Capture_getBufTab(hCapture), 0),
                             hDstBuf) < 0) {
            ERR("Failed to configure frame copy job\n");
            cleanup(THREAD_FAILURE);
        }
    }else {
        for (bufIdx = 0; bufIdx < VIDEO_PIPE_SIZE; bufIdx++) {
            /* Queue the video buffers for main thread processing */
            hBuf = BufTab_getFreeBuf(hBufTab);
            if (hBuf == NULL) {
                ERR("Failed to fill video pipeline\n");
                cleanup(THREAD_FAILURE);            
            }
            /* Send buffer to video thread for encoding */
            if (Fifo_put(envp->hOutFifo, hBuf) < 0) {
                ERR("Failed to send buffer to video thread\n");
                cleanup(THREAD_FAILURE);
            }
        }
    }
    /* Signal that initialization is done and wait for other threads */
    Rendezvous_meet(envp->hRendezvousInit);

    while (!gblGetQuit()) {
        /* Pause processing? */
        Pause_test(envp->hPauseProcess);

        /* Capture a frame */
        if (Capture_get(hCapture, &hCapBuf) < 0) {
            ERR("Failed to get capture buffer\n");
            cleanup(THREAD_FAILURE);
        }

        /* Get a buffer from the display device */
        if (Display_get(hDisplay, &hDisBuf) < 0) {
            ERR("Failed to get display buffer\n");
            cleanup(THREAD_FAILURE);
        }

        if (frameCopy == TRUE) {

            /* Get a buffer from the video thread */
            fifoRet = Fifo_get(envp->hInFifo, &hDstBuf);

            if (fifoRet < 0) {
                ERR("Failed to get buffer from video thread\n");
                cleanup(THREAD_FAILURE);
            }

            /* Did the video thread flush the fifo? */
            if (fifoRet == Dmai_EFLUSH) {
                cleanup(THREAD_SUCCESS);
            }

            /* Copy the captured buffer to the encode buffer */
            if (Framecopy_execute(hFcEnc, hCapBuf, hDstBuf) < 0) {
                ERR("Failed to execute frame copy job\n");
                cleanup(THREAD_FAILURE);
            }

            /* Send buffer to video thread for encoding */
            if (Fifo_put(envp->hOutFifo, hDstBuf) < 0) {
                ERR("Failed to send buffer to video thread\n");
                cleanup(THREAD_FAILURE);
            }            
        }else {
            /* Send buffer to video thread for encoding */
            if (Fifo_put(envp->hOutFifo, hCapBuf) < 0) {
                ERR("Failed to send buffer to video thread\n");
                cleanup(THREAD_FAILURE);
            }
        }
        if (frameCopy == TRUE) {
            /* Copy the captured buffer to the display buffer */
            if (Framecopy_execute(hFcDisp, hCapBuf, hDisBuf) < 0) {
                ERR("Failed to execute frame copy job\n");
                cleanup(THREAD_FAILURE);
            }

            /* Release display buffer to the display device driver */
            if (Display_put(hDisplay, hDisBuf) < 0) {
                ERR("Failed to put display buffer\n");
                cleanup(THREAD_FAILURE);
            }            
        } else {
            /* Release display buffer to the display device driver */
            if (Display_put(hDisplay, hCapBuf) < 0) {
                ERR("Failed to put display buffer\n");
                cleanup(THREAD_FAILURE);
            }
        }

        if (frameCopy == TRUE) {
            /* Return the buffer to the capture driver */
            if (Capture_put(hCapture, hCapBuf) < 0) {
                ERR("Failed to put capture buffer\n");
                cleanup(THREAD_FAILURE);
            }
        } else {
            /* Return the buffer to the capture driver */
            if (Capture_put(hCapture, hDstBuf) < 0) {
                ERR("Failed to put capture buffer\n");
                cleanup(THREAD_FAILURE);
            }
        }

        /* Incremement statistics for the user interface */
        gblIncFrames();

    }

cleanup:
    /* Make sure the other threads aren't waiting for us */
    Rendezvous_force(envp->hRendezvousCapStd);
    Rendezvous_force(envp->hRendezvousInit);
    Pause_off(envp->hPauseProcess);
    Fifo_flush(envp->hOutFifo);

    /* Meet up with other threads before cleaning up */
    Rendezvous_meet(envp->hRendezvousCleanup);

    /* Clean up the thread before exiting */
    if (hFcDisp) {
        Framecopy_delete(hFcDisp);
    }

    if (hFcEnc) {
        Framecopy_delete(hFcEnc);
    }

    if (hDisplay) {
        Display_delete(hDisplay);
    }

    if (hCapture) {
        Capture_delete(hCapture);
    }
    
    /* Clean up the thread before exiting */
    if (hBufTab) {
        BufTab_delete(hBufTab);
    }

    return status;
}

