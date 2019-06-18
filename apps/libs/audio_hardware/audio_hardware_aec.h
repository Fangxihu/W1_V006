/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_hardware_aec.h

DESCRIPTION
       Declarations for AEC chain management
*/

#ifndef _AUDIO_HARDWARE_AEC_H_
#define _AUDIO_HARDWARE_AEC_H_

#include "audio_hardware.h"
#include <chain.h>


/****************************************************************************
NAME
    hardwareAecConfigureMicSampleRate
    
DESCRIPTION
    Configure AEC sample rates and update UCID to match    
*/
void hardwareAecConfigureMicSampleRate(uint32 mic_sample_rate);

/****************************************************************************
NAME
    hardwareIsAecRequired

DESCRIPTION
    Check if the AEC block is required - This block is required for HFP and concurrent audio
*/
bool hardwareIsAecRequired(connection_type_t connection_type);


/****************************************************************************
NAME
    hardwareAecCreateAndConnect

DESCRIPTION
    Creates the AEC chain, connects the AEC chain to output and starts the AEC chain 
*/
void hardwareAecCreateAndConnect(const audio_hardware_connect_t* hw_connect_data);

/****************************************************************************
NAME
    hardwareAecDisconnect

DESCRIPTION
    Stop processing within the AEC object and destroy the chain
*/
void hardwareAecDisconnect(void);

/****************************************************************************
NAME
    hardwareAecMuteMicOutput

DESCRIPTION
    Mutes microphone output
*/
void hardwareAecMuteMicOutput(bool enable);

#endif /* _AUDIO_HARDWARE_AEC_H_ */
