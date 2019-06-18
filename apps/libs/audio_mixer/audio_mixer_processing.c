/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_mixer_processing.c
DESCRIPTION
    Implements functionality to create the processing chain
*/

#include <stream.h>
#include <stdlib.h>

#include "audio_mixer_processing.h"
#include "audio_mixer_processing_chain_config.h"
#include "audio_mixer.h"
#include "chain.h"
#include "operators.h"
#include "audio_music_processing.h"
#include "panic.h"
#include "audio_mixer_connection.h"

typedef struct __mixer_processing_data
{
    kymera_chain_handle_t  chain;
    processing_chain_t chain_type;
} mixer_processing_data_t;

/****************************************************************************
DESCRIPTION
    Helper function to register music processing operators with music processing lib 
*/
static void mixerProcessingRegisterMusicProcessingOperators(mixer_processing_data_t* mixer_processing_data)
{
    if ((mixer_processing_data->chain_type == mono_music_processing) ||
        (mixer_processing_data->chain_type == stereo_music_processing))
    {
        Operator compander_op = ChainGetOperatorByRole(mixer_processing_data->chain, processing_compander_role);
        Operator peq_op = ChainGetOperatorByRole(mixer_processing_data->chain, peq_role);
        Operator dbe_op = ChainGetOperatorByRole(mixer_processing_data->chain, dbe_role);

        AudioMusicProcessingRegisterRole(audio_music_processing_compander_role, compander_op);
        AudioMusicProcessingRegisterRole(audio_music_processing_user_peq_role, peq_op);
        AudioMusicProcessingRegisterRole(audio_music_processing_dynamic_bass_enhancement_role, dbe_op);
    }

    if (mixer_processing_data->chain_type == stereo_music_processing)
    {
        Operator vse_op = ChainGetOperatorByRole(mixer_processing_data->chain, vse_role);

        AudioMusicProcessingRegisterRole(audio_music_processing_volume_spatial_enhancement_role, vse_op);
    }

}

/****************************************************************************
DESCRIPTION
    Helper function to unregister music processing operators with music processing lib 
*/
static void mixerProcessingUnregisterMusicProcessingOperators(mixer_processing_data_t* mixer_processing_data)
{
    if ((mixer_processing_data->chain_type == mono_music_processing) ||
        (mixer_processing_data->chain_type == stereo_music_processing))
    {
        AudioMusicProcessingUnregisterRole(audio_music_processing_compander_role);
        AudioMusicProcessingUnregisterRole(audio_music_processing_user_peq_role);
        AudioMusicProcessingUnregisterRole(audio_music_processing_dynamic_bass_enhancement_role);
    }

    if (mixer_processing_data->chain_type == stereo_music_processing)
    {
        AudioMusicProcessingUnregisterRole(audio_music_processing_volume_spatial_enhancement_role);
    }
}

/****************************************************************************
DESCRIPTION
    Helper function to configure operators in the processing chain 
*/
static void mixerProcessingChainConfigure(kymera_chain_handle_t chain, uint32 output_sample_rate)
{
    Operator mixer_op = ChainGetOperatorByRole(chain, processing_stereo_to_mono_role);
    
    ChainConfigureSampleRate(chain, output_sample_rate, NULL, 0);

    if (mixer_op)
        OperatorsMixerSetChannelsPerStream(mixer_op, 1, 1, 0);
}

/****************************************************************************
DESCRIPTION
    Helper function to set the UCID of the passthrough, compander, mixer,
    vse and dbe
*/
static void mixerProcessingSetUcid(kymera_chain_handle_t chain)
{
    Operator passthrough_op;
    Operator compander_op;
    Operator vse_op;
    Operator dbe_op;
    Operator peq_op;

    passthrough_op = ChainGetOperatorByRole(chain, pre_processing_role);
    if (passthrough_op)
    {
        OperatorsStandardSetUCID(passthrough_op, ucid_passthrough_processing);
    }

    compander_op = ChainGetOperatorByRole(chain, processing_compander_role);
    if (compander_op)
    {
        OperatorsStandardSetUCID(compander_op, ucid_compander_processing);
    }

    vse_op = ChainGetOperatorByRole(chain, vse_role);
    if (vse_op)
    {
        OperatorsStandardSetUCID(vse_op, ucid_vse_processing);
    }

    dbe_op = ChainGetOperatorByRole(chain, dbe_role);
    if (dbe_op)
    {
        OperatorsStandardSetUCID(dbe_op, ucid_dbe_processing);
    }

    peq_op = ChainGetOperatorByRole(chain, peq_role);
    if (peq_op)
    {
        OperatorsStandardSetUCID(peq_op, ucid_peq_resampler_0);
    }
}

/******************************************************************************/
mixer_processing_context_t mixerProcessingCreate(connection_type_t connection_type,
                                                 uint32 output_sample_rate, 
                                                 bool is_mono,
                                                 bool attenuate_volume_pre_processing)
{
    processing_chain_t type = mixerProcessingGetChainType(connection_type, is_mono);
    const chain_config_t* config = mixerProcessingGetChainConfig(type);
    
    if(config)
    {
        const operator_filters_t* filter = mixerProcessingGetChainFilter(connection_type,
                                                                         is_mono,
                                                                         attenuate_volume_pre_processing);
        kymera_chain_handle_t chain = ChainCreateWithFilter(config, filter);
        
        if(chain)
        {
            mixer_processing_data_t* mixer_processing_data = PanicUnlessNew(mixer_processing_data_t);
            
            mixer_processing_data->chain = chain;
            mixer_processing_data->chain_type = type;
            
            mixerProcessingChainConfigure(mixer_processing_data->chain, output_sample_rate);
            ChainConnect(mixer_processing_data->chain);
            mixerProcessingSetUcid(mixer_processing_data->chain);
            mixerProcessingRegisterMusicProcessingOperators(mixer_processing_data);

            return mixer_processing_data;
        }
    }
    return NULL;
}

/******************************************************************************/
kymera_chain_handle_t mixerProcessingGetChain(mixer_processing_context_t context)
{
    if(context)
        return context->chain;
    
    return NULL;
}

/******************************************************************************/
void mixerProcessingStop(mixer_processing_context_t context)
{
    if(context)
        ChainStop(context->chain);
}

/******************************************************************************/
void mixerProcessingDestroy(mixer_processing_context_t context)
{
    if(context)
    {
        mixerProcessingUnregisterMusicProcessingOperators(context);
        ChainDestroy(context->chain);
        free(context);
    }
}

/******************************************************************************/
void mixerProcessingStart(mixer_processing_context_t context)
{
    if(context)
        ChainStart(context->chain);
}

/******************************************************************************/
void mixerProcessingChangeSampleRate(mixer_processing_context_t context,
                                     uint32 output_sample_rate)
{
    if(context)
        mixerProcessingChainConfigure(context->chain, output_sample_rate);
}
