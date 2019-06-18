/*!
\copyright  Copyright (c) 2017-2018  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\brief      Kymera common code
*/

#include <audio_clock.h>
#include <audio_power.h>

#include "av_headset_kymera_private.h"

#include "chains/chain_output_volume.h"

/*! Convert a channel ID to a bit mask */
#define CHANNEL_TO_MASK(channel) ((uint32)1 << channel)

/*!@{ \name Port numbers for the Source Sync operator */
#define KYMERA_SOURCE_SYNC_INPUT_PORT (0)
#define KYMERA_SOURCE_SYNC_OUTPUT_PORT (0)
/*!@} */

/* Configuration of source sync groups and routes */
static const source_sync_sink_group_t sink_groups[] =
{
    {
        .meta_data_required = TRUE,
        .rate_match = FALSE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_INPUT_PORT)
    }
};

static const source_sync_source_group_t source_groups[] =
{
    {
        .meta_data_required = TRUE,
        .ttp_required = TRUE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_OUTPUT_PORT)
    }
};

static source_sync_route_t routes[] =
{
    {
        .input_terminal = KYMERA_SOURCE_SYNC_INPUT_PORT,
        .output_terminal = KYMERA_SOURCE_SYNC_OUTPUT_PORT,
        .transition_samples = 0,
        .sample_rate = 0, /* Overridden later */
        .gain = 0
    }
};

int32 volTo60thDbGain(uint16 volume)
{
    int32 gain = -90;
    if (volume)
    {
        int32 minv = appConfigMinVolumedB();
        int32 maxv = appConfigMaxVolumedB();
        gain = volume * (maxv - minv);
        gain /= 127;
        gain += minv;
    }
    return gain * KYMERA_DB_SCALE;
}

void appKymeraConfigureDspPowerMode(bool tone_playing)
{
#if (defined(__QCC302X_APPS__) || defined(__QCC512X_APPS__) || defined(__QCC3400_APP__))
    kymeraTaskData *theKymera = appGetKymera();

    /* Assume we are switching to the low power slow clock unless one of the
     * special cases below applies */
    audio_dsp_clock_configuration cconfig = {
        .active_mode = AUDIO_DSP_SLOW_CLOCK,
        .low_power_mode = AUDIO_DSP_CLOCK_NO_CHANGE,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };
    audio_dsp_clock kclocks;
    audio_power_save_mode mode = AUDIO_POWER_SAVE_MODE_3;

    if (   theKymera->state == KYMERA_STATE_SCO_ACTIVE
        || theKymera->state == KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING
        || theKymera->state == KYMERA_STATE_SCOFWD_RX_ACTIVE)
    {
        /* Always jump up to normal clock for CVC */
        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
        mode = AUDIO_POWER_SAVE_MODE_1;
    }
    else if (tone_playing)
    {
        /* Always jump up to normal clock for tones - for most codecs there is
         * not enough MIPs when running on a slow clock to also play a tone */
        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
        mode = AUDIO_POWER_SAVE_MODE_1;
    }
    else
    {
        /* Either setting up for the first time or returning from a tone, in
         * either case return to the default clock rate for the codec in use */
        switch (theKymera->a2dp_seid)
        {
            case AV_SEID_APTX_SNK:
                /* Not enough MIPs to run aptX master (TWS standard) on slow clock */
                cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
                break;

            default:
                break;
        }
    }

    PanicFalse(AudioDspClockConfigure(&cconfig));
    PanicFalse(AudioPowerSaveModeSet(mode));

    PanicFalse(AudioDspGetClock(&kclocks));
    mode = AudioPowerSaveModeGet();
    DEBUG_LOGF("appKymeraConfigureDspPowerMode, kymera clocks %d %d %d, mode %d", kclocks.active_mode, kclocks.low_power_mode, kclocks.trigger_mode, mode);
#else
    UNUSED(tone_playing);
#endif
}

void appKymeraExternalAmpSetup(void)
{
    if (appConfigExternalAmpControlRequired())
    {
        int pio_mask = (1 << (appConfigExternalAmpControlPio() % 32));
        int pio_bank = appConfigExternalAmpControlPio() / 32;

        /* map in PIO */
        PioSetMapPins32Bank(pio_bank, pio_mask, pio_mask);
        /* set as output */
        PioSetDir32Bank(pio_bank, pio_mask, pio_mask);
        /* start disabled */
        PioSet32Bank(pio_bank, pio_mask,
                     appConfigExternalAmpControlDisableMask());
    }
}

void appKymeraExternalAmpControl(bool enable)
{
    kymeraTaskData *theKymera = appGetKymera();

    if (appConfigExternalAmpControlRequired())
    {
        int pio_mask = (1 << (appConfigExternalAmpControlPio() % 32));
        int pio_bank = appConfigExternalAmpControlPio() / 32;

        PioSet32Bank(pio_bank, pio_mask,
                     enable ? appConfigExternalAmpControlEnableMask() :
                              appConfigExternalAmpControlDisableMask());
    }

    if(enable)
    {
        if(MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE))
        {
            DEBUG_LOG("appKymeraExternalAmpControl, there is already a client for the audio SS");
        }
        else
        {
            DEBUG_LOG("appKymeraExternalAmpControl, adding a client to the audio SS");
            OperatorFrameworkEnable(MAIN_PROCESSOR_ON);
        }
    }
    else
    {
        DEBUG_LOG("appKymeraExternalAmpControl, sending later KYMERA_INTERNAL_AUDIO_SS_DISABLE");
        MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE, NULL, appConfigDacDisconnectionDelayMs());
    }
}

void appKymeraConfigureSpcMode(Operator op, bool is_consumer)
{
    uint16 msg[2];
    msg[0] = OPMSG_SPC_ID_TRANSITION; /* MSG ID to set SPC mode transition */
    msg[1] = is_consumer;
    PanicFalse(OperatorMessage(op, msg, 2, NULL, 0));
}

void appKymeraConfigureSpcDataFormat(Operator op, audio_data_format format)
{
    uint16 msg[2];
    msg[0] = OPMSG_OP_TERMINAL_DATA_FORMAT; /* MSG ID to set SPC data type */
    msg[1] = format;
    PanicFalse(OperatorMessage(op, msg, 2, NULL, 0));
}

static void appKymeraConfigureSourceSync(Operator op, uint32 rate, unsigned kick_period_us)
{
    /* Override sample rate in routes config */
    routes[0].sample_rate = rate;

    /* Send operator configuration messages */
    OperatorsStandardSetSampleRate(op, rate);
    OperatorsSourceSyncSetSinkGroups(op, DIMENSION_AND_ADDR_OF(sink_groups));
    OperatorsSourceSyncSetSourceGroups(op, DIMENSION_AND_ADDR_OF(source_groups));
    OperatorsSourceSyncSetRoutes(op, DIMENSION_AND_ADDR_OF(routes));

    /* Output buffer needs to be able to hold at least SS_MAX_PERIOD worth
     * of audio (default = 2 * Kp), but be less than SS_MAX_LATENCY (5 * Kp).
     * The recommendation is 2 Kp more than SS_MAX_PERIOD, so 4 * Kp. */
    OperatorsStandardSetBufferSize(op, US_TO_BUFFER_SIZE_MONO_PCM(4 * kick_period_us, rate));
}

void appKymeraSetMainVolume(kymera_chain_handle_t chain, uint16 volume)
{
    Operator volop;

    if (GET_OP_FROM_CHAIN(volop, chain, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainGain(volop, volTo60thDbGain(volume));
    }
}

void appKymeraSetVolume(kymera_chain_handle_t chain, uint16 volume)
{
    Operator volop;

    if (GET_OP_FROM_CHAIN(volop, chain, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainAndAuxGain(volop, volTo60thDbGain(volume));
    }
}

void appKymeraConfigureOutputChainOperators(kymera_chain_handle_t chain,
                                            uint32 sample_rate, unsigned kick_period,
                                            unsigned buffer_size, uint8 volume)
{
    Operator sync_op;
    Operator volume_op;
    Operator peq_op;

    /* Configure operators */
    if (GET_OP_FROM_CHAIN(sync_op, chain, OPR_SOURCE_SYNC))
    {
        /* SourceSync is optional in chains. */
        appKymeraConfigureSourceSync(sync_op, sample_rate, kick_period);
    }

    volume_op = ChainGetOperatorByRole(chain, OPR_VOLUME_CONTROL);
    OperatorsStandardSetSampleRate(volume_op, sample_rate);

	peq_op = ChainGetOperatorByRole(chain, OPR_PEQ);
	if(peq_op != INVALID_OPERATOR)
	{
	OperatorsStandardSetUCID(peq_op, 0x00);
	OperatorsStandardSetSampleRate(peq_op, sample_rate);
	}

    appKymeraSetVolume(chain, volume);

    if (buffer_size)
    {
        Operator op = ChainGetOperatorByRole(chain, OPR_LATENCY_BUFFER);
        OperatorsStandardSetBufferSize(op, buffer_size);
    }
}

void appKymeraCreateOutputChain(uint32 rate, unsigned kick_period,
                                unsigned buffer_size, uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();
    Sink dac;
    kymera_chain_handle_t chain;

    /* Create chain */
    chain = ChainCreate(&chain_output_volume_config);
    theKymera->chainu.output_vol_handle = chain;

    appKymeraConfigureOutputChainOperators(chain, rate, kick_period, buffer_size, volume);

    /* Configure the DAC channel */
    dac = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());
    PanicFalse(SinkConfigure(dac, STREAM_CODEC_OUTPUT_RATE, rate));
    PanicFalse(SinkConfigure(dac, STREAM_RM_ENABLE_DEFERRED_KICK, 0));

    ChainConnect(chain);
    PanicFalse(ChainConnectOutput(chain, dac, EPR_SOURCE_MIXER_OUT));
}

/*! \brief Set the UCID for a single operator */
static bool appKymeraSetOperatorUcid(kymera_chain_handle_t chain, chain_operator_role_t role, kymera_operator_ucid_t ucid)
{
    Operator op;
    if (GET_OP_FROM_CHAIN(op, chain, role))
    {
        OperatorsStandardSetUCID(op, ucid);
        return TRUE;
    }
    return FALSE;
}

void appKymeraSetOperatorUcids(bool is_sco, bool is_narrowband_sco)
{
    /* Operators that have UCID set either reside in sco_handle or output_vol_handle
       which are both in the chainu union. */
    kymera_chain_handle_t chain = appGetKymera()->chainu.output_vol_handle;

    if (is_sco)
    {
        // All SCO chains have AEC and CVC send.
        PanicFalse(appKymeraSetOperatorUcid(chain, OPR_SCO_AEC, is_narrowband_sco ? UCID_AEC_NB : UCID_AEC_WB));
        PanicFalse(appKymeraSetOperatorUcid(chain, OPR_CVC_SEND, UCID_CVC_SEND));
        // SCO|MIC fwd RX chains do not have CVC recv.
        appKymeraSetOperatorUcid(chain, OPR_CVC_RECEIVE, UCID_CVC_RECEIVE);
    }

    // Operators common to SCO/A2DP chains
    PanicFalse(appKymeraSetOperatorUcid(chain, OPR_VOLUME_CONTROL, UCID_VOLUME_CONTROL));
    PanicFalse(appKymeraSetOperatorUcid(chain, OPR_SOURCE_SYNC, UCID_SOURCE_SYNC));
}
