/*******************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    aov_graph.c

DESCRIPTION
    Functions to configure the AoV graph
*/
#include "aov_graph.h"
#include <operators.h>
#include <operator.h>
#include <audio_plugin_common.h>
#include <audio_clock.h>
#include <codec_.h>
#include "aov_private.h"
#include "chain.h"

#define AUDIO_SAMPLE_RATE 16000

typedef enum
{
    aov_role_sva,
    aov_role_vad
}aov_role_t;

enum
{
    aov_input_vad
}aov_input_t;

static const operator_config_t ops[] = {MAKE_OPERATOR_CONFIG(capability_id_vad, aov_role_vad),
                                        MAKE_OPERATOR_CONFIG(capability_id_sva, aov_role_sva)};

static const operator_endpoint_t inputs[] = {{aov_role_vad, aov_input_vad, 0}};

static const operator_connection_t connections[] = {{aov_role_vad, 0, aov_role_sva, 0, 1}};

static const chain_config_t config = MAKE_CHAIN_CONFIG_NO_OUTPUTS(chain_id_aov, audio_ucid_aov, ops,
                                                                  inputs, connections);

static void enableIIRFilter(audio_instance instance, bool enable);
static void svaOperatorMessageHandler(Task task, MessageId id, Message message);
const TaskData svaTask = {svaOperatorMessageHandler};

typedef struct {
    DataFileID trigger_data;
    kymera_chain_handle_t aov_chain_handle;
    audio_instance mic_instance;
    bool mic_is_digital;
} aov_graph_data_t;

static aov_graph_data_t graph_data = {0, NULL, 0, FALSE};

/******************************************************************************/
void aovCreateGraph(FILE_INDEX trigger_phrase_file, const audio_mic_params mic_params)
{
    if((trigger_phrase_file > FILE_ROOT) && (!graph_data.aov_chain_handle))
    {
        Operator sva_operator;

        Source mic_source;

        graph_data.aov_chain_handle = PanicNull(ChainCreate(&config));

        ChainConnect(graph_data.aov_chain_handle);

        sva_operator = ChainGetOperatorByRole(graph_data.aov_chain_handle, aov_role_sva);

        graph_data.trigger_data = PanicZero(OperatorDataLoad(trigger_phrase_file, DATAFILE_BIN, FALSE));

        graph_data.mic_is_digital = mic_params.is_digital;

        OperatorsConfigureSvaTriggerPhrase(sva_operator, graph_data.trigger_data);

        mic_source = PanicNull(AudioPluginMicSetup(AUDIO_CHANNEL_A, mic_params, AUDIO_SAMPLE_RATE));

        if(graph_data.mic_is_digital)
        {
            graph_data.mic_instance = AudioPluginGetMicInstance(mic_params);

            enableIIRFilter(graph_data.mic_instance, TRUE);
        }

        PanicNull(StreamConnect(mic_source, ChainGetInput(graph_data.aov_chain_handle, aov_input_vad)));

        MessageOperatorTask(sva_operator, (Task)&svaTask);

        /* Enable notification from SVA */
        PanicFalse(OperatorFrameworkTriggerNotificationStart(TRIGGER_ON_VTD, sva_operator));

        ChainStart(graph_data.aov_chain_handle);

        ChainSleep(graph_data.aov_chain_handle);

    }
}
/******************************************************************************/
void aovDestroyGraph(void)
{
    if(graph_data.aov_chain_handle)
    {

        ChainWake(graph_data.aov_chain_handle);

        if(graph_data.mic_is_digital)
        {
            enableIIRFilter(graph_data.mic_instance, FALSE);
        }

        StreamDisconnect(NULL, ChainGetInput(graph_data.aov_chain_handle, aov_input_vad));

        ChainStop(graph_data.aov_chain_handle);

        PanicFalse(OperatorFrameworkTriggerNotificationStop());

        PanicFalse(OperatorDataUnload(graph_data.trigger_data));

        ChainDestroy(graph_data.aov_chain_handle);

        graph_data.aov_chain_handle = NULL;
    }
}
/******************************************************************************/
void aovResetGraph(void)
{
    if(graph_data.aov_chain_handle)
    {
        ChainWake(graph_data.aov_chain_handle);
        ChainStop(graph_data.aov_chain_handle);
        OperatorFrameworkTriggerNotificationStop();
        PanicFalse(OperatorFrameworkTriggerNotificationStart(TRIGGER_ON_VTD,
                                                             ChainGetOperatorByRole(graph_data.aov_chain_handle,
                                                                                    aov_role_sva)));
        ChainStart(graph_data.aov_chain_handle);
        ChainSleep(graph_data.aov_chain_handle);
    }
}

/******************************************************************************/
static void enableIIRFilter(audio_instance instance, bool enable)
{
    static const uint16 mic_iir_coeff[] = { 0x7ac0 >> 5, 0x8000 >> 4, 0x4000 >> 4,
                                            0x8554 >> 4, 0x3ae2 >> 4, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00};

    PanicFalse(CodecSetIirFilter(instance,
                                 AUDIO_CHANNEL_A,
                                 enable,
                                 (uint16*)mic_iir_coeff));
}

/******************************************************************************/
static void svaOperatorMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(message);
    UNUSED(task);
    UNUSED(id);

    MessageSend((Task)&aovInternalTask, AOV_INT_MESSAGE_SVA_TRIGGERED, NULL);
}


/******************************************************************************/
void aovTestSetGraphData(DataFileID file, kymera_chain_handle_t chain_handle, bool digi_mic)
{
#ifndef AOV_TEST_BUILD
    UNUSED(file);
    UNUSED(chain_handle);
    UNUSED(digi_mic);
    Panic();
#else
    graph_data.trigger_data = file;
    graph_data.aov_chain_handle = chain_handle;
    graph_data.mic_is_digital = digi_mic;
#endif /* AOV_TEST_BUILD */
}
