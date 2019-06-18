/****************************************************************************
Copyright (c) 2016 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_music_processing.h

DESCRIPTION
    This library provides an interface into the music processing capabilities

*/

#ifndef _AUDIO_MUSIC_PROCESSING_H_
#define _AUDIO_MUSIC_PROCESSING_H_

#include <operators.h>
#include <message.h>
#include <audio_plugin_if.h>
#include <audio_plugin_ucid.h>

typedef enum
{
    audio_music_processing_compander_role,
    audio_music_processing_user_peq_role,
    audio_music_processing_volume_spatial_enhancement_role,
    audio_music_processing_dynamic_bass_enhancement_role,
    /* Do not modify below this line */
    max_music_processing_roles
} audio_music_processing_role_t;


/****************************************************************************
DESCRIPTION
    Function to handle callbacks from kymera
    Responses to Get User Eq Parameter messages are received and processed.
*/
void AudioMusicProcessingKymeraMessageHandler(Task task, MessageId id, Message message);

/****************************************************************************
DESCRIPTION
    Function to associate a specific operator with a specific role.
*/
void AudioMusicProcessingRegisterRole(audio_music_processing_role_t role, Operator op);

/****************************************************************************
DESCRIPTION
    Function to associate a specific operator with a specific role.
*/
void AudioMusicProcessingUnregisterRole(audio_music_processing_role_t role);

/****************************************************************************
DESCRIPTION
    Function to set the processing mode of the specified role.
*/
void AudioMusicProcessingEnableProcessing(audio_music_processing_role_t role, bool mode);

/****************************************************************************
DESCRIPTION
    Function to set a new config for the specified peq role.
*/
void AudioMusicProcessingSelectUserPeqConfig(peq_config_t config);

/****************************************************************************
DESCRIPTION
    Function to return the id of the currently provisioned peq setting.
*/
peq_config_t AudioMusicProcessingGetPeqConfig(void);

/****************************************************************************
DESCRIPTION
    Function to set a group of peq parameters.
*/
void AudioMusicProcessingSetUserEqParameter(const audio_plugin_user_eq_param_t* param);

/****************************************************************************
DESCRIPTION
    Function to apply a collection of stored peq parameters.
    Note: After applying the stored parameters, the values are cleared.
*/
void AudioMusicProcessingApplyUserEqParameters(void);

/****************************************************************************
DESCRIPTION
    Function to clear a collection of stored peq parameters.
*/
void AudioMusicProcessingClearUserEqParameters(void);

/****************************************************************************
DESCRIPTION
    Function to return a peq parameter.
    Note: Data is returned via the messaging interface
*/
void AudioMusicProcessingGetUserEqParameter(const audio_plugin_user_eq_param_id_t* param_id, Task audio_plugin);

/****************************************************************************
DESCRIPTION
    Function to return a group of peq parameters.
    Note: Data is returned via the messaging interface
*/
void AudioMusicProcessingGetUserEqParameters(const unsigned number_of_params, const audio_plugin_user_eq_param_id_t* param_ids, Task audio_plugin);

#ifdef HOSTED_TEST_ENVIRONMENT
/****************************************************************************
DESCRIPTION
    Reset any static variables
    This is only intended for unit test and will panic if called in a release build.
*/
void AudioMusicProcessingTestReset(void);
#endif

#endif /* _AUDIO_MUSIC_PROCESSING_H_ */
