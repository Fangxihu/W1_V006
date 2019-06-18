/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_pairing.h
\brief      Header file for the Pairing task
*/

#ifndef _AV_HEADSET_PAIRING_H_
#define _AV_HEADSET_PAIRING_H_

#include <connection.h>

/*!
    @startuml

    [*] -down-> NULL
    NULL -down-> INITIALISING : appPairingInit()
    INITIALISING : Registering SDP record
    INITIALISING -down-> IDLE : CL_SDP_REGISTER_CFM
    IDLE : Page and Inquiry scan disabled
    IDLE -up-> CHECKING_HANDSET_LINKKEYS : CHECK_HANDSET_LINKKEYS

    CHECKING_HANDSET_LINKKEYS : Derive handset link keys and send to peer.
    CHECKING_HANDSET_LINKKEYS -up-> IDLE : Check Complete

    state LeftEarbudPeerPairing {
        LeftEarbudPeerPairing : Page scan enabled
        state CancellableLeftPairing {
            IDLE -left-> PEER_INQUIRY : PEER_PAIR_REQ (LEFT)
            PEER_INQUIRY : Inquiry Enabled
            PEER_INQUIRY : Searching for right earbud
            PEER_INQUIRY -down-> PEER_SDP_SEARCH : Found peer perform SDP search
            PEER_INQUIRY -right-> IDLE : PEER_PAIR_CFM(timeout)
            PEER_SDP_SEARCH : Find peer TWS+ version
            PEER_SDP_SEARCH -right-> IDLE : PEER_PAIR_CFM(SDP fail)
        }
        CancellableLeftPairing -right-> IDLE : PEER_PAIR_CFM(cancelled)
        PEER_SDP_SEARCH -down-> PEER_AUTHENTICATE : SDP search ok start auth
        PEER_AUTHENTICATE -left-> IDLE : PEER_PAIR_CFM(success/fail)
        PEER_AUTHENTICATE : Pairing in progress
    }

    state RightEarbudPeerPairing {
        RightEarbudPeerPairing : Page scan enabled
        IDLE -right-> PEER_DISCOVERABLE : PEER_PAIR_REQ (RIGHT)
        PEER_DISCOVERABLE : Inquiry scan enabled
        PEER_DISCOVERABLE : Awaiting left earbud connection
        PEER_DISCOVERABLE -left-> IDLE : PEER_PAIR_CFM(timeout/cancelled)
        PEER_DISCOVERABLE -down-> PEER_PENDING_AUTHENTICATION : Start auth
        PEER_PENDING_AUTHENTICATION -down-> PEER_SDP_SEARCH_AUTHENTICATED : Auth Success, SDP search
        PEER_PENDING_AUTHENTICATION : Pairing in progress
        PEER_PENDING_AUTHENTICATION --> IDLE : PEER_PAIR_CFM(failed)
        PEER_SDP_SEARCH_AUTHENTICATED --> IDLE : PEER_PAIR_CFM(success/failed)
        PEER_SDP_SEARCH_AUTHENTICATED : Find peer TWS+ version
    }

    state HandsetPairing {
        HandsetPairing : Page scan enabled
        IDLE -down-> HANDSET_DISCOVERABLE : HANDSET_PAIR_REQ
        IDLE -down-> HANDSET_PENDING_AUTHENTICATION : HANDSET_PAIR_REQ(known addr)
        HANDSET_DISCOVERABLE : Inquiry scan enabled
        HANDSET_DISCOVERABLE : Awaiting handset connection
        HANDSET_DISCOVERABLE -up-> IDLE : HANDSET_PAIR_CFM(timeout/cancelled)
        HANDSET_DISCOVERABLE -down-> HANDSET_PENDING_AUTHENTICATION : Start authentication       
        HANDSET_PENDING_AUTHENTICATION -down-> HANDSET_SDP_SEARCH_AUTHENTICATED : Auth Success, SDP search
        HANDSET_PENDING_AUTHENTICATION : Pairing in progress
        HANDSET_PENDING_AUTHENTICATION --> IDLE : HANDSET_PAIR_CFM(failed)
        HANDSET_SDP_SEARCH_AUTHENTICATED --> IDLE : HANDSET_PAIR_CFM(success/failed)
        HANDSET_SDP_SEARCH_AUTHENTICATED : Find peer TWS+ version
    }

    @enduml
*/

/*! \brief Pairing module state machine states */
typedef enum pairing_states
{
    PAIRING_STATE_NULL,                /*!< Startup state */
    PAIRING_STATE_INITIALISING,        /*!< Initialising state */
    PAIRING_STATE_IDLE,                /*2!< No pairing happening */

    PAIRING_STATE_PEER_INQUIRY,        /*!< Inquiring for peer earbud */
    PAIRING_STATE_PEER_SDP_SEARCH,     /*!< SDP search for Earbud TWS+ service record */
    PAIRING_STATE_PEER_AUTHENTICATE,   /*5!< Authentication with peer earbud */

    PAIRING_STATE_PEER_DISCOVERABLE,   /*!< Discoverable to the peer */
    PAIRING_STATE_PEER_PENDING_AUTHENTICATION, /*!< Waiting to authenticate with peer */
    PAIRING_STATE_PEER_SDP_SEARCH_AUTHENTICATED, /*8!< Performing SDP search when authenticated */

    PAIRING_STATE_PEER_SCO_FWD_SDP_SEARCH,

    PAIRING_STATE_HANDSET_DISCOVERABLE, /*10!< Discoverable to the handset */
    PAIRING_STATE_HANDSET_PENDING_AUTHENTICATION, /*!< Waiting to authenticate with handset */
    PAIRING_STATE_HANDSET_SDP_SEARCH_AUTHENTICATED, /*!< Waiting for TWS version of handset */
    PAIRING_STATE_CHECKING_HANDSET_LINKKEYS, /*13!< Checking if the handset-link keys need to be forwarded to the peer */
} pairingState;

/*! \brief Internal message IDs */
enum pairing_internal_message_ids
{
    PAIRING_INTERNAL_PEER_PAIR_REQ,             /*!< Pair with peer earbud */
    PAIRING_INTERNAL_HANDSET_PAIR_REQ,          /*!< Pair with handset/phone/AV source */
    PAIRING_INTERNAL_TIMEOUT_IND,               /*!< Pairing has timed out */
    PAIRING_INTERNAL_CHECK_HANDSET_LINKKEYS,    /*!< Check for outstanding link keys */
    PAIRING_INTERNAL_PEER_PAIR_CANCEL,          /*!< Cancel in progress peer pairing */
    PAIRING_INTERNAL_HANDSET_PAIR_CANCEL,       /*!< Cancel in progress hanset pairing */
    PAIRING_INTERNAL_PEER_SDP_SEARCH,           /*!< Delayed message to retry peer SDP search */
    PAIRING_INTERNAL_HANDSET_SDP_SEARCH,        /*!< Delayed message to retry handset SDP search */
    PAIRING_INTERNAL_DISABLE_SCAN               /*!< Delayed message to disable page and inquiry scan */
};

/*! \brief Definition of the #PAIRING_INTERNAL_PEER_PAIR_REQ message content */
typedef struct
{
    /*! The requester's task */
    Task client_task;
    /*! Address to pair */
    bdaddr addr;
    /*! If the request was user initiated (opposed to automatically initiated by software */
    bool is_user_initiated;
} PAIR_REQ_T;

/*! Pairing task structure */
typedef struct
{
    /*! The pairing module task */
    TaskData task;
    /*! The pairing module client's task */
    Task     client_task;
    /*! The current pairing state */
    pairingState state;
    /*! Set if the current pairing is user initiated */
    bool     is_user_initiated:1;
    /*! Stores the number of attempts at SDP search */
    unsigned sdp_search_attempts:3;
    /*! The SDP service handle for TWS sink */
    uint32   tws_sink_service_handle;
    /*! Peer pairing: BT address of the 2 'loudest' devices found during inquiry
        Handset pairing: BT address of handset if pairing request by peer, 0 otherwise */
    bdaddr   bd_addr[2];
    /*! RSSI measurement of the 2 'loudest' devices */
    int8     inquiry_rssi[2];
    /*! The peer's TWS version */
    uint16   peer_tws_version;
    /*! The handset's TWS version (if applicable) */
    uint16   handset_tws_version;
    /*! Non zero when SDP record are being (re)registered */
    uint16   sdp_lock;
    /*! Ensure only 1 pairing operation can be running. */
    uint16   pairing_lock;
    /*! Number of unacknowledged peer signalling msgs */
    uint16   outstanding_peer_sig_req;
} pairingTaskData;

/*! Pairing status codes */
typedef enum pairing_status
{
    pairingSuccess,
    pairingNotReady,
    pairingNoPeerFound,
    pairingNoPeerServiceRecord,
    pairingPeerAuthenticationFailed,
    pairingPeerVersionMismatch,
    pairingPeerTimeout,
    pairingPeerCancelled,

    pairingHandsetSuccess,
    pairingHandsetAuthenticationFailed,
    pairingHandsetNoLinkKey,
    pairingHandsetTimeout,
    pairingHandsetUnknown,
    pairingHandsetCancelled
} pairingStatus;

/*! \brief Message IDs from Pairing task to main application task */
enum av_headset_pairing_messages
{
    /*! Message confirming pairing module initialisation is complete. */
    PAIRING_INIT_CFM = PAIRING_MESSAGE_BASE,
    /*! Message confirming peer pairing is complete. */
    PAIRING_PEER_PAIR_CFM,
    /*! Message confirming handset pairing is complete. */
    PAIRING_HANDSET_PAIR_CFM,
};

/*! \brief Definition of #PAIRING_PEER_PAIR_CFM message. */
typedef struct
{
    /*! The status result of the peer pairing */
    pairingStatus status;
    /*! The address of the paired peer */
    bdaddr peer_bd_addr;
    /*! The peer's major version number */
    uint8 peer_version_major;
    /*! The peer's minor version number */
    uint8 peer_version_minor;
} PAIRING_PEER_PAIR_CFM_T;

/*! \brief Definition of #PAIRING_HANDSET_PAIR_CFM message. */
typedef struct
{
    /*! The status result of the handset pairing */
    pairingStatus status;
    /*! The address of the paired handset */
    bdaddr handset_bd_addr;
} PAIRING_HANDSET_PAIR_CFM_T;

/*! \brief Initialise the pairing application module.
 */
extern void appPairingInit(void);

/*! \brief Pair with peer earbud, using RSSI pairing.

    \param client_task [IN] Task to send #PAIRING_PEER_PAIR_CFM response message to.
    \param is_user_initiated TRUE if this is a user initiated request.
 */
extern void appPairingPeerPair(Task client_task, bool is_user_initiated);

/*! \brief Pair with a handset, where inquiry is required.

    \param client_task [IN] Task to send #PAIRING_HANDSET_PAIR_CFM response message to.
    \param is_user_initiated TRUE if this is a user initiated request.
 */
extern void appPairingHandsetPair(Task client_task, bool is_user_initiated);

/*! \brief Pair with a handset where the address is already known.

    Used to pair with a handset where the BT address is already known and inquiry is
    not required. Typically in response to receiving the address from peer earbud
    via peer signalling channel. 

    \param client_task [IN] Task to send #PAIRING_HANDSET_PAIR_CFM response message to.
    \param handset_addr [IN] Pointer to BT address of handset.
 */
extern void appPairingHandsetPairAddress(Task client_task, bdaddr* handset_addr);

/*! \brief Cancel a handset pairing.
 
    If successfully cancelled the client will receive a PAIRING_HANDSET_PAIR_CFM
    message with a status code of pairingHandsetCancelled.

    Note that it may be too late to cancel the pairing, if it already in the
    latter stages of completion. In such circumstances the client may receive
    a PAIRING_HANDSET_PAIR_CFM message with a status code indicating the result
    of the pairing operation.
 */
extern void appPairingHandsetPairCancel(void);

/*! \brief Cancel a peer pairing.
 
    If successfully cancelled the client will receive a PAIRING_PEER_PAIR_CFM
    message with a status code of pairingHandsetCancelled.

    Note that it may be too late to cancel the pairing, if it already in the
    latter stages of completion. In such circumstances the client may receive
    a PAIRING_PEER_PAIR_CFM message with a status code indicating the result
    of the pairing operation.
 */
extern void appPairingPeerPairCancel(void);

/***************************************************************************** 
 * Messages from connection library, forwarded by main
 * Expose con_manager module message handler and pass messages via it, so that when
 * connection library can send direct to the pairing module no changes will be
 * required.
 *****************************************************************************/

/*! \brief Handler for CL_SM_AUTHENTICATE_CFM.
    \param cfm The confirmation message.
 */
extern void appPairingHandleClSmAuthenticateConfirm(const CL_SM_AUTHENTICATE_CFM_T *cfm);

/*! \brief Handler for CL_SM_AUTHORISE_IND.
    \param ind The indication message.
    \return TRUE if this function has handled sending the ConnectionSmAuthoriseResponse
            to the connection library. FALSE if the caller must send the response.
 */
extern bool appPairingHandleClSmAuthoriseIndication(const CL_SM_AUTHORISE_IND_T *ind);

/*! \brief Handler for CL_SM_IO_CAPABILITY_REQ_IND.
    \param ind The indication message.
 */
extern void appPairingHandleClSmIoCapabilityReqIndication(const CL_SM_IO_CAPABILITY_REQ_IND_T *ind);

/*! \brief Handler for CL_SM_REMOTE_IO_CAPABILITY_IND.
    \param ind The indication message.
 */
extern void appPairingHandleClSmRemoteIoCapabilityIndication(const CL_SM_REMOTE_IO_CAPABILITY_IND_T *ind);

/*! \brief Handler for CL_SM_SEC_MODE_CONFIG_CFM.
    \param cfm The confirmation message.
 */
extern void appPairingHandleClSmSecurityModeConfigConfirm( const CL_SM_SEC_MODE_CONFIG_CFM_T *cfm);

/*! \brief Handler for CL_SM_PIN_CODE_IND.

    \param ind The indication message.
 */
extern void appPairingHandleClPinCodeIndication(const CL_SM_PIN_CODE_IND_T *ind);

/*! \brief Handler for CL_SM_USER_CONFIRMATION_REQ_IND.
    \param ind The indication message.
 */
extern void appPairingHandleClSmUserConfirmationReqIndication(const CL_SM_USER_CONFIRMATION_REQ_IND_T *ind);

/*! \brief Handler for CL_SM_USER_PASSKEY_REQ_IND.

    This function is called to handle indication to the user that the passkey
    should now be entered, since the headset has a fixed passkey, we should respond
    indicating that the passkey has been cancelled.

    \param ind The indication message.
 */
extern void appPairingHandleClSmUserPasskeyReqIndication(const CL_SM_USER_PASSKEY_REQ_IND_T *ind);

/***************************************************************************** 
 * End connection library forwarded message handlers
 *****************************************************************************/

/*! \brief Request pairing derives and transmits any outstanding handset keys to peer.
 */
extern void appPairingTransmitHandsetLinkKeysReq(void);

/*! \brief TEST FUNCTION to force link key TX to peer on reboot. */
void appPairingSetHandsetLinkTxReqd(void);

/*! \brief TEST FUNCTION */
void appPairingClearHandsetLinkTxReqd(void);

/*! \brief TEST FUNCTION to test pairing link key derivation.
    \param bd_addr The paired device's address.
    \param lk_packed The paired device's link key.
    \param key_id_in The link key id.
    \param lk_derived [OUT] The derived link key.
*/
void appPairingGenerateLinkKey(const bdaddr *bd_addr, const uint16 *lk_packed, uint32 key_id_in, uint16 *lk_derived);

/*! \brief Is idle */
#define appPairingIsIdle() \
    (appGetPairing()->state == PAIRING_STATE_IDLE)

#endif
