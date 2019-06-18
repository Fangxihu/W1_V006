/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd 
Part of 6.3.0

FILE NAME
    dm_ble_latency.h      

DESCRIPTION
    This file is a stub for configuration of latency control.
    Builds requiring this should include CONFIG_LATENCY in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_LATENCY

NOTES

*/

/****************************************************************************
NAME    
    connectionBluestackHandlerDmLatency

DESCRIPTION
    Handler for Latency messages

RETURNS
    TRUE if message handled, otherwise false
*/
bool connectionBluestackHandlerDmLatency(const DM_UPRIM_T *message);



