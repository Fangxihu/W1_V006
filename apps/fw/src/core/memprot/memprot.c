/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */

#define IO_DEFS_MODULE_APPS_BANKED_EXCEPTIONS_P0
#define IO_DEFS_MODULE_APPS_BANKED_TBUS_INT_P0
#define IO_DEFS_MODULE_APPS_REMOTE_SUBSYSTEM_ACCESSOR
#define IO_DEFS_MODULE_APPS_SYS_APU
#define IO_DEFS_MODULE_APPS_SYS_CLKGEN
#define IO_DEFS_MODULE_APPS_SYS_CPU0_VM
#define IO_DEFS_MODULE_APPS_SYS_CPU1_VM
#define IO_DEFS_MODULE_APPS_SYS_SQIF_WINDOWS
#define IO_DEFS_MODULE_APPS_SYS_SYS
#define IO_DEFS_MODULE_APPS_DMAC_CPU0
#define IO_DEFS_MODULE_READ_DECRYPT
#define IO_DEFS_MODULE_KALIMBA_READ_CACHE
#define IO_DEFS_MODULE_K32_TRACE
#define IO_DEFS_MODULE_SQIF
#define IO_DEFS_MODULE_SQIF_DATAPATH
#define IO_DEFS_MODULE_SQIF_DATAPATH_BANKED
#define IO_DEFS_MODULE_SQIF_DATAPATH_TBUS_BRIDGE


#include "memprot/memprot_private.h"



void memprot_enable(void)
{

#ifdef FW_UNIT_TEST
    /* Register the APPCMD command handler, used for testing */
    memprot_register_test_handler();
#endif

}

void memprot_disable(void)
{
}




