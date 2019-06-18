/*!
\copyright  Copyright (c) 2015 - 2017 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_gatt.c
\brief      Application support for GATT, GATT Server and GAP Server
*/

#include "av_headset.h"

#ifdef INCLUDE_GATT

#include "av_headset_db.h"
#include "av_headset_log.h"

#include <gatt.h>
#include <gatt_manager.h>
#include <gatt_server.h>
#include <gatt_gap_server.h>
#include <panic.h>

/*! Earbud GATT database, for the required GATT and GAP servers. */
extern const uint16 gattDatabase[];

/*! @brief Reply with our MTU. */
static void appGattHandleGattExchangeMtuInd(GATT_EXCHANGE_MTU_IND_T* ind)
{
    GattExchangeMtuResponse(ind->cid, 23);
}

/*! @brief Handle confirmation about GATT Manager registration. */
static void appGattHandleGattManRegisterWithGattCfm(GATT_MANAGER_REGISTER_WITH_GATT_CFM_T* cfm)
{
    UNUSED(cfm);
    /* GATT Manager registration handler */
}

static void appGattHandleGattManRemoteClientConnectCfm(GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T* cfm)
{
    UNUSED(cfm);
    /* GATT Manager connection handler */
}

static void appGattMessageHandler(Task task, MessageId id, Message message)
{
//    gattTaskData *gatt = (gattTaskData*)task;
    UNUSED(task);

    switch (id)
    {
        /******************** GATT LIB MESSAGES ******************/
        case GATT_EXCHANGE_MTU_IND:
            appGattHandleGattExchangeMtuInd((GATT_EXCHANGE_MTU_IND_T*)message);
            break;

        /************ GATT MANAGER LIB MESSAGES ******************/
        case GATT_MANAGER_REGISTER_WITH_GATT_CFM:
            appGattHandleGattManRegisterWithGattCfm((GATT_MANAGER_REGISTER_WITH_GATT_CFM_T*)message);
            break;

        case GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM:
            appGattHandleGattManRemoteClientConnectCfm((GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T*)message);
            break;

        case GATT_MANAGER_DISCONNECT_IND:
            DEBUG_LOG("APP:GATT: GATT Manager disconnect ind");
            break;

        /******************** GAP MESSAGES ******************/
        case GATT_GAP_SERVER_READ_DEVICE_NAME_IND:
        {
            uint8 my_name[] = "Earbud1";
            GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T* ind = (GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T*)message;
            DEBUG_LOG("APP:GATT: Read local name");
//            ConnectionReadLocalName(appGetAppTask());
            GattGapServerReadDeviceNameResponse(appGetGapServer(), ind->cid,
                                                sizeof(my_name),
                                                &my_name[0]);
        }
        break;

        default:
            DEBUG_LOGF("APP:GATT: Unhandled message id:%x", id);
            break;
    }
}

#if 0
void appGattReturnLocalName(uint8 *local_name, uint16 size_local_name)
{
    DEBUG_LOG("APP:GATT: Return local name");
    GattGapServerReadDeviceNameResponse(appGetGapServer(), appGetGatt()->cid,
                                        size_local_name,
                                        local_name);
}
#endif

/*! @brief Initialise the GATT component. */
bool appGattInitialise(void)
{
    /* setup the GATT task */
    gattTaskData *gatt = appGetGatt();
    gatt->gatt_task.handler = appGattMessageHandler;

    DEBUG_LOG("APP:GATT: appGattInitialise");

    /* Initialise the GATT Manager */
    if (GattManagerInit(appGetGattTask()))
    {
        if (GattManagerRegisterConstDB(&gattDatabase[0], GattGetDatabaseSize()/sizeof(uint16)))
        {
            if (GattServerInit(appGetGattServer(), appGetGattTask(), HANDLE_GATT_SERVICE, HANDLE_GATT_SERVICE_END) != gatt_server_status_success)
            {
                DEBUG_LOG("APP:GATT: GATT server init failed");
                return FALSE;
            }

            if (GattGapServerInit(appGetGapServer(), appGetGattTask(), HANDLE_GAP_SERVICE, HANDLE_GAP_SERVICE_END) != gatt_gap_server_status_success)
            {
                DEBUG_LOG("APP:GATT: GAP server init failed");
                return FALSE;
            }

            /* complete registration of servers */
            GattManagerRegisterWithGatt();
            return TRUE;
        }
    }
    return FALSE;
}

#endif /* INCLUDE_GATT */
