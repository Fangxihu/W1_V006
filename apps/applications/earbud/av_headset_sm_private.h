/*!
\copyright  Copyright (c) 2005 - 2018 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_sm_private.h
\brief      Internal interface for SM components.
*/

#ifndef _AV_HEADSET_SM_PRIVATE_H_
#define _AV_HEADSET_SM_PRIVATE_H_

#include "av_headset_sm.h"

/*! \brief Application state machine internal message IDs */
enum sm_internal_message_ids
{
    SM_INTERNAL_PAIR_HANDSET,                      /*!< Start pairing with a handset. */
    SM_INTERNAL_DELETE_HANDSETS,                   /*!< Delete all paired handsets. */
    SM_INTERNAL_FACTORY_RESET,                     /*!< Reset device to factory defaults. */
    SM_INTERNAL_ENTER_DFU_UI,                      /*!< Button for DFU has been pressed */
    SM_INTERNAL_ENTER_DFU_GAIA,                    /*!< We have been connected to over GAIA */
    SM_INTERNAL_ENTER_DFU_UPGRADED,                /*!< We have restarted after an upgrade requested reboot */
    SM_INTERNAL_ENTER_DFU_STARTUP,                 /*!< We have restarted while a DFU was in progress */
    SM_INTERNAL_DFU_ENDED,                         /*!< DFU / GAIA has ended */
    SM_INTERNAL_START_UPGRADE,                     /*!< The upgrade library has commenced an upgrade */
    SM_INTERNAL_NO_DFU,                            /*!< DFU/Upgrade is not supported, move to startup */
    SM_INTERNAL_SEND_PEER_SYNC,                    /*!< Send a peer sync message */
    SM_INTERNAL_REBOOT,                            /*!< Reboot the earbud immediately */
    SM_INTERNAL_LINK_DISCONNECTION_COMPLETE,       /*!< All links are now disconnected */

    SM_INTERNAL_TIMEOUT_DFU_ENTRY = 0x80,          /*!< Timeout occurred because no DFU activity after entering GAIA */
    SM_INTERNAL_TIMEOUT_LINK_DISCONNECTION,        /*!< Timeout if link disconnection takes too long. */
    SM_INTERNAL_TIMEOUT_OUT_OF_EAR_A2DP,           /*!< Timeout to pause A2DP when earbud removed from ear while streaming. */
    SM_INTERNAL_TIMEOUT_OUT_OF_EAR_SCO,            /*!< Timeout to transfer SCO to AG when earbud removed from ear while call active. */
    SM_INTERNAL_TIMEOUT_IN_EAR_A2DP_START,         /*!< Timeout within which restart audio if earbud put back in ear. */
    SM_INTERNAL_TIMEOUT_IDLE,                      /*!< Timeout when idle. */
};

#endif /* _AV_HEADSET_SM_PRIVATE_H_ */
