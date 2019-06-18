/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 * Debounce input bits on the pio port.
 */

#include "piodebounce/piodebounce_private.h"


/** Stores the piodebounce database. */
static piodebounce_db piodb;

/* Forward references. */
static void sched_debounce(void);
static void update_output(uint16 bank);
static void register_kick(void);   

void init_piodebounce( void )
{
    uint16 i;

    for(i = 0; i < NUMBER_OF_PIO_BANKS; i++)
    {
        piodb.mask[i] = 0x00000000;
        piodb.output[i] = 0x00000000;
    }
    piodb.bank_mask = 0x0000;
    register_kick();
}

void piodebounce_config(uint16 bank,
                        pio_size_bits mask,
                        uint16 nreads,
                        uint16 period,
                        void (*callback)(uint16 bank))
{
        piodb.mask[bank] = mask;
        piodb.period = period;
        piodb.nreads = nreads;
        piodb.callback = callback;

        /* Set an initial output value. If piodb.mask is zero this will
        return zero. */
        piodb.output[bank] = pio_get_levels_all(bank) & mask;

        L4_DBG_MSG3("Piodebounce: Registered callback 0x%08x on bank %d mask "
                    "0x%08x", piodb.callback, bank, piodb.mask[bank]);
        register_kick();
}

void piodebounce_kick( void )
{
    uint16 i;

    L4_DBG_MSG("Piodebounce: Received kick"); 
    for(i = 0; i < NUMBER_OF_PIO_BANKS; i++)
    {
        piodb.tmp[i] = pio_get_levels_mask(i, piodb.mask[i]);
    }

    switch (piodb.nreads)
    {
        case 0:
            /* Special case 1: if piodb.nreads is 0 then always accept the
               input pins current value and always raise an event. */
            update_output(0);
            break;
        case 1:
            /* Special case 2: if piodb.nreads is 1 then always accept the
               input pins current value and raise an event if it differs
               from the last debounced value. */
            for(i = 0; i < NUMBER_OF_PIO_BANKS; i++)
            {
                if(piodb.output[i] != piodb.tmp[i])
                {
                    update_output(i);
                    break;
                }
            }
            break;
        default:
            /* The normal case: start a sequence of reads. */
            piodb.nleft = (uint16)(piodb.nreads - 1);
            sched_debounce();
            break;
    }
}

uint32 piodebounce_get(uint16 bank)
{
    return piodb.output[bank];
}

static void sched_debounce( void )
{
    /* Cancel any pending scheduled call of debounce(). */
    (void)cancel_itimed_event(itid_piodebounce);

    /* Call debounce_it_handler() after piodb.period milliseconds at int
       level 1. */
    itimed_event_in(1000 * (TIME)(piodb.period), itid_piodebounce);
}

void debounce_it_handler(void)
{
    uint32 current[NUMBER_OF_PIO_BANKS];
    uint16 i;

    L4_DBG_MSG("Piodebounce: Polling PIOs");
    for(i = 0; i < NUMBER_OF_PIO_BANKS; i++)
    {
        current[i] = pio_get_levels_mask(i, piodb.mask[i]);
    }

    /* Restart the sequence of reads if the current port value does not match
       the candidate new debounce value. */
    for(i = 0; i < NUMBER_OF_PIO_BANKS; i++)
    {
        if(piodb.tmp[i] != current[i])
        {
            L4_DBG_MSG("Piodebounce: PIOs changed, kicking"); 
            piodebounce_kick();
            return;
        }
    }

    if(piodb.nleft)
    {
        /* Go round again if we've not read the pins enough times. */
        --piodb.nleft;
        sched_debounce();
    }
    else
    {
        /* Accept candidate value as stable and signal the change. */
        L4_DBG_MSG("Piodebounce: PIOs stable"); 
        for(i = 0; i < NUMBER_OF_PIO_BANKS; i++)
        {
            if(piodb.output[i] != piodb.tmp[i])
            {
                update_output(i);
            }
        }
    }
}

static void register_kick(void)
{
    uint16 i;

    for(i = 0; i < NUMBER_OF_PIO_BANKS; i++)
    {
        pioint_configure(i, piodb.mask[i], piodebounce_kick);
    }
}

static void update_output(uint16 bank)
{
    ATOMIC_BLOCK_START {
        piodb.bank_mask = piodb.bank_mask | (1 << bank);
        piodb.output[bank] = piodb.tmp[bank];
    } ATOMIC_BLOCK_END;
    /* Drop to background and call the handler. */
    GEN_BG_INT(piodebounce);
}   

void piodebounce_bg_int_handler(void)
{
    uint32 bank_mask;
    uint16 bank;

    ATOMIC_BLOCK_START {
        bank_mask = piodb.bank_mask;
        piodb.bank_mask = 0;
    } ATOMIC_BLOCK_END;

    bank = 0;
    while(bank_mask)
    {
        if(bank_mask & 0x01)
        {
            piodb.callback(bank);
        }
        bank_mask >>= 1;
        bank++;
    }
}

