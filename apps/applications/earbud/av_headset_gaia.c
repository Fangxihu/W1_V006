/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       
\brief      Handling of the GAIA transport interface

This is a minimal implementation that only supports upgrade.
*/

#ifdef INCLUDE_DFU

#include "av_headset.h"
#include "av_headset_log.h"

#include <panic.h>

/*! Enumerated type of internal message IDs used by this module */
typedef enum av_handset_gaia_internal_messages
{
        /*! Disconnect GAIA */
    APP_GAIA_INTERNAL_DISCONNECT = INTERNAL_MESSAGE_BASE,
        /*! Response to incoming GAIA - accept */
    APP_GAIA_INTERNAL_CONNECT_ALLOWED_IND,
        /*! Response to incoming GAIA - reject */
    APP_GAIA_INTERNAL_CONNECT_REJECTED_IND,
};


static void appGaiaMessageHandler(Task task, MessageId id, Message message);
static void gaia_handle_command(Task task, const GAIA_UNHANDLED_COMMAND_IND_T *command);
static bool gaia_handle_status_command(Task task, const GAIA_UNHANDLED_COMMAND_IND_T *command);
static void gaia_send_response(uint16 vendor_id, uint16 command_id, uint16 status,
                          uint16 payload_length, uint8 *payload);
static void gaia_send_packet(uint16 vendor_id, uint16 command_id, uint16 status,
                          uint16 payload_length, uint8 *payload);


/********************  PUBLIC FUNCTIONS  **************************/



/*! Initialise the GAIA Module */
void appGaiaInit(void)
{
    appGetGaia()->gaia_task.handler = appGaiaMessageHandler;
    GaiaInit(appGetGaiaTask(), 1);
}


static void appGaiaForwardInitCfm(const GAIA_INIT_CFM_T *cfm)
{
    GAIA_INIT_CFM_T *copy = PanicUnlessNew(GAIA_INIT_CFM_T);
    *copy = *cfm;

    MessageSend(appGetAppTask(), GAIA_INIT_CFM, copy);
}

static void appGaiaHandleConnectInd(const GAIA_CONNECT_IND_T *ind)
{
    GAIA_TRANSPORT *transport;

    if (!ind || !ind->success)
    {
        DEBUG_LOG("GAIA_CONNECT_IND: Success = FAILED");
        return;
    }

    transport = ind->transport;

    if (!transport)
    {
        DEBUG_LOG("GAIA_CONNECT_IND: No transport ?");

        /* Can't actually disconnect nothing, but if we were sent the
         * message - reasonable that Gaia may have some tidy up */
        GaiaDisconnectRequest(transport);
        return;
    }

    switch (appGetState())
    {
        case APP_STATE_IN_CASE_IDLE:
        case APP_STATE_IN_CASE_DFU:
//        case APP_STATE_DFU_CHECK:
            break;

        default:
            DEBUG_LOGF("GAIA_CONNECT_IND: App state %d not expected, disconnecting",appGetState());
            GaiaDisconnectRequest(transport);
            return;
    }

    DEBUG_LOGF("GAIA_CONNECT_IND: Success. Transport:%p",transport);

    appSetGaiaTransport(transport);
    appSmEnterDfuMode(FALSE);

}

static void appGaiaHandleInternalConnect(bool accepted)
{
    GAIA_TRANSPORT *transport = appGetGaiaTransport();

    if (accepted)
    {
        GaiaSetSessionEnable(transport, TRUE);
        GaiaOnTransportConnect(transport);
    }
    else
    {
        appSetGaiaTransport(NULL);
        GaiaDisconnectRequest(transport);
    }

}

static void appGaiaMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOGF("appGaiaMessageHandler 0x%X (%d)",id,id);

    switch (id)
    {
        case APP_GAIA_INTERNAL_CONNECT_ALLOWED_IND:
            appGaiaHandleInternalConnect(TRUE);
            break;

        case APP_GAIA_INTERNAL_CONNECT_REJECTED_IND:
            appGaiaHandleInternalConnect(FALSE);
            break;

        case APP_GAIA_INTERNAL_DISCONNECT:
            if (appGetGaiaTransport())
            {
                GaiaDisconnectRequest(appGetGaiaTransport());
                appSetGaiaTransport(NULL);
            }
            break;

        case GAIA_INIT_CFM:
            {
                const GAIA_INIT_CFM_T* init_cfm = (const GAIA_INIT_CFM_T*)message;

                DEBUG_LOGF("GAIA_INIT_CFM: %d (succ)",init_cfm->success);

                GaiaSetApiMinorVersion(GAIA_API_MINOR_VERSION);
                GaiaSetAppWillHandleCommand(GAIA_COMMAND_DEVICE_RESET, TRUE);
                GaiaStartService(gaia_transport_spp);

                /* Successful initialisation of the library. The application needs 
                 * this to unblock.
                 */
                appGaiaForwardInitCfm(init_cfm);
            }
            break;

        case GAIA_CONNECT_IND:                   /* Indication of an inbound connection */
            appGaiaHandleConnectInd((const GAIA_CONNECT_IND_T *)message);
            break;

        case GAIA_DISCONNECT_IND:                /* Indication that the connection has closed */
            DEBUG_LOG("AV_GAIA_DISCONNECT_IND");
            GaiaDisconnectResponse(((GAIA_DISCONNECT_IND_T *)message)->transport);
            appSetGaiaTransport(NULL);
            appSmEndDfuMode();
            break;
            
        case GAIA_DISCONNECT_CFM:                /* Confirmation that a requested disconnection has completed */
            /* We probably want to take note of this to send an event to the state
               machine, but it is mainly upgrade we care about. Not gaia connections. */
            DEBUG_LOG("AV_GAIA_DISCONNECT_CFM");
            appSetGaiaTransport(NULL);
            break;

        case GAIA_START_SERVICE_CFM:             /* Confirmation that a Gaia server has started */
            DEBUG_LOG("AV_GAIA_START_SERVICE_CFM (nothing to do)");
            break;

        case GAIA_DEBUG_MESSAGE_IND:             /* Sent as a result of a GAIA_COMMAND_SEND_DEBUG_MESSAGE command */
            DEBUG_LOG("AV_GAIA_DEBUG_MESSAGE_IND");
            break;

        case GAIA_UNHANDLED_COMMAND_IND:         /* Indication that an unhandled command has been received */
            gaia_handle_command(task, (const GAIA_UNHANDLED_COMMAND_IND_T *) message);
            break;

        case GAIA_SEND_PACKET_CFM:               /* Confirmation that a GaiaSendPacket request has completed */
            {
                GAIA_SEND_PACKET_CFM_T *m = (GAIA_SEND_PACKET_CFM_T *) message;
                DEBUG_LOG("AV_GAIA_SEND_PACKET_CFM");

                free(m->packet);
            }
            break;

        case GAIA_DFU_CFM:                       /* Confirmation of a Device Firmware Upgrade request */
            /* If the confirm is a fail, then we can raise an error, but
               not much to do */
            DEBUG_LOG("AV_GAIA_DFU_CFM");
            break;

        case GAIA_DFU_IND:                       /* Indication that a Device Firmware Upgrade has begun */
            /* This could be used to update the link policy for faster
               data transfer. */
            DEBUG_LOG("AV_GAIA_DFU_IND");
            appSmNotifyDfuActivity();
            break;

        case GAIA_UPGRADE_CONNECT_IND:           /* Indication of VM Upgrade successful connection */
            /* This tells us the type of transport connection made, so we can
               remember it if needed */
            DEBUG_LOG("GAIA_UPGRADE_CONNECT_IND");
            appSmUpgradeStartedByGaia();
            break;

        case GAIA_CONNECT_CFM:                   /* Confirmation of an outbound connection request */
            DEBUG_LOG("GAIA_CONNECT_CFM Unexpected");
            Panic();
            break;

        default:
            DEBUG_LOGF("Unknown GAIA message 0x%x (%d)",id,id);
            Panic();
            break;
    }
}

/*************************************************************************
NAME
    gaia_handle_command

DESCRIPTION
    Handle a GAIA_UNHANDLED_COMMAND_IND from the Gaia library
*/
static void gaia_handle_command(Task task, const GAIA_UNHANDLED_COMMAND_IND_T *command)
{
    bool handled = FALSE;

    DEBUG_LOGF("GAIA Vendor ID %d , Id:0x%04x Len:%d",
                command->vendor_id, command->command_id, command->size_payload);

    if (command->vendor_id == GAIA_VENDOR_CSR)
    {
        appSmNotifyDfuActivity();

        switch (command->command_id & GAIA_COMMAND_TYPE_MASK)
        {
        case GAIA_COMMAND_TYPE_CONFIGURATION:
            DEBUG_LOG("AV_GAIA_COMMAND_TYPE_CONFIGURATION");
            break;

        case GAIA_COMMAND_TYPE_CONTROL:
            DEBUG_LOG("AV_GAIA_COMMAND_TYPE_CONTROL");
            break;

        case GAIA_COMMAND_TYPE_STATUS :
            handled = gaia_handle_status_command(task, command);
            break;

        case GAIA_COMMAND_TYPE_NOTIFICATION:
            if (command->command_id & GAIA_ACK_MASK)
            {
                DEBUG_LOG("AV_GAIA: NOTIF ACK");
                handled = TRUE;
            }
            else
            {
                DEBUG_LOG("AV_GAIA_COMMAND_TYPE_NOTIFICATION");
            }
            break;

        default:
            DEBUG_LOGF("AV_Unexpected GAIA command 0x%x",command->command_id & GAIA_COMMAND_TYPE_MASK);
            break;
        }
    }

    if (!handled)
    {
        gaia_send_response(command->vendor_id, command->command_id, GAIA_STATUS_NOT_SUPPORTED, 0, NULL);
    }
}

/*************************************************************************
NAME
    gaia_handle_status_command

DESCRIPTION
    Handle a Gaia polled status command or return FALSE if we can't
*/
static bool gaia_handle_status_command(Task task, const GAIA_UNHANDLED_COMMAND_IND_T *command)
{
    UNUSED(task);
    
    switch (command->command_id)
    {
    case GAIA_COMMAND_GET_APPLICATION_VERSION:
        DEBUG_LOG("AV_GAIA_COMMAND_GET_APPLICATION_VERSION");
        return FALSE;

    default:
        DEBUG_LOGF("AV_GAIA_COMMAND 0x%x (%d)",command->command_id,command->command_id);
        return FALSE;
    }
}

/*************************************************************************
NAME
    gaia_send_response

DESCRIPTION
    Build and Send a Gaia acknowledgement packet

*/
static void gaia_send_response(uint16 vendor_id, uint16 command_id, uint16 status,
                          uint16 payload_length, uint8 *payload)
{
    gaia_send_packet(vendor_id, command_id | GAIA_ACK_MASK, status,
                     payload_length, payload);
}

/*************************************************************************
NAME
    gaia_send_packet

DESCRIPTION
    Build and Send a Gaia protocol packet

*/
static void gaia_send_packet(uint16 vendor_id, uint16 command_id, uint16 status,
                          uint16 payload_length, uint8 *payload)
{
    GAIA_TRANSPORT *transport = appGetGaiaTransport();
    
    if(transport) /* Only attempt to send when transport up */
    {
        uint16 packet_length;
        uint8 *packet;
        uint8 flags = GaiaTransportGetFlags(transport);

        DEBUG_LOGF("gaia_send_packet cmd:%d sts:%d len:%d [flags x%x]",command_id,status,payload_length,flags);

        packet_length = GAIA_HEADER_SIZE + payload_length + 2;
        packet = PanicNull(malloc(packet_length));

        if (packet)
        {
            packet_length = GaiaBuildResponse(packet, flags,
                                              vendor_id, command_id,
                                              status, payload_length, payload);

            GaiaSendPacket(transport, packet_length, packet);
        }
    }
}


/*! \brief Disconnect any active gaia connection
 */
void appGaiaDisconnect(void)
{
    MessageSend(appGetGaiaTask(), APP_GAIA_INTERNAL_DISCONNECT, NULL);
}

/*! \brief Let GAIA know whether current connection is allowed
 */
void appGaiaConnectionAllowed(bool accepted)
{
    MessageId id = accepted ? APP_GAIA_INTERNAL_CONNECT_ALLOWED_IND
                          :   APP_GAIA_INTERNAL_CONNECT_REJECTED_IND;

    MessageSend(appGetGaiaTask(),id, NULL);
}



#endif /* INCLUDE_DFU */
