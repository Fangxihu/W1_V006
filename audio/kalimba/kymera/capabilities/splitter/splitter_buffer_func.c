/****************************************************************************
 * Copyright (c) 2015 - 2018 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \file  splitter.c
 * \ingroup  capabilities
 *
 *  Splitter capability.
 *
 */
/****************************************************************************
Include Files
*/
#include "splitter_private.h"

#ifdef INSTALL_CBUFFER_EX

/****************************************************************************
Private Function Definitions
*/
unsigned pack_audio(tCbuffer * dst, tCbuffer *src, unsigned num_samples)
{
    unsigned num_octets, temp_data_space;

    temp_data_space = cbuffer_calc_amount_data_in_words(src);
    if (temp_data_space < num_samples)
    {
        num_samples = temp_data_space;
    }

    /* Because the audio is packed a sample only occupies 2 octets.*/
    num_octets = num_samples<<1;

    temp_data_space = cbuffer_calc_amount_space_ex(dst);
    if (temp_data_space < num_octets)
    {
        num_octets = temp_data_space;
    }

    /* Call splitters packing function. */
    cbuffer_copy_audio_shift_to_packed(dst, src, num_octets>>1);

    /*we still need to return samples*/
    return num_octets>>1;
}


unsigned un_pack_audio(tCbuffer * dst, tCbuffer *src, unsigned num_samples)
{
    unsigned num_octets, temp_data_space;


    num_octets = num_samples<<1;

    PL_ASSERT(cbuffer_get_usable_octets(src) == 4);
    temp_data_space = cbuffer_calc_amount_data_ex(src);
    if (temp_data_space < num_octets)
    {
        num_octets = temp_data_space;
    }

    /* Convert from samples to octets. */
    temp_data_space = cbuffer_calc_amount_space_in_words(dst)<<1;
    if (temp_data_space < num_octets)
    {
        num_octets = temp_data_space;
    }

    cbuffer_copy_packed_to_audio_shift(dst, src, num_octets>>1);

    /*we still need to return samples*/
    return num_octets>>1;
}

void advance_packed(tCbuffer *cbuffer, unsigned num_samples)
{

    unsigned num_octets;
    num_octets = num_samples<<1;
    cbuffer_advance_read_ptr_ex(cbuffer, num_octets);
}

/**
 * Function returns the number of samples which can be copied to the internal buffer.
 */
unsigned packed_space(tCbuffer *cbuffer)
{
    return cbuffer_calc_amount_space_ex(cbuffer) >>1;
}
#endif
