/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_voice_assistant_start.h

DESCRIPTION
    Voice assistant plugin start/stop handler interfaces .
*/

#ifndef LIBS_AUDIO_VOICE_ASSISTANT_CONNECT_H_
#define LIBS_AUDIO_VOICE_ASSISTANT_CONNECT_H_

#include <message.h>
#include <audio_plugin_if.h>

#include "audio_voice_assistant_private.h"

/****************************************************************************
DESCRIPTION
    Function for VA Start capture message handler
*/
void VaStartVoiceCaptureMsgHandler(AUDIO_PLUGIN_START_VOICE_CAPTURE_MSG_T *msg,
                                            va_plugin_context_t *va_context);

/****************************************************************************
DESCRIPTION
    Function for VA Stop capture message handler
*/
void VaStopVoiceCaptureMsgHandler(va_plugin_context_t *va_context);

#endif /* LIBS_AUDIO_VOICE_ASSISTANT_CONNECT_H_ */

