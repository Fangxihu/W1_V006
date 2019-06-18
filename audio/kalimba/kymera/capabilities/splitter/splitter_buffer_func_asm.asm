// *****************************************************************************
// Copyright (c) 2016 - 2017 Qualcomm Technologies International, Ltd.
// *****************************************************************************
#ifdef INSTALL_CBUFFER_EX
#include "cbuffer_asm.h"
#include "io_defs.asm"
#include "patch/patch_asm_macros.h"

// --------- copy_unpacked_to_packed ----------
//void cbuffer_copy_audio_shift_to_packed(tCbuffer * dst, tCbuffer *src, unsigned num_words); -----------------------------------------------------
// in r0 destination cbuffer address
// in r1 source cbuffer address
// in r2 number of words to copy
// trashed r3, r10, B0, B4
.MODULE $M.cbuffer_copy_audio_shift_to_packed;
    .CODESEGMENT BUFFER_PM;
$_cbuffer_copy_audio_shift_to_packed:
    // save the input paramerters for later
    pushm <FP(=SP), r0, r1, r2, r4, r5, r6, rLink>;
    pushm <I0, I4, M0, L0, L4>;

    // load a few constants
    M0 = ADDR_PER_WORD;
    r4 = -16;
    // get dest buffer true write address and size
    call $cbuffer.get_write_address_ex;
    I4 = r0;
    L4 = r2;
    r6 = r1;        // save write octet offset
    push r3;
    pop B4;
    r5 = r3;        // prepare for checking for in-place

    // get src buffer read address and size
    r0 = M[FP + 2*ADDR_PER_WORD];                // cbuffer_src
    // get the read address and size
    call $cbuffer.get_read_address_ex;
    I0 = r0;
    L0 = r2;

    // check if cbuffer base addresses are the same
    Null = r5 - r3;
    if NZ jump not_in_place_copy;
        // only advance pointers, no copy for in-place
        r5 = M[FP + 3*ADDR_PER_WORD];            // copy amount
        r0 = M[FP + 1*ADDR_PER_WORD];            // cbuffer_dest
        r1 = r5;
        call $cbuffer.advance_write_ptr_ex;

        r0 = M[FP + 2*ADDR_PER_WORD];            // cbuffer_src
        r1 = r5;
        call $cbuffer.advance_read_ptr_ex;

        jump cp_pop_and_exit;

    not_in_place_copy:
    
    // init base for src ahead of doloop
    push r3;
    pop B0;
    // get the amount of data to copy and set up a few masks
    r5 = M[FP + 3*ADDR_PER_WORD];
    r3 = 0xFFFF0000;
    r2 = 0x0000FFFF;

    Null = r6;
    if Z jump dst_word_aligned;
        // the previous write left the offset mid-word;
        // we first get word aligned to set up an efficient packing loop
        r1 = M[I4, 0];
        r1 = r1 AND r2, r0 = M[I0, M0];
        r0 = r0 AND r3;
        r1 = r1 OR r0;
        M[I4, M0] = r1;
        r5 = r5 - 1;
    dst_word_aligned:
    
    // loop count is half the number of words to pack
    r10 = r5 ASHIFT -1;
    r6 = 0;

    do copy_loop;
        r0 = M[I0, M0];
        r0 = r0 LSHIFT r4, r1 = M[I0, M0];
        r1 = r1 AND r3;
        r1 = r1 OR r0;
        M[I4,  M0] = r1;
    copy_loop:
    
    // if any leftover sample pack it here
    Null = r5 AND 0x1;
    if Z jump upd_ptrs;
        r1 = M[I4, 0];
        r0 = M[I0, M0];
        r1 = r1 AND r3;
        r0 = r0 LSHIFT r4;
        r1 = r1 OR r0;
        M[I4, 0] = r1;
        r6 = 2;

    upd_ptrs:
    // Update the write address
    r0 = M[FP + 1*ADDR_PER_WORD];                // cbuffer_dest
    r2 = r6;
    r1 = I4;
    call $cbuffer.set_write_address_ex;

    // Update the read address
    r0 = M[FP + 2*ADDR_PER_WORD];                // cbuffer_src
    r1 = I0;
    r2 = 0;
    call $cbuffer.set_read_address_ex;

    cp_pop_and_exit:
    // Restore index, modify & length registers
    popm <I0, I4, M0, L0, L4>;
    popm <FP, r0, r1, r2, r4, r5, r6, rLink>;
    rts;

.ENDMODULE;



// --------- copy_packed_to_unpacked ----------
//void cbuffer_copy_packed_to_audio_shift(tCbuffer * dst, tCbuffer *src, unsigned num_words); -----------------------------------------------------
// in r0 destination cbuffer address
// in r1 source cbuffer address
// in r2 number of words to copy
// trashed r3, r10, B0, B4
.MODULE $M.cbuffer_copy_packed_to_audio_shift;
    .CODESEGMENT BUFFER_PM;
$_cbuffer_copy_packed_to_audio_shift:
    // save the input paramerters for later
    pushm <FP(=SP), r0, r1, r2, r4, r5, r6, rLink>;
    pushm <I0, I4, M0, L0, L4>;

    // load a few constants
    M0 = ADDR_PER_WORD;
    // get dest buffer true write address and size
    call $cbuffer.get_write_address_ex;
    I0 = r0;
    L0 = r2;
    push r3;
    pop B0;
    r5 = r3;        // prepare for checking for in-place

    // get src buffer read address and size
    r0 = M[FP + 2*ADDR_PER_WORD];                // cbuffer_src
    // get the read address and size
    call $cbuffer.get_read_address_ex;
    I4 = r0;
    r6 = r1;        // save read octet offset
    L4 = r2;

    // check if cbuffer base addresses are the same
    Null = r5 - r3;
    if NZ jump not_in_place_copy;
        // only advance pointers, no copy for in-place
        r5 = M[FP + 3*ADDR_PER_WORD];            // copy amount
        r0 = M[FP + 1*ADDR_PER_WORD];            // cbuffer_dest
        r1 = r5;
        call $cbuffer.advance_write_ptr_ex;

        r0 = M[FP + 2*ADDR_PER_WORD];            // cbuffer_src
        r1 = r5;
        call $cbuffer.advance_read_ptr_ex;

        jump cp_pop_and_exit;

    not_in_place_copy:
    // init base for src ahead of doloop
    push r3;
    pop B4;

    // get the amount of data to copy and set up a few masks
    r5 = M[FP + 3*ADDR_PER_WORD];
    r3 = 0xFFFF0000;

    Null = r6;
    if Z jump src_word_aligned;
        // the previous read left the offset mid-word;
        // we first get word aligned to set up an efficient packing loop
        r0 = M[I4, M0];
        r0 = r0 AND r3;
        r5 = r5 - 1, M[I0, M0] = r0;
    src_word_aligned:

    // loop count is half the number of words to pack
    r10 = r5 ASHIFT -1;
    r6 = 0;

    do copy_loop;
        r2 = M[I4, M0];
        r0 = r2 LSHIFT 16;
        r1 = r2 AND r3, M[I0,  M0] = r0;
        M[I0,  M0] = r1;
    copy_loop:

    // if any leftover sample unpack it here
    Null = r5 AND 0x1;
    if Z jump upd_ptrs;
        r0 = M[I4, 0];
        r0 = r0 LSHIFT 16;
        M[I0, M0] = r0;
        r6 = 2;

    upd_ptrs:
    // Update the write address
    r0 = M[FP + 1*ADDR_PER_WORD];                // cbuffer_dest
    r2 = 0;
    r1 = I0;
    call $cbuffer.set_write_address_ex;

    // Update the read address
    r0 = M[FP + 2*ADDR_PER_WORD];                // cbuffer_src
    r1 = I4;
    r2 = r6;
    call $cbuffer.set_read_address_ex;

    cp_pop_and_exit:
    // Restore index, modify & length registers
    popm <I0, I4, M0, L0, L4>;
    popm <FP, r0, r1, r2, r4, r5, r6, rLink>;
    rts;

.ENDMODULE;

#endif /* INSTALL_CBUFFER_EX */
