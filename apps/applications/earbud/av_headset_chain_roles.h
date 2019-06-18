/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_chain_roles.h
\brief	    Definitions of chain roles
*/

#ifndef AV_HEADSET_CHAIN_ROLES_H
#define AV_HEADSET_CHAIN_ROLES_H

/*! These names may be used in chain operator definitions.
   If used, the chain definition should include this file using
   \verbatim <include_header name="../av_headset_chain_roles.h"/> \endverbatim and
   be configured with the attribute \verbatim generate_operator_roles_enum="False" \endverbatim.
*/
typedef enum chain_operator_roles
{
    /*! Role identifier used for RTP decoder */
    OPR_RTP_DECODER = 0x1000,
    /*! Role identifier used for APTX demultiplexer (splitter) operator */
    OPR_APTX_DEMUX,
    /*! Role identifier used for switched passthrough operator. */
    OPR_SWITCHED_PASSTHROUGH_CONSUMER,
    /*! Role identifier used for APTX mono decoder operator */
    OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC,
    /*! Role identifier used for SBC decoder operator */
    OPR_SBC_DECODER,
    /*! Role identifier used for SBC encoder operator */
    OPR_SBC_ENCODER,
    /*! Role identifier used for the AAC decoder operator */
    OPR_AAC_DECODER,
    /*! Role identifier used for the splitter */
    OPR_SPLITTER,
    /*! Role identifer used for a consumer operator */
    OPR_CONSUMER,

    /* SCO_roles - Operator roles used in the chains for SCO audio */
        /*! The operator processing incoming SCO */
    OPR_SCO_RECEIVE,
        /*! The operator processing outgoing SCO */
    OPR_SCO_SEND,
        /*! The clear voice capture operator for incoming SCO */
    OPR_CVC_RECEIVE,
        /*! The clear voice capture operator for outgoing SCO */
    OPR_CVC_SEND,
        /*! The adaptive echo cancellation operator used in SCO audio chain */
    OPR_SCO_AEC,

        /*! Buffer added to chains when using SCO forwarding chain as 
            the TTP values used delay data */
    OPR_SCOFWD_BUFFERING,
        /*! Splitter operator used to double-up SCO ready for forwarding */
    OPR_SCOFWD_SPLITTER,
        /*! Encoder operator used in the SCO forwarding chain */
    OPR_SCOFWD_SEND,
        /*! Decoder operator used in the SCO forwarding receive chain */
    OPR_SCOFWD_RECV,
        /*! Re-sampler used to keep SCO forwarding at 16KHz when receiving narrowband SCO */
    OPR_SCO_UP_SAMPLE,
        /*! Re-sampler used to downsample forwarded MIC (from slave) to 8kHz for narrowband SCO */
    OPR_MIC_DOWN_SAMPLE,
        /*! Basic-passthrough */
    OPR_SCOFWD_BASIC_PASS,

        /*! Passthrough Consumer used as a simple switch when processing received Mic */
    OPR_MICFWD_SPC_SWITCH,
        /*! Splitter used for the MIC signal when MIC fwding. */
    OPR_MICFWD_SPLITTER,
        /*! Encoder operator used in the MIC forwarding chain */
    OPR_MICFWD_SEND,
        /*! Decoder operator used in the MIC forwarding receive chain */
    OPR_MICFWD_RECV,
        /*! Passthrough consumer used to permit chain start with no input connected
         * to OPR_MICFWD_RECV. */
    OPR_MICFWD_RECV_SPC,

    /* Common_roles - Common operator roles used between chains */
        /*! Common synchronisation operator role */
    OPR_SOURCE_SYNC,
        /*! Common volume control operator role */
    OPR_VOLUME_CONTROL,

    /*! The buffering (passthrough) operator used in volume control chain */
    OPR_LATENCY_BUFFER,

    /*! Resampler used to change tone-generator/prompt sample rate to match the audio chain */
    OPR_TONE_PROMPT_RESAMPLER,

    /*! Tone generator */
    OPR_TONE_GEN,

    /*! User PEQ */
    OPR_PEQ,

    /*! Prompt decoder */
    OPR_PROMPT_DECODER,
} chain_operator_role_t;

/*! These names may be used in chain endpoint definitions.
   If used, the chain definition should include this file using
   \verbatim <include_header name="../av_headset_chain_roles.h"/>\endverbatim and
   be configured with the attribute \verbatim generate_endpoint_roles_enum="False"\endverbatim.
*/
typedef enum chain_endpoint_roles
{
    /*! The sink of AVDTP media, typically the RTP decoder */
    EPR_SINK_MEDIA = 0x2000,
    /*! The source of decoded PCM, typically the output of a codec decoder */
    EPR_SOURCE_DECODED_PCM,
    /*! The source of encoded media for forwarding, typically the output of
        a codec encoder or a aptx demultiplexer */
    EPR_SOURCE_FORWARDING_MEDIA,
    /*! The sink of the final output volume mixer main channel */
    EPR_SINK_MIXER_MAIN_IN,
    /*! Additional input to volume control, typically used for tones/prompts */
    EPR_VOLUME_AUX,
    /*! The source of the final output, typically connected to a DAC output */
    EPR_SOURCE_MIXER_OUT,
    /*! The input to SCO receive portion of SCO chain */
    EPR_SCO_FROM_AIR,
    /*! The final output from SCO send portion of SCO chain */
    EPR_SCO_TO_AIR,
    /*! The first, or only, MIC input to echo cancellation of SCO chain */
    EPR_SCO_MIC1,
    /*! The second MIC input to echo cancellation of SCO chain */
    EPR_SCO_MIC2,
    /*! The speaker output from SCO chain */
    EPR_SCO_SPEAKER,

    /*! The input to chains for forwarded SCO data */
    EPR_SCOFWD_RX_OTA,
    /*! The chain output that supplies SCO data to forward */
    EPR_SCOFWD_TX_OTA,

    /*! The input to chains for forwarded MIC data */
    EPR_MICFWD_RX_OTA,
    /*! The chain output that supplies MIC data to forward */
    EPR_MICFWD_TX_OTA,

    /*! Output from the tone-generator/prompt chain */
    EPR_TONE_PROMPT_CHAIN_OUT,

    /*! Input to the prompt chain */
    EPR_PROMPT_IN,
} chain_endpoint_role_t;

#ifndef CAP_ID_DOWNLOAD_AEC_REFERENCE
#define CAP_ID_DOWNLOAD_AEC_REFERENCE 0x4007
#else
#error "Now have define for downloadable AEC"
#endif

#endif // AV_HEADSET_CHAIN_ROLES_H
