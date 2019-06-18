/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 *  This contains a register access workaround for the data_conv.
 */

#include "hal/hal_data_conv_access.h"

#if CHIP_HAS_AUX_DATA_CONV && defined(CHIP_HAS_BROKEN_DATA_CONVERTER_ACCESS)

static uint32 create_bit_mask(uint16 lsb_pos, uint16 msb_pos);


void hal_set_data_conv_register_field(volatile uint32 *reg,
                                      uint16 lsb_pos,
                                      uint16 msb_pos,
                                      uint32 value)
{
    uint32 reg_value;
    uint32 bit_mask;

    /* delay added due to B-218641*/
    hal_delay_us(DATA_CONV_REGISTER_ACCESS_DELAY);

    /* get register value */    
    reg_value = (uint32)hal_get_register(*reg, 1);

    /* create bit mask for modifying those bits in the read register value */
    bit_mask = create_bit_mask(lsb_pos, msb_pos);

    /* modify register value as per the field */
    reg_value = (uint32)((reg_value & (~bit_mask)) | (value << lsb_pos));

    /* delay added due to B-218641*/
    hal_delay_us(DATA_CONV_REGISTER_ACCESS_DELAY);

    /* write into the register */
    hal_set_register(*reg, reg_value, 1);
}  

void hal_set_data_conv_register(volatile uint32 *reg,
                                uint32 value,
                                uint16 width)
{
    /* width parameter being passed to hal macro call but not being used inside
       that macro */
    UNUSED(width);

    /* delay added due to B-218641*/
    hal_delay_us(DATA_CONV_REGISTER_ACCESS_DELAY);

    /* write into the register */
    hal_set_register(*reg, value, width);
}  

uint32 hal_get_data_conv_register(volatile uint32 *reg, uint16 width)
{
    uint32 reg_value;

    /* width parameter being passed to hal macro call but not being used inside
       that macro */
    UNUSED(width);

    /* delay added due to B-218641*/
    hal_delay_us(DATA_CONV_REGISTER_ACCESS_DELAY);

    /* get register value */    
    reg_value = (uint32)hal_get_register(*reg, width);

    return reg_value;
}  

uint8 hal_get_data_conv_register_field8(volatile uint32 *reg,
                                        uint16 lsb_pos,
                                        uint16 msb_pos)
{
    uint32 reg_value;
    uint32 bit_mask;
    uint8 reg_field;

    /* delay added due to B-218641*/
    hal_delay_us(DATA_CONV_REGISTER_ACCESS_DELAY);

    /* get register value */    
    reg_value = (uint32)hal_get_register(*reg, 1);

    /* create bit mask for masking out other bits from the read register
       value */
    bit_mask = create_bit_mask(lsb_pos, msb_pos);

    /* get specific field value */
    reg_value = reg_value & bit_mask;
    reg_field = (uint8)(reg_value >> lsb_pos);

    return reg_field;
}  

uint16 hal_get_data_conv_register_field16(volatile uint32 *reg)
{
    uint32 reg_value;

    /* delay added due to B-218641*/
    hal_delay_us(DATA_CONV_REGISTER_ACCESS_DELAY);

    /* get register value */    
    reg_value = (uint32)hal_get_register(*reg, 1);

    return (uint16)reg_value;
}  

uint32 hal_get_data_conv_register_field32(volatile uint32 *reg)
{
    uint32 reg_value;

    /* delay added due to B-218641*/
    hal_delay_us(DATA_CONV_REGISTER_ACCESS_DELAY);

    /* get register value */    
    reg_value = (uint32)hal_get_register(*reg, 2);

    return reg_value;
}

static uint32 create_bit_mask(uint16 lsb_pos, uint16 msb_pos)
{
    uint8 num_bits_set = 0;
    uint32 ones = 0;
    uint32 bit_mask = 0;

    /* Create bit mask */
    if (msb_pos)
    {
        /* calculate no of bits that need to be set in bit_mask */
        num_bits_set = (uint8)(1 + msb_pos - lsb_pos);
        /* set 1 for no of bits calculated above */
        ones = ((uint32)1 << num_bits_set) - 1;
        /* shift it by lsb position to get the bit_mask with 1 set in right
           positions */
        bit_mask = ones << lsb_pos;
    }
    else
    {
        /* set lsb_pos bit */
        bit_mask = 1 << lsb_pos;
    }
    return bit_mask;
}

#endif /* CHIP_HAS_AUX_DATA_CONV &&
          defined(CHIP_HAS_BROKEN_DATA_CONVERTER_ACCESS) */
