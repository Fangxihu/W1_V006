/* Copyright (c) 2018 Qualcomm Technologies International, Ltd. */
/* Part of 6.3.0 */
/*!
  @file sport_health_pmalloc_pools.c
  @brief Creation of pmalloc pools used in Activity Monitoring configs only.
*/

#include "sport_health_hub_private.h"

#include <pmalloc.h>

_Pragma ("unitsuppress Unused")

_Pragma("datasection apppool") static const pmalloc_pool_config app_pools[] =
{
    {2048, 1}
};

/* This exported function is temporarily here to work round discarding of the
 * app_pools structure by the Linker, and will be removed in a future release. */
void sport_health_pmalloc_init(void)
{
    /* Do nothing */
}
