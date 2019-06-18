/* Copyright (c) 2016 - 2018 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * Internal header file holding the fixed pool configurations
 * known for each subsystem on processor P1 builds
 * This configuration provides the *minimum* allocation, which 
 * is topped up with sane allocations for the remaining heap space
 * for each module or subsystem, feel free to edit *your* allocation.
 * The pool size put in needs to be multiple of 4.
 */
#ifndef PMALLOC_CONFIG_H
#define PMALLOC_CONFIG_H

/*****************************************************************************
 * DEFAULT MEMORY POOL CONFIGURATION
 *****************************************************************************/
static const pmalloc_pool_config pools_reqd[] =
{ 
    {   4, 18 },
    {   8, 27 },
    {  12, 19 },
    {  16, 15 },
    {  20, 24 },
    {  24, 15 },
    {  28, 63 },
    {  32, 21 },
    {  36, 21 },
    {  40, 10 },
    {  56,  9 },
    {  64,  7 },
    {  80,  9 },
    { 120, 16 },
    { 140,  4 },
    { 180,  6 },
    { 220,  3 },
    { 288,  1 },    /* for theGatt = gattState */
    { 512,  1 },
    { 692,  3 },    /* Needed for Broadcast Audio Receiver */
    /* 289 blocks, total 11880 bytes */
};
  
#endif /* PMALLOC_CONFIG_H */
