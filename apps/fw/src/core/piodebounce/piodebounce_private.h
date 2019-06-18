/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * Internal header file.
 */        
#ifndef __PIODEBOUNCE_PRIVATE_H__
#define __PIODEBOUNCE_PRIVATE_H__

#include "piodebounce/piodebounce.h"
#include "hydra_log/hydra_log.h"
#include "int/int.h"
#include "pioint/pioint.h"
#include "itime/itime.h"
#include "sched/sched.h"


/** Type definition for the database. */
typedef struct piodeboince_db
{
    /** The bits being monitored on the pio port. */
    pio_size_bits mask[NUMBER_OF_PIO_BANKS];
    /** The number of samples that must be the same before accepting.  Value 0
        has a special meaning - see piodebounce.h. */
    uint16 nreads;
    /** The number of milliseconds to wait between samples. */
    uint16 period;
    /** Last known stable value from the pio port (output of this subsystem). */
    pio_size_bits output[NUMBER_OF_PIO_BANKS];
    /** The candidate new value for piodb_output. */
    pio_size_bits tmp[NUMBER_OF_PIO_BANKS];
    /** The PIO banks which triggered the interrupt. */
    uint32 bank_mask;
    /** The number of samples remaining to be taken. */
    uint16 nleft;
    /** The callback to be called when a stable change is detected. */
    void (*callback)(uint16 bank);   
}piodebounce_db;

#endif  /* __PIODEBOUNCE_PRIVATE_H__ */

