/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_auth.h
\brief      Header file for authentication and security functions
*/

#ifndef __AV_HEADSET_AUTH_H
#define __AV_HEADSET_AUTH_H

#include <connection.h>

/*! \brief Handle an encryption change indication

    The encryption type is used to update the devices link mode to indicate
    if it supports secure connections or not

    \param  ind   Pointer to the connection lib CL_SM_ENCRYPTION_CHANGE_IND_T structure
 */
void appAuthHandleClSmEncryptionChangeInd(CL_SM_ENCRYPTION_CHANGE_IND_T *ind);

/*! \brief Handle an encryption confirmation

    The encryption type is used to update the devices link mode to indicate
    if it supports secure connections or not

    \param  cfm   Pointer to the connection lib CL_SM_ENCRYPT_CFM_T structure
 */
void appAuthHandleClSmEncryptionCfm(const CL_SM_ENCRYPT_CFM_T *cfm);

#endif
