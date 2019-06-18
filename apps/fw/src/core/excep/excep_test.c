/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */

#include "excep/excep_private.h"

#define EXCEPTION_TEST_BUFFER_SIZE  (MMU_BUFFER_SIZE_32K)


/**
 * Function that calls itself to use up the stack. More elaborate than
 * you would expect because the compiler turns out to be rather good at
 * optimising the recursive function call into a jump.
 * @param count Controls how deep the iteration goes
 * @return Nothing useful. Just there to help stop the compiler from
 * optimising out the calls.
 */
static uint32 blow_the_stack_away(uint32 count)
{
    uint32 ar[4];
    uint32 i;
    uint32 sum = 0;
    for(i=0; i<4; ++i)
    {
        ar[i] = blow_the_stack_away(count + i);
    }
    for(i=0; i<4; ++i)
    {
        sum = sum | ar[i];
    }
    return sum;
}

/**
 * Handler for the various sub-tests of the \c excep firmware test
 * @param command UNUSED
 * @param params First entry indicates which subtest
 * @param result UNUSED
 * @return UNUSED
 */
static APPCMD_RESPONSE excep_test_handler(APPCMD_TEST_ID command,
                                          uint32 * params,
                                          uint32 * result)
{
    APPCMD_TEST_MEMORY_EXCEPTION_TEST_ID testid =
                            (APPCMD_TEST_MEMORY_EXCEPTION_TEST_ID)params[0];
    APPCMD_RESPONSE ret = APPCMD_RESPONSE_UNSPECIFIED;

    UNUSED(command);
    UNUSED(result);

#ifndef CHIP_DOESNT_HAVE_INT_PRIORITY_BLOCKING
    /*
     * If the chip has the interrupt blocking to level then we can expect
     * exceptions whilst interrupts are blocked to work properly. So
     * block them now to make sure.
     */
    block_interrupts();
#endif
    switch(testid)
    {
    case EXCEP_APPCMD_BRANCH_THROUGH_ZERO:
    {
        /* coverity issues are the point of this test! */
        /* coverity[assign_zero] */
        void (*null_fn_ptr)(void) = NULL;
        /* coverity[var_deref_op] */
        /*lint -e413 Use of NULL pointer */
        null_fn_ptr();
        break;
    }
    case EXCEP_APPCMD_UNALIGNED_16BIT_ACCESS:
    {
        uint16 *unaligned_ptr = (uint16 *)0x1;
        uint16 unaligned_value = *unaligned_ptr;
        L1_DBG_MSG1("excep_test: unexpectedly managed to read an unaligned "
                    "16-bit value: 0x%x!", unaligned_value);
        break;
    }
    case EXCEP_APPCMD_UNALIGNED_32BIT_ACCESS:
    {
        uint32 *unaligned_ptr = (uint32 *)0x2;
        uint32 unaligned_value = *unaligned_ptr;
        L1_DBG_MSG1("excep_test: unexpectedly managed to read an unaligned "
                     "32-bit value: 0x%x!", unaligned_value);
        break;
    }
    case EXCEP_APPCMD_STACK_OVERFLOW:
        (void)blow_the_stack_away(512);
        break;

    default:
        ret = APPCMD_RESPONSE_UNIMPLEMENTED;
        break;
    }

#ifndef CHIP_DOESNT_HAVE_INT_PRIORITY_BLOCKING
    unblock_interrupts();
#endif
    return ret;
}

void excep_register_test_handler(void)
{
    if (!appcmd_add_test_handler(APPCMD_TEST_ID_MEMORY_EXCEPTION,
                                    excep_test_handler))
    {
        L1_DBG_MSG("excep_test: Couldn't register test handler!");
        return;
    }
}

/*@}*/
