/*!
\copyright  Copyright (c) 2005 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_sm.h
\brief	    Header file for the application state machine
*/

#ifndef _AV_HEADSET_SM_H_
#define _AV_HEADSET_SM_H_

#include "av_headset_conn_rules.h"
#include "av_headset_phy_state.h"

/*!
    @startuml

    note "For clarity not all state transitions shown" as N1
    note "For clarity TERMINATING substate is not shown in all parent states" as N2
    note top of STARTUP
      _IDLE is any of the idle states
      * IN_CASE_IDLE
      * OUT_OF_CASE_IDLE
      * IN_EAR_IDLE
    end note

    [*] -down-> INITIALISING : Power On
    INITIALISING : App module and library init
    INITIALISING --> DFU_CHECK : Init Complete

    DFU_CHECK : Is DFU in progress?
    DFU_CHECK --> STARTUP : No DFU in progress

    STARTUP : Check for paired peer earbud
    STARTUP : Attempt peer synchronisation
    STARTUP : After attempt go to _IDLE
    STARTUP --> PEER_PAIRING : No paired peer

    FACTORY_RESET : Disconnect links, deleting all pairing, reboot
    FACTORY_RESET : Only entered from _IDLE
    FACTORY_RESET -r-> INITIALISING : Reboot

    PEER_PAIRING : Pair with peer earbud
    PEER_PAIRING --> STARTUP
    HANDSET_PAIRING : Pair with handset
    HANDSET_PAIRING : Return to _IDLE state

    state IN_CASE #LightBlue {
        IN_CASE : Charger Active
        IN_CASE : Buttons Disabled
        DFU : Device Upgrade
        DFU --> IN_CASE_IDLE #LightGreen : DFU Complete
        DFU_CHECK --> DFU : DFU in progress
        IN_CASE_IDLE : May have BT connection(s)
        IN_CASE_IDLE -up-> DFU : Start DFU
    }

    state OUT_OF_CASE #LightBlue {
        OUT_OF_CASE_IDLE : May have BT connection(s)
        OUT_OF_CASE_IDLE : Start dormant timer
        OUT_OF_CASE_SOPORIFIC : Allow sleep
        OUT_OF_CASE_SOPORIFIC_TERMINATING : Disconnect links
        OUT_OF_CASE_SOPORIFIC_TERMINATING : Inform power prepared to sleep
        OUT_OF_CASE_IDLE #LightGreen --> IN_CASE_IDLE : In Case
        IN_CASE_IDLE --> OUT_OF_CASE_IDLE : Out of Case
        OUT_OF_CASE_IDLE -u-> OUT_OF_CASE_SOPORIFIC : Idle timeout
        OUT_OF_CASE_SOPORIFIC -->  OUT_OF_CASE_SOPORIFIC_TERMINATING : POWER_SLEEP_PREPARE_IND
        OUT_OF_CASE_IDLE --> HANDSET_PAIRING : User or Auto pairing
        OUT_OF_CASE_BUSY : Earbud removed from ear
        OUT_OF_CASE_BUSY : Audio still playing
        OUT_OF_CASE_BUSY #LightGreen --> OUT_OF_CASE_IDLE : Out of ear audio timeout
        OUT_OF_CASE_BUSY --> OUT_OF_CASE_IDLE : Audio Inactive
    }

    state IN_EAR #LightBlue {
        IN_EAR_IDLE : May have BT connection(s)
        IN_EAR_IDLE #LightGreen -l-> OUT_OF_CASE_IDLE : Out of Ear
        IN_EAR_IDLE -u-> IN_EAR_BUSY : Audio Active
        OUT_OF_CASE_IDLE --> IN_EAR_IDLE : In Ear
        IN_EAR_BUSY : Streaming Audio Active (A2DP or SCO)
        IN_EAR_BUSY : Tones audio available in other states
        IN_EAR_BUSY #LightGreen -d-> IN_EAR_IDLE : Audio Inactive
        IN_EAR_BUSY -l-> OUT_OF_CASE_BUSY : Out of Ear
        OUT_OF_CASE_BUSY -l-> IN_EAR_BUSY : In Ear
        IN_EAR_IDLE --> HANDSET_PAIRING : User or Auto pairing
    }
    @enduml
*/

/*! \brief The state machine substates, note that not all parent states support
           all substates. */
typedef enum sm_application_sub_states
{
    APP_SUBSTATE_TERMINATING           = 0x0001, /*!< Preparing to shutdown (e.g. disconnecting links) */
    APP_SUBSTATE_SOPORIFIC             = 0x0002, /*!< Allowing sleep */
    APP_SUBSTATE_SOPORIFIC_TERMINATING = 0x0004, /*!< Preparing to sleep (e.g. disconnecting links) */
    APP_SUBSTATE_IDLE                  = 0x0008, /*!< Audio is inactive */
    APP_SUBSTATE_BUSY                  = 0x0010, /*!< Audio is active */
    APP_SUBSTATE_DFU                   = 0x0020, /*!< Upgrading firmware */

    APP_SUBSTATE_INITIALISING          = 0x0040, /*!< App module and library initialisation in progress. */
    APP_SUBSTATE_DFU_CHECK             = 0x0080, /*!< Interim state, to see if DFU is in progress. */
    APP_SUBSTATE_FACTORY_RESET         = 0x0100, /*!< Resetting the earbud to factory defaults. */
    APP_SUBSTATE_STARTUP               = 0x0200, /*!< Startup, syncing with peer. */

    APP_SUBSTATE_PEER_PAIRING          = 0x0400, /*!< Pairing with peer earbud */
    APP_SUBSTATE_HANDSET_PAIRING       = 0x0800, /*!< Pairing with a handset */

    APP_END_OF_SUBSTATES = APP_SUBSTATE_HANDSET_PAIRING, /*!0x0800< The last substate */
    APP_SUBSTATE_MASK    = ((APP_END_OF_SUBSTATES << 1) - 1), /*!0x0fff< Bitmask to retrieve substate from full state */
} appSubState;

/*! \brief Application states.
 */
typedef enum sm_application_states
{
    /*!< Initial state before state machine is running. */
    APP_STATE_NULL                = 0x0000,
    APP_STATE_INITIALISING        = APP_SUBSTATE_INITIALISING,/*0x0040*/
    APP_STATE_DFU_CHECK           = APP_SUBSTATE_DFU_CHECK,/*0x0080*/
    APP_STATE_FACTORY_RESET       = APP_SUBSTATE_FACTORY_RESET,/*0x0100*/
    APP_STATE_STARTUP             = APP_SUBSTATE_STARTUP,/*0x0200*/
    APP_STATE_PEER_PAIRING        = APP_SUBSTATE_PEER_PAIRING,/*0x0400*/
    APP_STATE_HANDSET_PAIRING     = APP_SUBSTATE_HANDSET_PAIRING,/*0x0800*/
    APP_STATE_TERMINATING         = APP_SUBSTATE_TERMINATING,/*0x0001*/

    /*! Earbud is in the case, parent state */
    APP_STATE_IN_CASE                 = APP_END_OF_SUBSTATES<<1,/*0x1000*/
        APP_STATE_IN_CASE_TERMINATING = APP_STATE_IN_CASE + APP_SUBSTATE_TERMINATING,/*0x1001*/
        APP_STATE_IN_CASE_IDLE        = APP_STATE_IN_CASE + APP_SUBSTATE_IDLE,/*0x1008*/
        APP_STATE_IN_CASE_DFU         = APP_STATE_IN_CASE + APP_SUBSTATE_DFU,/*0x1020*/

    /*!< Earbud is out of the case, parent state */
    APP_STATE_OUT_OF_CASE                           = APP_STATE_IN_CASE<<1,/*0x2000*/
        APP_STATE_OUT_OF_CASE_TERMINATING           = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_TERMINATING,/*0x2001*/
        APP_STATE_OUT_OF_CASE_SOPORIFIC             = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_SOPORIFIC,/*0x2002*/
        APP_STATE_OUT_OF_CASE_SOPORIFIC_TERMINATING = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_SOPORIFIC_TERMINATING,/*0x2004*/
        APP_STATE_OUT_OF_CASE_IDLE                  = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_IDLE,/*0x2008*/
        APP_STATE_OUT_OF_CASE_BUSY                  = APP_STATE_OUT_OF_CASE + APP_SUBSTATE_BUSY,/*0x2010*/

    /*!< Earbud in in ear, parent state */
    APP_STATE_IN_EAR                 = APP_STATE_OUT_OF_CASE<<1,/*0x4000*/
        APP_STATE_IN_EAR_TERMINATING = APP_STATE_IN_EAR + APP_SUBSTATE_TERMINATING,/*0x4001*/
        APP_STATE_IN_EAR_IDLE        = APP_STATE_IN_EAR + APP_SUBSTATE_IDLE,/*0x4008*/
        APP_STATE_IN_EAR_BUSY        = APP_STATE_IN_EAR + APP_SUBSTATE_BUSY,/*0x4010*/

} appState;

#ifdef RECONNECT_HANDSET
/*! \brief reconnect states.
 */
typedef enum sm_reconnect_states
{
    /*!< Initial state before state machine is running. */
    RECONNECT_STATE_NULL            = 0x01,
	RECONNECT_STATE_ING				= 0x02,
	RECONNECT_STATE_END				= 0x04,
} reconnectState;

#endif

/*! \brief Return TRUE if the state is in the case */
#define appSmStateInCase(state) (0 != ((state) & APP_STATE_IN_CASE))

/*! \brief Return TRUE if the state is out of case */
#define appSmStateOutOfCase(state) (0 != ((state) & APP_STATE_OUT_OF_CASE))

/*! \brief Return TRUE if the state is in the ear */
#define appSmStateInEar(state) (0 != ((state) & APP_STATE_IN_EAR))

/*! \brief Return TRUE if the sub-state is terminating */
#define appSmSubStateIsTerminating(state) \
    (0 != ((state) & (APP_SUBSTATE_TERMINATING | APP_SUBSTATE_SOPORIFIC_TERMINATING)))

/*! \brief Return TRUE if the state is idle */
#define appSmStateIsIdle(state) (0 != ((state) & (APP_SUBSTATE_IDLE | APP_SUBSTATE_SOPORIFIC)))

/*! \brief Check if the application is in a core state.
    \note Warning, ensure this macro is updated if enum #sm_application_states
    or #sm_application_sub_states is changed.
*/
#define appSmIsCoreState() (0 != (appGetSubState() & (APP_SUBSTATE_IDLE | \
                                                      APP_SUBSTATE_BUSY | \
                                                      APP_SUBSTATE_SOPORIFIC)))

/*! \brief Main application state machine task data. */
typedef struct
{
    TaskData task;                      /*!< SM task */
    appState state;                     /*!< Application state */
    phyState phy_state;                 /*!< Cache the current physical state */
    uint16 lock;                        /*!< Message lock */
    bool user_pairing:1;                /*!< User initiated pairing */
#ifdef RECONNECT_HANDSET
	bool reconnect_and_pairing:1;				/*!< User initiated pairing */
	bool rule_connect_user:1;				/*!< User initiated pairing */

	reconnectState rec_state; 					/*!< Application state */
#endif
#ifdef	AUTO_POWER_OFF
	uint16 auto_power_off_times;
#endif

#ifdef	SYNC_VOL
	bool vol_sync_flag:1;				/*!< User initiated pairing */
	uint8	volume;
#endif

} smTaskData;

/*! \brief Bitmask use to signal connect links in lock */
#define APP_SM_LOCK_MASK_CONNECTED_LINKS 1U

/*! \brief Clear all bits in the lock */
#define appSmLockClearAll() appGetSm()->lock = 0;
/*! \brief Set the connected links bit in the lock */
#define appSmLockSetConnectedLinks() appGetSm()->lock |= APP_SM_LOCK_MASK_CONNECTED_LINKS
/*! \brief Clear the connected links bit in the lock */
#define appSmLockClearConnectedLinks() appGetSm()->lock &= ~APP_SM_LOCK_MASK_CONNECTED_LINKS


#ifdef	SYNC_VOL
extern bool appSmSyncVolGet(void);
extern void appSmSyncVolSet(bool value);
extern void appSmSyncVolSendMessage(uint8 times);

#endif

/*! \brief Change application state.

    \param new_state [IN] State to move to.
 */
extern void appSetState(appState new_state);

/*! \brief Get current application state.

    \return appState Current application state.
 */
extern appState appGetState(void);

/*! \brief Initialise the main application state machine.
 */
extern void appSmInit(void);

/*! \brief Application state machine message handler.
    \param task The SM task.
    \param id The message ID to handle.
    \param message The message content (if any).
*/
extern void appSmHandleMessage(Task task, MessageId id, Message message);

/* FUNCTIONS THAT CHECK THE STATE OF THE SM
 *******************************************/

/*! @brief Query if this earbud is in-ear.
    @return TRUE if in-ear, otherwise FALSE. */
extern bool appSmIsInEar(void);
/*! @brief Query if this earbud is out-of-ear.
    @return TRUE if out-of-ear, otherwise FALSE. */
extern bool appSmIsOutOfEar(void);
/*! @brief Query if this earbud is in-case.
    @return TRUE if in-case, otherwise FALSE. */
extern bool appSmIsInCase(void);
/*! @brief Query if this earbud is out-of-case.
    @return TRUE if out-of-case, otherwise FALSE. */
extern bool appSmIsOutOfCase(void);
/*! @brief Query if the earbud is connectable.
    @return TRUE if connectable, otherwise FALSE. */
extern bool appSmIsConnectable(void);

/*! @brief Query if this earbud is intentionally disconnecting links. */
#define appSmIsDisconnectingLinks() \
    (0 != (appGetSubState() & (APP_SUBSTATE_SOPORIFIC_TERMINATING | \
                               APP_SUBSTATE_TERMINATING | \
                               APP_SUBSTATE_FACTORY_RESET)))

/*! @brief Query if this earbud is pairing. */
#define appSmIsPairing() \
    (appGetState() == APP_STATE_HANDSET_PAIRING)
/*! @brief Query if this state is a sleepy state. */
#define appSmStateIsSleepy(state) \
    (0 != ((state) & (APP_SUBSTATE_SOPORIFIC | \
                      APP_SUBSTATE_SOPORIFIC_TERMINATING)))
/*! @brief Query if this state is a connectable state. */
#define appSmStateIsConnectable(state) \
    (0 == ((state) & (APP_SUBSTATE_SOPORIFIC_TERMINATING | \
                      APP_SUBSTATE_TERMINATING | \
                      APP_SUBSTATE_FACTORY_RESET)))
/*! @brief Get the physical state as received from last update message. */
#define appSmGetPhyState() (appGetSm()->phy_state)

/*! @brief Query if pairing has been initiated by the user. */
#define appSmIsUserPairing() (appGetSm()->user_pairing)
/*! @brief Set user initiated pairing flag. */
#define appSmSetUserPairing()  (appGetSm()->user_pairing = TRUE)
/*! @brief Clear user initiated pairing flag. */
#define appSmClearUserPairing()  (appGetSm()->user_pairing = FALSE)

#ifdef RECONNECT_HANDSET
extern void appSmHandleReconnectHandset(void);
extern bool appSmReconnectAndPairing(void);
extern void appSmSetReconnectAndPairing(void);
extern void appSmClearReconnectAndPairing(void);
extern void ReconnectSetState(reconnectState new_state);
extern reconnectState ReconnectGetState(void);

extern bool appSmRuleConnectUser(void);
extern void appSmSetRuleConnectUser(bool value);

extern void appSmHandleConnRulesConnectHandsetUser(void);

#endif

#ifdef INCLUDE_DUT
extern void Dut_User_Exit_Peer_Pairing(void);
#endif

/* FUNCTIONS TO INITIATE AN ACTION IN THE SM
 ********************************************/
/*! \brief Initiate pairing with a handset. */
extern void appSmPairHandset(void);
/*! \brief Delete paired handsets. */
extern void appSmDeleteHandsets(void);
/*! \brief Connect to paired handset. */
extern void appSmConnectHandset(void);
/*! \brief Request a factory reset. */
extern void appSmFactoryReset(void);
/*! \brief Reboot the earbud. */
extern void appSmReboot(void);

#ifdef CHG_FINISH_LED
extern void UserDisconnectAllLinks(void);
#endif
#ifdef SINGLE_PEER
extern void appSmSinglePeerConHandset(void);
#endif

extern void appSmDeletePairingAndReset(void);

#ifdef INCLUDE_DFU
/*! \brief Tell the state machine to enter DFU mode.
    \param from_ui_request Set to TRUE if the DFU request was made using the UI,
           as opposed to remotely, for example, using GAIA.
 */
extern void appSmEnterDfuMode(bool from_ui_request);
/*! \brief Tell the state machine to enter DFU mode following a reboot.
    \param upgrade_reboot If TRUE, indicates that DFU triggered the reboot.
                          If FALSE, indicates the device was rebooted whilst
                          an upgrade was in progress.
*/
extern void appSmEnterDfuOnStartup(bool upgrade_reboot);
/*! \brief Tell the state machine that DFU is ended. */
extern void appSmEndDfuMode(void);
/*! \brief Tell the state machine that DFU was start by GAIA */
extern void appSmUpgradeStartedByGaia(void);
/*! \brief Notify the state machine of DFU activity */
extern void appSmNotifyDfuActivity(void);

#else
/*! \brief Not implemented.
    \param from_ui_request Unused.
*/
#define appSmEnterDfuMode(from_ui_request) UNUSED(from_ui_request)

#endif

#endif
#ifdef INCLUDE_DUT
void Dut_User_Exit_Peer_Pairing(void);
#endif
