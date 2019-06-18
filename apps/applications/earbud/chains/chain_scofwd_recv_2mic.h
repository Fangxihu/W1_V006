/*!
    \copyright Copyright (c) 2019 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version Part of 6.3.0
    \file chain_scofwd_recv_2mic.h
    \brief The chain_scofwd_recv_2mic chain. This file is generated by E:/qtil/ADK_QCC512x_QCC302x_WIN_6.3.0.154/tools/chaingen/chaingen.py.
*/

#ifndef _CHAIN_SCOFWD_RECV_2MIC_H__
#define _CHAIN_SCOFWD_RECV_2MIC_H__

/*!
    @startuml
        object OPR_SCOFWD_RECV
        OPR_SCOFWD_RECV : id = CAP_ID_DOWNLOAD_ASYNC_WBS_DEC
        object OPR_SOURCE_SYNC
        OPR_SOURCE_SYNC : id = CAP_ID_SOURCE_SYNC
        object OPR_VOLUME_CONTROL
        OPR_VOLUME_CONTROL : id = CAP_ID_VOL_CTRL_VOL
        object OPR_SCO_AEC
        OPR_SCO_AEC : id = CAP_ID_DOWNLOAD_AEC_REFERENCE
        OPR_SOURCE_SYNC "IN(0)"<-- "OUT(0)" OPR_SCOFWD_RECV
        OPR_VOLUME_CONTROL "MAIN_IN(0)"<-- "OUT(0)" OPR_SOURCE_SYNC
        OPR_SCO_AEC "INPUT1(0)"<-- "OUT(0)" OPR_VOLUME_CONTROL
        object EPR_SCOFWD_RX_OTA #lightgreen
        OPR_SCOFWD_RECV "FORWARDED_AUDIO(0)" <-- EPR_SCOFWD_RX_OTA
        object EPR_SCO_MIC1 #lightgreen
        OPR_SCO_AEC "MIC1(2)" <-- EPR_SCO_MIC1
        object EPR_SCO_MIC2 #lightgreen
        OPR_SCO_AEC "MIC2(3)" <-- EPR_SCO_MIC2
        object EPR_VOLUME_AUX #lightgreen
        OPR_VOLUME_CONTROL "AUX_IN(1)" <-- EPR_VOLUME_AUX
        object EPR_SCO_SPEAKER #lightblue
        EPR_SCO_SPEAKER <-- "SPEAKER1(1)" OPR_SCO_AEC
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_scofwd_recv_2mic_config;

#endif /* _CHAIN_SCOFWD_RECV_2MIC_H__ */

