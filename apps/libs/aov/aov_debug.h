/*******************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    aov_debug.h

DESCRIPTION
    Definition of macros used to help debug the AOV VM library
*/

#ifndef AOV_DEBUG_H_
#define AOV_DEBUG_H_

#include <panic.h>
#include <assert.h>

/* Macro used to generate debug version of this library */
#ifdef AOV_DEBUG_LIB

#ifndef DEBUG_PRINT_ENABLED
#define DEBUG_PRINT_ENABLED
#endif

#include <print.h>
#include <stdio.h>

#define AOV_DEBUG_RAW(x) PRINT(x)
#define AOV_DEBUG_INFO(x) {PRINT(("%s:%d - ", __FILE__, __LINE__)); PRINT(x);}
#define AOV_DEBUG_PANIC(x) {AOV_DEBUG_INFO(x); AOV_PANIC();}
#define AOV_DEBUG_ASSERT(x) AOV_ASSERT(x)

#else /* AOV_DEBUG_LIB */

#define AOV_DEBUG_RAW(x)
#define AOV_DEBUG_INFO(x)
#define AOV_DEBUG_PANIC(x)
#define AOV_DEBUG_ASSERT(x)

#endif /* AOV_DEBUG_LIB */

/* Macros defined in all variants of the AOV VM library */
#define AOV_PANIC() Panic()
#define AOV_ASSERT(x) {if (!(x)) Panic();}

#endif
