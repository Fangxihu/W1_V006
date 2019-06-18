/****************************************************************************
 * Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  opmgr_endpoint_override.c
 * \ingroup override
 *
 * Override module from Operator Manager.
 * This file contains the override specific operations for endpoints.
 *
 */
#include "stream/stream_endpoint.h"
#include "stream/stream_for_override.h"
#include "cbops_mgr/cbops_flags.h"
#include "cbops_mgr/cbops_mgr.h"
#include "opmgr/opmgr_endpoint_override.h"
#define EP_RATE_ADJUST_OP ((ENDPOINT_INT_CONFIGURE_KEYS)0x10018)
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
/* get_override_ep_rate_adjust_op */
bool get_override_ep_rate_adjust_op(OVERRIDE_EP_HANDLE ep_hdl, uint32* value)
{
    ENDPOINT_GET_CONFIG_RESULT result;
    result.u.value = 0;
    bool success = stream_get_connected_to_endpoint_config((ENDPOINT*)ep_hdl, EP_RATE_ADJUST_OP, &result);
    *value = result.u.value;
    return success;
}
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */
