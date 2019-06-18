/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_upgrade.c
\brief      Application upgrade management.

Over the air upgrade is managed from this file. Interaction
with the headset software is required as well as support from 
the \b upgrade and \b gaia VM libraries.

This is a minimal implementation of upgrade.
*/

#ifdef INCLUDE_DFU

#include "av_headset.h"
#include "av_headset_log.h"

#include <vmal.h>
#include <panic.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/*! Identifiers for messages used internally by the Upgrade module */
typedef enum av_headset_upgrade_internal_messages
{
    AV_UPGRADE_INTERNAL_RESET_DFUSTART_TIMER = INTERNAL_MESSAGE_BASE + 0x80,
};

/* The factory-set upgrade version and PS config version.

   After a successful upgrade the values from the upgrade header
   will be written to the upgrade PS key and used in future.
*/
static const upgrade_version earbud_upgrade_init_version = { 1, 0 };  /* Values should come from config file */
static const uint16 earbud_upgrade_init_config_version = 1;

/* The upgrade libraries use of partitions is not relevant to the
   partitions as used on devices targetted by this application.

   As it is not possible to pass 0 partitions in the Init function
   use a simple entry */
static const UPGRADE_UPGRADABLE_PARTITION_T logicalPartitions[]
                    = {UPGRADE_PARTITION_SINGLE(0x1000,DFU)
                      };

/*! Maximum size of buffer used to hold the variant string 
    supplied by the application. 6 chars, plus NULL terminator */
#define VARIANT_BUFFER_SIZE (7)

/*!@{ \name Details of the Persistent Store Key that is used to track status
   of the upgrdae, such that it can be resumed.
 */
#define EARBUD_UPGRADE_CONTEXT_KEY              7   /*!< User PSKEY Identifier */
#define EARBUD_UPGRADE_LIBRARY_CONTEXT_OFFSET   2   /*!< Offset with that key */
/*!@} */


static void appUpgradeMessageHandler(Task task, MessageId id, Message message);



/*************************************************************************
    Provide the logical partition map.

    For earbuds this is initially hard coded, but may come from other
    storage in time.

    This function allocates a memory buffer to hold the partition table
    but does not free it. The upgrade library takes ownership of it when
    it is passed in to UpgradeInit. 
*/
static void appUpgradeGetLogicalPartitions(const UPGRADE_UPGRADABLE_PARTITION_T **partitions, uint16 *count)
{
    uint16 num_partitions = sizeof(logicalPartitions)/sizeof(logicalPartitions[0]);

    UPGRADE_UPGRADABLE_PARTITION_T *alloced_partitions = (UPGRADE_UPGRADABLE_PARTITION_T *)PanicFalse(calloc(1,sizeof(logicalPartitions)));
    memcpy(alloced_partitions, logicalPartitions, sizeof(logicalPartitions));

    *partitions = alloced_partitions;
    *count = num_partitions;
}

/*************************************************************************
    Get the variant Id from the firmware and convert it into a variant
    string that can be passed to UpgradeInit.

    This function allocates a buffer for the string which must be freed
    after the call to UpgradeInit.
*/
static void appUpgradeGetVariant(char *variant, size_t length)
{
    int i = 0;
    char chr;
    uint32 product_id;

    PanicFalse(length >= VARIANT_BUFFER_SIZE);

    product_id = VmalVmReadProductId();
    if (product_id == 0)
    {
        variant[0] = '\0';
        return;
    }

    /* The product Id is encoded as two ascii chars + 4 integers in BCD format. */

    /* The ascii chars may be undefined or invalid (e.g. '\0'). 
       If so, don't include them in the variant string. */
    chr = (char)((product_id >> 8) & 0xFF);
    if (isalnum(chr))
        variant[i++] = chr;
    
    chr = (char)(product_id & 0xFF);
    if (isalnum(chr))
        variant[i++] = chr;

    sprintf(&variant[i], "%04X", ((uint16)((product_id >> 16) & 0xFFFF)));
}


/********************  PUBLIC FUNCTIONS  **************************/


/*! Initialisation point for the over the air support in the upgrade library.
 *
 */
void appUpgradeInit(void)
{
    upgradeTaskData *the_upgrade=appGetUpgrade();
    uint16 num_partitions;
    const UPGRADE_UPGRADABLE_PARTITION_T *logical_partitions;
    char variant[VARIANT_BUFFER_SIZE];

    the_upgrade->upgrade_task.handler = appUpgradeMessageHandler;
    the_upgrade->enter_dfu_in_case = FALSE;

    appUpgradeGetVariant(variant, sizeof(variant));

    appUpgradeGetLogicalPartitions(&logical_partitions, &num_partitions);

    /* Allow storage of info at end of (SINK_UPGRADE_CONTEXT_KEY) */
    UpgradeInit(appGetUpgradeTask(), EARBUD_UPGRADE_CONTEXT_KEY, EARBUD_UPGRADE_LIBRARY_CONTEXT_OFFSET,
            logical_partitions,
            num_partitions,
            UPGRADE_INIT_POWER_MANAGEMENT,
            variant,
            upgrade_perm_always_ask,
            &earbud_upgrade_init_version,
            earbud_upgrade_init_config_version);
}

/*! \brief Note that a UI request has been received for DFU mode

    The application will only allow DFU when connected to a charger.
    If no charger is connected then a timeout is started and if 
    the device is placed in the case within that timeout then DFU 
    mode will be entered.

    \returns TRUE if DFU entered or request sent.
*/
bool appUpgradeUiDfuRequest(void)
{
    MessageCancelAll(appGetUpgradeTask(), AV_UPGRADE_INTERNAL_RESET_DFUSTART_TIMER);

    if (appChargerIsConnected())
    {
        DEBUG_LOG("appUpgradeUiDfuRequest. Charger connected, entering Dfu");

        appSmEnterDfuMode(TRUE);
    }
    else
    {
        DEBUG_LOG("appUpgradeUiDfuRequest. Will enter DFU in case...");

        appGetUpgrade()->enter_dfu_in_case = TRUE;
        MessageSendLater(appGetUpgradeTask(), 
                         AV_UPGRADE_INTERNAL_RESET_DFUSTART_TIMER, NULL,
                         APP_DFU_TIMEOUT_TO_PLACE_CASE);
    }
    return TRUE;
}


/*! \brief Is an upgrade pending

    \return TRUE if we are already in the upgrade state, or if the flag
            is set to enter that state upon insertion in the case.
 */
bool appUpgradeDfuPending(void) 
{
    return (   appGetUpgrade()->enter_dfu_in_case
           || APP_STATE_IN_CASE_DFU == appGetState());
}


/*! \brief Let the upgrade module know that DFU state has been entered

    This is called on state transitions to ensure that we can't have 
    repeated, and unexpected connections.
*/
void appUpgradeEnteredDfuMode(void)
{
    appGetUpgrade()->enter_dfu_in_case = FALSE;
    MessageCancelAll(appGetUpgradeTask(), AV_UPGRADE_INTERNAL_RESET_DFUSTART_TIMER);
}

/*! Helper function for routing a message to the upgrade task,
    without the need to find the task information.

    \param id       Identifier of the message to route
    \param message  Content of the message to route (or NULL)
 */
void appUpgradeHandleMessage(MessageId id, Message message)
{
    Task upg = appGetUpgradeTask();

    upg->handler(upg, id, message);
}

static void appUpgradeForwardInitCfm(const UPGRADE_INIT_CFM_T *cfm)
{
    UPGRADE_INIT_CFM_T *copy = PanicUnlessNew(UPGRADE_INIT_CFM_T);
    *copy = *cfm;

    MessageSend(appGetAppTask(), UPGRADE_INIT_CFM, copy);
}

static void appUpgradeHandleRestartedInd(const UPGRADE_RESTARTED_IND_T *restart)
{
    /* This needs to base its handling on the reason in the message, 
       for instance upgrade_reconnect_not_required is a hint that errr, 
       reconnect isn't a priority. */

    DEBUG_LOG("AV_UPGRADE_RESTARTED_IND.");
    switch (restart->reason)
    {
        case upgrade_reconnect_not_required:
            /* No need to reconnect, not even sure why we got this */
            break;

        case upgrade_reconnect_required_for_confirm:
            appSmEnterDfuOnStartup(TRUE);
            break;

        case upgrade_reconnect_recommended_as_completed:
        case upgrade_reconnect_recommended_in_progress:
            appSmEnterDfuOnStartup(FALSE);
            break;
    }
}


static void appUpgradeHandleUpgradeStatusInd(const UPGRADE_STATUS_IND_T *sts)
{
    appSmNotifyDfuActivity();

    switch (sts->state)
    {
        case upgrade_state_idle:
            DEBUG_LOGF("AV_UPGRADE_STATUS_IND. idle(%d)",sts->state);
            break;

        case upgrade_state_downloading:
            DEBUG_LOGF("AV_UPGRADE_STATUS_IND. downloading(%d)",sts->state);
            break;

        case upgrade_state_commiting:
            DEBUG_LOGF("AV_UPGRADE_STATUS_IND. commiting(%d)",sts->state);
            break;

        case upgrade_state_done:
            DEBUG_LOGF("AV_UPGRADE_STATUS_IND. done(%d)",sts->state);
            appSmEndDfuMode();
            break;

        default:
            DEBUG_LOGF("AV_UPGRADE_STATUS_IND. Unexpected state %d",sts->state);
            Panic();
            break;
    }
}

static void appUpgradeHandleCaseTimeout(void)
{
    DEBUG_LOG("appUpgradeHandleCaseTimeout. Timed out before inserting in case");
    appGetUpgrade()->enter_dfu_in_case = FALSE;
}


static void appUpgradeSwapImage(void)
{
    UpgradeImageSwap();
}

static void appUpgradeHandleUpgradeShutAudio(void)
{
    DEBUG_LOG("AV_UPGRADE_SHUT_AUDIO");
    appUpgradeSwapImage();
}


static void appUpgradeHandleUpgradeCopyAudioImageOrSwap(void)
{
    DEBUG_LOG("AV_UPGRADE_UPRGADE_COPY_AUDIO_IMAGE_OR_SWAP");
    appUpgradeSwapImage();
}

static void appUpgradeMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOGF("appUpgradeMessageHandler 0x%X (%d)",id,id);
    
    switch (id)
    {
        case AV_UPGRADE_INTERNAL_RESET_DFUSTART_TIMER:
            appUpgradeHandleCaseTimeout();
            break;

            /* Message sent in response to UpgradeInit(). 
             * In this case we need to forward to the app to unblock initialisation.
             */
        case UPGRADE_INIT_CFM:
            {
                const UPGRADE_INIT_CFM_T *init_cfm = (const UPGRADE_INIT_CFM_T *)message;

                DEBUG_LOGF("UPGRADE_INIT_CFM: %d (sts)",init_cfm->status);

                appUpgradeForwardInitCfm(init_cfm);
            }
            break;

            /* Message sent during initialisation of the upgrade library
                to let the VM application know that a restart has occurred 
                and reconnection to a host may be required. */

        case UPGRADE_RESTARTED_IND:
            appUpgradeHandleRestartedInd((UPGRADE_RESTARTED_IND_T*)message);
            break;
                
            /* Message sent to application to request applying a downloaded upgrade.
                Note this may include a warm reboot of the device.
                Application must respond with UpgradeApplyResponse() */
        case UPGRADE_APPLY_IND:
            DEBUG_LOG("AV_UPGRADE_APPLY_IND saying now !");
            appSmNotifyDfuActivity();
            UpgradeApplyResponse(0);
            break;
                
            /* Message sent to application to request blocking the system for an extended
                period of time to erase serial flash partitions.
                Application must respond with UpgradeBlockingResponse() */
        case UPGRADE_BLOCKING_IND:
            DEBUG_LOG("AV_UPGRADE_BLOCKING_IND");
            appSmNotifyDfuActivity();
            UpgradeBlockingResponse(0);
            break;
                
            /* Message sent to application to indicate that blocking operation is finished */
        case UPGRADE_BLOCKING_IS_DONE_IND:
            appSmNotifyDfuActivity();
            DEBUG_LOG("AV_UPGRADE_BLOCKING_IS_DONE_IND");
            break;
                
            /* Message sent to application to inform of the current status of an upgrade. */
        case UPGRADE_STATUS_IND:
            appUpgradeHandleUpgradeStatusInd((const UPGRADE_STATUS_IND_T *)message);
            break;

            /* Message sent to application to request any audio to get shut */
        case UPGRADE_SHUT_AUDIO:
            appUpgradeHandleUpgradeShutAudio();
            break;

            /* Message sent to application set the audio busy flag and copy audio image */
        case UPRGADE_COPY_AUDIO_IMAGE_OR_SWAP:
            appUpgradeHandleUpgradeCopyAudioImageOrSwap();
            break;

            /* Message sent to application to reset the audio busy flag */
        case UPGRADE_AUDIO_COPY_FAILURE:
            DEBUG_LOG("av_headset_upgrade: UPGRADE_AUDIO_COPY_FAILURE (not handled)");
            break;
            /* Message sent to application to inform that the actual upgrade has started */

        case UPGRADE_START_DATA_IND:
            DEBUG_LOG("av_headset_upgrade: UPGRADE_START_DATA_IND (not handled)");
            break;
            /* Message sent to application to inform that the actual upgrade has ended */

        case UPGRADE_END_DATA_IND:
            DEBUG_LOG("av_headset_upgrade: UPGRADE_END_DATA_IND (not handled)");
            break;

        case MESSAGE_IMAGE_UPGRADE_ERASE_STATUS:
            DEBUG_LOG("AV_MESSAGE_IMAGE_UPGRADE_ERASE_STATUS");

            appSmNotifyDfuActivity();
            UpgradeEraseStatus(message);
            break;

        case MESSAGE_IMAGE_UPGRADE_COPY_STATUS:
            DEBUG_LOG("AV_MESSAGE_IMAGE_UPGRADE_COPY_STATUS");

            appSmNotifyDfuActivity();
            UpgradeCopyStatus(message);
            break;

            /* Catch-all panic for unexpected messages */
        default:
            if (UPGRADE_UPSTREAM_MESSAGE_BASE <= id && id <  UPGRADE_UPSTREAM_MESSAGE_TOP)
            {
                DEBUG_LOGF("av_headset_upgrade: Unexpected upgrade library message 0x%x (%d)",id,id);
            }
            else
            {
                DEBUG_LOGF("av_headset_upgrade: Unexpected message 0x%x (%d)",id,id);
            }
            Panic();
            break;
    }

}

#endif /* INCLUDE_DFU */
