/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_voice_assistant.c

DESCRIPTION
    Message dispatcher.
*/
#include <stdlib.h>

#include <message.h>
#include <audio.h>
#include <audio_plugin_if.h>
#include <audio_input_common.h>
#include <vmtypes.h>
#include <print.h>


#include "audio_voice_assistant.h"
#include "audio_plugin_voice_assistant_variants.h"
#include "audio_voice_assistant_chain_config.h"
#include "audio_voice_assistant_private.h"
#include "audio_voice_assistant_start.h"


static va_plugin_context_t *va_context = NULL;

/* Plugin Task */
const TaskData voice_assistant_plugin = { AudioPluginVaMessageHandler };


/****************************************************************************
DESCRIPTION
    Function to Clean UP Voice Assistant Plugin Context
*/
static void VaCleanUpContext(void)
{
    if(va_context)
    {
        free(va_context);
        va_context = NULL;
    }
}

/****************************************************************************
DESCRIPTION
    Function to Create UP Voice Assistant Plugin Context
*/
static void VaCreateContext(void)
{
    PanicNotNull(va_context);
    va_context = calloc(1, sizeof(*va_context));
}

/****************************************************************************
DESCRIPTION
    Function to returns Voice Assistant Plugin Context
*/
static va_plugin_context_t * VaGetContext(void)
{
    return va_context;
}

/*****************************************************************************
DESCRIPTION
    Function for Handling VA audio messages
*/
static void VaHandleAudioMessage(Task task, MessageId id, Message message)
{
  UNUSED(task);
  switch(id)
  {
    case AUDIO_PLUGIN_START_VOICE_CAPTURE_MSG:

        PRINT(("VA_Plugin: AUDIO_PLUGIN_START_VOICE_CAPTURE_MSG received \n"));
        /* Create VA context */
        VaCreateContext();
        VaStartVoiceCaptureMsgHandler((AUDIO_PLUGIN_START_VOICE_CAPTURE_MSG_T*)message,VaGetContext());
        break;
        
    case AUDIO_PLUGIN_STOP_VOICE_CAPTURE_MSG:

        PRINT(("VA_Plugin: AUDIO_PLUGIN_STOP_VOICE_CAPTURE_MSG received \n"));

        VaStopVoiceCaptureMsgHandler(VaGetContext());
        /* Clean up and free va context */
        VaCleanUpContext();
        break;
        
    case AUDIO_PLUGIN_TEST_RESET_MSG:
        VaCleanUpContext();
        break;
        
    default:
        PRINT(("VA_Plugin: Unknown message in VaHandleAudioMessage()\n"));
        Panic();
        break;
  }
}

/*****************************************************************************/
void AudioPluginVaMessageHandler (Task task, MessageId id, Message message)
{
    if(id >= AUDIO_DOWNSTREAM_MESSAGE_BASE && id < AUDIO_DOWNSTREAM_MESSAGE_TOP)
    {
        VaHandleAudioMessage(task, id, message);
    }  
    else
    {
        PRINT(("VA_Plugin: Unknown message in AudioPluginVaMessageHandler()\n"));
        Panic();
    }
}

