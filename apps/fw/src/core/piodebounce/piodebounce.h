/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * Debounce input bits on the pio port.
 */
 

#ifndef PIODEBOUNCE_H
#define PIODEBOUNCE_H

#include "pio/pio.h"


/**
 * Initialise the PIO debounce system.
 */   
extern void init_piodebounce(void);

/**
 * Configures the piodebounce subsystem to monitor and debounce the pio input
 * lines defined by the set of 1s in "mask". The debounce algorithm requires
 * that "nreads" readings of the monitored pio pins, each "period" milliseconds
 * apart, to be the same before it accepts the pins' values are stable.  It
 * then compares the values with the last known stable values. If these differ
 * the piodebounce subsystem raises an evt event.
 * Ideally, all of the bits defined in "mask" should map to pio pins that have
 * hardware support for input edge-detection.  This function asks the pioint
 * subsystem to monitor these pins, but some or all of the pins may not have
 * edge-detection hardware; this misfortune is not reported to the caller.
 * Although not advised, the monitor/debouncer can be kicked into activity
 * manually by calling piodebounce_kick().  This simulates a pioint call -
 * pretending that a value change has been detected on a pio input port.
 * If "nreads" is 1 then the debouncing mechanism is disabled; any perceived
 * change on the pio pins provokes an evt.
 * If "nreads" is 0 then any stimulus, either from pioint or from
 * piodebounce_kick() provokes an evt.
 * Calls to this function replace any existing configuration - calling once to
 * monitor pin 4, then again to monitor pin 5 results in only pin5 being
 * monitored. However, separate records are kept for different PIO banks and
 * changing one will not alter the others.
 * If "bitmask" is zero then the monitoring is disabled.
 * It is up to the caller to ensure the input bits to be monitored are
 * available.
 * This call does not configure the bit directions of the pio port. The caller
 * will normally set this using the pio API.
 * \param bank PIO bank index.
 * \param mask PIO mask.
 * \param nreads Number of reads to perform in order to determine if the pins
 * are stable.
 * \param period Time in milliseconds between each successive read.
 * \param callback Function pointer to user's callback function.
 */
extern void piodebounce_config(uint16 bank, pio_size_bits mask, uint16 nreads,
                uint16 period, void (*callback)(uint16 bank));

/**
 * Force the debounce engine into life. This simulates an interrupt being
 * received from pioint.
 */
extern void piodebounce_kick(void);

/**
 * Read the debounced pio port value.
 * \param bank PIO bank index.
 * \return PIO mask indicationg the PIO values.
*/
extern pio_size_bits piodebounce_get(uint16 bank);


#endif  /* PIODEBOUNCE_H */

