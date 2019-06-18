/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    audio_hardware_aec.c

DESCRIPTION
       Implementation of AEC management
*/

#include <stddef.h>
#include <string.h>
#include <audio_ports.h>

#include <panic.h>

#include "audio_hardware_aec.h"
#include "audio_hardware_aec_chain_config.h"
#include "audio_hardware_output.h"
#include "audio_plugin_ucid.h"
#include "audio_config.h"

#define DEFAULT_MIC_SAMPLE_RATE     ((uint32) 8000)
#define MIC_SAMPLE_RATE_16K     ((uint32) 16000)

typedef struct aec_data
{
    kymera_chain_handle_t chain;
    unsigned number_of_mics;
    uint32   output_sample_rate;
    uint32   mic_sample_rate;
} aec_data_t;

static aec_data_t aec_data =
{
    .chain = NULL,
    .number_of_mics = 0,
    .mic_sample_rate = DEFAULT_MIC_SAMPLE_RATE,
    .output_sample_rate = 0
};


/****************************************************************************
DESCRIPTION
    Register the AEC operator with the ports library
*/
static void hardwareAecRegisterExternalPorts(void)
{
    AudioPortsSetAecReference(ChainGetOutput(aec_data.chain, mic_ref_channel));

    AudioPortsSetAecOutputTerminal(ChainGetOutput(aec_data.chain, mic_a_channel), 0);
    AudioPortsSetAecOutputTerminal(ChainGetOutput(aec_data.chain, mic_b_channel), 1);

    AudioPortsSetAecMicInput(ChainGetInput(aec_data.chain, mic_a_channel), 0);
    AudioPortsSetAecMicInput(ChainGetInput(aec_data.chain, mic_b_channel), 1);
}

/****************************************************************************
DESCRIPTION
    Derive AEC UCID configuration from mic sample rate
*/
static ucid_aec_t hardwareAecGetUcid(uint32 mic_sample_rate)
{
    if (mic_sample_rate == MIC_SAMPLE_RATE_16K)
        return ucid_aec_wb;
    else
        return ucid_aec_nb;
}


/****************************************************************************
DESCRIPTION
    Apply configuration settings to the AEC operator
    Set AEC UCID and Sample Rates
*/
static void hardwareAecConfigureChain(void)
{
    Operator aec_op;

    aec_op = ChainGetOperatorByRole(aec_data.chain, aec_role_aec);
    if (aec_op)
    {
        OperatorsStandardSetUCID(aec_op, hardwareAecGetUcid(aec_data.mic_sample_rate));
        OperatorsAecSetSampleRate(aec_op, aec_data.output_sample_rate, aec_data.mic_sample_rate);
    }
}

/****************************************************************************
DESCRIPTION
    Configure the rate at which to run the microphone/reference AEC outputs
*/
static void hardwareAecSetMicRate(uint32 mic_sample_rate)
{
    aec_data.mic_sample_rate = mic_sample_rate;
}

/****************************************************************************
DESCRIPTION
    Configure the output sample rate
*/
static void hardwareAecSetOutputSampleRate(uint32 output_sample_rate)
{
    aec_data.output_sample_rate = output_sample_rate;
}

/****************************************************************************
DESCRIPTION
    Unregister any AEC ports previously registered with audio_ports
*/
static void hardwareAecClearRegisteredPorts(unsigned number_of_mics)
{
    unsigned i;
    AudioPortsSetAecReference(NULL);

    for (i = 0; i < number_of_mics; i++)
    {
        AudioPortsSetAecOutputTerminal(NULL, i);
        AudioPortsSetAecMicInput(NULL, i);
    }
}

/****************************************************************************
DESCRIPTION
    Initialise the AEC object
*/
static void hardwareInitAec(unsigned number_of_mics)
{
    aec_data.number_of_mics = number_of_mics;
    hardwareAecClearRegisteredPorts(number_of_mics);
}

/****************************************************************************
DESCRIPTION
    Create the AEC object
*/
static void hardwareAecCreate(void)
{
    const chain_config_t* config = hardwareAecGetChainConfig();

    aec_data.chain = PanicNull(ChainCreate(config));

    /*AEC sample rates must be set before it is connected */
    hardwareAecConfigureChain();

    ChainConnect(aec_data.chain);
    hardwareAecRegisterExternalPorts();
}

/****************************************************************************
DESCRIPTION
    Get the chain handle for the AEC object
*/
static kymera_chain_handle_t hardwareAecGetChain(void)
{
    return aec_data.chain;
}

/****************************************************************************
DESCRIPTION
    Start processing within the AEC object
*/
static void hardwareAecStart(void)
{
    if (aec_data.chain != NULL)
    {
        ChainStart(aec_data.chain);
    }
}

/****************************************************************************
DESCRIPTION
    Stop processing within the AEC object
*/
static void hardwareAecStop(void)
{
    if (aec_data.chain != NULL)
    {
        ChainStop(aec_data.chain);
    }
}

/****************************************************************************
DESCRIPTION
    Destroy the AEC object
*/
static void hardwareAecDestroy(void)
{
    if (aec_data.chain != NULL)
    {
        ChainDestroy(aec_data.chain);
        hardwareAecClearRegisteredPorts(aec_data.number_of_mics);
        
        aec_data.chain = NULL;
        aec_data.mic_sample_rate = DEFAULT_MIC_SAMPLE_RATE;
        aec_data.number_of_mics = 0;
    }
}


/****************************************************************************/
void hardwareAecConfigureMicSampleRate(uint32 mic_sample_rate)
{
    hardwareAecSetMicRate(mic_sample_rate);
   
    hardwareAecConfigureChain();   
}

/****************************************************************************/
static bool hardwareAecIsMusicInput(connection_type_t connection_type)
{
    return ((connection_type == CONNECTION_TYPE_MUSIC)||(connection_type == CONNECTION_TYPE_MUSIC_A2DP));
}

/****************************************************************************/
bool hardwareIsAecRequired(connection_type_t connection_type)
{
    if(connection_type == CONNECTION_TYPE_VOICE)
        return TRUE;

    if (hardwareAecIsMusicInput(connection_type))
    {
        if (AudioConfigGetMaximumConcurrentInputs() > 1)
            return TRUE;
    }

    return FALSE;
}


/****************************************************************************/
void hardwareAecCreateAndConnect(const audio_hardware_connect_t* hw_connect_data)
{
    hardwareInitAec(MAX_NUMBER_OF_MICS);

    hardwareAecSetOutputSampleRate(hw_connect_data->output_sample_rate);

    if(hw_connect_data->connection_type == CONNECTION_TYPE_VOICE)
    {
        hardwareAecSetMicRate(hw_connect_data->sample_rate);
    }

    hardwareAecCreate();
  
    hardwareMapAecToAudioOutput(hardwareAecGetChain());

    /*Connection order of AEC is important, connect outputs first */
    hardwareConnectToAudioOutput(hw_connect_data->output_sample_rate);

    hardwareConnectAecInputs(hw_connect_data, hardwareAecGetChain());

    hardwareAecStart();
}

/****************************************************************************/
void hardwareAecDisconnect(void)
{
    hardwareAecStop();
    hardwareAecDestroy();
}

/****************************************************************************/
void hardwareAecMuteMicOutput(bool enable)
{
    Operator aec_op;

    aec_op = ChainGetOperatorByRole(aec_data.chain, aec_role_aec);
    if (aec_op)
    {
        OperatorsAecMuteMicOutput(aec_op, enable);
    }
}

