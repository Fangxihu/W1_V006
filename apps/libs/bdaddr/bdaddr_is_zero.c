/* Copyright (c) 2005 - 2015 Qualcomm Technologies International, Ltd. */
/* Part of 6.3.0 */

#include <bdaddr.h>

bool BdaddrIsZero(const bdaddr *in)
{ 
    return !in->nap && !in->uap && !in->lap; 
}
