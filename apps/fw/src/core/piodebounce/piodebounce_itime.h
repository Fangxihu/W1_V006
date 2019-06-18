/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * Defines itime events.
 */

#ifndef PIODEBOUNCE_ITIME_H
#define PIODEBOUNCE_ITIME_H

/**
 * Handler function, defined in piodebounce.c.
 */
extern void debounce_it_handler(void);

/** Define the itime event identifier for this module. */
#define CORE_PIODEBOUNCE_ITIME_EVENT(m) \
    ITIME_EVENT_LO(m, (itid_piodebounce, debounce_it_handler))


#endif /* PIODEBOUNCE_ITIME_H */

