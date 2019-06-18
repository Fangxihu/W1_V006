/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_con_manager.h
\brief      Header file for Connection Manager State Machine
*/

#ifndef __AV_HEADSET_CON_MANAGER_H
#define __AV_HEADSET_CON_MANAGER_H

#include <message.h>
#include <connection.h>
#include <bdaddr.h>

#include "av_headset_message.h"
#include "av_headset_tasklist.h"

/*! Maximum number of devices the connection manager can track connection
 * status for. */
#define CON_MANAGER_MAX_DEVICES 4

/*! Flag used on ACL states to indicate if the state represents an activity
    that will finish. */
#define ACL_STATE_LOCK (0x08)
/*! Internal state for an individual ACK link */
typedef enum
{
    /*! The ACL is connecting. Lock variables will clear once connected, or failed */
    ACL_CONNECTING              = 0 + ACL_STATE_LOCK,
    /*! The ACL is connected. */
    ACL_CONNECTED               = 1,
    /*! The ACL is searching for a matching SDP record. Lock variables will clear
        once completed. */
    ACL_CONNECTED_SDP_SEARCH    = 2 + ACL_STATE_LOCK,
    /*! The ACL is disconnected. */
    ACL_DISCONNECTED            = 3,
    /*! The ACL is disconected due to link-loss. */
    ACL_DISCONNECTED_LINK_LOSS  = 4,
} conManagerAclState;

/*! Structure used to hold information about a single device, managed by
    the connection manager. 
    This structure should not be accessed directly.
 */
typedef struct
{
    /*! Bluetooth address of the device */
    bdaddr addr;
    /*! A non-zero value indicates that a procedure is operational on
        the device. It will clear to 0 when complete */
    uint16 lock;
    /*! State of the device */
    conManagerAclState state:4;
    /*! Number of functions interested in this device currently.
        \note The count relies on calls to \ref appConManagerCreateAcl
        and \ref appConManagerReleaseAcl being paired */
    unsigned users:3;
    /*! Flag that indicates if this is an incoming ACL */
    bool local:1;
    /*! The current link policy for this connection */
    lpPerConnectionState lpState;
} conManagerDevice;

/*! Connection Manager module task data. */
typedef struct
{
    /*! The task (message) information for the connection manager module */
    TaskData task;
    
    /*! List of client tasks which have registered to receive CON_MANAGER_CONNECTION_IND
        messages */
    TaskList* connection_client_tasks;

    /*! List of devices which are currently connected. */
    conManagerDevice devices[CON_MANAGER_MAX_DEVICES];

    /*! Lock to control handling of internal messages pending other activities
     * completing */
    uint16 lock;

    /*! Flag indicating if incoming handset connections are allowed */
    bool handset_connect_allowed:1;

} conManagerTaskData;

/*! \brief Message IDs for connection manager messages to other tasks. */
enum    av_headset_conn_manager_messages
{
    /*! Message ID for a \ref CON_MANAGER_CONNECTION_IND_T message sent when
        the state of an ACL changes */
    CON_MANAGER_CONNECTION_IND = CON_MANAGER_MESSAGE_BASE,
};

/*! Definition of message sent to clients to indicate connection status. */
typedef struct
{
    /*! BT address of (dis)connected device. */
    bdaddr bd_addr;
    /*! Connection status of the device, TRUE connected, FALSE disconnected. */
    bool connected;
    /*! Reason given for disconnection. For a connection, this will always be hci_success. */
    hci_status reason;
} CON_MANAGER_CONNECTION_IND_T;

/*! \brief Initialise the connection manager module.
 */
extern void appConManagerInit(void);


/*! \brief Request to create ACL to device

    Called to request an ACL to the specified device.  If the ACL already exists
    then this function does nothing apart from increment usage count on the ACL.
    If the ACL doens't exist then this function will request Bluestack to open
    an ACL.

    \param addr [IN] Pointer to a BT address.

    \return uint16 Pointer to lock that will be cleared when ACL is available, or paging failed.
*/
extern uint16 *appConManagerCreateAcl(const bdaddr *addr);


/*! \brief Release ownership on ACL

    Called to release ownership on an ACL, if that ACL has no other users the
    ACL will be 'closed'.  Bluestack will only actually close the ACL if there
    are no L2CAP connections, so it's safe to call this function once a profile
    connection has been setup, the ACL will be closed automatically once the
    profiles are closed.

    \param addr [IN] Pointer to a BT address.
*/
extern void appConManagerReleaseAcl(const bdaddr *addr);

/***************************************************************************** 
 * Messages from connection library, forwarded by main
 * Expose con_manager module message handler and pass messages via it, so that when
 * connection library can send direct to the pairing module no changes will be
 * required.
 *****************************************************************************/
/*! \brief ACL opened indication handler

    If a new ACL is opened successfully and it is to a handset (where the TWS+
    version needs to be checked everytime) a service attribute search is started.

    \param[in] ind  Pointer to the received connection library indication.
*/
extern void appConManagerHandleClDmAclOpenedIndication(const CL_DM_ACL_OPENED_IND_T *ind);

/*! \brief ACL closed indication handler

    \param[in] ind  Pointer to the received connection library indication.
*/
extern void appConManagerHandleClDmAclClosedIndication(const CL_DM_ACL_CLOSED_IND_T *ind);

/*! \brief Handle authentication.

    \param[in] ind  Pointer to the received connection library indication.

    \return TRUE
 */
extern bool appConManagerHandleClSmAuthoriseIndication(const CL_SM_AUTHORISE_IND_T *ind);
/***************************************************************************** 
 * End connection library forwarded message handlers
 *****************************************************************************/

/*! \brief Perform SDP query of TWS version of handset

    \param bd_addr [IN] Pointer to a BT address.
 */
extern void appConManagerQueryHandsetTwsVersion(const bdaddr *bd_addr);

/*! \brief Register a client task to receive notifications of connections.

    \param client_task [IN] Task which will receive CON_MANAGER_CONNECTION_IND message
 */
extern void appConManagerRegisterConnectionsClient(Task client_task);

/*! \brief Query if a device is currently connected.

    \param addr [IN] Pointer to a BT address.

    \return bool TRUE device is connected, FALSE device is not connected.
 */
extern bool appConManagerIsConnected(const bdaddr *addr);

/*! \brief Query if a ACL to device was locally initiated.

    \param addr [IN] Pointer to a BT address.

    \return bool TRUE ACL to device was locally initiated, FALSE is remotely initiated.
 */
extern bool appConManagerIsAclLocal(const bdaddr *addr);

/*! \brief Set the link policy per-connection state.

    \param addr [IN] Pointer to a BT address.
    \param lpState [IN] Address of state to store.
*/
extern void appConManagerSetLpState(const bdaddr *addr, const lpPerConnectionState *lpState);

/*! \brief Get the link policy per-connection state.

    \param addr [IN] Pointer to a BT address.
    \param lpState [OUT] Address of state to update with retrieved state.
*/
extern void appConManagerGetLpState(const bdaddr *addr, lpPerConnectionState *lpState);

/*! \brief Manually close the ACL to a device.

    \param addr [IN] Pointer to a BT address.
    \param force [IN] TRUE aggressively close regardless of L2CAP connectivity.
*/
extern void appConManagerSendCloseAclRequest(const bdaddr *addr, bool force);


/*! \brief Check if there are any CONNECTED links.

    \return bool TRUE if any link managed by Con Manager is active,
            FALSE if no links 

    \note This function ignores links that are connecting or discovering
        services.
 */
extern bool appConManagerAnyLinkConnected(void);


/*! \brief Control if handset connections are allowed.

    \param allowed [IN] TRUE to allow connections, FALSE to reject.
 */
void appConManagerAllowHandsetConnect(bool allowed);

#endif
