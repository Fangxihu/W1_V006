/* Copyright (c) 2005 - 2015 Qualcomm Technologies International, Ltd. */
/* Part of 6.3.0 */

#include <bdaddr.h>

void BdaddrTpSetEmpty(tp_bdaddr *in)
{
    in->transport = TRANSPORT_NONE;
    BdaddrTypedSetEmpty(&in->taddr);
}

