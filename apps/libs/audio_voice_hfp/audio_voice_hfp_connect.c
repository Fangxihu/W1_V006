/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_voice_hfp_connect.c

DESCRIPTION
    Audio voice HFP connect implementation.
*/

#include "audio_voice_hfp_connect.h"

#include <stdlib.h>

#include <audio.h>
#include <print.h>
#include <message.h>
#include <audio_voice_common.h>
#include <audio_plugin_voice_variants.h>
#include <chain.h>

#include "audio_voice_hfp.h"

#define WBS_SAMPLE_RATE         ((uint32)16000)
#define NBS_SAMPLE_RATE         ((uint32)8000)
#define MSBC_SCO_FRAME_LENGTH   (60)
#define SCO_METADATA_ENABLE     (1)

static operator_config_t hfp_nb[] =
{
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_sco_receive, audio_voice_receive_role), \
    MAKE_OPERATOR_CONFIG_PRIORITY_HIGH(capability_id_sco_send, audio_voice_send_role)
};

static operator_config_t hfp_wb[] =
{
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_wbs_receive, audio_voice_receive_role), \
    MAKE_OPERATOR_CONFIG_PRIORITY_HIGH(capability_id_wbs_send, audio_voice_send_role)
};

/******************************************************************************/

static operator_config_t* getOperatorFilter(bool wideband)
{
    return (wideband ? hfp_wb : hfp_nb);
}

static operator_filters_t* getFilters(bool wideband)
{
    operator_filters_t* filters = (operator_filters_t*)calloc(1,sizeof(operator_filters_t));

    filters->num_operator_filters = 2;
    filters->operator_filters = getOperatorFilter(wideband);

    return filters;
}

static void configureSourceSink( Message msg, audio_voice_context_t* ctx)
{
    AUDIO_PLUGIN_CONNECT_MSG_T* connect_msg =  (AUDIO_PLUGIN_CONNECT_MSG_T*) msg;
    if(ctx->encoder == link_encoding_msbc)
    {
        SinkConfigure(connect_msg->audio_sink, VM_SINK_SCO_SET_FRAME_LENGTH, MSBC_SCO_FRAME_LENGTH);
    }

    SourceConfigure(StreamSourceFromSink(connect_msg->audio_sink), VM_SOURCE_SCO_METADATA_ENABLE, SCO_METADATA_ENABLE);
}

/******************************************************************************/
void AudioVoiceHfpConnectAndFadeIn(Task task, Message msg, audio_voice_context_t* ctx)
{
    CvcPluginTaskdata* plugin = (CvcPluginTaskdata*)task;
    operator_filters_t* filters = getFilters(AudioVoiceCommonIsWideband(plugin->encoder));

    ctx->variant = plugin->cvc_plugin_variant;
    ctx->two_mic = plugin->two_mic;
    ctx->encoder = plugin->encoder;
    ctx->mic_sample_rate_16kHz = plugin->adc_dac_16kHz;
    ctx->ttp_latency.target_in_ms = AUDIO_RX_TTP_LATENCY;
    ctx->ttp_latency.min_in_ms = 0;
    ctx->ttp_latency.max_in_ms = 0;

    /* Need to enable dsp before sending messages to sink/source */
    AudioVoiceCommonDspPowerOn();
    configureSourceSink(msg, ctx);
    AudioVoiceCommonConnectAndFadeIn(task, msg, ctx, filters);
    free(filters);
}

