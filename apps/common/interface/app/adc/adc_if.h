/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/****************************************************************************
FILE
        adc_if.h  -  ADC interface
 
CONTAINS
        The GLOBAL definitions for the ADC subsystem from the VM
 
DESCRIPTION
        This file is seen by the stack, and VM applications, and
        contains things that are common between them.

*/

#ifndef __APP_ADC_IF_H__
#define __APP_ADC_IF_H__

/*! 
   @brief Current source for AdcReadRequest
*/
typedef enum
{
    /*! Standard measurement, no current source enabled. */
    ADC_STANDARD = 0,
    /*! Before measuring, enable the internal current source on the same pin we
        need to measure from. */
    ADC_INTERNAL_CURRENT_SOURCE_EN = 1,
    /*! Before measuring, enable the internal current source on LED0. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_LED0 = 2,
    /*! Before measuring, enable the internal current source on LED1. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_LED1 = 3,
    /*! Before measuring, enable the internal current source on LED2. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_LED2 = 4,
    /*! Before measuring, enable the internal current source on LED3. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_LED3 = 5,
    /*! Before measuring, enable the internal current source on LED4. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_LED4 = 6,
    /*! Before measuring, enable the internal current source on LED5. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_LED5 = 7,
    /*! Before measuring, enable the internal current source on LED6. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_LED6 = 8,
    /*! Before measuring, enable the internal current source on XIO0. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO0 = 9,
    /*! Before measuring, enable the internal current source on XIO1. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO1 = 10,
    /*! Before measuring, enable the internal current source on XIO2. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO2 = 11,
    /*! Before measuring, enable the internal current source on XIO3. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO3 = 12,
    /*! Before measuring, enable the internal current source on XIO4. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO4 = 13,
    /*! Before measuring, enable the internal current source on XIO5. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO5 = 14,
    /*! Before measuring, enable the internal current source on XIO6. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO6 = 15,
    /*! Before measuring, enable the internal current source on XIO7. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO7 = 16,
    /*! Before measuring, enable the internal current source on XIO8. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO8 = 17,
    /*! Before measuring, enable the internal current source on XIO9. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO9 = 18,
    /*! Before measuring, enable the internal current source on XIO10. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO10 = 19,
    /*! Before measuring, enable the internal current source on XIO11. */
    ADC_INTERNAL_CURRENT_SOURCE_EN_XIO11 = 20
} vm_adc_extra_flag;

/** Helper macro to convert a VM ADC extra flag into a VM ACD extra flag index */
#define vm_extra_flag_to_idx(x) \
    ((uint16)(x) - (uint16)ADC_STANDARD)
/** Helper macro to convert a VM ACD extra flag index into a VM ADC extra flag */
#define vm_extra_idx_to_flag(x) \
    (vm_adc_extra_flag)((x) + (uint16)ADC_STANDARD)

enum adcsel_enum {
        /*! Test pin AIO0.  Read with VM and BCCMD. */
        adcsel_aio0,

        /*! Test pin AIO1.  Read with VM and BCCMD. */
        adcsel_aio1,

        /*! Test pin AIO2.  Read with VM and BCCMD.
            Not available on BC7 or BC7-derived chips, e.g.
            CSR8670 */
        adcsel_aio2,

        /*! Test pin AIO3.  Read with VM and BCCMD.
            Not available on BC7 or BC7-derived chips, e.g.
            CSR8670 */
        adcsel_aio3,

        /*! The internal reference voltage in the chip.
            Read with VM and BCCMD. */
        adcsel_vref,

        /*! Battery voltage at output of the charger (CHG_VBAT).
            Read with VM and BCCMD. */
        adcsel_vdd_bat,

        /*! Input to bypass regulator (VCHG).
            Read with VM and BCCMD. */
        adcsel_byp_vregin,

        /*! Battery voltage sense (CHG_VBAT_SENSE).
            Read with VM and BCCMD. */
        adcsel_vdd_sense,

        /*! VREG_ENABLE voltage. Read with VM and BCCMD. */
        adcsel_vregen,

        /* IP_USB_HST_DATP */
        adcsel_usb_hst_datp,

        /* IP_USB_HST_DATM */
        adcsel_usb_hst_datm,

        /* BOOT_REF_VMAX */
        adcsel_boot_ref,

        /* CHG_IMON */
        adcsel_chg_mon,

        /* CHG_IMON_VREF */
        adcsel_chg_mon_vref,

        /* IO_PMU_VBAT_SNS*/
        adcsel_pmu_vbat_sns,

        /* IO_PMU_VCHG_SNS*/
        adcsel_pmu_vchg_sns,

        /* VREF*/
        adcsel_vref_hq_buff,

        adcsel_led0,

        adcsel_led1,

        adcsel_led2,

        adcsel_led3,

        adcsel_led4,

        adcsel_led5,

        adcsel_led6,

        adcsel_xio0,

        adcsel_xio1,

        adcsel_xio2,

        adcsel_xio3,

        adcsel_xio4,

        adcsel_xio5,

        adcsel_xio6,

        adcsel_xio7,

        adcsel_xio8,

        adcsel_xio9,

        adcsel_xio10,

        adcsel_xio11,

        adcsel_xio12

};
typedef enum adcsel_enum vm_adc_source_type;

/** Helper macro to convert a VM ADC internal source into a VM ACD internal source index */
#define vm_int_src_to_idx(x) \
    ((uint16)(x) - (uint16)adcsel_usb_hst_datp)
/** Helper macro to convert a VM ACD internal source index into a VM ADC internal source */
#define vm_int_idx_to_src(x) \
    (vm_adc_source_type)((x) + (uint16)adcsel_usb_hst_datp)

/** Helper macro to convert a VM ADC LED source into a VM ACD LED source index */
#define vm_led_src_to_idx(x) \
    ((uint16)(x) - (uint16)adcsel_led0)
/** Helper macro to convert a VM ACD LED source index into a VM ADC LED source */
#define vm_led_idx_to_src(x) \
    (vm_adc_source_type)((x) + (uint16)adcsel_led0)

/** Helper macro to convert a VM ADC XIO source into a VM ACD XIO source index */
#define vm_xio_src_to_idx(x) \
    ((uint16)(x) - (uint16)adcsel_xio0)
/** Helper macro to convert a VM ACD XIO source index into a VM ADC XIO source */
#define vm_xio_idx_to_src(x) \
    (vm_adc_source_type)((x) + (uint16)adcsel_xio0)

#endif /* ifndef __APP_ADC_IF_H__ */
