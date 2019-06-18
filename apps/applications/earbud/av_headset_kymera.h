/*!
\copyright  Copyright (c) 2017-2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_kymera.h
\brief      Header file for the Kymera Manager

*/

#ifndef AV_HEADSET_KYMERA_H
#define AV_HEADSET_KYMERA_H

#include <chain.h>
#include <transform.h>
#include <hfp.h>

/*! \brief The kymera module states. */
typedef enum app_kymera_states
{
    /*! Kymera is idle. */
    KYMERA_STATE_IDLE,
    /*! Starting master A2DP kymera in three steps. */
    KYMERA_STATE_A2DP_STARTING_A,
    KYMERA_STATE_A2DP_STARTING_B,
    KYMERA_STATE_A2DP_STARTING_C,
    /*! Kymera is streaming A2DP locally. */
    KYMERA_STATE_A2DP_STREAMING,
    /*! Kymera is streaming A2DP locally and forwarding to the slave. */
    KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING,
    /*! Kymera is streaming SCO locally. */
    KYMERA_STATE_SCO_ACTIVE,
    /*! Kymera is streaming SCO locally, and may be forwarding */
    KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING,
    /*! Kymera is receiving forwarded SCO over a link */
    KYMERA_STATE_SCOFWD_RX_ACTIVE,
    /*! Kymera is playing a tone or a prompt. */
    KYMERA_STATE_TONE_PLAYING,
} appKymeraState;

/*! \brief Enumeration of microphone selection options.

    The enumeration numbering is important, they match
    the input numbering in the Switched Passthrough
    Consumer operator.
 */
typedef enum
{
    /*! Local earbud microphone selected. */
    MIC_SELECTION_LOCAL = 1,

    /*! Remote earbud microphone selected. */
    MIC_SELECTION_REMOTE = 2,
} micSelection;

/*! \brief Kymera instance structure.

    This structure contains all the information for Kymera audio chains.
*/
typedef struct
{
    /*! The kymera module's task. */
    TaskData          task;
    /*! The current state. */
    appKymeraState    state;

    /*! The input chain is used in TWS master and slave roles for A2DP streaming
        and is typified by containing a decoder. */
    kymera_chain_handle_t chain_input_handle;
    /*! The tone chain is used when a tone is played. */
    kymera_chain_handle_t chain_tone_handle;

    /*! The output_vol_handle/sco_handle chain are used mutually exclusively.
        The output_vol_handle/sco_handle both contain OPR_SOURCE_SYNC/OPR_VOLUME_CONTROL.
        These chains are unioned to simplify volume control code: output_vol_handle
        can always be used with OPR_VOLUME_CONTROL regardless of whether SCO/A2DP is active. */
    union
    {
        /*! Used in TWS master and slave roles for A2DP streaming */
        kymera_chain_handle_t output_vol_handle;
        /*! Used for SCO audio. */
        kymera_chain_handle_t sco_handle;
    } chainu;

    /*! The TWS master packetiser transform packs compressed audio frames
        (SBC, AAC, aptX) from the audio subsystem into TWS packets for transmission
        over the air to the TWS slave.
        The TWS slave packetiser transform receives TWS packets over the air from
        the TWS master. It unpacks compressed audio frames and writes them to the
        audio subsystem. */
    Transform packetiser;

    /*! The current output sample rate. */
    uint32 output_rate;
    /*! A lock bitfield. Internal messages are typically sent conditionally on
        this lock meaning events are queued until the lock is cleared. */
    uint16 lock;
    /*! The current A2DP stream endpoint identifier. */
    uint8  a2dp_seid;
    /*! The current playing tone client's lock. */
    uint16 *tone_client_lock;
    /*! The current playing tone client lock mask - bits to clear in the lock
         when the tone is stopped. */
    uint16 tone_client_lock_mask;

    /*! Which microphone to use during mic forwarding */
    micSelection mic;

    /*! The prompt file source whilst prompt is playing */
    Source prompt_source;

} kymeraTaskData;

/*! \brief Start streaming A2DP audio.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when A2DP is started, or if an A2DP stop is requested,
           before A2DP has started.
    \param client_lock_mask A mask of bits to clear in the client_lock.
    \param codec_settings The A2DP codec settings.
    \param volume The start volume.
    \param master_pre_start_delay This function always sends an internal message
    to request the module start kymera. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent master_pre_start_delay additional times before the
    start of kymera commences. The intention of this is to allow the caller to
    delay the starting of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera. This delay is only applied
    when starting the 'master' (a non-TWS sink SEID).
*/
void appKymeraA2dpStart(uint16 *client_lock, uint16 client_lock_mask,
                        const a2dp_codec_settings *codec_settings,
                        uint8 volume, uint8 master_pre_start_delay);

/*! \brief Stop streaming A2DP audio.
    \param seid The stream endpoint ID to stop.
    \param source The source associatied with the seid.
*/
void appKymeraA2dpStop(uint8 seid, Source source);

/*! \brief Set the A2DP streaming volume.
    \param volume The desired volume in the range 0 (mute) to 127 (max).
*/
void appKymeraA2dpSetVolume(uint16 volume);

/*! \brief Start SCO audio.
    \param audio_sink The SCO audio sink.
    \param codec WB-Speech codec bit masks.
    \param wesco The link Wesco.
    \param volume The starting volume.
    \param pre_start_delay This function always sends an internal message
    to request the module start SCO. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent pre_start_delay additional times before
    starting kymera. The intention of this is to allow the caller to
    delay the start of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera.
    \param allow_scofwd Allow the use of SCO forwarding if it is supported by
    this device.
*/
void appKymeraScoStart(Sink audio_sink, hfp_wbs_codec_mask codec, uint8 wesco,
                       uint16 volume, uint8 pre_start_delay, bool allow_scofwd);


/*! \brief Stop SCO audio.
*/
void appKymeraScoStop(void);

/*! \brief Start a chain for receiving forwarded SCO audio.

    \param link_source      The source used for data received from the SCO forwarding link
    \param volume           volume level to use
    \param enable_mic_fwd   Should MIC forwarding be enabled or not.
    \param pre_start_delay  Maximum number of times message is resent before being actioned
*/
void appKymeraScoFwdStartReceive(Source link_source, uint8 volume, bool enable_mic_fwd,
                                 uint16 pre_start_delay);

/*! \brief Stop chain receiving forwarded SCO audio.
*/
void appKymeraScoFwdStopReceive(void);

/*! \brief Start sending forwarded audio.

    \param forwarding_sink The BT sink to the air.
    \param enable_mic_fwd Should MIC forwarding be enabled or not.
    \return Always TRUE.

    \note If the SCO is to be forwarded then the full chain,
    including local playback, is started by this call.
*/
bool appKymeraScoStartForwarding(Sink forwarding_sink, bool enable_mic_fwd);

/*! \brief Stop sending received SCO audio to the peer

    \return Always TRUE.

    Local playback of SCO will continue, although in most cases
    will be stopped by a separate command almost immediately.
 */
bool appKymeraScoStopForwarding(void);

/*! \brief Enable/Disable generation of forwarded SCO audio.

    \param pause [IN] If TRUE will stop generating SCO audio data, if
                      FALSE will resume.

    This function leaves the SCO/SFWD chains up and running and flips
    the switched passthrough consumer between 'consume' and 'passthrough'
    modes to control if forwarded SCO data generated on the stream
    source for the SCOFWD packetiser to send.
*/
void appKymeraSfwdForwardingPause(bool pause);

/*! \brief Set SCO volume.
    \param volume [IN] HFP volume in the range 0 (mute) to 15 (max).
 */
void appKymeraScoSetVolume(uint8 volume);

/*! \brief Enable or disable MIC muting.
    \param mute [IN] TRUE to mute MIC, FALSE to unmute MIC.
 */
void appKymeraScoMicMute(bool mute);

/*! \brief Get the SCO CVC voice quality.
    \return The voice quality.
 */
uint8 appKymeraScoVoiceQuality(void);

#ifdef INCLUDE_MICFWD
/*! \brief Select the local Microphone for use

    Switches audio from remote (forwarded) Microphone to local Mic src.
 */
void appKymeraScoUseLocalMic(void);

/*! \brief Select the remote Microphone for use

    Switches audio from local Microphone to the remote (forwarded) Mic src.
 */
void appKymeraScoUseRemoteMic(void);
#else

#define appKymeraScoUseLocalMic()   ((void)0)
#define appKymeraScoUseRemoteMic()  ((void)0)

#endif /* INCLUDE_MICFWD */

/*! \brief Play a tone.
    \param tone The address of the tone to play.
    \param interruptible If TRUE, the tone may be interrupted by another event
           before it is completed. If FALSE, the tone may not be interrupted by
           another event and will play to completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when the tone finishes - either on completion, or when
           interrupted.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appKymeraTonePlay(const ringtone_note *tone, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask);

/*! \brief The prompt file format */
typedef enum prompt_format
{
    PROMPT_FORMAT_PCM,
    PROMPT_FORMAT_SBC,
} promptFormat;

/*! \brief Play a prompt.
    \param prompt The file index of the prompt to play.
    \param format The prompt file format.
    \param rate The prompt sample rate.
    \param interruptible If TRUE, the prompt may be interrupted by another event
           before it is completed. If FALSE, the prompt may not be interrupted by
           another event and will play to completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when the prompt finishes - either on completion, or when
           interrupted.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appKymeraPromptPlay(FILE_INDEX prompt, promptFormat format,
                         uint32 rate, bool interruptible,
                         uint16 *client_lock, uint16 client_lock_mask);

/*! \brief Initialise the kymera module. */
void appKymeraInit(void);

#endif // AV_HEADSET_KYMERA_H
