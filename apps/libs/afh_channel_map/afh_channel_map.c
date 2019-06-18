/****************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.

FILE NAME
    afh_channel_map.c

DESCRIPTION
      A utility library to handle hardware specific dependency of AFH channel map update.
*/

#include <panic.h>
#include <kalimba.h>
#include <broadcast_msg_interface.h>
#include "afh_channel_map.h"

/*!
    @brief Updates the Bluecore Plugin that AFH channel map is pending
*/

/******************************************************************************/
void afhChannelMapChangeIsPending(void)
{
    PanicFalse(KalimbaSendMessage(KALIMBA_MSG_AFH_CHANNEL_MAP_CHANGE_PENDING, 0, 0, 0, 0));
}


