/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * Header file for the data_conv register access workaround.
 *
 * \section hal_data_conv_access_introduction Introduction
 * These functions will hijack any register access made by P0 or P1 for the
 * purpose of adding a delay. This delay is needed to avoid sending a
 * transaction before any previous one has completed. Not doing so will result
 * in a locked transaction bus and an inaccessible Apps subsystem. This is a
 * workaround for a hardware bug.
 */

#ifndef _HAL_DATA_CONV_ACCESS_H_
#define _HAL_DATA_CONV_ACCESS_H_

#include "hydra/hydra_types.h"
#include "hydra/hydra_macros.h"
#include "hal/haltime.h"
#include "hal/hal_registers.h"
#include "hal/hal_macros.h"

#if CHIP_HAS_AUX_DATA_CONV && defined(CHIP_HAS_BROKEN_DATA_CONVERTER_ACCESS)

/* max delay calculated as per 20MHz FOSC - 1 MICROSECOND */
#define DATA_CONV_REGISTER_ACCESS_DELAY  1

extern void hal_set_data_conv_register_field(volatile uint32 *reg,
                                             uint16 lsb_pos,
                                             uint16 msb_pos,
                                             uint32 value);
extern void hal_set_data_conv_register(volatile uint32 *reg,
                                       uint32 value,
                                       uint16 width);
extern uint32 hal_get_data_conv_register(volatile uint32 *reg,
                                         uint16 width);
extern uint8 hal_get_data_conv_register_field8(volatile uint32 *reg,
                                               uint16 lsb_pos,
                                               uint16 msb_pos);
extern uint16 hal_get_data_conv_register_field16(volatile uint32 *reg);
extern uint32 hal_get_data_conv_register_field32(volatile uint32 *reg);
#endif /* CHIP_HAS_AUX_DATA_CONV &&
          defined(CHIP_HAS_BROKEN_DATA_CONVERTER_ACCESS) */

#endif /* _HAL_DATA_CONV_ACCESS_H_ */
