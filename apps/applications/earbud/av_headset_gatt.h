/*!
\copyright  Copyright (c) 2015 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_gatt.h
\brief      Header file for GATT, GATT Server and GAP Server
*/

#ifndef _AV_HEADSET_GATT_H_
#define _AV_HEADSET_GATT_H_

#include <gatt_server.h>
#include <gatt_gap_server.h>

/*! Structure holding information for the gatt task */
typedef struct
{
    /*! Task for handling messaging from GATT Manager */
    TaskData    gatt_task;

    /*! Gatt Server information. Required as we're using BLE */
    GGATTS      gatt_server;

    /*! GAP server information. Required for using BLE */
    GGAPS       gap_server;
} gattTaskData;

/*! @brief Initialise the GATT component. 

    \return TRUE if all gatt initialiation was successful, FALSE otherwise.
*/
bool appGattInitialise(void);

//void appGattReturnLocalName(uint8 *local_name, uint16 size_local_name);

#endif /* _AV_HEADSET_GATT_H_ */
