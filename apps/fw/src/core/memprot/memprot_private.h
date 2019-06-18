/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
#ifndef MEMPROT_PRIVATE_H_
#define MEMPROT_PRIVATE_H_

#include "hydra/hydra.h"
#include "hal/hal.h"

#define IO_DEFS_MODULE_K32_CORE
#define IO_DEFS_MODULE_APPS_SYS_APU
#define IO_DEFS_MODULE_K32_DEBUG_PROG_FLOW
#define IO_DEFS_MODULE_ACCESS_CONTROL
#define IO_DEFS_MODULE_K32_MISC
#define IO_DEFS_MODULE_APPS_SYS_CPU_MEMORY_MAP
#include "io/io_defs.h"
#include "int/int.h"
#include "mmu/mmu.h"
#include "panic/panic.h"
#include "appcmd/appcmd.h"
#include "hydra_log/hydra_log.h"
#include "memprot/memprot.h"
#include "hal/hal_cross_cpu_registers.h"
#include "ipc/ipc.h"

/**
 * Register the appcmd handler for use by memory_protection_test.py
 *
 * \ingroup memprot_test
 */
extern void memprot_register_test_handler(void);

/* Create macros for the different access permissions for P1. */
#define MEMPROT_READONLY    (APU_AREA_ENABLE_MASK|APU_AREA_READ_ENABLE_MASK)             
#define MEMPROT_WRITEONLY   (APU_AREA_ENABLE_MASK|APU_AREA_WRITE_ENABLE_MASK)  
#define MEMPROT_NO_ACCESS   (APU_AREA_ENABLE_MASK) 
#define MEMPROT_FULL_ACCESS (APU_AREA_ENABLE_MASK|APU_AREA_READ_ENABLE_MASK|APU_AREA_WRITE_ENABLE_MASK)  
#define MEMPROT_DISABLED                        ( APU_AREA_READ_ENABLE_MASK|APU_AREA_WRITE_ENABLE_MASK) 

#endif /* MEMPROT_PRIVATE_H_ */
/*@}*/
