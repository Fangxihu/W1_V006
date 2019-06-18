/* Copyright (c) 2018 Qualcomm Technologies International, Ltd. */
/* Part of 6.3.0 */
/*!
  @file broadcast_pmalloc_pools.c
  @brief Dummy pmalloc interface for non-Hydracore builds.
*/

#include "broadcast_private.h"

/* This function is here temporarily, and will be removed in a future release. */
void broadcast_pmalloc_init(void)
{
    /* Do nothing */
}
