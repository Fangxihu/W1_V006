/*******************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    aov.c

DESCRIPTION
    AOV VM Library functions.
*/

#include <stdio.h>
#include <stdlib.h>
#include <vmtypes.h>
#include <panic.h>
#include <resource_manager.h>
#include "aov_private.h"
#include "aov_graph.h"
#include "aov_debug.h"

typedef enum {
    aov_state_not_running,
    aov_state_running,
}aov_state_t;

static void aovPluginMessageHandler(Task task, MessageId id, Message message);
static void handlePluginConnectMsg(AUDIO_PLUGIN_CONNECT_MSG_T *message);
static void handlePluginDisconnectMsg(void);
static void handlePluginChangeTriggerPhraseMsg(AUDIO_PLUGIN_CHANGE_TRIGGER_PHRASE_MSG_T *message);
static bool handleMicrophoneAvailChange(rm_msg_id_t msg);
static void internalMessageHandler(Task task, MessageId id, Message message);
static aov_state_t GetGraphState(void);

bool aovResourceCallback(rm_msg_id_t msg, rm_resource_id_t res);


const TaskData aovInternalTask = {internalMessageHandler};
const TaskData aov_plugin = {aovPluginMessageHandler};

static Task appTask = NULL;
static FILE_INDEX trigger_phrase_data_file = FILE_NONE;
static audio_mic_params *mic_params;
static unsigned conditionMask = 0;
static uint16 graphTimeoutMs = 1000;
static aov_state_t reported_graph_state = aov_state_not_running;


typedef enum {
    aov_condition_mic,
    aov_condition_file,
    aov_condition_enabled,
    aov_condition_running /* This must always be the last one */
}aov_condition_type_t;

#define CONDITION_RUNNING_BIT ((unsigned)1 << aov_condition_running)
#define CONDITION_ENABLED_BIT ((unsigned)1 << aov_condition_enabled)
#define IS_GRAPH_RUNNING ((conditionMask & CONDITION_RUNNING_BIT))
#define IS_AOV_ENABLED ((conditionMask & CONDITION_ENABLED_BIT))
#define ALL_CONDITION_BITS ((1 << aov_condition_running) - 1 )

/******************************************************************************/
static aov_state_t GetGraphState(void)
{
    if(IS_GRAPH_RUNNING)
    {
        return aov_state_running;
    }
    return aov_state_not_running;
}

/******************************************************************************/
static bool CanGraphBeEnabled(void)
{
    return ((conditionMask & ALL_CONDITION_BITS) == ALL_CONDITION_BITS);
}

/******************************************************************************/
static void UpdateGraphState(void)
{
    if(CanGraphBeEnabled() && !IS_GRAPH_RUNNING)
    {
        aovCreateGraph(trigger_phrase_data_file, *mic_params);
        conditionMask |= CONDITION_RUNNING_BIT;
    }
    else if(!CanGraphBeEnabled() && IS_GRAPH_RUNNING)
    {
        MessageCancelAll((Task)&aovInternalTask, AOV_INT_MESSAGE_RESET_GRAPH);
        aovDestroyGraph();
        conditionMask &= ~CONDITION_RUNNING_BIT;
    }
    MessageCancelAll((Task)&aovInternalTask, AOV_INT_MESSAGE_CHECK_NOTIFY_STATE_CHANGE);
    MessageSend((Task)&aovInternalTask, AOV_INT_MESSAGE_CHECK_NOTIFY_STATE_CHANGE, NULL);
}
/******************************************************************************/
static void SetConditionState(aov_condition_type_t condition, bool available)
{
    if(available)
    {
        conditionMask |= (1 << condition);
    }
    else
    {
        conditionMask &= ~((unsigned)1 << condition);
    }
    UpdateGraphState();
}

/******************************************************************************/
static void UpdateTriggerPhraseDataFile(FILE_INDEX file)
{
    if(trigger_phrase_data_file != file)
    {
        trigger_phrase_data_file = file;
        /* Always go to false first as this will bring down the 
           graph if we are changing a running graph */
        SetConditionState(aov_condition_file, FALSE);
        SetConditionState(aov_condition_file, (file > FILE_ROOT));
     }
}


/******************************************************************************/
static void aovPluginMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    AOV_DEBUG_INFO(("AoV Message Handler Message = %d\n", id));

    switch(id)
    {
        case AUDIO_PLUGIN_CONNECT_MSG:
            AOV_DEBUG_INFO(("AoV: AUDIO_PLUGIN_CONNECT_MSG\n"));
            handlePluginConnectMsg((AUDIO_PLUGIN_CONNECT_MSG_T*)message);
            break;

        case AUDIO_PLUGIN_DISCONNECT_MSG:
            AOV_DEBUG_INFO(("AoV: AUDIO_PLUGIN_DISCONNECT_MSG\n"));
            handlePluginDisconnectMsg();
            break;

        case AUDIO_PLUGIN_CHANGE_TRIGGER_PHRASE_MSG:
            AOV_DEBUG_INFO(("AoV: AUDIO_PLUGIN_CHANGE_TRIGGER_PHRASE_MSG\n"));
            handlePluginChangeTriggerPhraseMsg((AUDIO_PLUGIN_CHANGE_TRIGGER_PHRASE_MSG_T*)message);
            break;

        default:
            AOV_DEBUG_INFO(("AoV: Unknown message in plugin\n"));
            Panic();
            break;
    }
}

/******************************************************************************/
static void handlePluginConnectMsg(AUDIO_PLUGIN_CONNECT_MSG_T *message)
{

    rm_status_t mic_request_status;

    PanicNull(message);
    PanicNull(message->app_task);
    PanicNull(message->params);

    if(!(IS_AOV_ENABLED))
    {
        SetConditionState(aov_condition_enabled, TRUE);

        appTask = message->app_task;

        UpdateTriggerPhraseDataFile(((aov_connect_params_t*)(message->params))->trigger_phrase_data_file);

        mic_params = &((aov_connect_params_t*)(message->params))->mic_params->mic_a;

        graphTimeoutMs = ((aov_connect_params_t*)(message->params))->graph_timeout_ms;

        free(message->params);

        mic_request_status = ResourceManagerAcquireResource(rm_resource_voice_mic, 
                                                            (rm_callback)aovResourceCallback);

        switch(mic_request_status)
        {
            case rm_status_success:
                SetConditionState(aov_condition_mic, TRUE);
                break;

            case rm_status_in_use_queued:
                /* Set up graph once resource avail */
                break;

            case rm_status_bad_parameter:
                Panic();
                break;

            case rm_status_in_use_rejected:
                AOV_DEBUG_INFO(("AoV: Failed to obtain microphone resource\n"));
                Panic();
                break;

            case rm_status_no_memory:
                AOV_DEBUG_INFO(("AoV: Resource manager failed with rm_status_no_memory\n"));
                Panic();
                break;

            default:
                AOV_DEBUG_INFO(("AoV: Unknown response from resource manager\n"));
                SetConditionState(aov_condition_enabled, FALSE);
                break;

        }
    }
}
/******************************************************************************/
static void handlePluginDisconnectMsg(void)
{
    SetConditionState(aov_condition_enabled, FALSE);
    SetConditionState(aov_condition_mic, FALSE);
    ResourceManagerReleaseResource(rm_resource_voice_mic, (rm_callback)aovResourceCallback);
}

/******************************************************************************/
static void handlePluginChangeTriggerPhraseMsg(AUDIO_PLUGIN_CHANGE_TRIGGER_PHRASE_MSG_T *message)
{
    PanicNull(message);
    UpdateTriggerPhraseDataFile(message->trigger_phrase_data_file);
}
/******************************************************************************/
bool aovResourceCallback(rm_msg_id_t msg, rm_resource_id_t res)
{
    switch(res)
    {
        case rm_resource_voice_mic:
            return handleMicrophoneAvailChange(msg);

        default:
            break;
    }
    return FALSE;
}

/******************************************************************************/
static bool handleMicrophoneAvailChange(rm_msg_id_t msg)
{
    switch(msg)
    {
        case rm_msg_release_req:
            SetConditionState(aov_condition_mic, FALSE);
            return TRUE;

        case rm_msg_available_ind:
            SetConditionState(aov_condition_mic, TRUE);
            return TRUE;

        default:
            break;
    }
    return FALSE;
}

/******************************************************************************/
static void internalMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch(id)
    {
         case AOV_INT_MESSAGE_RESET_GRAPH:
            aovResetGraph();
            if(appTask)
            {
                MessageSend(appTask, AOV_MESSAGE_RESET_TIMEOUT, NULL);
            }
            break;

        case AOV_INT_MESSAGE_SVA_TRIGGERED:
            if(appTask)
            {
                MessageSend(appTask, AOV_MESSAGE_TRIGGERED, NULL);
            }
            MessageSendLater((Task)&aovInternalTask, AOV_INT_MESSAGE_RESET_GRAPH, NULL, graphTimeoutMs);
            break;

        case AOV_INT_MESSAGE_CHECK_NOTIFY_STATE_CHANGE:
            if(reported_graph_state != GetGraphState())
            {
                reported_graph_state = GetGraphState();
                if(appTask)
                {
                    MESSAGE_MAKE(app_message, AOV_PLUGIN_AOV_ENABLED_MSG_T);
                    app_message->enabled = (GetGraphState() == aov_state_running) ? TRUE : FALSE;
                    MessageSend(appTask, AOV_MESSAGE_AOV_ENABLED, app_message);
                }
            }
            break;

        default:
            break;
    }
}

/******************************************************************************/
#ifdef HOSTED_TEST_ENVIRONMENT
void aovTestResetLib(void)
{
    appTask = NULL;
    conditionMask = 0;
    trigger_phrase_data_file = FILE_NONE;
    reported_graph_state = aov_state_not_running;
}
#endif
