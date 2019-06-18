#ifndef __FEATURE_H__
#define __FEATURE_H__
#include <app/feature/feature_if.h>

/*! file  @brief Related to licensing of new features
*/

#if TRAPSET_CORE

/**
 *  \brief By default, the VM software watchdog is disabled. Calling this trap with a
 *  valid timeout value will initiate/reset the VM software watchdog timer. If the
 *  VM software watchdog timer expires, then a VM Panic will be raised with Panic
 *  code PANIC_VM_SW_WD_EXPIRED and it will reset the chip.
 *    
 *     \note
 *     The purpose of the 3-stage disable sequence is to ensure that rogue
 *     applications do not randomly disable the watchdog by kicking it with some
 *     single disable codes. The return value of the this trap is designed to
 *     indicate to the user that either the watchdog kick has succeeded due to a
 *     valid timeout within the specified range or that the entire 3-stage disable
 *     sequence has succeeded. Returning TRUE for a call on the first disable
 *  sequence
 *     alone would actually go against the notion that the operation of disabling
 *  the
 *     watchdog has succeeded. In-fact at that moment (after having kicked with the
 *     first disable code), there is no guarantee of whether the disabling is
 *  either
 *     being done deliberately (in a valid manner) or the kick has been called by
 *     some rouge code. Hence, unless the entire disable operation does not
 *  succeed,
 *     the VM must not return TRUE.
 *  \param timeout The timeout period in seconds, in the range 1 to 300 or the specific disable
 *  codes VM_SW_WATCHDOG_DISABLE_CODE1, VM_SW_WATCHDOG_DISABLE_CODE2,
 *  VM_SW_WATCHDOG_DISABLE_CODE3.
 *  \return returns TRUE, 1.Whenever the VM software watchdog is kicked i.e., valid range
 *  (1-300 seconds). Eg: VmsoftwareWDKick(20); - Returns TRUE 2.Whenever the
 *  3-stage disable sequence is followed as per the requirement. Eg:
 *  VmsoftwareWDKick(20); - Returns TRUE
 *  VmSoftwareWDKick(VM_SW_WATCHDOG_DISABLE_CODE1); - Returns FALSE
 *  VmSoftwareWDKick(VM_SW_WATCHDOG_DISABLE_CODE2); - Returns FALSE
 *  VmSoftwareWDKick(VM_SW_WATCHDOG_DISABLE_CODE3); - Returns TRUE returns FALSE,
 *  1.Whenever the VM software watchdog timeout doesn't fall under 1-300 seconds
 *  range. 2.Whenever the VM software watchdog doesn't follow the 3-stage sequence
 *  or disabling the VM software watchdog fails. Eg: VmsoftwareWDKick(20); -
 *  Returns TRUE VmSoftwareWDKick(VM_SW_WATCHDOG_DISABLE_CODE2); - Returns FALSE
 *  VmSoftwareWDKick(VM_SW_WATCHDOG_DISABLE_CODE1); - Returns FALSE
 *  VmSoftwareWDKick(VM_SW_WATCHDOG_DISABLE_CODE3); - Returns FALSE
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_core
 */
bool VmSoftwareWdKick(uint16 timeout);

/**
 *  \brief This trap checks whether a feature is licensed for a device.
 *     
 *     This trap verifies whether a device is licensed to use a given feature.
 *  
 *  \param feature Identifier for a given feature. Refer \#feature_id
 *  \return TRUE if device has valid license for feature, else FALSE.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_core
 */
bool FeatureVerifyLicense(feature_id feature);
#endif /* TRAPSET_CORE */
#endif
