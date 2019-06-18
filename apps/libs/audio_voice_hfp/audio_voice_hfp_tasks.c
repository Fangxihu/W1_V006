/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_voice_hfp_tasks.c

DESCRIPTION
    Definitions of audio voice HFP tasks.
*/

#include <audio_plugin_voice_variants.h>
#include <audio_voice_common.h>
#include "audio_voice_hfp.h"


/* Supported plug-in tasks */

/* Heasdset variants */
const CvcPluginTaskdata csr_cvsd_cvc_1mic_headset_plugin = {{AudioVoiceHfpMessageHandler},cvc_1_mic_headset_cvsd, link_encoding_cvsd, 0, 0, BITFIELD_CAST(6, 0)};
const CvcPluginTaskdata csr_wbs_cvc_1mic_headset_plugin = {{AudioVoiceHfpMessageHandler},cvc_1_mic_headset_msbc, link_encoding_msbc, 0, 1, BITFIELD_CAST(6, 0)};

const CvcPluginTaskdata csr_cvsd_cvc_2mic_headset_plugin = {{AudioVoiceHfpMessageHandler},cvc_2_mic_headset_cvsd, link_encoding_cvsd, 1, 0, BITFIELD_CAST(6, 0)};
const CvcPluginTaskdata csr_wbs_cvc_2mic_headset_plugin = {{AudioVoiceHfpMessageHandler},cvc_2_mic_headset_msbc, link_encoding_msbc, 1, 1, BITFIELD_CAST(6, 0)};

const CvcPluginTaskdata csr_cvsd_cvc_2mic_headset_binaural_plugin = {{AudioVoiceHfpMessageHandler},cvc_2_mic_headset_binaural_nb, link_encoding_cvsd, 1, 0, BITFIELD_CAST(6, 0)};
const CvcPluginTaskdata csr_wbs_cvc_2mic_headset_binaural_plugin = {{AudioVoiceHfpMessageHandler},cvc_2_mic_headset_binaural_wb, link_encoding_msbc, 1, 1, BITFIELD_CAST(6, 0)};

/* Speaker variants */
const CvcPluginTaskdata csr_cvsd_cvc_1mic_speaker_plugin = {{AudioVoiceHfpMessageHandler},cvc_1_mic_speaker_cvsd, link_encoding_cvsd, 0, 0, BITFIELD_CAST(6, 0)};
const CvcPluginTaskdata csr_wbs_cvc_1mic_speaker_plugin = {{AudioVoiceHfpMessageHandler},cvc_1_mic_speaker_msbc, link_encoding_msbc, 0, 1, BITFIELD_CAST(6, 0)};

const CvcPluginTaskdata csr_cvsd_cvc_2mic_speaker_plugin = {{AudioVoiceHfpMessageHandler},cvc_2_mic_speaker_cvsd, link_encoding_cvsd, 1, 0, BITFIELD_CAST(6, 0)};
const CvcPluginTaskdata csr_wbs_cvc_2mic_speaker_plugin = {{AudioVoiceHfpMessageHandler},cvc_2_mic_speaker_msbc, link_encoding_msbc, 1, 1, BITFIELD_CAST(6, 0)};

