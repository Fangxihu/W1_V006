/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       
\brief      Header file for the earbud upgrade management.
*/

#ifndef _AV_HEADSET_UPGRADE_H_
#define _AV_HEADSET_UPGRADE_H_

#ifdef INCLUDE_DFU

#include <upgrade.h>

/*! Structure holding data for the Upgrade module */
typedef struct
{
        /*! Task for handling messaging from upgrade library */
    TaskData    upgrade_task;
        /*! Flag to allow transition to DFU mode. This flag is set when
            using the UI to request DFU. The device will need to be 
            placed into the case (attached to a charger) before DFU
            will be allowed */
    bool        enter_dfu_in_case;
} upgradeTaskData;


extern void appUpgradeInit(void);

extern void appUpgradeHandleMessage(MessageId id, Message message);

extern bool appUpgradeUiDfuRequest(void);

extern void appUpgradeEnteredDfuMode(void);

extern bool appUpgradeDfuPending(void);

#else
#define appUpgradeDfuPending() FALSE
#define appUpgradeEnteredDfuMode() ((void)(0))

#endif /* INCLUDE_DFU */

#endif /* _AV_HEADSET_UPGRADE_H_ */
