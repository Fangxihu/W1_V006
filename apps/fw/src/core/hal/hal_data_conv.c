/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * This contains the HAL layer of the data_conv driver.
 */

#include "hal/hal_data_conv.h"
#include <string.h>
#include <assert.h>

#if CHIP_HAS_AUX_DATA_CONV

#define hal_data_conv_disable_fake_adc_value_if_needed() \
    hal_set_reg_data_conv_fake_adc_value_enable(0)

struct hal_data_conv_store
{
    uint8 fast_update_period;
    uint8 slow_update_period;
} hal_data_conv_store[HAL_DATA_CONV_NUM_CHANNELS];

bool hal_data_conv_cfg_chan_setup(
         hal_data_conv_channels chan_sel,
         uint16 acquisition_delay,
         hal_data_conv_over_sample_rate_adc over_sample_rate,
         hal_data_conv_fe_mode fe_mode,
         hal_data_conv_channels pad_n,
         hal_data_conv_vscl_gain vscl_gain)
{
    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if ((acquisition_delay > HAL_DATA_CONV_MAX_ACQUISITION_DELAY) ||
        (over_sample_rate > MAX_DATA_CONV_OVER_SMPL_RATE_ADC_ENUM) ||
        (fe_mode > HAL_DATA_CONV_FE_MODE_CALIBRATION) ||
        (pad_n > HAL_DATA_CONV_CHANNEL_NONE) ||
        ((vscl_gain < HAL_DATA_CONV_GAIN_1) ||
         (vscl_gain > HAL_DATA_CONV_GAIN_8)))
    {
        return FALSE;
    }

    hal_set_reg_data_conv_acquisition_delay(acquisition_delay);
    hal_set_reg_data_conv_over_smpl_rate_adc(over_sample_rate);
    hal_set_reg_data_conv_fe_mode((data_conv_fe_mode_enum)fe_mode);
    hal_set_reg_data_conv_sel_pad_p_n(pad_n);
    hal_set_reg_data_conv_vscl_gain(vscl_gain);

    hal_set_data_conv_chan_ctrl_group_setup(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_setup(0);

    return TRUE;
}

bool hal_data_conv_cfg_chan_charge_sample_time(hal_data_conv_channels chan_sel,
                                               uint8 charge_time,
                                               uint8 sample_time)
{
    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if ((charge_time > HAL_DATA_CONV_MAX_CHARGE_TIME) ||
        (sample_time > HAL_DATA_CONV_MAX_SAMPLE_TIME))
    {
        return FALSE;
    }

    hal_set_data_conv_fe_charge_time(charge_time);
    hal_set_data_conv_fe_sample_time(sample_time);

    hal_set_data_conv_chan_ctrl_group_chrg_smpl_time(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_chrg_smpl_time(0);

    return TRUE;
}

bool hal_data_conv_cfg_chan_fast_slow_update_period(
         hal_data_conv_channels chan_sel,
         uint8 fast_update_period,
         uint8 slow_update_period)
{
    uint32 chan_idx;

    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if ((fast_update_period > HAL_DATA_CONV_MAX_FAST_UPDATE_PERIOD) ||
        (slow_update_period > HAL_DATA_CONV_MAX_SLOW_UPDATE_PERIOD))
    {
        return FALSE;
    }

    chan_idx = hal_data_conv_ch_src_to_idx(chan_sel);
    hal_data_conv_store[chan_idx].fast_update_period = fast_update_period;
    hal_data_conv_store[chan_idx].slow_update_period = slow_update_period;

    hal_set_reg_data_conv_update_period(
        (fast_update_period << DATA_CONV_FAST_UPDATE_PERIOD_LSB_POSN) |
        (slow_update_period << DATA_CONV_SLOW_UPDATE_PERIOD_LSB_POSN));

    hal_set_data_conv_chan_ctrl_group_update_period(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_update_period(0);

    return TRUE;
}

bool hal_data_conv_cfg_chan_fast_slow_accum_preload(
         hal_data_conv_channels chan_sel,
         uint32 fast_accum,
         uint32 slow_accum)
{
    uint32 chan_idx;
    uint8 fast_update_period, slow_update_period;

    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if ((fast_accum > HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE) ||
        (slow_accum > HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE))
    {
        return FALSE;
    }

    chan_idx = hal_data_conv_ch_src_to_idx(chan_sel);
    fast_update_period = hal_data_conv_store[chan_idx].fast_update_period;
    slow_update_period = hal_data_conv_store[chan_idx].slow_update_period;

    hal_set_reg_data_conv_chan_preload_fast_accum(
        fast_accum << fast_update_period);
    hal_set_reg_data_conv_chan_preload_slow_accum_hi(
        slow_accum >> (32 - slow_update_period));
    hal_set_reg_data_conv_chan_preload_slow_accum_lo(
        slow_accum << slow_update_period);

    hal_set_data_conv_chan_ctrl_group_accum(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_accum(0);

    return TRUE;
}

bool hal_data_conv_cfg_chan_touch_offset(hal_data_conv_channels chan_sel,
                                         uint32 touch_offset)
{
    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if (touch_offset > HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE)
    {
        return FALSE;
    }

    hal_set_reg_data_conv_touch_offset(touch_offset);

    hal_set_data_conv_chan_ctrl_group_touch_offset(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_touch_offset(0);

    return TRUE;
}

bool hal_data_conv_cfg_chan_trigger_state(hal_data_conv_channels chan_sel,
                                          uint8 trigger_state)
{
    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if (trigger_state > HAL_DATA_CONV_MAX_TRIGGER_STATE)
    {
        return FALSE;
    }

    hal_set_reg_data_conv_chan_trigger_state(trigger_state);
    hal_set_reg_data_conv_chan_trigger_state_sel(chan_sel);

    return TRUE;
}

bool hal_data_conv_cfg_chan_triggers(hal_data_conv_channels chan_sel,
                                     int32 trigger_t0,
                                     int32 trigger_t1,
                                     int32 trigger_t2,
                                     int32 trigger_t3,
                                     uint8 trigger_flags)
{
    uint32 i, tmp = 0;
    uint8 trigger_en_mask = 0;
    uint8 trigger_timestamp_en_mask = 0;

    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T0_ENABLE)
    {
        trigger_en_mask |= 1 << 0;
    }
    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T1_ENABLE)
    {
        trigger_en_mask |= 1 << 1;
    }
    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T2_ENABLE)
    {
        trigger_en_mask |= 1 << 2;
    }
    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T3_ENABLE)
    {
        trigger_en_mask |= 1 << 3;
    }

    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T0_TIMESTAMP)
    {
        trigger_timestamp_en_mask |= 1 << 0;
    }
    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T1_TIMESTAMP)
    {
        trigger_timestamp_en_mask |= 1 << 1;
    }
    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T2_TIMESTAMP)
    {
        trigger_timestamp_en_mask |= 1 << 2;
    }
    if (trigger_flags & HAL_DATA_CONV_TRIGGER_T3_TIMESTAMP)
    {
        trigger_timestamp_en_mask |= 1 << 3;
    }

    if ((trigger_t0 > HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE) ||
        (trigger_t0 < HAL_DATA_CONV_MIN_INTERNAL_VOLTAGE_VALUE) ||
        (trigger_t1 > HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE) ||
        (trigger_t1 < HAL_DATA_CONV_MIN_INTERNAL_VOLTAGE_VALUE) ||
        (trigger_t2 > HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE) ||
        (trigger_t2 < HAL_DATA_CONV_MIN_INTERNAL_VOLTAGE_VALUE) ||
        (trigger_t3 > HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE) ||
        (trigger_t3 < HAL_DATA_CONV_MIN_INTERNAL_VOLTAGE_VALUE))
    {
        return FALSE;
    }

    for (i = 0; i < 4; i++)
    {
        if (trigger_timestamp_en_mask & (1 << i))
        {
            assert(trigger_en_mask & (1 << i));
            switch (tmp)
            {
                case 0:
                    hal_set_data_conv_chan_ts0_to_trigg(i);
                    tmp++;
                    break;
                case 1:
                    hal_set_data_conv_chan_ts1_to_trigg(i);
                    tmp++;
                    break;
                default:
                    return FALSE;
            }
        }
    }

    hal_set_reg_data_conv_trigger_t0((uint32)trigger_t0);
    hal_set_reg_data_conv_trigger_t1((uint32)trigger_t1);
    hal_set_reg_data_conv_trigger_t2((uint32)trigger_t2);
    hal_set_reg_data_conv_trigger_t3((uint32)trigger_t3);
    hal_set_reg_data_conv_trigger_en(trigger_en_mask);

    hal_set_data_conv_chan_ctrl_group_timestamp(1);
    hal_set_data_conv_chan_ctrl_group_triggers(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_timestamp(0);
    hal_set_data_conv_chan_ctrl_group_triggers(0);

    return TRUE;
}

bool hal_data_conv_cfg_chan_trigger_event_count(hal_data_conv_channels chan_sel,
                                                uint8 trigger_event_count)
{
    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if (trigger_event_count > HAL_DATA_CONV_MAX_TRIGGER_EVENT_COUNT)
    {
        return FALSE;
    }

    hal_set_data_conv_chan_event_count(trigger_event_count);

    hal_set_data_conv_chan_ctrl_group_event_count(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_event_count(0);

    return TRUE;
}

bool hal_data_conv_cfg_chan_flags(hal_data_conv_channels chan_sel,
                                  uint8 flags)
{
    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    if (flags & (~HAL_DATA_CONV_FLAGS))
    {
        return FALSE;
    }

    hal_set_data_conv_chan_event_en(
        (flags & HAL_DATA_CONV_FLAGS_EVENT_EN)?1:0);
    hal_set_data_conv_chan_v_detect_en(
        (flags & HAL_DATA_CONV_FLAGS_V_DETECT_EN)?1:0);
    hal_set_data_conv_chan_one_shot_en(
        (flags & HAL_DATA_CONV_FLAGS_ONE_SHOT_EN)?1:0);
    hal_set_data_conv_chan_ghost_en(
        (flags & HAL_DATA_CONV_FLAGS_GHOST_EN)?1:0);

    hal_set_data_conv_chan_ctrl_group_flags(1);
    hal_set_reg_data_conv_chan_ctrl_sel(chan_sel);
    hal_set_data_conv_chan_ctrl_group_flags(0);

    return TRUE;
}

void hal_data_conv_rpt_chan(hal_data_conv_channels chan_sel,
                            hal_data_conv_chan_rpt *p_chan_rpt,
                            hal_data_conv_rpt_flags flags)
{
    uint32 chan_idx;
    uint8 fast_update_period, slow_update_period;

    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);
    assert(p_chan_rpt != NULL);

    chan_idx = hal_data_conv_ch_src_to_idx(chan_sel);
    fast_update_period = hal_data_conv_store[chan_idx].fast_update_period;
    slow_update_period = hal_data_conv_store[chan_idx].slow_update_period;

    hal_set_reg_data_conv_chan_rpt_sel(chan_sel);

    p_chan_rpt->chan = chan_sel;
    p_chan_rpt->time_stamp_t0 =
        (flags & HAL_DATA_CONV_RPT_TIMESTAMP0)?
            hal_get_reg_data_conv_time_stamp_t0():0xdead;
    p_chan_rpt->time_stamp_t1 =
        (flags & HAL_DATA_CONV_RPT_TIMESTAMP1)?
            hal_get_reg_data_conv_time_stamp_t1():0xdead;
    p_chan_rpt->adc_value =
        (flags & HAL_DATA_CONV_RPT_ADC_VALUE)?
            (int16)hal_get_reg_data_conv_adc_value():(int16)0xdead;
    p_chan_rpt->reading_acquired =
        (uint8)hal_get_data_conv_chan_status_reading_acq();
    p_chan_rpt->fast_accum =
        (flags & HAL_DATA_CONV_RPT_FAST_ACCUM)?
            (uint16)(hal_get_reg_data_conv_fast_accum() >> fast_update_period):
            0xdead;
    p_chan_rpt->slow_accum =
        (flags & HAL_DATA_CONV_RPT_SLOW_ACCUM)?
            (uint16)((hal_get_reg_data_conv_slow_accum_hi() <<
                      (32 - slow_update_period)) |
                     (hal_get_reg_data_conv_slow_accum_lo() >>
                      slow_update_period)):
            0xdead;
    p_chan_rpt->trigger_sticky =
        (flags & HAL_DATA_CONV_RPT_TRIGGER)?
            (uint8)hal_get_data_conv_chan_status_trigger_status():0xde;
    p_chan_rpt->trigger_state =
        (flags & HAL_DATA_CONV_RPT_TRIGGER)?
            (uint8)hal_get_data_conv_chan_status_trigger_state_status():0xad;
}

void hal_data_conv_setup(void)
{
    memset(hal_data_conv_store, 0, sizeof(hal_data_conv_store));

    /* do not touch data_conv_bandgap_en since this is controlled by the
       Curator (it is also used by the aux PLL) */

    hal_set_data_conv_macro_enable(1);
    hal_set_data_conv_cfg_enable(1);
    hal_set_data_conv_fe_en(1);
    hal_set_data_conv_adc_enable(1);
    hal_set_reg_data_conv_power_mode_en(0);
    hal_set_data_conv_timer_clear(1);
    hal_set_data_conv_timer_clear(0);
    hal_set_data_conv_timer_en(1);
    hal_set_data_conv_power_mode_pulse(HAL_DATA_CONV_ROUND_ROBIN_INTERVAL);
    hal_set_data_conv_power_mode_count(HAL_DATA_CONV_ROUND_ROBIN_DELAY);
    /* The following recommended by Dimitrios */
    hal_set_data_conv_hq_adc_comp_ctrl(HAL_DATA_CONV_COMP_2_0);
    /* Reset global mux bus from PMU to 0 */
    hal_set_data_conv_pmu_mux_sel(0);
    hal_data_conv_update_static_signals();
}

void hal_data_conv_enable_chan(hal_data_conv_channels chan_sel, bool en)
{
    uint16 channel_bitmap;

    assert(chan_sel < HAL_DATA_CONV_NUM_CHANNELS);

    /* DATA_CONV_CHANNELS_IN_PROGRESS is the actual internal representation of
       the active channels. One shot measurements will cause the channel to
       auto-disable. The DATA_CONV_ACTIVE_CHANNELS register is only read by the
       HW while DATA_CONV_CHANNELS_IN_PROGRESS is never read but always written.
       However we need to ban one shot measurements since it's impossible to
       predict what channels need to be enabled here. */ 
    channel_bitmap = (uint16)hal_get_reg_data_conv_active_channels();

    if (en)
    {
        /* enable channel for adc measurement*/
        channel_bitmap = (uint16)(channel_bitmap | (1 << chan_sel));
    }
    else
    {
        /* disable channel */
        channel_bitmap = (uint16)(channel_bitmap & (~(1 << chan_sel)));
    }

    hal_set_reg_data_conv_active_channels(channel_bitmap);
}

void hal_data_conv_reset_ctrl(void)
{
    /* to disable the CTRL block the CFG block needs to be disable, otherwise
       it would hang */

    hal_data_conv_disable_ctrl();

    /* reset the ctrl block */
    hal_set_reg_data_conv_soft_reset(1);
}

void hal_data_conv_restart_ctrl(void)
{
    /* take away the reset */
    hal_set_reg_data_conv_soft_reset(0);

    hal_data_conv_re_enable_ctrl();
}

void hal_data_conv_disable_ctrl(void)
{
    hal_data_conv_disable_fake_adc_value_if_needed();
    hal_set_data_conv_cfg_enable(0);
}

void hal_data_conv_re_enable_ctrl(void)
{
    hal_data_conv_disable_fake_adc_value_if_needed();
    hal_set_data_conv_cfg_enable(1);

    /* Set the minimum value to wait (in us) for the analogue to come up after
     * a en macro de-assert */
    hal_set_data_conv_power_mode_count(HAL_DATA_CONV_ROUND_ROBIN_DELAY);
    /* commit the 5 us */
    hal_set_reg_data_conv_power_mode_en(1);
}

void hal_data_conv_start_single_adc_read(void)
{
    hal_set_reg_data_conv_adc_reading_enable(1);
    hal_set_reg_data_conv_start(1);
}

void hal_data_conv_update_static_signals(void)
{
    hal_set_data_conv_update_static_signals(0);
    hal_set_data_conv_update_static_signals(1);
}

void hal_data_conv_event_clear(void)
{
    /* Clear the event with the events disabled do avoid a race in the
       digits. */
    hal_set_reg_data_conv_event_en(0);
    hal_set_reg_data_conv_event_clear(0);
    hal_set_reg_data_conv_event_en(1);
    /* Check if any interrupts occured while the events were disabled.
       We need to have the stickies cleared at this point */
    if (hal_get_reg_data_conv_int_sources())
    {
        /* Manually raise the data_conv interrupt */
        hal_set_reg_data_conv_event_clear(1);
    }
}

#endif /* CHIP_HAS_AUX_DATA_CONV */
