/*!
\copyright  Copyright (c) 2017-2018  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\brief      Kymera SCO
*/

#include <vmal.h>
#include <packetiser_helper.h>

#include "av_headset_kymera_private.h"

#ifndef HFP_USE_2MIC

#include "chains/chain_sco_nb.h"
#include "chains/chain_sco_wb.h"

#include "chains/chain_micfwd_send.h"
#include "chains/chain_micfwd_wb.h"
#include "chains/chain_micfwd_nb.h"

#include "chains/chain_scofwd_recv.h"
#include "chains/chain_scofwd_wb.h"
#include "chains/chain_scofwd_nb.h"

#else /* HFP_USE_2MIC */

#include "chains/chain_sco_nb_2mic.h"
#include "chains/chain_sco_wb_2mic.h"

#include "chains/chain_micfwd_send_2mic.h"
#include "chains/chain_micfwd_wb_2mic.h"
#include "chains/chain_micfwd_nb_2mic.h"

#include "chains/chain_scofwd_recv_2mic.h"
#include "chains/chain_scofwd_wb_2mic.h"
#include "chains/chain_scofwd_nb_2mic.h"

#endif /* HFP_USE_2MIC */

#define AWBSDEC_SET_BITPOOL_VALUE    0x0003
#define AWBSENC_SET_BITPOOL_VALUE    0x0001

#define AEC_TX_BUFFER_SIZE_MS 15

#define SCOFWD_BASIC_PASS_BUFFER_SIZE 512

typedef struct set_bitpool_msg_s
{
    uint16 id;
    uint16 bitpool;
}set_bitpool_msg_t;


#ifdef INCLUDE_SCOFWD
static void OperatorsAwbsSetBitpoolValue(Operator op, uint16 bitpool, bool decoder)
{
    set_bitpool_msg_t bitpool_msg;
    bitpool_msg.id = decoder ? AWBSDEC_SET_BITPOOL_VALUE : AWBSENC_SET_BITPOOL_VALUE;
    bitpool_msg.bitpool = (uint16)(bitpool);

    PanicFalse(VmalOperatorMessage(op, &bitpool_msg, SIZEOF_OPERATOR_MESSAGE(bitpool_msg), NULL, 0));
}

/*! AEC REF message ID */
#define OPMSG_AEC_REFERENCE_ID_SAME_INPUT_OUTPUT_CLK_SOURCE 0x0008

/*! \brief Message AECREF operator that the back-end of the operator are coming
    from same clock source. This is for optimisation purpose and it's recommended
    to be enabled for use cases where speaker input and microphone output are
    synchronised (e.g. SCO and USB voice use cases). Note: Send/Resend this message
    when all microphone input/output and REFERENCE output are disconnected.
    \param aec_op The AEC Reference operator.
*/
static void appKymeraSetAecSameIOClockSource(Operator aec_op)
{
    uint16 msg[2];
    msg[0] = OPMSG_AEC_REFERENCE_ID_SAME_INPUT_OUTPUT_CLK_SOURCE;
    msg[1] = 1;
    PanicFalse(OperatorMessage(aec_op, msg, 2, NULL, 0));
}
#endif

static kymera_chain_handle_t appKymeraGetScoChain(void)
{
    kymeraTaskData *theKymera = appGetKymera();
    if (theKymera)
    {
        return theKymera->chainu.sco_handle;
    }
    return (kymera_chain_handle_t)NULL;
}

#ifdef INCLUDE_MICFWD
/* Set the SPC switch to a specific input 
    Inputs are renumbered 1,2 etc.
    0 will equal consume
 */
static void appKymeraSelectSpcSwitchInput(Operator op, micSelection input)
{
    uint16 msg[2];
    msg[0] = OPMSG_SPC_ID_SELECT_PASSTHROUGH;
    msg[1] = input;
    PanicFalse(OperatorMessage(op, msg, 2, NULL, 0));
}

static Operator appKymeraGetMicSwitch(void)
{
    kymera_chain_handle_t chain = appKymeraGetScoChain();
    
    if (chain)
    {
        return ChainGetOperatorByRole(chain, OPR_MICFWD_SPC_SWITCH);
    }
    return INVALID_OPERATOR;
}

void appKymeraSwitchSelectMic(micSelection mic)
{
    kymeraTaskData *theKymera = appGetKymera();
    
    /* always remember currently selected mic */
    theKymera->mic = mic;

    if (theKymera->state == KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING)
    {
        Operator spc_switch = appKymeraGetMicSwitch();
        if (spc_switch)
        {
            DEBUG_LOGF("appKymeraSwitchSelectMic %u", mic);
            appKymeraSelectSpcSwitchInput(spc_switch, mic);
        }
        else
        {
            DEBUG_LOG("appKymeraSwitchSelectMic failed to get OPR_MICFWD_SPC_SWITCH");
        }
    }
    else
    {
        DEBUG_LOGF("appKymeraSwitchSelectMic invalid state to switch mic %u", theKymera->state);
    }
}
#endif /* INCLUDE_MICFWD */

/*  \brief Set an operator's terminal buffer sizes.
    \param op The operator.
    \param rate The sample rate is Hz.
    \param buffer_size_ms The amount of audio the buffer should contain in ms.
    \param input_terminals The input terminals for which the buffer size should be set (bitmask).
    \param output_terminals The output terminals for which the buffer size should be set (bitmask).
*/
static void appKymeraSetTerminalBufferSize(Operator op, uint32 rate, uint32 buffer_size_ms,
                                           uint16 input_terminals, uint16 output_terminals)
{
    uint16 msg[4];
    msg[0] = OPMSG_COMMON_ID_SET_TERMINAL_BUFFER_SIZE;
    msg[1] = (rate * buffer_size_ms) / 1000;
    msg[2] = input_terminals;
    msg[3] = output_terminals;
    OperatorMessage(op, msg, ARRAY_DIM(msg), NULL, 0);
}

#ifdef INCLUDE_SCOFWD
static const bool sco_forwarding_supported = TRUE;
#else
static const bool sco_forwarding_supported = FALSE;
#endif

/* SCO chain selection is wrapped in this function.
   
   This code relies on the NB and WB chains being basically 
   "the same" other than operators.
 */
static void appKymeraGetScoChainConfig(const chain_config_t **chosen,
                                       hfp_wbs_codec_mask codec,
                                       bool allow_sco_forwarding)
{
    /* Select narrowband or wideband chain depending on CODEC */
    bool is_wideband = (codec == hfp_wbs_codec_mask_msbc);

#ifndef HFP_USE_2MIC
    DEBUG_LOGF("appKymeraGetScoChainConfig. 1 Mic. Sco Forwarding : supported=%d, requested=%d",
                                                sco_forwarding_supported, allow_sco_forwarding);
    if (allow_sco_forwarding)
    {
#ifdef INCLUDE_MICFWD
        *chosen = is_wideband ? &chain_micfwd_wb_config : &chain_micfwd_nb_config;
#else
        *chosen = is_wideband ? &chain_scofwd_wb_config : &chain_scofwd_nb_config;
#endif /* INCLUDE_MICFWD */
    }
    else
    {
        *chosen = is_wideband ? &chain_sco_wb_config : &chain_sco_nb_config;
    }
#else /* HFP_USE_2MIC */
    DEBUG_LOGF("appKymeraGetScoChainConfig. 2 Mic. Sco Forwarding : supported=%d. requested=%d",
                                                sco_forwarding_supported, allow_sco_forwarding);

    if (allow_sco_forwarding)
    {
#ifdef INCLUDE_MICFWD
        *chosen = is_wideband ? &chain_micfwd_wb_2mic_config : &chain_micfwd_nb_2mic_config;
#else
        *chosen = is_wideband ? &chain_scofwd_wb_2mic_config : &chain_scofwd_nb_2mic_config;
#endif /* INCLUDE_MICFWD */
    }
    else
    {
        *chosen = is_wideband ? &chain_sco_wb_2mic_config : &chain_sco_nb_2mic_config;
    }
#endif /* HFP_USE_2MIC */
}


void appKymeraHandleInternalScoStart(Sink audio_sink, hfp_wbs_codec_mask codec,
                                     uint8 wesco, uint16 volume, bool allow_scofwd)
{
    UNUSED(wesco);

    kymeraTaskData *theKymera = appGetKymera();
    const uint32_t rate = (codec == hfp_wbs_codec_mask_msbc) ? 16000 : 8000;
    const chain_config_t *chain_config;
    kymera_chain_handle_t scoChain = appKymeraGetScoChain();
    bool is_narrowband = (rate == 8000);

    DEBUG_LOGF("appKymeraHandleInternalScoStart, sink 0x%x, rate %u, wesco %u, state %u", audio_sink, rate, wesco, theKymera->state);

    if (theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptible tone, so cut it off */
        appKymeraTonePromptStop();
    }

    /* Can't start voice chain if we're not idle */
    PanicFalse(theKymera->state == KYMERA_STATE_IDLE);

    /* SCO chain must be destroyed if we get here */
    PanicNotNull(scoChain);

    /* Move to SCO active state now, what ever happens we end up in this state
      (even if it's temporary) */
    theKymera->state = KYMERA_STATE_SCO_ACTIVE;

    /* Configure DSP power mode appropriately for CVC */
    appKymeraConfigureDspPowerMode(FALSE);

    appKymeraGetScoChainConfig(&chain_config, codec, allow_scofwd);

    /* Create chain */
    scoChain = ChainCreate(chain_config);
    theKymera->chainu.sco_handle = scoChain;

    /* Get sources and sinks for chain */
    Source sco_src = ChainGetOutput(scoChain, EPR_SCO_TO_AIR);
    Sink sco_sink = ChainGetInput(scoChain, EPR_SCO_FROM_AIR);
    Source audio_source = StreamSourceFromSink(audio_sink);
    Source mic_src1 = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_A);
    Sink mic_sink1 = ChainGetInput(scoChain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Source mic_src1b = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_B);
    Sink mic_sink1b = ChainGetInput(scoChain, EPR_SCO_MIC2);
#endif
    Source speaker_src = ChainGetOutput(scoChain, EPR_SCO_SPEAKER);
    Sink speaker_snk = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());

    /* Set AEC REF sample rate */
    Operator aec_op = ChainGetOperatorByRole(scoChain, OPR_SCO_AEC);
    OperatorsAecSetSampleRate(aec_op, rate, rate);
    appKymeraSetTerminalBufferSize(aec_op, rate, AEC_TX_BUFFER_SIZE_MS, 1<<0, 0);

    Operator sco_op;
    PanicFalse(GET_OP_FROM_CHAIN(sco_op, scoChain, OPR_SCO_RECEIVE));

#ifdef INCLUDE_SCOFWD
    if (allow_scofwd)
    {
        Operator awbs_op;
        PanicFalse(GET_OP_FROM_CHAIN(awbs_op, scoChain, OPR_SCOFWD_SEND));
        OperatorsAwbsSetBitpoolValue(awbs_op, SFWD_MSBC_BITPOOL, FALSE);

        if (is_narrowband)
        {
            Operator upsampler_op;
            PanicFalse(GET_OP_FROM_CHAIN(upsampler_op, scoChain, OPR_SCO_UP_SAMPLE));
            OperatorsResamplerSetConversionRate(upsampler_op, 8000, 16000);

        }
        else
        {
            /* Wideband chains add a basic passthrough in place of the resampler.
               This is currently required to avoid issues when the splitter
               is connected directly to the encoder. */
            Operator basic_pass;
            PanicFalse(GET_OP_FROM_CHAIN(basic_pass, scoChain, OPR_SCOFWD_BASIC_PASS));
            OperatorsStandardSetBufferSize(basic_pass, SCOFWD_BASIC_PASS_BUFFER_SIZE);
        }
#ifdef INCLUDE_MICFWD
        /* setup the mic recv spc for encoded data */
        Operator spc_op;
        PanicFalse(GET_OP_FROM_CHAIN(spc_op, scoChain, OPR_MICFWD_RECV_SPC));
        appKymeraConfigureSpcDataFormat(spc_op, ADF_16_BIT_WITH_METADATA);

        Operator mic_recv = ChainGetOperatorByRole(scoChain,OPR_MICFWD_RECV);
        OperatorsAwbsSetBitpoolValue(mic_recv, SFWD_MSBC_BITPOOL, TRUE);
        OperatorsStandardSetBufferSize(mic_recv, SFWD_MICFWD_RECV_CHAIN_BUFFER_SIZE);

        Operator mic_switch = ChainGetOperatorByRole(scoChain, OPR_MICFWD_SPC_SWITCH);
        appKymeraConfigureSpcDataFormat(mic_switch, ADF_PCM);

        /* resample the incoming mic data to the SCO sample rate if necessary */
        if (is_narrowband)
        {
            Operator downsampler_op;
            PanicFalse(GET_OP_FROM_CHAIN(downsampler_op, scoChain, OPR_MIC_DOWN_SAMPLE));
            OperatorsResamplerSetConversionRate(downsampler_op, 16000, 8000);
        }

#endif /* MICFWD */

        Operator splitter_op;
        PanicFalse(GET_OP_FROM_CHAIN(splitter_op, scoChain, OPR_SCOFWD_SPLITTER));
        OperatorsStandardSetBufferSize(splitter_op, SFWD_SEND_CHAIN_BUFFER_SIZE);
        OperatorsSplitterSetDataFormat(splitter_op, operator_data_format_pcm);

        /* Configure passthrough for encoded data so we can connect. */
        Operator switch_op;
        PanicFalse(GET_OP_FROM_CHAIN(switch_op, scoChain, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
        appKymeraConfigureSpcDataFormat(switch_op,  ADF_GENERIC_ENCODED);

        OperatorsStandardSetTimeToPlayLatency(sco_op, SFWD_TTP_DELAY_US);
    }
    else
#endif /* INCLUDE_SCOFWD */
    {
        /*! \todo Need to decide ahead of time if we need any latency.
            Simple enough to do if we are legacy or not. Less clear if
            legacy but no peer connection */
        /* Enable Time To Play if supported */
        if (appConfigScoChainTTP(wesco) != 0)
        {
            OperatorsStandardSetTimeToPlayLatency(sco_op, appConfigScoChainTTP(wesco));
            /*! \todo AEC Gata is V2 silicon, downloadable has native TTP support*/
            OperatorsAecEnableTtpGate(aec_op, TRUE, 50, TRUE);
        }
    }

    appKymeraConfigureOutputChainOperators(scoChain, rate, KICK_PERIOD_VOICE, 0, 0);

    appKymeraSetOperatorUcids(TRUE, is_narrowband);

    /* Set DAC and ADC sample rate */
    SourceConfigure(mic_src1, STREAM_CODEC_INPUT_RATE, rate);
    SourceConfigure(mic_src1, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());

#ifdef HFP_USE_2MIC
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_RATE, rate);
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());
    SourceSynchronise(mic_src1,mic_src1b);
#endif
    SinkConfigure(speaker_snk, STREAM_CODEC_OUTPUT_RATE, rate);

    /* Conect it all together */
    StreamConnect(sco_src, audio_sink);
    StreamConnect(audio_source, sco_sink);
    StreamConnect(mic_src1, mic_sink1);
#ifdef HFP_USE_2MIC
    StreamConnect(mic_src1b, mic_sink1b);
#endif
    StreamConnect(speaker_src, speaker_snk);
    ChainConnect(scoChain);

#ifdef INCLUDE_MICFWD
    if (allow_scofwd)
    {
        /* Chain connection sets the switch into consume mode.
           Select the local Microphone */
        appKymeraSelectSpcSwitchInput(appKymeraGetMicSwitch(), MIC_SELECTION_LOCAL);
    }
#endif

    /* Enable external amplifier if required */
    appKymeraExternalAmpControl(TRUE);

    /* Turn on MIC bias */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_FORCE_ON);

    /* The chain can fail to start if the SCO source disconnects whilst kymera
    is queuing the SCO start request or starting the chain. If the attempt fails,
    ChainStartAttempt will stop (but not destroy) any operators it started in the chain. */
    if (ChainStartAttempt(scoChain))
    {
        theKymera->output_rate = rate;
        appKymeraHandleInternalScoSetVolume(volume);
    }
    else
    {
        DEBUG_LOG("appKymeraHandleInternalScoStart could not start chain");
        /* Stop/destroy the chain, returning state to KYMERA_STATE_IDLE.
        This needs to be done here, since between the failed attempt to start
        and the subsequent stop (when appKymeraScoStop() is called), a tone
        may need to be played - it would not be possible to play a tone in a
        stopped SCO chain. The state needs to be KYMERA_STATE_SCO_ACTIVE for
        appKymeraHandleInternalScoStop() to stop/destroy the chain. */
        appKymeraHandleInternalScoStop();
    }
}

void appKymeraHandleInternalScoStop(void)
{
    kymeraTaskData *theKymera = appGetKymera();
    kymera_chain_handle_t scoChain;

    DEBUG_LOGF("appKymeraHandleInternalScoStop, state %u", theKymera->state);

    if (theKymera->state == KYMERA_STATE_IDLE)
    {
        /* Attempting to stop a SCO chain when IDLE. This happens when the user
        calls appKymeraScoStop() following a failed attempt to start the SCO
        chain - see ChainStartAttempt() in appKymeraHandleInternalScoStart().
        In this case, there is nothing to do, since the failed start attempt
        cleans up by calling this function in state KYMERA_STATE_SCO_ACTIVE */
        DEBUG_LOG("appKymeraHandleInternalScoStop, not stopping - already idle");
        return;
    }

    /* Stop forwarding first */
    if (theKymera->state == KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING)
    {
        PanicFalse(appKymeraHandleInternalScoStopForwardingTx());
    }

    PanicFalse(theKymera->state == KYMERA_STATE_SCO_ACTIVE);
    scoChain = appKymeraGetScoChain();
    PanicNull(scoChain);

    Source send_src = ChainGetOutput(scoChain, EPR_SCO_TO_AIR);
    Sink rcv_sink = ChainGetInput(scoChain, EPR_SCO_FROM_AIR);
    Sink send_sink1 = ChainGetInput(scoChain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Sink send_sink1b = ChainGetInput(scoChain, EPR_SCO_MIC2);
#endif
    Source rcv_src = ChainGetOutput(scoChain, EPR_SCO_SPEAKER);

    /* A tone still playing at this point must be interruptable */
    appKymeraTonePromptStop();

    /* Stop chains */
    ChainStop(scoChain);

    StreamDisconnect(send_src, send_sink1);
#ifdef HFP_USE_2MIC
    StreamDisconnect((Source)NULL, send_sink1b);
#endif
    StreamDisconnect(rcv_src, rcv_sink);

    /* Destroy chains */
    ChainDestroy(scoChain);
    theKymera->chainu.sco_handle = scoChain = NULL;

    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Turn off MIC bias */
    /*! \todo Check if MIC_BIAS different for 2 mic */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_OFF);

    /* Update state variables */
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
}

#ifdef INCLUDE_SCOFWD
static void appKymeraSfwdSetSwitchedPassthrough(switched_passthrough_states state)
{
    kymera_chain_handle_t scoChain = appKymeraGetScoChain();

    Operator spc_op = (Operator)PanicZero(ChainGetOperatorByRole(
                                                scoChain, 
                                                OPR_SWITCHED_PASSTHROUGH_CONSUMER));
    appKymeraConfigureSpcMode(spc_op, state);
}

void appKymeraSfwdForwardingPause(bool pause)
{
    DEBUG_LOGF("appKymeraSfwdForwardingPause %u", pause);
    appKymeraSfwdSetSwitchedPassthrough(pause ? CONSUMER_MODE : PASSTHROUGH_MODE);
}

void appKymeraHandleInternalScoStartForwardingTx(Sink forwarding_sink)
{
    kymeraTaskData *theKymera = appGetKymera();
    kymera_chain_handle_t scoChain = appKymeraGetScoChain();

    if (   KYMERA_STATE_SCO_ACTIVE != theKymera->state
        || NULL == scoChain)
    {
        return;
    }

    Source audio_source = ChainGetOutput(scoChain, EPR_SCOFWD_TX_OTA);
    
    DEBUG_LOGF("appKymeraHandleInternalScoStartForwardingTx, sink %p, source %p, state %d",
                forwarding_sink, audio_source, theKymera->state);

    PanicNotZero(theKymera->lock);

    /* SCOFWD setup */
    /* Tell scoFwd what the audio source of SCO frames is, and enable the 
     * passthrough to give it the SCO frames. */
    appScoFwdInitScoPacketising(audio_source);
    appKymeraSfwdSetSwitchedPassthrough(PASSTHROUGH_MODE);

#ifdef INCLUDE_MICFWD
    /* MICFWD setup */
    /* Tell scoFwd what the forwarded mic data sink is */
    Sink micfwd_sink = ChainGetInput(scoChain, EPR_MICFWD_RX_OTA);
    appScoFwdNotifyIncomingMicSink(micfwd_sink);
    /* use the currently selected microphone */
    appKymeraSelectSpcSwitchInput(appKymeraGetMicSwitch(), theKymera->mic); 
    /* put the mic recv spc into passthrough mode */
    Operator spc_op = ChainGetOperatorByRole(scoChain, OPR_MICFWD_RECV_SPC);
    appKymeraConfigureSpcMode(spc_op, FALSE);
#endif

    theKymera->state = KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING;
}

bool appKymeraHandleInternalScoStopForwardingTx(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraHandleInternalScoStopForwardingTx, state %u", theKymera->state);

    if (theKymera->state != KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING)
    {
        return FALSE;
    }

    appKymeraSfwdSetSwitchedPassthrough(CONSUMER_MODE);

#ifdef INCLUDE_MICFWD
    /* put the mic recv spc into consume mode */
    kymera_chain_handle_t scoChain = appKymeraGetScoChain();
    Operator spc_op = ChainGetOperatorByRole(scoChain, OPR_MICFWD_RECV_SPC);
    appKymeraConfigureSpcMode(spc_op, TRUE);
#endif

    theKymera->state = KYMERA_STATE_SCO_ACTIVE;

    return TRUE;
}

void appKymeraHandleInternalScoForwardingStartRx(const KYMERA_INTERNAL_SCOFWD_RX_START_T *start_req)
{
    kymeraTaskData *theKymera = appGetKymera();
    const uint32_t rate = 16000;
    kymera_chain_handle_t scoChain = appKymeraGetScoChain();
    const chain_config_t *config;
    bool enable_mic_fwd = start_req->enable_mic_fwd;
#ifndef INCLUDE_MICFWD
    enable_mic_fwd = FALSE;
#endif
    DEBUG_LOGF("appKymeraHandleInternalScoForwardingStartRx, start source 0x%x, rate %u, state %u, micfwd %d", 
                start_req->link_source, rate, theKymera->state, enable_mic_fwd);

    PanicNotZero(theKymera->lock);

    if (theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptible tone, so cut it off */
        appKymeraTonePromptStop();
    }

    /* If we are not idle (a pre-requisite) and this message can be delayed,
       then re-send it. The normal situation is message delays when stopping 
       A2DP/AV. That is calls were issued in the right order to stop A2DP then 
       start SCO receive but the number of messages required for each were 
       different, leading the 2nd action to complete 1st. */
    if (   start_req->pre_start_delay
        && theKymera->state != KYMERA_STATE_IDLE)
    {
        DEBUG_LOG("appKymeraHandleInternalScoForwardingStartRx. Re-queueing.");
        appKymeraScoFwdStartReceiveHelper(start_req->link_source, start_req->volume, 
                                          enable_mic_fwd, 
                                          start_req->pre_start_delay - 1);
        return;
    }

    /* Can't start voice chain if we're not idle */
    PanicFalse(theKymera->state == KYMERA_STATE_IDLE);

    /* SCO chain must be destroyed if we get here */
    PanicNotNull(scoChain);

#ifdef HFP_USE_2MIC
    config = enable_mic_fwd ? &chain_micfwd_send_2mic_config : &chain_scofwd_recv_2mic_config;
#else
    config = enable_mic_fwd ? &chain_micfwd_send_config : &chain_scofwd_recv_config;
#endif
    scoChain = ChainCreate(config);

    theKymera->chainu.sco_handle = scoChain;

    /* Get sources and sinks for chain */
    Sink sco_sink = ChainGetInput(scoChain, EPR_SCOFWD_RX_OTA);
    PanicFalse(SinkMapInit(sco_sink, STREAM_TIMESTAMPED, AUDIO_FRAME_METADATA_LENGTH));

    appScoFwdNotifyIncomingSink(sco_sink);

#ifdef INCLUDE_MICFWD
    if (enable_mic_fwd)
    {
        Source micfwd_src = ChainGetOutput(scoChain, EPR_MICFWD_TX_OTA);
        appScoFwdInitMicPacketising(micfwd_src);
    }
#endif

    Source mic_src1 = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_A);
    Sink mic_sink1 = ChainGetInput(scoChain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Source mic_src1b = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_B);
    Sink mic_sink1b = ChainGetInput(scoChain, EPR_SCO_MIC2);
#endif
    Source speaker_src = ChainGetOutput(scoChain, EPR_SCO_SPEAKER);
    Sink speaker_snk = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());

    /* Set AEC REF sample rate */
    Operator aec_op = ChainGetOperatorByRole(scoChain, OPR_SCO_AEC);
    OperatorsAecSetSampleRate(aec_op, rate, rate);
    OperatorsStandardSetTimeToPlayLatency(aec_op, SFWD_TTP_DELAY_US);
    appKymeraSetAecSameIOClockSource(aec_op);

    Operator awbs_op = ChainGetOperatorByRole(scoChain, OPR_SCOFWD_RECV);
    OperatorsAwbsSetBitpoolValue(awbs_op, SFWD_MSBC_BITPOOL,TRUE);
    OperatorsStandardSetBufferSize(awbs_op, SFWD_RECV_CHAIN_BUFFER_SIZE);

    if (enable_mic_fwd)
    {
        Operator micwbs_op = ChainGetOperatorByRole(scoChain, OPR_MICFWD_SEND);
        OperatorsAwbsSetBitpoolValue(micwbs_op, SFWD_MSBC_BITPOOL, FALSE);
    }

    /*! \todo Before updating from Products, this was not muting */
    appKymeraConfigureOutputChainOperators(scoChain, rate, KICK_PERIOD_VOICE, 0, 0);

    appKymeraSetOperatorUcids(TRUE, FALSE);

    /* Set DAC and ADC sample rate */
    SourceConfigure(mic_src1, STREAM_CODEC_INPUT_RATE, rate);
    SourceConfigure(mic_src1, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());

#ifdef HFP_USE_2MIC
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_RATE, rate);
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());
    SourceSynchronise(mic_src1,mic_src1b);
#endif
    SinkConfigure(speaker_snk, STREAM_CODEC_OUTPUT_RATE, rate);

    /* Conect it all together */
    PanicFalse(StreamConnect(mic_src1, mic_sink1));
#ifdef HFP_USE_2MIC
    StreamConnect(mic_src1b, mic_sink1b);
#endif
    PanicFalse(StreamConnect(speaker_src, speaker_snk));

    ChainConnect(scoChain);

    /* Enable external amplifier if required */
    appKymeraExternalAmpControl(TRUE);

    /* Turn on MIC bias */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_FORCE_ON);

    /* Start chain */
    ChainStart(scoChain);

    if (enable_mic_fwd)
    {
        DEBUG_LOG("STARTING MIC SIDE OF CHAIN");
        appKymeraSfwdSetSwitchedPassthrough(PASSTHROUGH_MODE);
    }

    /* Move to SCO active state */
    theKymera->state = KYMERA_STATE_SCOFWD_RX_ACTIVE;
    theKymera->output_rate = rate;

    appKymeraHandleInternalScoSetVolume(start_req->volume);
}

void appKymeraHandleInternalScoForwardingStopRx(void)
{
    kymeraTaskData *theKymera = appGetKymera();
    kymera_chain_handle_t scoChain = theKymera->chainu.sco_handle;

    DEBUG_LOGF("appKymeraScoFwdStop now, state %u", theKymera->state);

    PanicNotZero(theKymera->lock);

    PanicFalse(theKymera->state == KYMERA_STATE_SCOFWD_RX_ACTIVE);
    PanicNull(scoChain);

    Source mic_src1 = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_A);
    Sink mic_sink1 = ChainGetInput(scoChain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Source mic_src1b = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_B);
    Sink mic_sink1b = ChainGetInput(scoChain, EPR_SCO_MIC2);
#endif
    Source speaker_src = ChainGetOutput(scoChain, EPR_SCO_SPEAKER);
    Sink speaker_snk = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());

    /* A tone still playing at this point must be interruptable */
    appKymeraTonePromptStop();

    /* Stop chains */
    ChainStop(scoChain);

    StreamDisconnect(mic_src1, mic_sink1);
#ifdef HFP_USE_2MIC
    StreamDisconnect(mic_src1b, mic_sink1b);
#endif
    StreamDisconnect(speaker_src, speaker_snk);

    /* Destroy chains */
    ChainDestroy(scoChain);
    theKymera->chainu.sco_handle = scoChain = NULL;

    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Turn off MIC bias */
    /*! \todo Check if MIC_BIAS different for 2 mic */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_OFF);

    /* Update state variables */
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
}

#endif /* INCLUDE_SCOFWD */

void appKymeraHandleInternalScoSetVolume(uint8 volume)
{
    kymera_chain_handle_t scoChain = appKymeraGetScoChain();

    DEBUG_LOGF("appKymeraHandleInternalScoSetVolume, vol %u", volume);

    switch (appGetKymera()->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
        case KYMERA_STATE_SCOFWD_RX_ACTIVE:
        {
            uint16 volume_scaled = ((uint16)volume * 127) / 15;
            appKymeraSetMainVolume(scoChain, volume_scaled);
        }
        break;
        default:
            break;
    }
}

void appKymeraHandleInternalScoMicMute(bool mute)
{
    DEBUG_LOGF("appKymeraHandleInternalScoMicMute, mute %u", mute);

    switch (appGetKymera()->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        {
            Operator aec_op;

            if (GET_OP_FROM_CHAIN(aec_op, appKymeraGetScoChain(), OPR_SCO_AEC))
            {
                OperatorsAecMuteMicOutput(aec_op, mute);
            }
        }
        break;

        default:
            break;
    }
}

uint8 appKymeraScoVoiceQuality(void)
{
    uint8 quality = appConfigVoiceQualityWorst();

    if (appConfigVoiceQualityMeasurementEnabled())
    {
        Operator cvc_send_op;
        if (GET_OP_FROM_CHAIN(cvc_send_op, appKymeraGetScoChain(), OPR_CVC_SEND))
        {
            uint16 rx_msg[2], tx_msg = OPMSG_COMMON_GET_VOICE_QUALITY;
            PanicFalse(OperatorMessage(cvc_send_op, &tx_msg, 1, rx_msg, 2));
            quality = MIN(appConfigVoiceQualityBest() , rx_msg[1]);
            quality = MAX(appConfigVoiceQualityWorst(), quality);
        }
    }
    else
    {
        quality = appConfigVoiceQualityWhenDisabled();
    }

    DEBUG_LOGF("appKymeraScoVoiceQuality %u", quality);

    return quality;
}
