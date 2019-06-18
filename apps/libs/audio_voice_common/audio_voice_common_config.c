/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_voice_common_config.c

DESCRIPTION
    Implementation of chain construction functions.
*/

#include "audio_voice_common_config.h"

#include <audio_config.h>

#include "audio_voice_common_dsp.h"

#define UNCONNECTED           (255)
#define WBS_SAMPLE_RATE         ((uint32)16000)
#define NBS_SAMPLE_RATE         ((uint32)8000)

#define CVC_OPS_NB \
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_none, receive_role), \
    MAKE_OPERATOR_CONFIG_PRIORITY_HIGH(capability_id_none, send_role), \
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_cvc_receive_nb, cvc_receive_role), \
    MAKE_OPERATOR_CONFIG(capability_id_none, rate_adjustment_send_role)
    

#define CVC_OPS_WB \
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_none, receive_role), \
    MAKE_OPERATOR_CONFIG_PRIORITY_HIGH(capability_id_none, send_role), \
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_cvc_receive_wb, cvc_receive_role), \
    MAKE_OPERATOR_CONFIG(capability_id_none, rate_adjustment_send_role)
    


static operator_config_t ops_nb_1mic_hs[] =
{
    CVC_OPS_NB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hs_1mic_nb, cvc_send_role)
};

static operator_config_t ops_nb_1mic_speaker[] =
{
    CVC_OPS_NB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_spk_1mic_nb, cvc_send_role)
};

static operator_config_t ops_nb_1mic_handsfree[] =
{
    CVC_OPS_NB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hf_1mic_nb, cvc_send_role)
};

static const operator_config_t ops_wb_1mic_hs[] =
{
    CVC_OPS_WB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hs_1mic_wb, cvc_send_role)
};

static const operator_config_t ops_wb_1mic_speaker[] =
{
    CVC_OPS_WB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_spk_1mic_wb, cvc_send_role)
};

static const operator_config_t ops_wb_1mic_handsfree[] =
{
    CVC_OPS_WB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hf_1mic_wb, cvc_send_role)
};


static const operator_config_t ops_nb_2mic_hs[] =
{
    CVC_OPS_NB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hs_2mic_90deg_nb, cvc_send_role)
};

static const operator_config_t ops_nb_2mic_speaker[] =
{
    CVC_OPS_NB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_spk_2mic_0deg_nb, cvc_send_role)
};

static const operator_config_t ops_nb_2mic_handsfree[] =
{
    CVC_OPS_NB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hf_2mic_nb, cvc_send_role)
};

static const operator_config_t ops_wb_2mic_hs[] =
{
    CVC_OPS_WB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hs_2mic_90deg_wb, cvc_send_role)
};

static const operator_config_t ops_wb_2mic_speaker[] =
{
    CVC_OPS_WB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_spk_2mic_0deg_wb, cvc_send_role)
};

static const operator_config_t ops_wb_2mic_handsfree[] =
{
    CVC_OPS_WB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hf_2mic_wb, cvc_send_role)
};

static const operator_config_t ops_binaural_nb_2mic_hs[] =
{
    CVC_OPS_NB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hs_2mic_0deg_nb, cvc_send_role)
};

static const operator_config_t ops_binaural_wb_2mic_hs[] =
{
    CVC_OPS_WB,
    MAKE_OPERATOR_CONFIG_PRIORITY_LOWEST(capability_id_cvc_hs_2mic_0deg_wb, cvc_send_role)
};

static const operator_config_t ops_nb_no_cvc[] =
{
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_none, receive_role),
    MAKE_OPERATOR_CONFIG_PRIORITY_HIGH(capability_id_none, send_role)
};

static const operator_config_t ops_wb_no_cvc[] =
{
    MAKE_OPERATOR_CONFIG_PRIORITY_MEDIUM(capability_id_none, receive_role),
    MAKE_OPERATOR_CONFIG_PRIORITY_HIGH(capability_id_none, send_role)
};

static const operator_setup_item_t sample_rate_ra_nb[] =
{
    OPERATORS_SETUP_STANDARD_SAMPLE_RATE(NBS_SAMPLE_RATE)
};

static const operator_setup_item_t sample_rate_ra_wb[] =
{
    OPERATORS_SETUP_STANDARD_SAMPLE_RATE(WBS_SAMPLE_RATE)
};

static operator_config_t ra_nb[] =
{
    MAKE_OPERATOR_CONFIG_WITH_SETUP(capability_id_rate_adjustment, audio_voice_rate_adjustment_send_role, sample_rate_ra_nb)
};

static operator_config_t ra_wb[] =
{
    MAKE_OPERATOR_CONFIG_WITH_SETUP(capability_id_rate_adjustment, audio_voice_rate_adjustment_send_role, sample_rate_ra_wb)
};


/* --------------------------------------------------------------*/

static const operator_path_node_t receive[] =
{
    {receive_role, 0, 0},
    {cvc_receive_role, 0, 0},
};

static const operator_path_node_t send_mic1[] =
{
    {cvc_send_role, 1, 0},
    {rate_adjustment_send_role, 0, 0},
    {send_role, 0, 0}
};

static const operator_path_node_t send_mic2[] =
{
    {cvc_send_role, 2, UNCONNECTED},
};

static const operator_path_node_t aec_ref[] =
{
    {cvc_send_role, 0, UNCONNECTED}
};

/* --------------------------------------------------------------*/

static const operator_path_t paths[] =
{
    {path_receive, path_with_in_and_out, ARRAY_DIM((receive)), receive},
    {path_send_mic1, path_with_in_and_out, ARRAY_DIM((send_mic1)), send_mic1},
    {path_send_mic2, path_with_input, ARRAY_DIM((send_mic2)), send_mic2},
    {path_aec_ref, path_with_input, ARRAY_DIM((aec_ref)), aec_ref}
};

/*Headset varaints*/
static const chain_config_t audio_voice_hfp_config_1mic_nb_hs =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_headset, ops_nb_1mic_hs, paths);

static const chain_config_t audio_voice_hfp_config_1mic_wb_hs =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_headset, ops_wb_1mic_hs, paths);

static const chain_config_t audio_voice_hfp_config_2mic_nb_hs =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_headset, ops_nb_2mic_hs, paths);

static const chain_config_t audio_voice_hfp_config_2mic_wb_hs =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_headset, ops_wb_2mic_hs, paths);

static const chain_config_t audio_voice_hfp_config_2mic_binaural_nb_hs =
        MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_headset, ops_binaural_nb_2mic_hs, paths);

static const chain_config_t audio_voice_hfp_config_2mic_binaural_wb_hs =
        MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_headset, ops_binaural_wb_2mic_hs, paths);

/*Handsfree varaints*/
static const chain_config_t audio_voice_hfp_config_1mic_nb_handsfree =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_handsfree, ops_nb_1mic_handsfree, paths);

static const chain_config_t audio_voice_hfp_config_1mic_wb_handsfree =
        MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_handsfree, ops_wb_1mic_handsfree, paths);

static const chain_config_t audio_voice_hfp_config_2mic_nb_handsfree =
        MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_handsfree, ops_nb_2mic_handsfree, paths);

static const chain_config_t audio_voice_hfp_config_2mic_wb_handsfree =
        MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_handsfree, ops_wb_2mic_handsfree, paths);

/*Speaker varaints*/
static const chain_config_t audio_voice_hfp_config_1mic_nb_speaker =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_speaker, ops_nb_1mic_speaker, paths);

static const chain_config_t audio_voice_hfp_config_1mic_wb_speaker =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_speaker, ops_wb_1mic_speaker, paths);

static const chain_config_t audio_voice_hfp_config_2mic_nb_speaker =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_speaker, ops_nb_2mic_speaker, paths);

static const chain_config_t audio_voice_hfp_config_2mic_wb_speaker =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp_cvc_speaker, ops_wb_2mic_speaker, paths);


/* --------------------------------------------------------------*/

static const operator_path_node_t receive_no_cvc[] =
{
    {receive_role, 0, 0},
};

static const operator_path_node_t send_mic1_no_cvc[] =
{
    {send_role, 0, 0}
};

static const operator_path_t paths_no_cvc[] =
{
    {path_receive, path_with_in_and_out, ARRAY_DIM((receive_no_cvc)), receive_no_cvc},
    {path_send_mic1, path_with_in_and_out, ARRAY_DIM((send_mic1_no_cvc)), send_mic1_no_cvc},
};

static const chain_config_t audio_voice_hfp_config_nb_no_cvc =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp, ops_nb_no_cvc, paths_no_cvc);


static const chain_config_t audio_voice_hfp_config_wb_no_cvc =
    MAKE_CHAIN_CONFIG_WITH_PATHS(chain_id_cvc_common, audio_ucid_hfp, ops_wb_no_cvc, paths_no_cvc);



/******************************************************************************/
const chain_config_t* AudioVoiceCommonGetChainConfig(const audio_voice_context_t* ctx)
{
    const unsigned plugin_variant = ctx->variant;

    if(AudioConfigGetQuality(audio_stream_voice) == audio_quality_low_power)
    {
        if(ctx->encoder == link_encoding_msbc)
            return &audio_voice_hfp_config_wb_no_cvc;

        return &audio_voice_hfp_config_nb_no_cvc;
    }

    switch(plugin_variant)
    {
        case cvc_1_mic_headset_cvsd:
            return &audio_voice_hfp_config_1mic_nb_hs;

        case cvc_1_mic_headset_msbc:
            return &audio_voice_hfp_config_1mic_wb_hs;

        case cvc_2_mic_headset_cvsd:
            return &audio_voice_hfp_config_2mic_nb_hs;

        case cvc_2_mic_headset_msbc:
            return &audio_voice_hfp_config_2mic_wb_hs;

        case cvc_2_mic_headset_binaural_nb:
            return &audio_voice_hfp_config_2mic_binaural_nb_hs;

        case cvc_2_mic_headset_binaural_wb:
            return &audio_voice_hfp_config_2mic_binaural_wb_hs;

        case cvc_1_mic_handsfree_cvsd:
            return &audio_voice_hfp_config_1mic_nb_handsfree;

        case cvc_1_mic_handsfree_msbc:
            return &audio_voice_hfp_config_1mic_wb_handsfree;

        case cvc_2_mic_handsfree_cvsd:
            return &audio_voice_hfp_config_2mic_nb_handsfree;

        case cvc_2_mic_handsfree_msbc:
            return &audio_voice_hfp_config_2mic_wb_handsfree;

        case cvc_1_mic_speaker_cvsd:
            return &audio_voice_hfp_config_1mic_nb_speaker;

        case cvc_1_mic_speaker_msbc:
            return &audio_voice_hfp_config_1mic_wb_speaker;

        case cvc_2_mic_speaker_cvsd:
            return &audio_voice_hfp_config_2mic_nb_speaker;

        case cvc_2_mic_speaker_msbc:
            return &audio_voice_hfp_config_2mic_wb_speaker;

        default:
            return NULL;
    }
}

operator_config_t* AudioVoiceCommonGetRAFilter(bool wideband)
{
    return (wideband ? ra_wb : ra_nb);
}

bool AudioVoiceCommonIsWideband(unsigned encoder)
{
    return (encoder == link_encoding_msbc);
}
