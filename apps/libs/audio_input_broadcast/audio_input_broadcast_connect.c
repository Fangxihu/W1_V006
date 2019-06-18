/****************************************************************************
Copyright (c) 2017-2018 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_input_broadcast_connect.c

DESCRIPTION
    Handlers for when connecting or disconnecting the plugin.
*/

#include <audio.h>
#include <audio_config.h>
#include <audio_input_common.h>
#include <audio_mixer.h>
#include <audio_plugin_music_params.h>
#include <audio_plugin_music_variants.h>
#include <audio_plugin_ucid.h>
#include <broadcast_stream_service_record.h>
#include <broadcast_context.h>
#include <operators.h>

#include "audio_input_broadcast.h"
#include "audio_input_broadcast_connect.h"
#include "audio_input_broadcast_chain_config.h"
#include "audio_input_broadcast_packetiser.h"
#include "audio_input_broadcast_private.h"

#define CHAIN_INPUT_BUFFER_SIZE 4096

static void connectChainToMixer(audio_input_context_t *ctx, uint32 sample_rate);
static bool pluginTypeIsBa(A2DP_DECODER_PLUGIN_TYPE_T plugin_type);
static Sink connectCreateOperators(audio_input_context_t *ctx, uint32 rate);

/****************************************************************************
DESCRIPTION
    AUDIO_PLUGIN_CONNECT_MSG message handler.
    It creates a Kymera decoder chain and connects it to source and
    audio_mixer via the CSB (broadcast) unpacketiser.
*/
void audioInputBroadcastConnectHandler(Task task, Message message, audio_input_context_t *ctx)
{
    const AUDIO_PLUGIN_CONNECT_MSG_T *msg = (const AUDIO_PLUGIN_CONNECT_MSG_T*)message;
    A2DP_DECODER_PLUGIN_TYPE_T decoder = ((A2dpPluginTaskdata*)task)->a2dp_plugin_variant;
    A2dpPluginConnectParams* params = (A2dpPluginConnectParams *)msg->params;

    if (pluginTypeIsBa(decoder))
    {
        Sink audio_frame_sink;

        AudioInputCommonDspPowerOn();
        OperatorsFrameworkSetKickPeriod(BA_KICK_PERIOD);

        /* Make sure the source is disconnected before doing anything else */
        ctx->left_source = StreamSourceFromSink(msg->audio_sink);
        StreamDisconnect(ctx->left_source, NULL);

        /* Create and start the operator chain */
        AudioInputCommonSetMusicProcessingContext(ctx, params);
        audio_frame_sink = connectCreateOperators(ctx, msg->rate);

        /* Create the broadcast_packetiser. Note: this completes asynchronously
           with a BROADCAST_PACKETISER_INIT_CFM msg. */
        audioInputBroadcastPacketiserCreate(ctx->left_source, audio_frame_sink);

        ChainStart(ctx->chain);

        SetAudioBusy(task);
    }
    else
    {
        Panic();
    }
}

/****************************************************************************
DESCRIPTION
    Connect decoder chain to the audio mixer.
*/
static void connectChainToMixer(audio_input_context_t *ctx, uint32 sample_rate)
{
    audio_mixer_connect_t connect_data;

    connect_data.left_src = ChainGetOutput(ctx->chain, decoded_audio_output_left);
    connect_data.right_src = ChainGetOutput(ctx->chain, decoded_audio_output_right);
    connect_data.connection_type = CONNECTION_TYPE_MUSIC_BA_RX;
    connect_data.sample_rate = sample_rate;
    connect_data.channel_mode = CHANNEL_MODE_STEREO;
    connect_data.variable_rate = FALSE;

    ctx->mixer_input = AudioMixerConnect(&connect_data);

    PanicFalse(ctx->mixer_input != audio_mixer_input_error_none);
}

/****************************************************************************
DESCRIPTION
    Confgure Kymera chain for decoder.
*/
static void configureDecoderChain(kymera_chain_handle_t chain, uint32 rate, codec_config_celt *codec_config)
{
    Operator op;
    celt_encoder_params_t celt_params;

    /* Configure the input buffer passthrough */
    op = ChainGetOperatorByRole(chain, input_buffer_role);
    OperatorsStandardSetBufferSize(op, CHAIN_INPUT_BUFFER_SIZE);
    OperatorsSetPassthroughDataFormat(op, operator_data_format_encoded);

    /* configure the CELT decoder */
    op = ChainGetOperatorByRole(chain, decoder_role);
    celt_params.sample_rate = rate;
    if (codec_config)
    {
        /* Kymera CELT decoder only supports 512 samples per frame modes */
        PanicFalse(codec_config->frame_samples == CELT_FRAME_SAMPLES);

        celt_params.frame_size = codec_config->frame_size;
    }
    else
    {
        celt_params.frame_size = 0;
    }
    OperatorsCeltEncoderSetEncoderParams(op, &celt_params);

    /* Configure the pre-processing passthrough */
    op = ChainGetOperatorByRole(chain, pre_processing_role);
    /* Apply -12dB attenuation through ucid, to provide headroom for music processing.*/
    OperatorsStandardSetUCID(op, ucid_passthrough_processing);
    OperatorsStandardSetBufferSize(op, CHAIN_INPUT_BUFFER_SIZE);
    OperatorsSetPassthroughDataFormat(op, operator_data_format_pcm);
}

/****************************************************************************
DESCRIPTION
    Create Kymera chain for decoder and connect the output to the mixer.
*/
static Sink connectCreateOperators(audio_input_context_t *ctx, uint32 rate)
{
    ctx->chain = audioInputBroadcastCreateChain();
    configureDecoderChain(ctx->chain, rate, NULL);

    ChainConnect(ctx->chain);
    connectChainToMixer(ctx, rate);

    return ChainGetInput(ctx->chain, encoded_audio_input);
}

/****************************************************************************
DESCRIPTION
    Checks whether decoder type is supported by this csb input plugin.
*/
void audioInputBroadcastConnectReconfigureDecoder(audio_input_context_t *ctx, uint32 rate, codec_config_celt *codec_config)
{
    PanicNull(ctx->chain);

    /* Stop the chain */
    ChainStop(ctx->chain);
    /*It is possible to set variable_rate connect parameter to TRUE and change mixer's sample rate without
      disconnection the mixer. Which would look better in the code.
      However, the result would be that a resampler would be always created even when input and output
      sample rate would be the same. Scenario when input and output sample rate is quite likely,
      thus to save the resources, it is better to not force creation of the resampler.
     */
    AudioMixerDisconnect(ctx->mixer_input);


    /* reconfigure decoder chain */
    configureDecoderChain(ctx->chain, rate, codec_config);
    /* reconfigure and connect mixer */
    connectChainToMixer(ctx, rate);
    /* Restart chain */
    ChainStart(ctx->chain);

    AudioMixerFadeIn(ctx->mixer_input, NULL);
}

/****************************************************************************
DESCRIPTION
    Checks whether decoder type is supported by this csb input plugin.
*/
static bool pluginTypeIsBa(A2DP_DECODER_PLUGIN_TYPE_T plugin_type)
{
    switch(plugin_type)
    {
        case BA_CELT_DECODER:
            return TRUE;
        default:
            return FALSE;
    }
}

/****************************************************************************
DESCRIPTION
    AUDIO_MIXER_FADE_OUT_CFM message handler.
    Destroys the broadcast packetiser and disconnects the chain.
*/
void audioInputBroadcastFadeOutCompleteHandler(Task task, Message message, audio_input_context_t *ctx)
{
    PanicNull(ctx->chain);

    ChainStop(ctx->chain);
    audioInputBroadcastPacketiserDestroy(ChainGetInput(ctx->chain, encoded_audio_input));
    AudioInputCommonFadeOutCompleteHandler(task, message, ctx);
}

#ifdef HOSTED_TEST_ENVIRONMENT
void audioInputBroadcastTestReset(void)
{
    audioInputBroadcastPacketiserDestroy(NULL);
    AudioMixerTestReset();
}
#endif
