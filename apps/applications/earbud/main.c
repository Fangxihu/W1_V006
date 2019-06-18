/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       main.c
\brief      Main application task
*/

#include <hfp.h>
#include <a2dp.h>
#include <avrcp.h>
#include <connection.h>
#include <panic.h>
#include <pio.h>
#include <ps.h>
#include <string.h>
#include <boot.h>
#include <os.h>

#include "av_headset.h"
#include "av_headset_auth.h"
#include "av_headset_ui.h"
#include "av_headset_pairing.h"
#include "av_headset_scan_manager.h"
#include "av_headset_log.h"

/*! Application data structure */
appTaskData globalApp;

/*! Global debug line counter */
uint16 globalDebugLineCount = 0;

/*! \brief Handle ACL Mode Changed Event

    This message is received when the link mode of an ACL has changed from sniff
    to active or vice-versa.  Currently the application ignores this message.
*/    
static void appHandleClDmModeChangeEvent(CL_DM_MODE_CHANGE_EVENT_T *evt)
{
    DEBUG_LOGF("appHandleClDmModeChangeEvent, event %d, %x,%x,%lx", evt->mode, evt->bd_addr.nap, evt->bd_addr.uap, evt->bd_addr.lap);
#ifdef INCLUDE_SCOFWD
    CL_DM_MODE_CHANGE_EVENT_T *fwd = PanicUnlessNew(CL_DM_MODE_CHANGE_EVENT_T);

    /* Support for the Sco Forwarding test command, appTestScoFwdLinkStatus() */
    *fwd = *evt;
    MessageSend(&testTask,CL_DM_MODE_CHANGE_EVENT,fwd);
#endif

    UNUSED(evt);
}

/*! \brief Handle subsystem event report. */
static void appHandleSubsystemEventReport(MessageSubsystemEventReport *evt)
{
    UNUSED(evt);
    DEBUG_LOGF("appHandleSubsystemEventReport, ss_id=%d, level=%d, id=%d, cpu=%d, occurrences=%d, time=%d",
        evt->ss_id, evt->level, evt->id, evt->cpu, evt->occurrences, evt->time);
}

/*! \brief System Message Handler

    This function is the message handler for system messages. They are 
    routed to existing handlers. If execution reaches the end of the 
    function then it is assumed that the message is unhandled.
*/
static void appHandleSystemMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case MESSAGE_SUBSYSTEM_EVENT_REPORT:
            appHandleSubsystemEventReport((MessageSubsystemEventReport *)message);
            return;
#ifdef INCLUDE_DFU
        case MESSAGE_IMAGE_UPGRADE_ERASE_STATUS:
        case MESSAGE_IMAGE_UPGRADE_COPY_STATUS:
        case MESSAGE_IMAGE_UPGRADE_AUDIO_STATUS:
            appUpgradeHandleMessage(id, message);
            return;
#endif

        default:
            break;
    }

    appHandleSysUnexpected(id);
}

/*  Handler for the INIT_CFM message.

    Used to register the handler that decides whether to allow entry
    to low power mode, before passing the #INIT_CFM message to 
    the state machine handler.

    \param message The INIT_CFM message received (if any).
 */
static void appHandleInitCfm(Message message)
{
    appSmHandleMessage(appGetSmTask(), INIT_CFM, message);
}


/*! \brief Message Handler

    This function is the main message handler for the main application task, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/    
static void appHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    if (id == appInitWaitingForMessageId())
    {
        appInitHandleMessage(id, message);
        return;
    }

    /* Handle Connection Library messages */       
    switch (id)
    {
        case CL_SM_SEC_MODE_CONFIG_CFM:
            appPairingHandleClSmSecurityModeConfigConfirm((CL_SM_SEC_MODE_CONFIG_CFM_T *)message);
            return;

        case CL_SM_PIN_CODE_IND:
            appPairingHandleClPinCodeIndication((CL_SM_PIN_CODE_IND_T *)message);
            return;

        case CL_SM_AUTHENTICATE_CFM:
            appPairingHandleClSmAuthenticateConfirm((CL_SM_AUTHENTICATE_CFM_T *)message);
            return;

        case CL_SM_AUTHORISE_IND:
            if (!appPairingHandleClSmAuthoriseIndication((CL_SM_AUTHORISE_IND_T *)message))
                appConManagerHandleClSmAuthoriseIndication((CL_SM_AUTHORISE_IND_T *)message);
            return;

        case CL_SM_IO_CAPABILITY_REQ_IND:
            appPairingHandleClSmIoCapabilityReqIndication((CL_SM_IO_CAPABILITY_REQ_IND_T *)message);
            return;

        case CL_SM_USER_CONFIRMATION_REQ_IND:
            appPairingHandleClSmUserConfirmationReqIndication((CL_SM_USER_CONFIRMATION_REQ_IND_T *)message);
            return;
    
        case CL_SM_REMOTE_IO_CAPABILITY_IND:
            appPairingHandleClSmRemoteIoCapabilityIndication((CL_SM_REMOTE_IO_CAPABILITY_IND_T *)message);
            return;

        case CL_SM_USER_PASSKEY_NOTIFICATION_IND:
        case CL_SM_KEYPRESS_NOTIFICATION_IND:
        case CL_DM_WRITE_APT_CFM:
            return;

        case CL_SM_ENCRYPTION_CHANGE_IND:
            appAuthHandleClSmEncryptionChangeInd((CL_SM_ENCRYPTION_CHANGE_IND_T *)message);
            return;
            
        case CL_SM_USER_PASSKEY_REQ_IND:
            appPairingHandleClSmUserPasskeyReqIndication((CL_SM_USER_PASSKEY_REQ_IND_T *)message);
            return;
                        
        case CL_DM_LINK_SUPERVISION_TIMEOUT_IND:
            return;

        case CL_DM_ROLE_CFM:
            appLinkPolicyHandleClDmRoleConfirm((CL_DM_ROLE_CFM_T *)message);
            return;
        
        case CL_DM_ROLE_IND:
            appLinkPolicyHandleClDmRoleIndication((CL_DM_ROLE_IND_T *)message);
            return;

        case CL_DM_ACL_OPENED_IND:
            appConManagerHandleClDmAclOpenedIndication((CL_DM_ACL_OPENED_IND_T *)message);
            return;
        
        case CL_DM_ACL_CLOSED_IND:
            appConManagerHandleClDmAclClosedIndication((CL_DM_ACL_CLOSED_IND_T *)message);
            return;
            
        case CL_DM_MODE_CHANGE_EVENT:
            appHandleClDmModeChangeEvent((CL_DM_MODE_CHANGE_EVENT_T *)message);
            return;
            
        case CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND:
        {
            CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T *ind = (CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T *)message;
            ConnectionDmBleAcceptConnectionParUpdateResponse(TRUE, &ind->taddr,
                                                             ind->id,
                                                             ind->conn_interval_min, ind->conn_interval_max,
                                                             ind->conn_latency,
                                                             ind->supervision_timeout);
        }
        return;
     }

    /* AV messages */
    switch (id)
    {
        /* ignore */
        case AV_CREATE_IND:
        case AV_DESTROY_IND:
            return;
    }
     
    /* Handle internal messages */       
    switch (id)
    {       
        /* all handled in the SM module, forwarded from here until these messages
         * can be sent direct to the SM task handler. */
        case INIT_CFM:
            appHandleInitCfm(message);
            return;
    }

    appHandleUnexpected(id);
}


/*! \brief Application entry point

    This function is the entry point for the application, it performs basic
    initialisation of the application task state machine and then sets
    the state to 'initialising' which will start the initialisation procedure.

    \returns Nothing. Only exits by powering down.
*/
int main(void)
{
    OsInit();

    /* Set up task handlers */
    appGetApp()->task.handler = appHandleMessage;
    appGetApp()->systask.handler = appHandleSystemMessage;

    MessageSystemTask(appGetSysTask());

    /* Start the application module and library initialisation sequence */
    appInit();

    /* Start the message scheduler loop */
    MessageLoop();

    /* We should never get here, keep compiler happy */
    return 0;
}
