/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * This is the header file for the HAL layer of the data_conv driver.
*/

#ifndef _HAL_DATA_CONV_H_
#define _HAL_DATA_CONV_H_

#include "hydra/hydra_types.h"
#include "hydra/hydra_macros.h"
#define IO_DEFS_MODULE_AUX_DATA_CONV
#include "io/io.h"
#include "hal/hal.h"
#include "hal/hal_registers.h"
#include "hal/hal_macros.h"
#undef IO_DEFS_MODULE_AUX_DATA_CONV
#include "hal/haltime.h"

#if CHIP_HAS_AUX_DATA_CONV

#error Need to specify number of DATA_CONV channels on this chip

/**
 * Select the time between round robin reads. It takes the pulse from the
 * 32 bit timer.
 */
#define HAL_DATA_CONV_ROUND_ROBIN_INTERVAL 8

/**
 * Count us ticks before doing one round robin read. usually 5us
 */
#define HAL_DATA_CONV_ROUND_ROBIN_DELAY 5

/**
 * The magnitude of the smallest and largest values the ADC can return given
 * its bit width.
 */
#define HAL_DATA_CONV_ADC_RANGE 0x4000

/**
 * Maximum value that can be handled by the ADC HW (filters, threshold and
 * triggers).
 */
#define HAL_DATA_CONV_MAX_INTERNAL_VOLTAGE_VALUE 0xFFFF


/**
 * Minimum value that can be handled by the ADC HW (filters, threshold and
 * triggers).
 */
#define HAL_DATA_CONV_MIN_INTERNAL_VOLTAGE_VALUE (-0x10000)

/**
 * Maximum value that can be handled by the acquisition delay timer.
 */
#define HAL_DATA_CONV_MAX_ACQUISITION_DELAY 0xFFF

/**
 * Maximum value that can be taken by the charge time.
 */
#define HAL_DATA_CONV_MAX_CHARGE_TIME 0xF

/**
 * Maximum value that can be taken by the sample time.
 */
#define HAL_DATA_CONV_MAX_SAMPLE_TIME 0x3F

/**
 * Maximum value that can be taken by the fast update period.
 */
#define HAL_DATA_CONV_MAX_FAST_UPDATE_PERIOD 10

/**
 * Maximum value that can be taken by the slow update period.
 */
#define HAL_DATA_CONV_MAX_SLOW_UPDATE_PERIOD 20

/**
 * Maximum value that can be taken by the trigger state.
 */
#define HAL_DATA_CONV_MAX_TRIGGER_STATE 3

/**
 * Maximum value that can be taken by the trigger event count.
 */
#define HAL_DATA_CONV_MAX_TRIGGER_EVENT_COUNT 0xF

/**
 * The maximum value that fits in a calibration register.
 */
#define hal_data_conv_max_cal_reg_val() \
    ((1 << ((unsigned)DATA_CONV_SAR_COMPARATOR_OFFSET_MSB_POSN - \
            (unsigned)DATA_CONV_SAR_COMPARATOR_OFFSET_LSB_POSN) \
      ) - 1)

/**
 * The minimum value that fits in a calibration register.
 */
#define hal_data_conv_min_cal_reg_val() \
    (~hal_data_conv_max_cal_reg_val())

/**
 * Type definition for the FE mode. This is chosen depending on the type of
 * measurement that needs to take place.
 */
typedef enum
{
    HAL_DATA_CONV_FE_MODE_ALL_OFF = DATA_CONV_FE_MODE_ALL_OFF,
    HAL_DATA_CONV_FE_MODE_CAPACITIVESENSOR = DATA_CONV_FE_MODE_CAPACITIVESENSOR,
    HAL_DATA_CONV_FE_MODE_VSCLR_SE = DATA_CONV_FE_MODE_VSCLR_SE,
    HAL_DATA_CONV_FE_MODE_VSCLR_DIFF = DATA_CONV_FE_MODE_VSCLR_DIFF,
    HAL_DATA_CONV_FE_MODE_ADC_TEST = DATA_CONV_FE_MODE_ADC_TEST,
    HAL_DATA_CONV_FE_MODE_TEMP = DATA_CONV_FE_MODE_TEMP_SENSE_READ,
    HAL_DATA_CONV_FE_MODE_GMUX2DATACONV_ADC =
        DATA_CONV_FE_MODE_GMUX2DATACONV_ADC,
    HAL_DATA_CONV_FE_MODE_CALIBRATION = DATA_CONV_FE_MODE_CALIBRATION
} hal_data_conv_fe_mode;

/**
 * Type definition for channel IDs. HAL_DATA_CONV_CHANNEL_NONE should be used
 * to specify an unused channel.
 */
typedef enum
{
    HAL_DATA_CONV_CHANNEL0 = 0,
    HAL_DATA_CONV_CHANNEL1,
    HAL_DATA_CONV_CHANNEL2,
    HAL_DATA_CONV_CHANNEL3,
    HAL_DATA_CONV_CHANNEL4,
    HAL_DATA_CONV_CHANNEL5,
    HAL_DATA_CONV_CHANNEL6,
    HAL_DATA_CONV_CHANNEL7,
    HAL_DATA_CONV_CHANNEL8,
    HAL_DATA_CONV_CHANNEL9,
    HAL_DATA_CONV_CHANNEL10,
    HAL_DATA_CONV_CHANNEL11,
    HAL_DATA_CONV_CHANNEL12,
    HAL_DATA_CONV_CHANNEL_NONE
} hal_data_conv_channels;

/** Helper macro to convert a channel source into a channel source index */
#define hal_data_conv_ch_src_to_idx(x) \
    ((uint16)(x) - (uint16)HAL_DATA_CONV_CHANNEL0)
/** Helper macro to convert a channel source index into a channel source */
#define hal_data_conv_ch_idx_to_src(x) \
    ((hal_data_conv_channels)((x) + (uint16)HAL_DATA_CONV_CHANNEL0))
/**
 * Helper macro to convert a channel source into a mask of channel source
 * indices
 */
#define hal_data_conv_ch_src_to_idx_mask(x) \
    ((uint16)(1 << hal_data_conv_ch_src_to_idx(x)))

/**
 * Type definition for controlling various aspects of a measurement.
 */
typedef enum
{
    HAL_DATA_CONV_FLAGS_NONE = 0,
    /** An interrupts (or event) is raised when the measurement completes. */
    HAL_DATA_CONV_FLAGS_EVENT_EN = 1 << 0,
    /** Voltage detect mode, DATA_CONV_TIME_STAMP_POS_LO is used to store the
        max value and DATA_CONV_TIME_STAMP_NEG_LO to store the min value.
        Timestamps are ignored. */
    HAL_DATA_CONV_FLAGS_V_DETECT_EN = 1 << 1,
    /** Enabling one shot causes the channel to be automatically deactivated
        after the first measurement is taken. Otherwise the channel remains
        active raising interrupts each time a measurement is periodically
        taken. */
    HAL_DATA_CONV_FLAGS_ONE_SHOT_EN = 1 << 2,
    /** In ghost mode the data_conv would not execute an ADC reading, but it
        would take the time to do an ADC reading. */
    HAL_DATA_CONV_FLAGS_GHOST_EN = 1 << 3,
    HAL_DATA_CONV_FLAGS = HAL_DATA_CONV_FLAGS_EVENT_EN |
                          HAL_DATA_CONV_FLAGS_V_DETECT_EN |
                          HAL_DATA_CONV_FLAGS_ONE_SHOT_EN |
                          HAL_DATA_CONV_FLAGS_GHOST_EN
} hal_data_conv_flags;

/**
 * Type definition for interrupt flags.
 */
typedef enum
{
    HAL_DATA_CONV_INT_CHANNEL0 = 1 << HAL_DATA_CONV_CHANNEL0,
    HAL_DATA_CONV_INT_CHANNEL1 = 1 << HAL_DATA_CONV_CHANNEL1,
    HAL_DATA_CONV_INT_CHANNEL2 = 1 << HAL_DATA_CONV_CHANNEL2,
    HAL_DATA_CONV_INT_CHANNEL3 = 1 << HAL_DATA_CONV_CHANNEL3,
    HAL_DATA_CONV_INT_CHANNEL4 = 1 << HAL_DATA_CONV_CHANNEL4,
    HAL_DATA_CONV_INT_CHANNEL5 = 1 << HAL_DATA_CONV_CHANNEL5,
    HAL_DATA_CONV_INT_CHANNEL6 = 1 << HAL_DATA_CONV_CHANNEL6,
    HAL_DATA_CONV_INT_CHANNEL7 = 1 << HAL_DATA_CONV_CHANNEL7,
    HAL_DATA_CONV_INT_CHANNEL8 = 1 << HAL_DATA_CONV_CHANNEL8,
    HAL_DATA_CONV_INT_CHANNEL9 = 1 << HAL_DATA_CONV_CHANNEL9,
    HAL_DATA_CONV_INT_CHANNEL10 = 1 << HAL_DATA_CONV_CHANNEL10,
    HAL_DATA_CONV_INT_CHANNEL11 = 1 << HAL_DATA_CONV_CHANNEL11,
    HAL_DATA_CONV_INT_CHANNEL12 = 1 << HAL_DATA_CONV_CHANNEL12,
    HAL_DATA_CONV_INT_CHANNELS =
        HAL_DATA_CONV_INT_CHANNEL0 | HAL_DATA_CONV_INT_CHANNEL1 |
        HAL_DATA_CONV_INT_CHANNEL2 | HAL_DATA_CONV_INT_CHANNEL3 |
        HAL_DATA_CONV_INT_CHANNEL4 | HAL_DATA_CONV_INT_CHANNEL5 |
        HAL_DATA_CONV_INT_CHANNEL6 | HAL_DATA_CONV_INT_CHANNEL7 |
        HAL_DATA_CONV_INT_CHANNEL8 | HAL_DATA_CONV_INT_CHANNEL9 |
        HAL_DATA_CONV_INT_CHANNEL10 | HAL_DATA_CONV_INT_CHANNEL11 |
        HAL_DATA_CONV_INT_CHANNEL12,
    HAL_DATA_CONV_INT_CAL = 1 << 16,
    HAL_DATA_CONV_INT_SINGLE_READ = 1 << 17
} hal_data_conv_int_sources;

/** Helper macro to convert a channel source into a channel interrupt */
#define hal_data_conv_ch_src_to_int(x) \
    ((hal_data_conv_int_sources)(1 << hal_data_conv_ch_src_to_idx(x)))

/**
 * Selects power(current) modes in the HQ ADC comparator 0: 1.0 * I (typical),
 * 1: 1.1 * I,  2: 1.3 * I,  3: 1.6 * I,  4: 2.0 * I,  5: 2.6 * I,  6: 4.0 * I,
 * 7: 8.0 * I
 */
typedef enum
{
    HAL_DATA_CONV_COMP_1_0 = 0,
    HAL_DATA_CONV_COMP_1_1 = 1,
    HAL_DATA_CONV_COMP_1_3 = 2,
    HAL_DATA_CONV_COMP_1_6 = 3,
    HAL_DATA_CONV_COMP_2_0 = 4,
    HAL_DATA_CONV_COMP_2_6 = 5,
    HAL_DATA_CONV_COMP_4_0 = 6,
    HAL_DATA_CONV_COMP_8_0 = 7
} hal_data_conv_comp_power_mode;

/**
 * Type definition for the gain. Please note that gains of 4 and 8 are very
 * imprecise.
 */
typedef enum
{
    HAL_DATA_CONV_GAIN_1 = 1,
    HAL_DATA_CONV_GAIN_0_85 = 2,
    HAL_DATA_CONV_GAIN_2 = 3,
    HAL_DATA_CONV_GAIN_4 = 4,
    HAL_DATA_CONV_GAIN_8 = 5,
    HAL_DATA_CONV_GAIN_DEFAULT = HAL_DATA_CONV_GAIN_1,
    HAL_DATA_CONV_GAIN_NONE = 0xFF
} hal_data_conv_vscl_gain;

/** Helper macro to convert a gain into an index */
#define hal_data_conv_gain_src_to_idx(x) \
    ((uint16)(x) - (uint16)HAL_DATA_CONV_GAIN_1)
/** Helper macro to convert a gain index into a source */
#define hal_data_conv_gain_idx_to_src(x) \
    ((hal_data_conv_vscl_gain)((x) + (uint16)HAL_DATA_CONV_GAIN_1))

typedef data_conv_over_smpl_rate_adc_enum hal_data_conv_over_sample_rate_adc;
/** Helper macro to convert oversampling rate into an index */
#define hal_data_conv_over_sample_rate_src_to_idx(x) \
    ((uint16)(x) - (uint16)DATA_CONV_OVER_SMPL_RATE_0)
/** Helper macro to convert oversampling rate index into a source */
#define hal_data_conv_over_sample_rate_idx_to_src(x) \
    ((hal_data_conv_over_sample_rate_adc)((x) + \
                                          (uint16)DATA_CONV_OVER_SMPL_RATE_0))

/**
 * Available Gmux bus for ADC mode of operation FE_MODE_GMUX2DATACONV_ADC.
 */
typedef enum
{
    HAL_DATA_CONV_GMUX_BUS_0 = 0,
    HAL_DATA_CONV_GMUX_BUS_1 = 1,
    HAL_DATA_CONV_GMUX_BUS_2 = 2,
    HAL_DATA_CONV_GMUX_BUS_NONE = 3
} hal_data_conv_gmux_bus;

/**
 * Typedef for trigger setup.
 */
typedef enum
{
    HAL_DATA_CONV_TRIGGER_ALL_DISABLE = 0,
    HAL_DATA_CONV_TRIGGER_ENABLE = 1,
    HAL_DATA_CONV_TRIGGER_TIMESTAMP = 2,
    HAL_DATA_CONV_TRIGGER_T0_ENABLE = HAL_DATA_CONV_TRIGGER_ENABLE << 0,
    HAL_DATA_CONV_TRIGGER_T0_TIMESTAMP = HAL_DATA_CONV_TRIGGER_TIMESTAMP << 0,
    HAL_DATA_CONV_TRIGGER_T1_ENABLE = HAL_DATA_CONV_TRIGGER_T0_ENABLE << 2,
    HAL_DATA_CONV_TRIGGER_T1_TIMESTAMP =
        HAL_DATA_CONV_TRIGGER_T0_TIMESTAMP << 2,
    HAL_DATA_CONV_TRIGGER_T2_ENABLE = HAL_DATA_CONV_TRIGGER_T1_ENABLE << 2,
    HAL_DATA_CONV_TRIGGER_T2_TIMESTAMP =
        HAL_DATA_CONV_TRIGGER_T1_TIMESTAMP << 2,
    HAL_DATA_CONV_TRIGGER_T3_ENABLE = HAL_DATA_CONV_TRIGGER_T2_ENABLE << 2,
    HAL_DATA_CONV_TRIGGER_T3_TIMESTAMP =
        HAL_DATA_CONV_TRIGGER_T2_TIMESTAMP << 2,
    HAL_DATA_CONV_TRIGGER_ALL_ENABLE = HAL_DATA_CONV_TRIGGER_T0_ENABLE |
                                       HAL_DATA_CONV_TRIGGER_T1_ENABLE |
                                       HAL_DATA_CONV_TRIGGER_T2_ENABLE |
                                       HAL_DATA_CONV_TRIGGER_T3_ENABLE
} aux_data_conv_trigger_flags;

/**
 * Typedef for channel reports.
 */
typedef struct hal_data_conv_chan_rpt
{
    hal_data_conv_channels chan; /**< Channel ID */
    uint32 time_stamp_t0; /**< [31:0]  Timestamp for TriggerT0 */
    uint16 time_stamp_comp_t0; /**< Compensation for timestamp overflows */
    uint32 time_stamp_t1; /**< [31:0]  Timestamp for TriggerT1 */
    uint16 time_stamp_comp_t1; /**< Compensation for timestamp overflows */
    int16 adc_value; /**< [15:0]  Last recorded ADC value */
    uint8 reading_acquired; /**< A Channel reading has been done */
    uint16 fast_accum; /**< [15:0] Fast accumulator */
    uint16 slow_accum; /**< [15:0] Slow accumulator*/
    uint8 trigger_sticky; /**< [ 3:0] If there has been a trigger event then
                               this bit is set */
    uint8 trigger_state; /**< We need to know which triggers are allowed to
                              happen */
} hal_data_conv_chan_rpt;

/**
 * Typedef for selecting the data to be grabbed in the report.
 */
typedef enum hal_data_conv_rpt_flags
{
    HAL_DATA_CONV_RPT_TIMESTAMP0 = 1 << 0,
    HAL_DATA_CONV_RPT_TIMESTAMP1 = 1 << 1,
    HAL_DATA_CONV_RPT_ADC_VALUE = 1 << 2,
    HAL_DATA_CONV_RPT_FAST_ACCUM = 1 << 3,
    HAL_DATA_CONV_RPT_SLOW_ACCUM = 1 << 4,
    HAL_DATA_CONV_RPT_TRIGGER = 1 << 5,
    HAL_DATA_CONV_RPT_ALL = HAL_DATA_CONV_RPT_TIMESTAMP0 |
                            HAL_DATA_CONV_RPT_TIMESTAMP1 |
                            HAL_DATA_CONV_RPT_ADC_VALUE |
                            HAL_DATA_CONV_RPT_FAST_ACCUM |
                            HAL_DATA_CONV_RPT_SLOW_ACCUM |
                            HAL_DATA_CONV_RPT_TRIGGER
} hal_data_conv_rpt_flags;

/**
 * Initialisation setup.
 */
extern void hal_data_conv_setup(void);

/**
 * Sets up a channel with the given parameters.
 * \param chan_sel Channel ID.
 * \param acquisition_delay Programamble delay applied after HW setup and
 * before taking the measurement.
 * \param over_sample_rate Oversample rate. The rate is a power of 2 and it
 * represents the number of samples to be accumulated.
 * \param fe_mode Mode of operation. This depends on the type of measurement
 * (capacitive sensing, single ended voltage measurement, differential voltage
 * measurement, ADC test mode, temperature sensor reading, internal source
 * reading, internal calibration).
 * \param pad_n Negative input for differential measurements. This is unused in
 * other modes.
 * \param vscl_gain Controls the gain for voltage measurements (VSCL modes).
 */
extern bool hal_data_conv_cfg_chan_setup(
                hal_data_conv_channels chan_sel,
                uint16 acquisition_delay,
                hal_data_conv_over_sample_rate_adc over_sample_rate,
                hal_data_conv_fe_mode fe_mode,
                hal_data_conv_channels pad_n,
                hal_data_conv_vscl_gain vscl_gain);

/**
 * Sets up a channel with the given parameters.
 * \param chan_sel Channel ID.
 * \param charge_time Wait time for charging in capacitive sensing and VSCL
 * modes.
 * \param sample_time Wait time for sampling in capacitive sensing and VSCL
 * modes.
 */
extern bool hal_data_conv_cfg_chan_charge_sample_time(
                hal_data_conv_channels chan_sel,
                uint8 charge_time,
                uint8 sample_time);

/**
 * Sets up a channel with the given parameters.
 * \param chan_sel Channel ID.
 * \param fast_update_period Update period for the fast integrator.
 * \param slow_update_period Update period for the slow integrator.
 */
extern bool hal_data_conv_cfg_chan_fast_slow_update_period(
                hal_data_conv_channels chan_sel,
                uint8 fast_update_period,
                uint8 slow_update_period);

/**
 * Sets up a channel with the given parameters.
 * \param chan_sel Channel ID.
 * \param fast_accum Preload value for the fast integrator.
 * \param slow_accum Preload value for the slow integrator.
 */
extern bool hal_data_conv_cfg_chan_fast_slow_accum_preload(
                hal_data_conv_channels chan_sel,
                uint32 fast_accum,
                uint32 slow_accum);

/**
 * Sets up a channel with the given parameters. This adds an offset to the
 * slow accumulator to not leak away a very long touch. This should be set as
 * soon as a touch or release is detected. On touch on, the offset should be as
 * big as the capacitance added by the touch.
 * \param chan_sel Channel ID.
 * \param touch_offset Touch offset.
 */
extern bool hal_data_conv_cfg_chan_touch_offset(
                hal_data_conv_channels chan_sel,
                uint32 touch_offset);

/**
 * Sets up a channel with the given parameters. The trigger state machine has
 * to go through all the enabled triggers in order. This sets the state of the
 * trigger state machine.
 * \param chan_sel Channel ID.
 * \param trigger_state Trigger state.
 */
extern bool hal_data_conv_cfg_chan_trigger_state(
                hal_data_conv_channels chan_sel,
                uint8 trigger_state);

/**
 * Sets up a channel with the given parameters. Please see
 * \c aux_data_conv_trigger_flags for details about the trigger flags.
 * \param chan_sel Channel ID.
 * \param trigger_t0 Trigger level for T0.
 * \param trigger_t1 Trigger level for T1.
 * \param trigger_t2 Trigger level for T2.
 * \param trigger_t3 Trigger level for T3.
 * \param trigger_flags Specifies which triggers are enabled and have
 * timestamps enabled.
 */
extern bool hal_data_conv_cfg_chan_triggers(hal_data_conv_channels chan_sel,
                                            int32 trigger_t0,
                                            int32 trigger_t1,
                                            int32 trigger_t2,
                                            int32 trigger_t3,
                                            uint8 trigger_flags);

/**
 * Sets up a channel with the given parameters. The trigger event count is the
 * number of times in a row a trigger condition needs to be true, for the
 * trigger state machine to advance. 
 * \param chan_sel Channel ID.
 * \param trigger_event_count Trigger event count.
 */
extern bool hal_data_conv_cfg_chan_trigger_event_count(
                hal_data_conv_channels chan_sel,
                uint8 trigger_event_count);

/**
 * Sets up a channel with the given parameters. The channel flags are detailed
 * in \c hal_data_conv_flags.
 * \param chan_sel Channel ID.
 * \param flags Channel flags.
 */
extern bool hal_data_conv_cfg_chan_flags(hal_data_conv_channels chan_sel,
                                         uint8 flags);

/**
 * Gets the channel report based on the selected flags.
 * \param chan_sel Channel ID.
 * \param p_chan_rpt Pointer to buffer for storing the channel report.
 * \param flags Report flags.
 */
extern void hal_data_conv_rpt_chan (hal_data_conv_channels chan_sel,
                                    hal_data_conv_chan_rpt *p_chan_rpt,
                                    hal_data_conv_rpt_flags flags);

/**
 * Enables or disables a channel.
 * \param chan_sel Channel ID.
 * \param en TRUE for enabling a channel, FALSE for disabling it.
 */
extern void hal_data_conv_enable_chan(hal_data_conv_channels chan_sel,
                                      bool en);

/**
 * Resets the CTRL block.
 */
extern void hal_data_conv_reset_ctrl(void);

/** Enables the ctrl block after a reset. */
extern void hal_data_conv_restart_ctrl(void);

/**
 * Disables the ctrl block.
 */
extern void hal_data_conv_disable_ctrl(void);

/** Enables the ctrl block after a disable. */
extern void hal_data_conv_re_enable_ctrl(void);

/** Start one single ADC read without using the channels. */
extern void hal_data_conv_start_single_adc_read(void);

/** Helper function for updating the static signals. */
void hal_data_conv_update_static_signals(void);

/** Clears the interrupts. This implements the event clear race workaround. */
void hal_data_conv_event_clear(void);

#endif /* CHIP_HAS_AUX_DATA_CONV */

#endif /* _HAL_DATA_CONV_H_ */

