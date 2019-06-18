/*******************************************************************************
Copyright (c) 2015-2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    anc_data.c

DESCRIPTION
    Encapsulation of the ANC VM Library data.
*/

#include "CONFIG_HYDRACORE/anc_config_data.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_config_read.h"

#include <stdlib.h>
#include <string.h>
#include <audio_config.h>

typedef struct
{
    anc_state state;
    anc_mode mode;
    anc_mic_params_t mic_params;
    anc_config_t config_data;
    anc_fine_tune_gain_step_t fine_tune_gain_step;
} anc_lib_data_t;


static anc_lib_data_t anc_lib_data;

/******************************************************************************/
bool ancDataInitialise(void)
{
    memset(&anc_lib_data, 0, sizeof(anc_lib_data_t));
    return TRUE;
}

/******************************************************************************/
bool ancDataDeinitialise(void)
{
    memset(&anc_lib_data, 0, sizeof(anc_lib_data_t));
    return TRUE;
}

/******************************************************************************/
void ancDataSetState(anc_state state)
{
    anc_lib_data.state = state;

    ancConfigDataUpdateOnStateChange();
}

/******************************************************************************/
anc_state ancDataGetState(void)
{
    return anc_lib_data.state;
}

/******************************************************************************/
void ancDataSetMicParams(anc_mic_params_t *mic_params)
{
    anc_lib_data.mic_params = *mic_params;
}

/******************************************************************************/
anc_mic_params_t* ancDataGetMicParams(void)
{
    return &anc_lib_data.mic_params;
}

/******************************************************************************/
void ancDataSetMode(anc_mode mode)
{
    anc_lib_data.mode = mode;

    ancConfigDataUpdateOnModeChange();
}

/******************************************************************************/
anc_mode ancDataGetMode(void)
{
    return anc_lib_data.mode;
}

anc_config_t * ancDataGetConfigData(void)
{
    return &anc_lib_data.config_data;
}

/******************************************************************************/
void ancDataSetFineTuneGain(anc_fine_tune_gain_step_t step)
{
    anc_lib_data.fine_tune_gain_step = step;
}

anc_fine_tune_gain_step_t ancDataGetFineTuneGain(void)
{
    return anc_lib_data.fine_tune_gain_step;
}

/******************************************************************************/
anc_mode_config_t * ancDataGetCurrentModeConfig(void)
{
    if(ancDataGetMode() == anc_mode_active)
    {
        return &ancDataGetConfigData()->active;
    }
    else if(ancDataGetMode() == anc_mode_leakthrough)
    {
        return &ancDataGetConfigData()->leakthrough;
    }
    ANC_PANIC();
    return NULL;
}

/******************************************************************************/
void ancDataRetrieveAndPopulateTuningData(void)
{
    ancConfigReadPopulateAncData(ancDataGetConfigData());
}

/******************************************************************************/
bool ancDataIsLeftChannelConfigurable(void)
{
    return (ancDataGetMicParams()->enabled_mics & (feed_forward_left | feed_back_left));
}

bool ancDataIsRightChannelConfigurable(void)
{
    return (ancDataGetMicParams()->enabled_mics & (feed_forward_right | feed_back_right));
}

audio_channel ancConfigDataGetAudioChannelForMicPath(anc_path_enable mic_path)
{
    anc_mic_params_t* anc_mic_params = ancDataGetMicParams();
    audio_channel mic_channel = AUDIO_CHANNEL_A;

    switch(anc_mic_params->enabled_mics)
    {
        case feed_forward_mode:
            if(anc_mic_params->feed_forward_left.instance == anc_mic_params->feed_forward_right.instance)
            {
                if(mic_path == feed_forward_right)
                {
                    mic_channel = AUDIO_CHANNEL_B;
                }
            }
            break;
        case feed_back_mode:
            if(anc_mic_params->feed_back_left.instance == anc_mic_params->feed_back_right.instance)
            {
                if(mic_path == feed_back_right)
                {
                    mic_channel = AUDIO_CHANNEL_B;
                }
            }
            break;
        case hybrid_mode:
            if(anc_mic_params->feed_forward_left.instance == anc_mic_params->feed_back_left.instance)
            {
                if(mic_path == feed_back_left || mic_path == feed_back_right)
                {
                    mic_channel = AUDIO_CHANNEL_B;
                }
            }
            else
            {
                if(mic_path == feed_forward_right || mic_path == feed_back_right)
                {
                    mic_channel = AUDIO_CHANNEL_B;
                }
            }
            break;
        case hybrid_mode_left_only:
            if(anc_mic_params->feed_forward_left.instance == anc_mic_params->feed_back_left.instance)
            {
                if(mic_path == feed_back_left)
                {
                    mic_channel = AUDIO_CHANNEL_B;
                }
            }
            break;
        case hybrid_mode_right_only:
            if(anc_mic_params->feed_forward_right.instance == anc_mic_params->feed_back_right.instance)
            {
                if(mic_path == feed_back_right)
                {
                    mic_channel = AUDIO_CHANNEL_B;
                }
            }
            break;
        default:
            mic_channel = AUDIO_CHANNEL_A;
            break;
    }
    return mic_channel;
}

audio_mic_params * ancConfigDataGetMicForMicPath(anc_path_enable mic_path)
{
    audio_mic_params * mic_params = &ancDataGetMicParams()->feed_forward_left;
    switch(mic_path)
    {
        case feed_forward_left:
            mic_params = &ancDataGetMicParams()->feed_forward_left;
            break;
        case feed_forward_right:
            mic_params = &ancDataGetMicParams()->feed_forward_right;
            break;
        case feed_back_left:
            mic_params = &ancDataGetMicParams()->feed_back_left;
            break;
        case feed_back_right:
            mic_params = &ancDataGetMicParams()->feed_back_right;
            break;
        default:
            Panic();
            break;
    }
    return mic_params;
}

uint32 ancConfigDataGetHardwareGainForMicPath(anc_path_enable mic_path)
{
    uint32 gain = 0;
    switch(mic_path)
    {
        case feed_forward_left:
            gain = ancDataGetConfigData()->hardware_gains.feed_forward_mic_left;
            break;
        case feed_forward_right:
            gain = ancDataGetConfigData()->hardware_gains.feed_forward_mic_right;
            break;
        case feed_back_left:
            gain = ancDataGetConfigData()->hardware_gains.feed_back_mic_left;
            break;
        case feed_back_right:
            gain = ancDataGetConfigData()->hardware_gains.feed_back_mic_right;
            break;
        default:
            Panic();
            break;
    }
    return gain;
}

