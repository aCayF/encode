/*
 * audio.h
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#ifndef _SPEECH_H
#define _SPEECH_H

#include <xdc/std.h>

#include <ti/sdo/dmai/Pause.h>
#include <ti/sdo/dmai/Sound.h>
#include <ti/sdo/dmai/Rendezvous.h>

/* Environment passed when creating the thread */
typedef struct SpeechEnv {
    Rendezvous_Handle       hRendezvousInit;
    Rendezvous_Handle       hRendezvousCleanup;
    Pause_Handle            hPauseProcess;
    Sound_Input             soundInput;
    Char                   *speechFile;
    Char                   *engineName;
    Char                   *speechEncoder;
    Void                   *params;
    Void                   *dynParams;
} SpeechEnv;

/* Thread function prototype */
extern Void *speechThrFxn(Void *arg);

#endif /* _SPEECH_H */
