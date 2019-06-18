/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * This contains the calibration HAL layer of the data_conv driver.
 */

#if CHIP_HAS_AUX_DATA_CONV
#include "hal/hal_data_conv_cal.h"
#define IO_DEFS_MODULE_AUX_DATA_CONV
#include "io/io.h"
#include "hal/hal.h"
#include "hal/hal_registers.h"
#include "hal/hal_macros.h"
#undef IO_DEFS_MODULE_AUX_DATA_CONV
#include "hydra_log/hydra_log.h"
#include <string.h>
#include <assert.h>


/** storage for offset due to redundancy */
static int16 offset_due_to_redundancy = 0;

/* Forward refernces */
extern void hal_data_conv_calc_error_p(uint8 use,
                                       int8 msb_data[],
                                       int8 sar_offset,
                                       int8 error[]);
extern void hal_data_conv_calc_error_n(uint8 use,
                                       int8 msb_data[],
                                       int8 sar_offset,
                                       int8 error[]);


void hal_data_conv_do_calibration(void)
{
    int8 error[4] = {0,0,0,0};

    /* setup the calibration and store the offset redundancy for later */
    hal_set_data_conv_calibration_range((uint8)CALIBRATION_LSB_OFFSET);

    /* reset these registers prior to do a calibration */
    hal_data_conv_set_msb_error_correction(error,error);
    hal_set_reg_data_conv_sar_comparator_offset(0);

    /* Before doing calibration make this 0, otherwise the calibration would
       be wrong */
    offset_due_to_redundancy =
        (int16)hal_get_reg_data_conv_offset_due_redundancies();
    hal_set_reg_data_conv_offset_due_redundancies(0);

    hal_set_data_conv_fe_charge_time(2);
    hal_set_data_conv_fe_sample_time(6);

    /* start the first part of the calibration - HW involved */
    hal_set_reg_data_conv_fe_mode(DATA_CONV_FE_MODE_CALIBRATION);
    hal_set_data_conv_cal_en(1);
    hal_set_reg_data_conv_calibration_start(1);
}

void hal_data_conv_stop_calibration(void)
{
    hal_set_data_conv_cal_en(0);
    hal_set_reg_data_conv_calibration_start(0);
    hal_set_reg_data_conv_offset_due_redundancies(
                                             (uint16)offset_due_to_redundancy);
}

void hal_data_conv_set_msb_error_correction(int8 *error_p, int8 *error_n)
{
    hal_set_reg_data_conv_msb_3_error((int16)((error_p[3]<<8)+(error_n[3]<<0)));
    hal_set_reg_data_conv_msb_2_error((int16)((error_p[2]<<8)+(error_n[2]<<0)));
    hal_set_reg_data_conv_msb_1_error((int16)((error_p[1]<<8)+(error_n[1]<<0)));
    hal_set_reg_data_conv_msb_0_error((int16)((error_p[0]<<8)+(error_n[0]<<0)));
}

int8 hal_data_conv_get_compensated_sar_comparator_offset(void)
{
    uint8 sar_offset;
    int8 sar_offset_signed;
    assert(CALIBRATION_LSB_OFFSET < 8);

    sar_offset = (uint8)hal_get_reg_data_conv_sar_comparator_offset_measured();
    sar_offset_signed = (int8)(sar_offset -
                                    ((1U << CALIBRATION_LSB_OFFSET) - 1U));

    L4_DBG_MSG2("sar_offset = %d sar_offset_signed = 0x%02x",
                sar_offset, (uint8)sar_offset_signed);

    return sar_offset_signed;
}

void hal_data_conv_set_sar_comparator_offset(int8 sar_offset_signed)
{
    hal_set_reg_data_conv_sar_comparator_offset((uint8)sar_offset_signed);
}

void hal_data_conv_get_error_comp_p_n(uint8 use,
                                      int8 *error_comp_p,
                                      int8 *error_comp_n)
{
    uint8 sar_offset;
    int8 measured_error_p[4];
    int8 measured_error_n[4];
    uint32 i;
    uint16 raw_data;
    
    /* get the sar_offset, the lowest value you would get from a reading if you
       were sampling 0 from a pad (two's complement) */
    sar_offset = (uint8)hal_get_reg_data_conv_sar_comparator_offset_measured();

    for (i = 0; i < 4; i++)
    {
        hal_set_reg_data_conv_msb_data_sel(i);
        raw_data = (uint16)hal_get_reg_data_conv_msb_data();
        measured_error_p[i] = (int8)(raw_data >> 8);
        measured_error_n[i] = (int8)(raw_data & 0xff);
        L4_DBG_MSG2("Error Measured: MsbP=%d MsbN=%d",
                    (int32)measured_error_p[i], (int32)measured_error_n[i]);
    }

    hal_data_conv_calc_error_p(use, measured_error_p, (int8)sar_offset,
                               error_comp_p);
    hal_data_conv_calc_error_n(use, measured_error_n, (int8)sar_offset,
                               error_comp_n);
}

void hal_data_conv_calc_error_p(uint8 use,
                                int8 msb_data[],
                                int8 sar_offset,
                                int8 error[])
{
    /*  Clear the errors in case they are not used! */
    memset(error, 0, 4*sizeof(error[0]));

    /* use selected error corrections */
    if (use >= 3) /* use = 3 means MSB-3 to MSB-0 should be corrected */
    {
        error[3] = (int8)(-(msb_data[3] - sar_offset)  - 1);
    }
    if (use >= 2) /* use = 2 means MSB-2 to MSB-0 should be corrected */
    {
        error[2] = (int8)((-(msb_data[2] - sar_offset)  - 1) + error[3]);
    }
    if (use >= 1) /* use = 1 means MSB-1 to MSB-0 should be corrected */
    {
        error[1] = (int8)((-(msb_data[1] - sar_offset) - 1) +
                          error[3] + error[2]);
    }
    error[0] = (int8)((-(msb_data[0] - sar_offset) - 1) +
                      error[3] + error[2] + error[1]);
}

void hal_data_conv_calc_error_n(uint8 use,
                                int8 msb_data[],
                                int8 sar_offset,
                                int8 error[])
{
    /*  Clear the errors in case they are not used! */
    memset(error, 0, 4*sizeof(error[0]));

    if (use >= 3) /* use = 3 means MSB-3 to MSB-0 should be corrected */
    {
        error[3] = (int8)((msb_data[3] - sar_offset) - 1);
    }
    if (use >= 2) /* use = 2 means MSB-2 to MSB-0 should be corrected */
    {
        error[2] = (int8)(((msb_data[2] - sar_offset) - 1) + error[3]);
    }
    if (use >= 1) /* use = 1 means MSB-1 to MSB-0 should be corrected */
    {
        error[1] = (int8)(((msb_data[1] - sar_offset) - 1) +
                          error[3] + error[2]);
    }
    error[0] = (int8)(((msb_data[0] - sar_offset) - 1) +
                      error[3] + error[2] + error[1]);
}

int16 hal_data_conv_get_offset_due_to_redundancy(void)
{
    return (int16)hal_get_reg_data_conv_offset_due_redundancies();
}


#endif /* CHIP_HAS_AUX_DATA_CONV */
