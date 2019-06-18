/****************************************************************************
Copyright (c) 2016 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    system_clock.c
*/

#include <system_clock.h>
#include <vm.h>

rtime_t SystemClockGetTimerTime(void)
{
    return (rtime_t)VmGetTimerTime();
}
