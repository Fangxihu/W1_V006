/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_mixer_processing_chain_config.h

DESCRIPTION
    Implementation of chain construction functions
*/

#ifndef _AUDIO_MIXER_RESAMPLER_CHAIN_CONFIG_H_
#define _AUDIO_MIXER_RESAMPLER_CHAIN_CONFIG_H_

#include <chain.h>

#include "audio_mixer.h"

enum
{
    processing_compander_role,
    peq_role,
    vse_role,
    dbe_role,
    processing_stereo_to_mono_role,
    pre_processing_role
};

typedef enum
{
    no_processing,
    voice_processing,
    mono_prompt_processing,
    stereo_prompt_processing,
    mono_music_processing,
    stereo_music_processing
} processing_chain_t;

/****************************************************************************
DESCRIPTION
     Function to return the required chain type given key input parameters
*/
processing_chain_t mixerProcessingGetChainType(connection_type_t connection_type, bool is_mono);

/****************************************************************************
DESCRIPTION
    returns the required chain config for a given chain type
*/
const chain_config_t* mixerProcessingGetChainConfig(processing_chain_t chain_type);

/****************************************************************************
DESCRIPTION
    returns the required filter config for volume attenuation / for mono music */
const operator_filters_t *mixerProcessingGetChainFilter(connection_type_t connection_type,
                                                        bool is_mono,
                                                        bool attenuate_volume_pre_processing);
#endif /* _AUDIO_MIXER_RESAMPLER_CHAIN_CONFIG_H_ */

