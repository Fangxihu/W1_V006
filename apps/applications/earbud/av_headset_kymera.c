/*!
\copyright  Copyright (c) 2017-2018 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_kymera.c
\brief      Kymera Manager
*/

#include "av_headset_kymera_private.h"

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

static const capability_bundle_t capability_bundle[] = {
    {
        "download_switched_passthrough_consumer.edkcs",
        capability_bundle_available_p0
    },
    {
        "download_aptx_demux.edkcs",
        capability_bundle_available_p0
    },
#if defined(INCLUDE_SCOFWD)
    /*  Chains for SCO forwarding.
        Likely to update to use the downloadable AEC regardless
        as offers better TTP support (synchronisation) and other
        extensions */
    {
        "download_async_wbs.edkcs",
        capability_bundle_available_p0
    },
    {
        "download_aec_reference.edkcs",
        capability_bundle_available_p0
    },
#endif
};

static const capability_bundle_config_t bundle_config = {capability_bundle, ARRAY_DIM(capability_bundle)};

void appKymeraPromptPlay(FILE_INDEX prompt, promptFormat format, uint32 rate,
                         bool interruptible, uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraPromptPlay, queue prompt %d, int %u", prompt, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = NULL;
    message->prompt = prompt;
    message->prompt_format = format;
    message->rate = rate;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
}

void appKymeraTonePlay(const ringtone_note *tone, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraTonePlay, queue tone %p, int %u", tone, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = tone;
    message->prompt = FILE_NONE;
    message->rate = KYMERA_TONE_GEN_RATE;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
}

void appKymeraA2dpStart(uint16 *client_lock, uint16 client_lock_mask,
                        const a2dp_codec_settings *codec_settings,
                        uint8 volume, uint8 master_pre_start_delay)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraA2dpStart, seid=%u", codec_settings->seid);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
    message->lock = client_lock;
    message->lock_mask = client_lock_mask;
    message->codec_settings = *codec_settings;
    message->volume = volume;
    message->master_pre_start_delay = master_pre_start_delay;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_START,
                             message, &theKymera->lock);
}

void appKymeraA2dpStop(uint8 seid, Source source)
{
    kymeraTaskData *theKymera = appGetKymera();
    MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                KYMERA_INTERNAL_A2DP_STOP;

    DEBUG_LOGF("appKymeraA2dpStop, seid=%u", seid);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_STOP);
    message->seid = seid;
    message->source = source;
    MessageSendConditionally(&theKymera->task, mid, message, &theKymera->lock);
}

bool appKymeraScoStartForwarding(Sink forwarding_sink, bool enable_mic_fwd)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraScoStartForwarding, queue sink %p, state %u", forwarding_sink, theKymera->state);

    PanicNull(forwarding_sink);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START_FORWARDING_TX);
    message->forwarding_sink = forwarding_sink;
    message->enable_mic_fwd = enable_mic_fwd;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START_FORWARDING_TX, message, &theKymera->lock);

    return TRUE;
}

bool appKymeraScoStopForwarding(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    if (!appKymeraHandleInternalScoStopForwardingTx())
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX, NULL, &theKymera->lock);
    }
    return TRUE;
}

void appKymeraA2dpSetVolume(uint16 volume)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraA2dpSetVolume msg, vol %u", volume);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_SET_VOL);
    message->volume = volume;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_SET_VOL, message, &theKymera->lock);
}

static void appKymeraScoStartHelper(Sink audio_sink, hfp_wbs_codec_mask codec, uint8 wesco,
                        uint16 volume, uint8 pre_start_delay, bool allow_scofwd, bool conditionally)
{
    kymeraTaskData *theKymera = appGetKymera();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START);
    PanicNull(audio_sink);

    message->audio_sink = audio_sink;
    message->codec      = codec;
    message->wesco      = wesco;
    message->volume     = volume;
    message->pre_start_delay = pre_start_delay;
    message->allow_scofwd = allow_scofwd;

    if (conditionally)
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START, message, &theKymera->lock);
    }
    else
    {
        MessageSend(&theKymera->task, KYMERA_INTERNAL_SCO_START, message);
    }
}
void appKymeraScoStart(Sink audio_sink, hfp_wbs_codec_mask codec, uint8 wesco,
                       uint16 volume, uint8 pre_start_delay, bool allow_scofwd)
{
    DEBUG_LOGF("appKymeraScoStart, queue sink 0x%x", audio_sink);
    appKymeraScoStartHelper(audio_sink, codec, wesco, volume, pre_start_delay, allow_scofwd, TRUE);
}

void appKymeraScoStop(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOG("appKymeraScoStop msg");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP, NULL, &theKymera->lock);
}

void appKymeraScoFwdStartReceiveHelper(Source link_source, uint8 volume, bool enable_mic_fwd, uint16 delay)
{
    kymeraTaskData *theKymera = appGetKymera();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCOFWD_RX_START);
    message->link_source = link_source;
    message->volume = volume;
    message->enable_mic_fwd = enable_mic_fwd;
    message->pre_start_delay = delay;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCOFWD_RX_START, message, &theKymera->lock);
}

void appKymeraScoFwdStartReceive(Source link_source, uint8 volume, bool enable_mic_fwd, uint16 pre_start_delay)
{
    DEBUG_LOGF("appKymeraScoFwdStartReceive, source 0x%x", link_source);

    PanicNull(link_source);
    appKymeraScoFwdStartReceiveHelper(link_source, volume, enable_mic_fwd, pre_start_delay);
}



void appKymeraScoFwdStopReceive(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOG("appKymeraScoFwdStopReceive");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCOFWD_RX_STOP, NULL, &theKymera->lock);
}

void appKymeraScoSetVolume(uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraScoSetVolume msg, vol %u", volume);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SET_VOL);
    message->volume = volume;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_SET_VOL, message, &theKymera->lock);
}

void appKymeraScoMicMute(bool mute)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraScoMicMute msg, mute %u", mute);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_MIC_MUTE);
    message->mute = mute;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_SCO_MIC_MUTE, message);
}

#ifdef INCLUDE_MICFWD
void appKymeraScoUseLocalMic(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOG("appKymeraScoUseLocalMic msg");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_MICFWD_LOCAL_MIC, NULL, &theKymera->lock);
}

void appKymeraScoUseRemoteMic(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOG("appKymeraScoUseRemoteMic msg");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_MICFWD_REMOTE_MIC, NULL, &theKymera->lock);
}
#endif /* INCLUDE_MICFWD */

static void kymera_dsp_msg_handler(MessageFromOperator *op_msg)
{
    PanicFalse(op_msg->len == KYMERA_OP_MSG_LEN);

    switch (op_msg->message[KYMERA_OP_MSG_WORD_MSG_ID])
    {
        case KYMERA_OP_MSG_ID_TONE_END:
            DEBUG_LOG("KYMERA_OP_MSG_ID_TONE_END");
            appKymeraTonePromptStop();
        break;

        default:
        break;
    }
}

static void kymera_msg_handler(Task task, MessageId id, Message msg)
{
    kymeraTaskData *theKymera = appGetKymera();
    UNUSED(task);
    switch (id)
    {
        case MESSAGE_FROM_OPERATOR:
            kymera_dsp_msg_handler((MessageFromOperator *)msg);
        break;

        case MESSAGE_SOURCE_EMPTY:
        break;

        case MESSAGE_STREAM_DISCONNECT:
            DEBUG_LOG("appKymera MESSAGE_STREAM_DISCONNECT");
            appKymeraTonePromptStop();
        break;

        case KYMERA_INTERNAL_A2DP_START:
        {
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            /* If there is no pre-start delay, or during the pre-start delay, the
            start can be cancelled if there is a stop on the message queue */
            uint8 seid = m->codec_settings.seid;
            MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                        KYMERA_INTERNAL_A2DP_STOP;
            if (MessageCancelFirst(&theKymera->task, mid))
            {
                /* A stop on the queue was cancelled, clear the starter's lock
                and stop starting */
                DEBUG_LOGF("appKymera not starting due to queued stop, seid=%u", seid);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }
                /* Also clear kymera's lock, since no longer starting */
                appKymeraClearStartingLock(theKymera);
                break;
            }
            if (m->master_pre_start_delay)
            {
                /* Send another message before starting kymera. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                --message->master_pre_start_delay;
                MessageSend(&theKymera->task, id, message);
                appKymeraSetStartingLock(theKymera);
                break;
            }
        }
        // fallthrough (no message cancelled, zero master_pre_start_delay)
        case KYMERA_INTERNAL_A2DP_STARTING:
        {
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            if (appKymeraHandleInternalA2dpStart(m))
            {
                /* Start complete, clear locks. */
                appKymeraClearStartingLock(theKymera);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }
            }
            else
            {
                /* Start incomplete, send another message. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                MessageSend(&theKymera->task, KYMERA_INTERNAL_A2DP_STARTING, message);
            }
        }
        break;

        case KYMERA_INTERNAL_A2DP_STOP:
        case KYMERA_INTERNAL_A2DP_STOP_FORWARDING:
            appKymeraHandleInternalA2dpStop(msg);
        break;

        case KYMERA_INTERNAL_A2DP_SET_VOL:
        {
            KYMERA_INTERNAL_A2DP_SET_VOL_T *m = (KYMERA_INTERNAL_A2DP_SET_VOL_T *)msg;
            appKymeraHandleInternalA2dpSetVolume(m->volume);
        }
        break;

        case KYMERA_INTERNAL_SCO_START:
        {
            const KYMERA_INTERNAL_SCO_START_T *m = (const KYMERA_INTERNAL_SCO_START_T *)msg;
            if (m->pre_start_delay)
            {
                /* Resends are sent unconditonally, but the lock is set blocking
                   other new messages */
                appKymeraSetStartingLock(appGetKymera());
                appKymeraScoStartHelper(m->audio_sink, m->codec, m->wesco, m->volume,
                                        m->pre_start_delay - 1, m->allow_scofwd, FALSE);
            }
            else
            {
                appKymeraHandleInternalScoStart(m->audio_sink, m->codec, m->wesco, 
                                                m->volume, m->allow_scofwd);
                appKymeraClearStartingLock(appGetKymera());
            }
        }
        break;

#ifdef INCLUDE_SCOFWD
        case KYMERA_INTERNAL_SCO_START_FORWARDING_TX:
        {
            const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T *m =
                    (const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T*)msg;
            appKymeraHandleInternalScoStartForwardingTx(m->forwarding_sink);
        }
        break;

        case KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX:
        {
            appKymeraHandleInternalScoStopForwardingTx();
        }
        break;
#endif

        case KYMERA_INTERNAL_SCO_SET_VOL:
        {
            KYMERA_INTERNAL_SCO_SET_VOL_T *m = (KYMERA_INTERNAL_SCO_SET_VOL_T *)msg;
            appKymeraHandleInternalScoSetVolume(m->volume);
        }
        break;

        case KYMERA_INTERNAL_SCO_MIC_MUTE:
        {
            KYMERA_INTERNAL_SCO_MIC_MUTE_T *m = (KYMERA_INTERNAL_SCO_MIC_MUTE_T *)msg;
            appKymeraHandleInternalScoMicMute(m->mute);
        }
        break;


        case KYMERA_INTERNAL_SCO_STOP:
        {
            appKymeraHandleInternalScoStop();
        }
        break;

#ifdef INCLUDE_SCOFWD
        case KYMERA_INTERNAL_SCOFWD_RX_START:
        {
            const KYMERA_INTERNAL_SCOFWD_RX_START_T *m = (const KYMERA_INTERNAL_SCOFWD_RX_START_T *)msg;

            appKymeraHandleInternalScoForwardingStartRx(m);
        }
        break;

        case KYMERA_INTERNAL_SCOFWD_RX_STOP:
        {
            appKymeraHandleInternalScoForwardingStopRx();
        }
        break;
#endif

        case KYMERA_INTERNAL_TONE_PROMPT_PLAY:
            appKymeraHandleInternalTonePromptPlay(msg);
        break;

#ifdef INCLUDE_MICFWD
        case KYMERA_INTERNAL_MICFWD_LOCAL_MIC:
            appKymeraSwitchSelectMic(MIC_SELECTION_LOCAL);
            break;

        case KYMERA_INTERNAL_MICFWD_REMOTE_MIC:
            appKymeraSwitchSelectMic(MIC_SELECTION_REMOTE);
            break;
#endif /* INCLUDE_MICFWD */

        case KYMERA_INTERNAL_AUDIO_SS_DISABLE:
            DEBUG_LOG("appKymera KYMERA_INTERNAL_AUDIO_SS_DISABLE");
            OperatorFrameworkEnable(MAIN_PROCESSOR_OFF);
            break;
        default:
        break;
    }
}

void appKymeraInit(void)
{
    kymeraTaskData *theKymera = appGetKymera();
    memset(theKymera, 0, sizeof(*theKymera));
    theKymera->task.handler = kymera_msg_handler;
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
    theKymera->lock = 0;
    theKymera->a2dp_seid = AV_SEID_INVALID;
    appKymeraExternalAmpSetup();
    ChainSetDownloadableCapabilityBundleConfig(&bundle_config);
    theKymera->mic = MIC_SELECTION_LOCAL;
}
