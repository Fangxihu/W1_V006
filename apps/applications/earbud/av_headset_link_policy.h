/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_link_policy.h
\brief	    Header file for the Link policy manager
*/

#ifndef __AV_HEADSET_LINK_POLICY_H
#define __AV_HEADSET_LINK_POLICY_H

#include "av_headset.h"

/*! Link policy task structure */
typedef struct
{
    TaskData task;              /*!< Link policy manager task */
    /*! \todo How do we end up with 3 (now 4) link states when
        we just have AG & peer */
#ifdef INCLUDE_HFP
    unsigned hfp_role:2;        /*!< Current role of HFP link */
#endif
#ifdef INCLUDE_AV
    unsigned av_sink_role:2;    /*!< Current role of AV link as A2DP Sink */
    unsigned av_source_role:2;  /*!< Current role of AV link as A2DP Source */
#endif
#ifdef INCLUDE_SCOFWD
    unsigned scofwd_role:2;     /*!< Current role of peer link */
#endif
} lpTaskData;

/*! Power table indexes */
typedef enum lp_power_table_index
{
    POWERTABLE_A2DP,
    POWERTABLE_A2DP_STREAMING_SOURCE,
    POWERTABLE_A2DP_STREAMING_TWS_SINK,
    POWERTABLE_A2DP_STREAMING_SINK,
    POWERTABLE_HFP,
    POWERTABLE_HFP_SCO,
    POWERTABLE_AVRCP,
    POWERTABLE_PEER_FORWARDING,
    /*! Must be the final value */
    POWERTABLE_UNASSIGNED,
} lpPowerTableIndex;

/*! Link policy state per ACL connection, stored for us by the connection manager. */
typedef struct
{
    lpPowerTableIndex pt_index;     /*!< Current powertable in use */
} lpPerConnectionState;

extern void appLinkPolicyInit(void);

/*! @brief Update the link supervision timeout.
    @param bd_addr The Bluetooth address of the peer device.
*/
extern void appLinkPolicyUpdateLinkSupervisionTimeout(const bdaddr *bd_addr);

/*! @brief Update the link power table based on the system state.
    @param bd_addr The Bluetooth address of the peer device.
*/
extern void appLinkPolicyUpdatePowerTable(const bdaddr *bd_addr);
extern void appLinkPolicyAllowRoleSwitch(const bdaddr *bd_addr);
extern void appLinkPolicyAllowRoleSwitchForSink(Sink sink);
extern void appLinkPolicyPreventRoleSwitch(const bdaddr *bd_addr);
extern void appLinkPolicyPreventRoleSwitchForSink(Sink sink);
extern void appLinkPolicyUpdateRoleFromSink(Sink sink);
extern void appLinkPolicyHandleClDmRoleConfirm(CL_DM_ROLE_CFM_T *cfm);
extern void appLinkPolicyHandleClDmRoleIndication(CL_DM_ROLE_IND_T *ind);

#endif
