/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_voice_assistant.h

DESCRIPTION
    Audio plugin lib for voice assistant .
*/

#ifndef LIBS_AUDIO_VOICE_ASSISTANT_H_
#define LIBS_AUDIO_VOICE_ASSISTANT_H_

#include <message.h>

/*the task message handler*/
void AudioPluginVaMessageHandler (Task task, MessageId id, Message message);

#endif /* LIBS_AUDIO_VOICE_ASSISTANT_H_ */

