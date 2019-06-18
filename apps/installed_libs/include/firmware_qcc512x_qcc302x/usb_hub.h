#ifndef __USB_HUB_H__
#define __USB_HUB_H__
#include <app/usb/usb_if.h>
#include <app/usb/usb_hub_if.h>

/*! file  @brief Control of internal USB hub. 
** 
**
The new USB device architecture includes an internal USB hub, these
traps allow some control over it, like attaching/detaching device
to/from the hub.
*/

#if TRAPSET_USB_HUB

/**
 *  \brief Attach USB device to the hub
 *     Device freezes all descriptors and starts attachment to the host by
 *     attaching to the hub. Changes to the descriptors are no longer allowed
 *     and correspondent traps will fail.
 *         
 *  \return FALSE if something has gone wrong.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_usb_hub
 */
bool UsbHubAttach(void );

/**
 *  \brief Detach USB device from the hub
 *     Detaches USB device from the hub and hence from the host so that
 *     USB descriptors can be modified.
 *         
 *  \return FALSE if something has gone wrong.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_usb_hub
 */
bool UsbHubDetach(void );

/**
 *  \brief Configure USB device
 *         Re-configure USB device using supplied parameters or 
 *         parameters from MIB keys (if "device_params" == NULL).
 *         Any descriptors previously added will be destroyed.
 *         Device must not be attached to the hub.
 *         
 *  \param device_params Pointer to a structure with USB device parameters
 *             (can be de-allocated after the trap call). If "NULL" is supplied,
 *             then parameters are taken from MIB keys.
 *             
 *  \return TRUE if finished successfully,
 *           FALSE if something has gone wrong.
 * 
 * \note This trap may NOT be called from a high-priority task handler
 * 
 * \ingroup trapset_usb_hub
 */
bool UsbHubConfigure(const usb_device_parameters * device_params);
#endif /* TRAPSET_USB_HUB */
#endif
