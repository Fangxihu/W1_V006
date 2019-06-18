/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_voice_assistant_connect.c

DESCRIPTION
    Handle Vocie Assistant Start/Stop message handlers
*/

#include <operators.h>
#include <audio_plugin_common.h>
#include <audio_config.h>
#include <vmtypes.h>
#include <print.h>
#include <audio_plugin_if.h>
#include <audio.h>
#include <chain.h>
#include <gain_utils.h>
#include <rtime.h>

#include "audio_plugin_voice_assistant_variants.h"
#include "audio_voice_assistant_chain_config.h"
#include "audio_voice_assistant_private.h"
#include "audio_voice_assistant_start.h"


/****************************************************************************
DESCRIPTION
    Function Sends Data Soruce to registered application task
*/
static void VaIndicateDataSource(Task app_task,Source src)
{
    PRINT(("VA_Plugin:Indicate Data Source to registred task \n"));

    MAKE_AUDIO_MESSAGE( AUDIO_VA_INDICATE_DATA_SOURCE, message ) ;
    message->plugin = (Task)&voice_assistant_plugin ;
    message->data_src = src ;
    MessageSend(app_task, AUDIO_VA_INDICATE_DATA_SOURCE, message);
}

/****************************************************************************
DESCRIPTION
    Function Connects/configure Mic Streams/DSP Streams 
*/
static void VaConnectStreams(Task app_task,va_plugin_context_t *va_context , audio_mic_params mic_params)
{

    Source va_src;

    PRINT(("VA_Plugin: VaConnectStreams() Enter \n"));
    /* Connect Mic srteams to Chain */
    PanicNull(StreamConnect(AudioPluginMicSetup(AUDIO_CHANNEL_A, mic_params, VA_AUDIO_SAMPLE_RATE),
                                   ChainGetInput(va_context->chain, voice_input)));
    /* Connect SBC encoded endpoint to DSP streams */
    va_src = ChainGetOutput(va_context->chain, voice_output);
    PanicFalse(SourceMapInit(va_src, STREAM_TIMESTAMPED, AUDIO_FRAME_VA_DATA_LENGTH));
    VaIndicateDataSource(app_task,va_src);
}

/****************************************************************************
DESCRIPTION
    Function Disconnect Mic Streams/DSP Streams 
*/
static void VaDisconnectStreams(va_plugin_context_t *va_context)
{    
    Source va_src;

    PRINT(("VA_Plugin: Disconnect Streams \n"));
    /* Disconnect Mic */
    StreamDisconnect(NULL,ChainGetInput(va_context->chain, voice_input));
    /* Disconnect DSP streams */
    va_src = ChainGetOutput(va_context->chain, voice_output);
    SourceUnmap(va_src);
    StreamDisconnect(va_src, 0);
}

/****************************************************************************
DESCRIPTION
    Function to Configure VA operators and 
*/
static void VaConfigureOperators(va_plugin_context_t *va_context)
{

    Operator op_ttp_passthrough;
    Operator op_sbc_encoder;
    sbc_encoder_params_t encoder_params;

    PRINT(("VA_Plugin: Configuring VA Operators  \n"));
 
    /* Configure TTP passthrough role */
    encoder_params = AudioConfigGetSbcEncoderParams();
    op_ttp_passthrough = ChainGetOperatorByRole(va_context->chain,va_ttp_passthrough_role);
    OperatorsConfigureTtpPassthrough(op_ttp_passthrough,VA_LATENCY_IN_US,encoder_params.sample_rate,operator_data_format_pcm);
    OperatorsStandardSetBufferSize(op_ttp_passthrough, VA_CHAIN_INPUT_BUFFER_SIZE);
    OperatorsSetPassthroughGain(op_ttp_passthrough, INITIAL_GAIN_SCALED_DB);
    /* Configure SBC encoder for voice capture */
    op_sbc_encoder = ChainGetOperatorByRole(va_context->chain,va_encoder_role);
    OperatorsSbcEncoderSetEncodingParams(op_sbc_encoder,&encoder_params);

    PRINT(("VA_Plugin: VaConfigureOperators() Exit \n"));
}

/****************************************************************************
DESCRIPTION
    Function to Create VA operator graph/chain
*/
static void VaCreateVoiceCaptureChain(Task app_task,va_plugin_context_t *va_context )
{
    if(va_context)
    {
        PRINT(("VA_Plugin: Create VA audio chain \n"));

        OperatorsFrameworkEnable();
        va_context->chain = PanicNull(ChainCreate(AudioPluginVaGetChainConfig()));
        VaConfigureOperators(va_context);
        VaConnectStreams(app_task,va_context,AudioConfigGetVaMicParams());
        ChainConnect(va_context->chain);
        ChainStart(va_context->chain);

        PRINT(("VA_Plugin: Va operator chain creation completed \n"));

    }
}

/****************************************************************************
DESCRIPTION
    Function to Stop/Distory VA operator graph/chain
*/
static void VaDestoryVoiceCaptureChain(va_plugin_context_t *va_context)
{
    if((va_context) && (va_context->chain))
    {
        PRINT(("VA_Plugin: Distroy VA capture chain \n"));

        ChainStop(va_context->chain);
        /* Disconnect Streams to operators */
        VaDisconnectStreams(va_context);
        ChainDestroy(va_context->chain);
        OperatorsFrameworkDisable();
    }
}


/*******************************Interface Function *******************************/
void VaStartVoiceCaptureMsgHandler(AUDIO_PLUGIN_START_VOICE_CAPTURE_MSG_T *msg,va_plugin_context_t *va_context)
{
     VaCreateVoiceCaptureChain(msg->app_task,va_context);
}

/******************************************************************************/
void VaStopVoiceCaptureMsgHandler(va_plugin_context_t *va_context)
{
    VaDestoryVoiceCaptureChain(va_context);
}


