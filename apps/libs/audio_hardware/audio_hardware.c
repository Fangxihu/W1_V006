/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_hardware.c

DESCRIPTION
    This plugin deals with connecting the audio chain to audio hardware
    Audio mixer would build everything up to the point before AEC reference and then call in to audio_hardware to connect to the output.
    Accoustic Echo Cancellation (AEC) is an optional feature in the audio_hardware - required only in cases of HFP and concurrent audio support.
    */

#include <vmtypes.h>
#include "audio_hardware.h"
#include "audio_ports.h"
#include "audio_hardware_aec.h"
#include "audio_output.h"
#include "csr_i2s_audio_plugin.h"
#include "audio_hardware_output.h"


static bool hardware_connected = FALSE;


/****************************************************************************/
bool AudioHardwareConnect(const audio_hardware_connect_t* hw_connect_data)
{  
    if (!hardware_connected)
    {
        /*Create AEC chain if applicable*/
        if (hardwareIsAecRequired(hw_connect_data->connection_type))
        {
            hardwareAecCreateAndConnect(hw_connect_data);
        }        
        else
        {
            hardwareConnectDirectToOutput(hw_connect_data);
        }

        hardware_connected = TRUE;
    }

    return hardware_connected;
}

/****************************************************************************/
bool AudioHardwareDisconnect(void)
{
    if (hardware_connected)
    {
        hardwareAecDisconnect();
        AudioOutputDisconnect();

        hardware_connected = FALSE;
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************/
audio_hardware_speaker_config_t AudioHardwareGetSpeakerConfig(void)
{
    audio_hardware_speaker_config_t speaker_config = speaker_stereo;

    bool lp = AudioOutputIsOutputMapped(audio_output_primary_left);
    bool rp = AudioOutputIsOutputMapped(audio_output_primary_right);
    bool ls = AudioOutputIsOutputMapped(audio_output_secondary_left);
    bool rs = AudioOutputIsOutputMapped(audio_output_secondary_right);
    bool sw = AudioOutputIsOutputMapped(audio_output_wired_sub);
    
    if (lp && rp && ls && rs)
    {
        speaker_config = speaker_stereo_xover;
    }
    else if (lp && rp && sw)
    {
        speaker_config = speaker_stereo_bass;
    }
    else if (lp && rp && !ls && !rs)
    {
        speaker_config = speaker_stereo;
    }
    else if (lp && !rp && !ls && !rs)
    {
        speaker_config = speaker_mono;
    }
    else
    {
        Panic();
    }

    return speaker_config;
}

void AudioHardwareConfigureMicSampleRate(uint32 mic_sample_rate)
{
    hardwareAecConfigureMicSampleRate(mic_sample_rate);
}

/****************************************************************************/
void AudioHardwareMuteMicOutput(bool enable)
{
    hardwareAecMuteMicOutput(enable);
}

