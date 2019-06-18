/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */

#include "memprot/memprot_private.h"

static uint32 mp_counter = 0;
static uint32 mp_run;

static uint32 callee1(uint32 a)
{
    return a + 2;
}

static uint32 callee2(uint32 a)
{
    return a - 1;
}

static uint32 caller1(uint32 a)
{
    uint32 loop = mp_run % 16;
    uint32 i;

    for(i=0;i<loop;i++)
    {
        a = callee1(a);
    }

    return a;
}

static uint32 caller2(uint32 a)
{
    uint32 loop = mp_run % 15;
    uint32 i;

    for(i=0;i<loop;i++)
    {
        a = callee2(a);
    }
    return a;
}

static void mp_test_finish(void)
{
    L2_DBG_MSG1("memprot: Finish %d",mp_counter);
}

/**
 * Static helper function which calls two functions 1000 times.
 * These caller functions call two sub functions a variable number
 * of times doing some arbitrary arithmetic. This slightly strange
 * sequence was used for reproducing a hardware bug in the trace
 * module and has been left in for future use.
 * 
 * The outer functions mp_test_start() and mp_test_finish() can be
 * used as triggers for the trace module.
 */
static void mp_test_start(void)
{
    mp_run = 1000;
    mp_counter = 10;

    while(mp_run)
    {
        mp_counter = caller1(mp_counter);
        mp_counter = caller2(mp_counter);
        mp_run--;
    }

    mp_test_finish();

}




/**
 * Handler for the various sub-tests of the \c memprot firmware test
 * @param command UNUSED
 * @param params First entry indicates which subtest
 * @param result UNUSED
 * @return UNUSED
 */
static APPCMD_RESPONSE memprot_test_handler(APPCMD_TEST_ID command,
                                          uint32 * params,
                                          uint32 * result)
{
    APPCMD_TEST_MEMORY_PROTECTION_TEST_ID testid =
                            (APPCMD_TEST_MEMORY_PROTECTION_TEST_ID)params[0];
    uint32 address = params[1];

    UNUSED(command);
    UNUSED(result);
    L2_DBG_MSG1("memprot: Test handler received CMD %d",testid);

    switch(testid)
    {
    case MEMPROT_APPCMD_READ:
    {
        result[0] = *(uint32*)(address);
        break;
    }
    case MEMPROT_APPCMD_WRITE:
    {

        *(uint32*)(address)= params[2];
        break;
    }
    case MEMPROT_APPCMD_READ16:
    {
        result[0] = *(uint16*)(address);
        break;
    }
    case MEMPROT_APPCMD_WRITE16:
    {

        *(uint16*)(address)= (uint16)params[2];
        break;
    }
    case MEMPROT_APPCMD_READ8:
    {
        result[0] = *(uint8*)(address);
        break;
    }
    case MEMPROT_APPCMD_WRITE8:
    {

        *(uint8*)(address)= (uint8)params[2];
        break;
    }
    case MEMPROT_APPCMD_FUNC_CALL:
    {
        mp_test_start();
        break;
    }
    default:
        return APPCMD_RESPONSE_UNIMPLEMENTED;
    }
    L2_DBG_MSG("memprot: Test handler complete");
    return APPCMD_RESPONSE_SUCCESS;
}

void memprot_register_test_handler(void)
{
    if (!appcmd_add_test_handler(APPCMD_TEST_ID_MEMORY_PROTECTION,
                                    memprot_test_handler))
    {
        L1_DBG_MSG("memprot_test: Couldn't register test handler!");
        return;
    }
}
/*@}*/
