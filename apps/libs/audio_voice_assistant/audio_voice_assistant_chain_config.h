/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_voice_assistant_chain_config.h

DESCRIPTION
    Voice assistant chain configuration.
*/

#ifndef AUDIO_PLUGIN_VOICE_ASSISTANT_CHAIN_CONFIG_H_
#define AUDIO_PLUGIN_VOICE_ASSISTANT_CHAIN_CONFIG_H_

#include <chain.h>

typedef enum
{
    va_ttp_passthrough_role,
    va_encoder_role
} va_operator_role_t;

typedef enum
{
    voice_input
}va_inputs_role_t;

typedef enum
{
    voice_output
}va_outputs_role_t;

/****************************************************************************
DESCRIPTION
    Return the chain configuration.
*/
const chain_config_t* AudioPluginVaGetChainConfig(void);

#endif /* AUDIO_PLUGIN_VOICE_ASSISTANT_CHAIN_CONFIG_H_ */

