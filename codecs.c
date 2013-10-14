/*
 * codecs.c
 *
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2009
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#include <xdc/std.h>

#include "../demo.h"

/* File extensions for G.711 */
static Char *g711Extensions[] = { ".g711", NULL };

/* NULL terminated list of speech encoders in the engine to use in the demo */
static Codec speechEncoders[] = {
    {
        "g711enc",            /* String name of codec for CE to locate it */
        "G.711 Speech",       /* The string to show on the UI for this codec */
        g711Extensions,       /* NULL terminated list of file extensions */
        NULL,                 /* Use default params */
        NULL                  /* Use default dynamic params */
    },
    { NULL }
};

/* File extensions for MPEG4 */
//static Char *mpeg4Extensions[] = { ".mpeg4", ".m4v", NULL };

/* File extensions for H.264 */
static Char *h264Extensions[] = { ".264", NULL };

/* NULL terminated list of video encoders in the engine to use in the demo */
static Codec videoEncoders[] = {
    //{
    //    "mpeg4enc",
    //    "MPEG4 Video",
    //    mpeg4Extensions,
    //    NULL,
    //    NULL
    //},
    {
        "h264enc",
        "H.264 HP",
        h264Extensions,
        NULL,
        NULL
    },    
    { NULL }
};

/* Declaration of the production engine and encoders shipped with the DVSDK */
static Engine encodeEngine = {
    "encode",           /* Engine string name used by CE to find the engine */
    NULL,               /* NULL terminated list of speech decoders in engine */
    NULL,               /* NULL terminated list of audio decoders in engine */
    NULL,               /* NULL terminated list of video decoders in engine */
    speechEncoders,     /* NULL terminated list of speech encoders in engine */
    NULL,               /* Audio encoders in engine (Not supported) */
    videoEncoders       /* NULL terminated list of video encoders in engine */
};

/*
 * This assignment selects which engine will be used by the demo. Note that
 * this file can contain several engine declarations, but this declaration
 * determines which one to use.
 */
Engine *engine = &encodeEngine;
