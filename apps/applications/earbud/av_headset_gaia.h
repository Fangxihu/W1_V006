/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       
\brief      Header file for handling the GAIA transport interface
*/

#ifndef _AV_HEADSET_GAIA_H_
#define _AV_HEADSET_GAIA_H_

#ifdef INCLUDE_DFU

#include <gaia.h>


/*! Data used by the GAIA module */
typedef struct
{
        /*! Task for handling messaging from upgrade library */
    TaskData        gaia_task;  
        /*! The current transport (if any) for GAIA */
    GAIA_TRANSPORT *transport;
} gaiaTaskData;

/*! Get the transport for the current GAIA connection */
#define appGetGaiaTransport()           (appGetGaia()->transport)

/*! Set the transport for the current GAIA connection */
#define appSetGaiaTransport(_transport)  do { \
                                            appGetGaia()->transport = (_transport);\
                                           } while(0)

/*! Initialise the GAIA Module */
extern void appGaiaInit(void);


/*! \brief Disconnect any active gaia connection
 */
extern void appGaiaDisconnect(void);

/*! \brief Let GAIA know whether current connection is allowed

    Acceptance should be a matter of whather the application 
    was in an allowed DFU mode at the time of the connection

    \param  accepted gaia connection is allowed
 */
extern void appGaiaConnectionAllowed(bool accepted);


#endif /* INCLUDE_DFU */
#endif /* _AV_HEADSET_GAIA_H_ */

