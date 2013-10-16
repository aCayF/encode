/*
 * main.c
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <strings.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>

#include <xdc/std.h>

#include <ti/sdo/ce/trace/gt.h>
#include <ti/sdo/ce/CERuntime.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Pause.h>
#include <ti/sdo/dmai/Sound.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Capture.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Rendezvous.h>

#include <ti/sdo/fc/rman/rman.h>

#include "video.h"
#include "capture.h"
#include "writer.h"
#include "speech.h"
#include "../ctrl.h"
#include "../demo.h"
#include "../ui.h"

/* The levels of initialization */
#define LOGSINITIALIZED         0x1
#define DISPLAYTHREADCREATED    0x20
#define CAPTURETHREADCREATED    0x40
#define WRITERTHREADCREATED     0x80
#define VIDEOTHREADCREATED      0x100
#define SPEECHTHREADCREATED     0x200

/* Thread priorities */
#define WRITER_THREAD_PRIORITY  sched_get_priority_max(SCHED_FIFO) - 3
#define SPEECH_THREAD_PRIORITY  sched_get_priority_max(SCHED_FIFO) - 2
#define VIDEO_THREAD_PRIORITY   sched_get_priority_max(SCHED_FIFO) - 1
#define CAPTURE_THREAD_PRIORITY sched_get_priority_max(SCHED_FIFO)

typedef struct Args {
    Display_Output          displayOutput;
    VideoStd_Type  videoStd;
    Char          *videoStdString;
    Sound_Input    soundInput;
    Capture_Input  videoInput;
    Char          *speechFile;
    Char          *videoFile;
    Codec         *speechEncoder;
    Codec         *videoEncoder;
    Int32          imageWidth;
    Int32          imageHeight;
    Int            videoBitRate;
    Int            keyboard;
    Int            time;
    Int            osd;
    Int            interface;
} Args;

#define DEFAULT_ARGS \
    { Display_Output_LCD, VideoStd_D1_NTSC, "D1 NTSC", Sound_Input_MIC,Capture_Input_COMPOSITE, NULL, NULL, NULL, NULL, 0, 0, -1, FALSE, FOREVER, FALSE, FALSE }

/* Global variable declarations for this application */
GlobalData gbl = GBL_DATA_INIT;

/******************************************************************************
 * Signal handler
 ******************************************************************************/
Void signalHandler(int sig)
{
    signal(SIGINT, SIG_DFL);
    gblSetQuit();
}

/******************************************************************************
 * getCodec
 ******************************************************************************/
static Codec *getCodec(Char *extension, Codec *codecs)
{
    Codec *codec = NULL;
    Int i, j;

    i = 0;
    while (codecs[i].codecName) {
        j = 0;
        while (codecs[i].fileExtensions[j]) {
            if (strcmp(extension, codecs[i].fileExtensions[j]) == 0) {
                codec = &codecs[i];
            }
            j++;
        }
        i++;
    }

    return codec;
}

/******************************************************************************
 * usage
 ******************************************************************************/
static void usage(void)
{
    fprintf(stderr, "Usage: encode [options]\n\n"
      "Options:\n"
      "-s | --speechfile       Speech file to record to\n"
      "-O | --display_output   Video output to use (see below).\n"
      "-v | --videofile        Video file to record to\n"
      "-y | --display_standard Video standard to use for display (see below).\n"
      "-r | --resolution       Video resolution ('width'x'height')\n"
      "                        [video standard default]\n"
      "-b | --videobitrate     Bit rate to encode video at [variable]\n"
      "-x | --svideo           Use s-video instead of composite video \n"
      "                        input [off]\n"
      "-l | --linein           Use linein for encoding sound instead of mic \n"
      "                        [off]\n"
      "-k | --keyboard         Enable keyboard interface [off]\n"
      "-t | --time             Number of seconds to run the demo [infinite]\n"
      "-o | --osd              Show demo data on an OSD [off]\n"
      "-i | --interface        Launch the demo interface when exiting [off]\n"
      "-h | --help             Print this message\n\n"
      "Video standards available:\n"
      "\t1\tD1 @ 30 fps (NTSC) [Default]\n"
      "\t2\tD1 @ 25 fps (PAL)\n"
      "\t3\t720P @ 60 fps\n"
      "\t4\t720P @ 50 fps\n"
      "\t5\t1080I @ 30 fps [Not supported for DM365]\n"
      "\t6\t1080I @ 25 fps [Not supported for DM365]\n"
      "Video outputs available:\n"
      "\tcomposite\n"
      "\tlcd [Default]\n"
      "\tcomponent (Only 720P available)\n"
      "You must supply at least a video or a speech file or both\n"
      "with appropriate extensions for the file formats.\n\n");
}

/******************************************************************************
 * parseArgs
 ******************************************************************************/
static Void parseArgs(Int argc, Char *argv[], Args *argsp)
{
    const Char shortOptions[] = "s:O:v:y:r:b:xlkt:oih";
    const struct option longOptions[] = {
        {"speechfile",       required_argument, NULL, 's'},
        {"display_output",   required_argument, NULL, 'O'},
        {"videofile",        required_argument, NULL, 'v'},
        {"display_standard", required_argument, NULL, 'y'},
        {"resolution",       required_argument, NULL, 'r'},
        {"videobitrate",     required_argument, NULL, 'b'},
        {"svideo",           no_argument,       NULL, 'x'},
        {"linein",           no_argument,       NULL, 'l'},
        {"keyboard",         no_argument,       NULL, 'k'},
        {"time",             required_argument, NULL, 't'},
        {"osd",              no_argument,       NULL, 'o'},
        {"interface",        no_argument,       NULL, 'i'},
        {"help",             no_argument,       NULL, 'h'},
        {0, 0, 0, 0}
    };

    Int     index;
    Int     c;
    Char    *extension;

    for (;;) {
        c = getopt_long(argc, argv, shortOptions, longOptions, &index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                break;

            case 's':
                extension = rindex(optarg, '.');
                if (extension == NULL) {
                    fprintf(stderr, "Speech file without extension: %s\n",
                            optarg);
                    exit(EXIT_FAILURE);
                }

                argsp->speechEncoder =
                    getCodec(extension, engine->speechEncoders);

                if (!argsp->speechEncoder) {
                    fprintf(stderr, "Unknown speech file extension: %s\n",
                            extension);
                    exit(EXIT_FAILURE);
                }
                argsp->speechFile = optarg;

                break;

            case 'v':
                extension = rindex(optarg, '.');
                if (extension == NULL) {
                    fprintf(stderr, "Video file without extension: %s\n",
                            optarg);
                    exit(EXIT_FAILURE);
                }

                argsp->videoEncoder =
                    getCodec(extension, engine->videoEncoders);

                if (!argsp->videoEncoder) {
                    fprintf(stderr, "Unknown video file extension: %s\n",
                            extension);
                    exit(EXIT_FAILURE);
                }
                argsp->videoFile = optarg;

                break;

            case 'y':
                switch (atoi(optarg)) {
                    case 1:
                        argsp->videoStd = VideoStd_D1_NTSC;
                        argsp->videoStdString = "D1 NTSC";
                        break;
                    case 2:
                        argsp->videoStd = VideoStd_D1_PAL;
                        argsp->videoStdString = "D1 PAL";
                        break;
                    case 3:
                        argsp->videoStd = VideoStd_720P_60;
                        argsp->videoStdString = "720P 60Hz";
                        break;
                    case 4:
                        argsp->videoStd = VideoStd_720P_50;
                        argsp->videoStdString = "720P 50Hz";
                        break;
                    case 5:
                        argsp->videoStd = VideoStd_1080I_30;
                        argsp->videoStdString = "1080I 30Hz";
                        break;
                    case 6:
                        argsp->videoStd = VideoStd_1080I_25;
                        argsp->videoStdString = "1080I 25Hz";
                        break;
                    default:
                        fprintf(stderr, "Unknown display resolution\n");
                        usage();
                        exit(EXIT_FAILURE);
                }
                break;

            case 'r':
            {
                Int32 imageWidth, imageHeight;

                if (sscanf(optarg, "%ldx%ld", &imageWidth,
                                              &imageHeight) != 2) {
                    fprintf(stderr, "Invalid resolution supplied (%s)\n",
                            optarg);
                    usage();
                    exit(EXIT_FAILURE);
                }

                /* Sanity check resolution */
                if (imageWidth < 2UL || imageHeight < 2UL ||
                    imageWidth > VideoStd_1080I_WIDTH ||
                    imageHeight > VideoStd_1080I_HEIGHT) {
                    fprintf(stderr, "Video resolution must be between %dx%d "
                            "and %dx%d\n", 2, 2, VideoStd_1080I_WIDTH,
                            VideoStd_1080I_HEIGHT);
                    exit(EXIT_FAILURE);
                }

                /* Width and height must be multiple of 16 */
                argsp->imageWidth  = imageWidth & ~0xf;
                argsp->imageHeight = imageHeight & ~0xf;
                break;
            }

            case 'b':
                argsp->videoBitRate = atoi(optarg);
                break;

            case 'x':
                argsp->videoInput = Capture_Input_SVIDEO;
                break;

            case 'l':
                argsp->soundInput = Sound_Input_LINE;
                break;

            case 'k':
                argsp->keyboard = TRUE;
                break;

            case 't':
                argsp->time = atoi(optarg);
                break;

            case 'o':
                argsp->osd = TRUE;
                break;

            case 'i':
                argsp->interface = TRUE;
                break;

            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 'O':
                if (strcmp(optarg, "component") == 0) {
                    argsp->displayOutput = Display_Output_COMPONENT;
                } else if (strcmp(optarg, "composite") == 0) {
                    argsp->displayOutput = Display_Output_COMPOSITE;
                } else if (strcmp(optarg, "lcd") == 0) {
                    argsp->displayOutput = Display_Output_LCD;
                } else {
                    fprintf(stderr, "Unknown video output: %s\n", optarg);
                    usage();
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    if (argsp->videoInput != Capture_Input_SVIDEO) {
        if (argsp->videoStd == VideoStd_D1_NTSC || argsp->videoStd == VideoStd_D1_PAL ) {
            argsp->videoInput = Capture_Input_COMPOSITE;
        } else {
            argsp->videoInput = Capture_Input_COMPONENT;
        }
    }

    /* Need at least one file to decode and only one sound file */
    if (!argsp->videoFile && !argsp->speechFile) {
        usage();
        exit(EXIT_FAILURE);
    }
}

/******************************************************************************
 * uiSetup
 ******************************************************************************/
static UI_Handle uiSetup(Args *argsp)
{
    UI_Attrs  uiAttrs;
    UI_Handle hUI;

    /* Create the user interface */
    uiAttrs.osd = argsp->osd;
    uiAttrs.videoStd = argsp->videoStd;
    uiAttrs.displayOutput = argsp->displayOutput;

    hUI = UI_create(&uiAttrs);

    if (hUI == NULL) {
        ERR("Failed to create UI\n");
        return NULL;
    }

    /* Initialize values */
    UI_updateValue(hUI, UI_Value_DemoName, "Encode");
    UI_updateValue(hUI, UI_Value_DisplayType, argsp->videoStdString);

    if (argsp->videoEncoder) {
        UI_updateValue(hUI, UI_Value_VideoCodec, argsp->videoEncoder->uiString);
    }
    else {
        UI_updateValue(hUI, UI_Value_VideoCodec, "N/A");
    }

    if (argsp->speechEncoder) {
        UI_updateValue(hUI, UI_Value_SoundCodec,argsp->speechEncoder->uiString);
    }
    else {
        UI_updateValue(hUI, UI_Value_SoundCodec, "N/A");
    }

    UI_updateValue(hUI, UI_Value_ImageResolution, "N/A");
    UI_updateValue(hUI, UI_Value_SoundFrequency, "N/A");

    /* Initialize the user interface */
    UI_init(hUI);

    return hUI;
}

/******************************************************************************
 * main
 ******************************************************************************/
Int main(Int argc, Char *argv[])
{
    Args                args                = DEFAULT_ARGS;
    Uns                 initMask            = 0;
    Int                 status              = EXIT_SUCCESS;
    Pause_Attrs         pAttrs              = Pause_Attrs_DEFAULT;
    Rendezvous_Attrs    rzvAttrs            = Rendezvous_Attrs_DEFAULT;
    Fifo_Attrs          fAttrs              = Fifo_Attrs_DEFAULT;
    Rendezvous_Handle   hRendezvousCapStd   = NULL;
    Rendezvous_Handle   hRendezvousInit     = NULL;
    Rendezvous_Handle   hRendezvousWriter   = NULL;
    Rendezvous_Handle   hRendezvousCleanup  = NULL;
    Pause_Handle        hPauseProcess       = NULL;
    UI_Handle           hUI                 = NULL;
    struct sched_param  schedParam;
    pthread_t           captureThread;
    pthread_t           writerThread;
    pthread_t           videoThread;
    pthread_t           speechThread;
    CaptureEnv          captureEnv;
    WriterEnv           writerEnv;
    VideoEnv            videoEnv;
    SpeechEnv           speechEnv;
    CtrlEnv             ctrlEnv;
    Int                 numThreads;
    pthread_attr_t      attr;
    Void               *ret;

    /* Zero out the thread environments */
    Dmai_clear(captureEnv);
    Dmai_clear(writerEnv);
    Dmai_clear(videoEnv);
    Dmai_clear(speechEnv);
    Dmai_clear(ctrlEnv);

    /* Parse the arguments given to the app and set the app environment */
    parseArgs(argc, argv, &args);

    printf("Encode demo started.\n");

    /* Initialize the mutex which protects the global data */
    pthread_mutex_init(&gbl.mutex, NULL);

    /* Set the priority of this whole process to max (requires root) */
    setpriority(PRIO_PROCESS, 0, -20);

    /* Initialize Codec Engine runtime */
    CERuntime_init();

    /* Initialize signal handler for SIGINT */
    signal(SIGINT, signalHandler);
    
    /* Initialize Davinci Multimedia Application Interface */
    Dmai_init();

    initMask |= LOGSINITIALIZED;

    /* Set up the user interface */
    hUI = uiSetup(&args);

    if (hUI == NULL) {
        cleanup(EXIT_FAILURE);
    }

    /* Create the Pause object */
    hPauseProcess = Pause_create(&pAttrs);

    if (hPauseProcess == NULL) {
        ERR("Failed to create Pause object\n");
        cleanup(EXIT_FAILURE);
    }

    /* Determine the number of threads needing synchronization */
    numThreads = 1;

    if (args.videoFile) {
        numThreads += 3;
    }

    if (args.speechFile) {
        numThreads += 1;
    }
    /* Create the objects which synchronizes the thread init and cleanup */
    hRendezvousCapStd  = Rendezvous_create(2, &rzvAttrs);
    hRendezvousInit = Rendezvous_create(numThreads, &rzvAttrs);
    hRendezvousCleanup = Rendezvous_create(numThreads, &rzvAttrs);
    hRendezvousWriter = Rendezvous_create(2, &rzvAttrs);

    if (hRendezvousCapStd  == NULL || hRendezvousInit == NULL || 
        hRendezvousCleanup == NULL || hRendezvousWriter == NULL) {
        ERR("Failed to create Rendezvous objects\n");
        cleanup(EXIT_FAILURE);
    }

    /* Initialize the thread attributes */
    if (pthread_attr_init(&attr)) {
        ERR("Failed to initialize thread attrs\n");
        cleanup(EXIT_FAILURE);
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        ERR("Failed to set schedule inheritance attribute\n");
        cleanup(EXIT_FAILURE);
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        ERR("Failed to set FIFO scheduling policy\n");
        cleanup(EXIT_FAILURE);
    }

    /* Create the video threads if a file name is supplied */
    if (args.videoFile) {
        /* Create the capture fifos */
        captureEnv.hInFifo = Fifo_create(&fAttrs);
        captureEnv.hOutFifo = Fifo_create(&fAttrs);

        if (captureEnv.hInFifo == NULL || captureEnv.hOutFifo == NULL) {
            ERR("Failed to open display fifos\n");
            cleanup(EXIT_FAILURE);
        }

        /* Set the capture thread priority */
        schedParam.sched_priority = CAPTURE_THREAD_PRIORITY;
        if (pthread_attr_setschedparam(&attr, &schedParam)) {
            ERR("Failed to set scheduler parameters\n");
            cleanup(EXIT_FAILURE);
        }

        /* Create the capture thread */
        captureEnv.hRendezvousInit    = hRendezvousInit;
        captureEnv.hRendezvousCapStd  = hRendezvousCapStd;
        captureEnv.hRendezvousCleanup = hRendezvousCleanup;
        captureEnv.hPauseProcess      = hPauseProcess;
        captureEnv.videoStd           = args.videoStd;
        captureEnv.videoInput         = args.videoInput;
        captureEnv.imageWidth         = args.imageWidth;
        captureEnv.imageHeight        = args.imageHeight;
        /* TODO */
        VideoStd_getResolution(VideoStd_CIF, &captureEnv.resizeWidth,
                                             &captureEnv.resizeHeight);

        if (pthread_create(&captureThread, &attr, captureThrFxn, &captureEnv)) {
            ERR("Failed to create capture thread\n");
            cleanup(EXIT_FAILURE);
        }

        initMask |= CAPTURETHREADCREATED;

        /*
         * Once the capture thread has detected the video standard, make it
         * available to other threads. The capture thread will set the
         * resolution of the buffer to encode in the environment (derived
         * from the video standard if the user hasn't passed a resolution).
         */
        Rendezvous_meet(hRendezvousCapStd);

        /* Create the writer fifos */
        writerEnv.hInFifo = Fifo_create(&fAttrs);
        writerEnv.hOutFifo = Fifo_create(&fAttrs);

        if (writerEnv.hInFifo == NULL || writerEnv.hOutFifo == NULL) {
            ERR("Failed to open display fifos\n");
            cleanup(EXIT_FAILURE);
        }

        /* Set the video thread priority */
        schedParam.sched_priority = VIDEO_THREAD_PRIORITY;
        if (pthread_attr_setschedparam(&attr, &schedParam)) {
            ERR("Failed to set scheduler parameters\n");
            cleanup(EXIT_FAILURE);
        }

        /* Create the video thread */
        videoEnv.hRendezvousInit    = hRendezvousInit;
        videoEnv.hRendezvousCleanup = hRendezvousCleanup;
        videoEnv.hRendezvousWriter  = hRendezvousWriter;
        videoEnv.hPauseProcess      = hPauseProcess;
        videoEnv.hCaptureOutFifo    = captureEnv.hOutFifo;
        videoEnv.hCaptureInFifo     = captureEnv.hInFifo;
        videoEnv.hWriterOutFifo     = writerEnv.hOutFifo;
        videoEnv.hWriterInFifo      = writerEnv.hInFifo;
        videoEnv.videoEncoder       = args.videoEncoder->codecName;
        videoEnv.params             = args.videoEncoder->params;
        videoEnv.dynParams          = args.videoEncoder->dynParams;
        videoEnv.videoBitRate       = args.videoBitRate;
        videoEnv.imageWidth         = captureEnv.imageWidth;
        videoEnv.imageHeight        = captureEnv.imageHeight;
        videoEnv.resizeWidth        = captureEnv.resizeWidth;
        videoEnv.resizeHeight       = captureEnv.resizeHeight;
        videoEnv.imgEncoder         = "jpegenc";
        videoEnv.engineName         = engine->engineName;
        if (args.videoStd == VideoStd_D1_PAL) {
            videoEnv.videoFrameRate     = 25000;
        } else {
            videoEnv.videoFrameRate     = 30000;
        }

        if (pthread_create(&videoThread, &attr, videoThrFxn, &videoEnv)) {
            ERR("Failed to create video thread\n");
            cleanup(EXIT_FAILURE);
        }

        initMask |= VIDEOTHREADCREATED;

        /*
         * Wait for the codec to be created in the video thread before
         * launching the writer thread (otherwise we don't know which size
         * of buffers to use).
         */
        Rendezvous_meet(hRendezvousWriter);

        /* Set the writer thread priority */
        schedParam.sched_priority = WRITER_THREAD_PRIORITY;
        if (pthread_attr_setschedparam(&attr, &schedParam)) {
            ERR("Failed to set scheduler parameters\n");
            cleanup(EXIT_FAILURE);
        }

        /* Create the writer thread */
        writerEnv.hRendezvousInit    = hRendezvousInit;
        writerEnv.hRendezvousCleanup = hRendezvousCleanup;
        writerEnv.hPauseProcess      = hPauseProcess;
        writerEnv.videoFile          = args.videoFile;
        writerEnv.outBufSize         = videoEnv.outBufSize;
        writerEnv.outsBufSize        = videoEnv.outsBufSize;

        if (pthread_create(&writerThread, &attr, writerThrFxn, &writerEnv)) {
            ERR("Failed to create writer thread\n");
            cleanup(EXIT_FAILURE);
        }

        initMask |= WRITERTHREADCREATED;

    }

    /* Create the speech thread if a file name is supplied */
    if (args.speechFile) {
        /* Set the thread priority */
        schedParam.sched_priority = SPEECH_THREAD_PRIORITY;
        if (pthread_attr_setschedparam(&attr, &schedParam)) {
            ERR("Failed to set scheduler parameters\n");
            cleanup(EXIT_FAILURE);
        }

        /* Create the speech thread */
        speechEnv.hRendezvousInit    = hRendezvousInit;
        speechEnv.hRendezvousCleanup = hRendezvousCleanup;
        speechEnv.hPauseProcess      = hPauseProcess;
        speechEnv.speechFile         = args.speechFile;
        speechEnv.soundInput         = args.soundInput;
        speechEnv.speechEncoder      = args.speechEncoder->codecName;
        speechEnv.params             = args.speechEncoder->params;
        speechEnv.dynParams          = args.speechEncoder->dynParams;
        speechEnv.engineName         = engine->engineName;

        if (pthread_create(&speechThread, &attr, speechThrFxn, &speechEnv)) {
            ERR("Failed to create speech thread\n");
            cleanup(EXIT_FAILURE);
        }

        initMask |= SPEECHTHREADCREATED;
    }

    /* Main thread becomes the control thread */
    ctrlEnv.hRendezvousInit    = hRendezvousInit;
    ctrlEnv.hRendezvousCleanup = hRendezvousCleanup;
    ctrlEnv.hPauseProcess      = hPauseProcess;
    ctrlEnv.keyboard           = args.keyboard;
    ctrlEnv.time               = args.time;
    ctrlEnv.hUI                = hUI;
    ctrlEnv.engineName         = engine->engineName;

    ret = ctrlThrFxn(&ctrlEnv);

    if (ret == THREAD_FAILURE) {
        status = EXIT_FAILURE;
    }

cleanup:
    /* Make sure the other threads aren't waiting for init to complete */
    if (hRendezvousCapStd) Rendezvous_force(hRendezvousCapStd);
    if (hRendezvousWriter) Rendezvous_force(hRendezvousWriter);
    if (hRendezvousInit) Rendezvous_force(hRendezvousInit);
    if (hPauseProcess) Pause_off(hPauseProcess);

    /* Wait until the other threads terminate */
    if (initMask & SPEECHTHREADCREATED) {
        if (pthread_join(speechThread, &ret) == 0) {
            if (ret == THREAD_FAILURE) {
                status = EXIT_FAILURE;
            }
        }
    }

    if (initMask & VIDEOTHREADCREATED) {
        if (pthread_join(videoThread, &ret) == 0) {
            if (ret == THREAD_FAILURE) {
                status = EXIT_FAILURE;
            }
        }
    }

    if (initMask & WRITERTHREADCREATED) {
        if (pthread_join(writerThread, &ret) == 0) {
            if (ret == THREAD_FAILURE) {
                status = EXIT_FAILURE;
            }
        }
    }

    if (writerEnv.hOutFifo) {
        Fifo_delete(writerEnv.hOutFifo);
    }

    if (writerEnv.hInFifo) {
        Fifo_delete(writerEnv.hInFifo);
    }

    if (initMask & CAPTURETHREADCREATED) {
        if (pthread_join(captureThread, &ret) == 0) {
            if (ret == THREAD_FAILURE) {
                status = EXIT_FAILURE;
            }
        }
    }

    if (captureEnv.hOutFifo) {
        Fifo_delete(captureEnv.hOutFifo);
    }

    if (captureEnv.hInFifo) {
        Fifo_delete(captureEnv.hInFifo);
    }

    if (hRendezvousCleanup) {
        Rendezvous_delete(hRendezvousCleanup);
    }

    if (hRendezvousInit) {
        Rendezvous_delete(hRendezvousInit);
    }

    if (hPauseProcess) {
        Pause_delete(hPauseProcess);
    }

    if (hUI) {
        UI_delete(hUI);
    }

    system("sync");
    system("echo 3 > /proc/sys/vm/drop_caches");


    pthread_mutex_destroy(&gbl.mutex);

    if (args.interface) {
        /* Launch the demo selection interface when exiting */
        if (execl("./interface", "interface", "-l 3", (char *) NULL) == -1) {
            status = EXIT_FAILURE;
        }
    }

    exit(status);
}
