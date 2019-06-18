/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_input_usb_connect.c

DESCRIPTION
    Audio input USB connect implementation.
*/

#include "audio_voice_usb_connect.h"

#include <stdlib.h>

#include <audio.h>
#include <print.h>
#include <message.h>
#include <audio_voice_common.h>
#include <audio_plugin_voice_variants.h>
#include <chain.h>

#include "audio_voice_usb.h"


#define WBS_SAMPLE_RATE         ((uint32)16000)
#define NBS_SAMPLE_RATE         ((uint32)8000)
#define MSBC_SCO_FRAME_LENGTH   (60)
#define SCO_METADATA_ENABLE     (1)


#define CHANNELS_NUMBER_VOICE    (1)
#define SAMPLE_RATE_8K        ((uint32)8000)
#define SAMPLE_RATE_16K       ((uint32)16000)
#define SAMPLE_SIZE_16_BIT       (2)
#define BUFFER_SIZE           (4096)
#define LATENCY_MS              (20)


#define OPERATORS_SETUP_USB_CONFIG(sample_rate, sample_size, num_channels) \
{ \
    .key = operators_setup_usb_config, \
    .value = {.usb_config = {sample_rate, sample_size, num_channels}} \
}

static const operator_setup_item_t usb_setup_nb[] =
{
    OPERATORS_SETUP_USB_CONFIG(SAMPLE_RATE_8K, SAMPLE_SIZE_16_BIT, CHANNELS_NUMBER_VOICE),
    OPERATORS_SETUP_STANDARD_BUFFER_SIZE(BUFFER_SIZE)
};

static const operator_setup_item_t usb_setup_wb[] =
{
    OPERATORS_SETUP_USB_CONFIG(SAMPLE_RATE_16K, SAMPLE_SIZE_16_BIT, CHANNELS_NUMBER_VOICE),
    OPERATORS_SETUP_STANDARD_BUFFER_SIZE(BUFFER_SIZE)
};

static operator_config_t usb_nb[] =
{
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM_WITH_SETUP(capability_id_usb_audio_rx, audio_voice_receive_role, usb_setup_nb), \
    MAKE_OPERATOR_CONFIG_WITH_SETUP(capability_id_usb_audio_tx, audio_voice_send_role, usb_setup_nb)
};

static operator_config_t usb_wb[] =
{
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM_WITH_SETUP(capability_id_usb_audio_rx, audio_voice_receive_role, usb_setup_wb), \
    MAKE_OPERATOR_CONFIG_WITH_SETUP(capability_id_usb_audio_tx, audio_voice_send_role, usb_setup_wb)
};

/******************************************************************************/

static operator_config_t* getOperatorFilter(bool wideband)
{
    return (wideband ? usb_wb : usb_nb);
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
}

/******************************************************************************/
void AudioInputUsbConnectAndFadeIn(Task task, Message msg, audio_voice_context_t* ctx)
{
    CvcPluginTaskdata* plugin = (CvcPluginTaskdata*)task;
    operator_filters_t* filters = getFilters(AudioVoiceCommonIsWideband(plugin->encoder));

    ctx->variant = plugin->cvc_plugin_variant;
    ctx->two_mic = plugin->two_mic;
    ctx->encoder = plugin->encoder;
    ctx->mic_sample_rate_16kHz = plugin->adc_dac_16kHz;
    ctx->ttp_latency.min_in_ms = AUDIO_RX_MIN_LATENCY;
    ctx->ttp_latency.target_in_ms = AUDIO_RX_TTP_LATENCY;
    ctx->ttp_latency.max_in_ms = AUDIO_RX_MAX_LATENCY;

    /* Need to enable dsp before sending messages to sink/source */
    AudioVoiceCommonDspPowerOn();
    configureSourceSink(msg, ctx);
    AudioVoiceCommonConnectAndFadeIn(task, msg, ctx, filters);
    free(filters);
}

