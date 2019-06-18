/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_voice_assistant_private.h

DESCRIPTION
    Voice Assistant Private header file contains defines and default values
*/

#include <stdlib.h>
#include <vmtypes.h>
#include <print.h>
#include <stdio.h>

#ifndef LIBS_AUDIO_VOICE_ASSISTANT_PRIVATE_H_
#define LIBS_AUDIO_VOICE_ASSISTANT_PRIVATE_H_

/* Voice Assistant Context structure */
typedef struct
{
    kymera_chain_handle_t chain;
}va_plugin_context_t;

/*-------------------  Defines -------------------*/

#define AUDIO_FRAME_VA_DATA_LENGTH 9
#define VA_AUDIO_SAMPLE_RATE       16000
#define VA_CHAIN_INPUT_BUFFER_SIZE 1204

#define INITIAL_GAIN_SCALED_DB     GainIn60thdB(-30)
#define VA_LATENCY_IN_MS           (20)
#define VA_LATENCY_IN_US           (VA_LATENCY_IN_MS * US_PER_MS)

#endif /* LIBS_AUDIO_VOICE_ASSISTANT_PRIVATE_H_ */

