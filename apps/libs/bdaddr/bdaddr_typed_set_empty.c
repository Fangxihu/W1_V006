/* Copyright (c) 2005 - 2015 Qualcomm Technologies International, Ltd. */
/* Part of 6.3.0 */

#include <bdaddr.h>

void BdaddrTypedSetEmpty(typed_bdaddr *in)
{
    in->type = TYPED_BDADDR_INVALID;
    BdaddrSetZero(&in->addr);
}
