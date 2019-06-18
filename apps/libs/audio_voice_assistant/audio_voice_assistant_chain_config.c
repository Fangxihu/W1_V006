/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_voice_assistant_chain_config.c

DESCRIPTION
    Implementation of function to return the appropriate chain configuration data.
*/

#include <operators.h>
#include <panic.h>
#include "audio_voice_assistant_chain_config.h"


static const operator_config_t va_op_config[] =
{
    MAKE_OPERATOR_CONFIG(capability_id_ttp_passthrough, va_ttp_passthrough_role),
    MAKE_OPERATOR_CONFIG(capability_id_sbc_encoder, va_encoder_role)
};

static const operator_endpoint_t va_inputs[] =
{
    {va_ttp_passthrough_role, voice_input,0},
};

static const operator_endpoint_t va_outputs[] =
{
    {va_encoder_role,voice_output,0},
};

static const operator_connection_t va_connections[] =
{
    {va_ttp_passthrough_role,0,va_encoder_role,0,1},
};

static const chain_config_t va_chain_config =
    MAKE_CHAIN_CONFIG(chain_id_va, audio_ucid_va, va_op_config, va_inputs, va_outputs,va_connections);

const chain_config_t* AudioPluginVaGetChainConfig(void)
{
  return &va_chain_config;
}

