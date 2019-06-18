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
#include "splitter.h"
#include "splitter_private.h"

/****************************************************************************
Private Constant Definitions
*/

/****************************************************************************
Public Constant Declarations
*/
/** The splitter capability function handler table */
const handler_lookup_struct splitter_handler_table =
{
    splitter_create,           /* OPCMD_CREATE */
    splitter_destroy,          /* OPCMD_DESTROY */
    splitter_start,            /* OPCMD_START */
    base_op_stop,              /* OPCMD_STOP */
    base_op_reset,             /* OPCMD_RESET */
    splitter_connect,          /* OPCMD_CONNECT */
    splitter_disconnect,       /* OPCMD_DISCONNECT */
    splitter_buffer_details,   /* OPCMD_BUFFER_DETAILS */
    splitter_get_data_format,  /* OPCMD_DATA_FORMAT */
    splitter_get_sched_info    /* OPCMD_GET_SCHED_INFO */
};

/** The splitter capability operator message handler table */
const opmsg_handler_lookup_table_entry splitter_opmsg_handler_table[] =
{
    {OPMSG_COMMON_ID_GET_CAPABILITY_VERSION,              base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_SET_BUFFER_SIZE,                     splitter_set_buffer_size},
    {OPMSG_SPLITTER_ID_SET_RUNNING_STREAMS,               splitter_set_running_streams},
    {OPMSG_SPLITTER_ID_ACTIVATE_STREAMS,                  splitter_activate_streams},
    {OPMSG_SPLITTER_ID_ACTIVATE_STREAMS_AFTER_TIMESTAMP,  splitter_activate_streams_after_timestamp},
    {OPMSG_SPLITTER_ID_DEACTIVATE_STREAMS,                splitter_deactivate_streams},
    {OPMSG_SPLITTER_ID_BUFFER_STREAMS,                    splitter_buffer_streams},
    {OPMSG_SPLITTER_ID_SET_DATA_FORMAT,                   splitter_set_data_format},
    {OPMSG_SPLITTER_ID_SET_WORKING_MODE,                  splitter_set_working_mode},
    {OPMSG_SPLITTER_ID_SET_BUFFERING_LOCATION,            splitter_set_location},
    {OPMSG_SPLITTER_ID_SET_PACKING,                       splitter_set_packing},
    {OPMSG_SPLITTER_ID_SET_REFRAMING,                     splitter_set_reframing},
    {OPMSG_COMMON_SET_SAMPLE_RATE,                        splitter_set_sample_rate},
    {0, NULL}
};

/** splitter capability data */
const CAPABILITY_DATA splitter_cap_data =
{
    SPLITTER_CAP_ID,                                       /* Capability ID */
    0, 3,                                                  /* Version information - hi and lo parts */
    SPLITTER_MAX_INPUTS, SPLITTER_MAX_OUTPUTS,             /* Max number of sinks/inputs and sources/outputs */
    &splitter_handler_table,                               /* Pointer to message handler function table */
    splitter_opmsg_handler_table,                          /* Pointer to operator message handler function table */
    splitter_process_data,                                 /* Pointer to data processing function */
    0,                                                     /* Reserved */
    sizeof(SPLITTER_OP_DATA)
};
#if !defined(CAPABILITY_DOWNLOAD_BUILD)
MAP_INSTANCE_DATA(CAP_ID_SPLITTER, SPLITTER_OP_DATA)
#else
MAP_INSTANCE_DATA(CAP_ID_DOWNLOAD_SPLITTER, SPLITTER_OP_DATA)
#endif /* CAPABILITY_DOWNLOAD_BUILD */

#ifdef INSTALL_METADATA
/** Cbuffer function to store audio in an unpacked form */
static const cbuffer_functions_t audio_copy_functions = {
    cbuffer_calc_amount_space_in_words,
    cbuffer_calc_amount_space_in_words,
    cbuffer_calc_amount_data_in_words,
    cbuffer_copy, /* The audio stays as it is. */
    cbuffer_copy,
    cbuffer_advance_read_ptr,
    4
};

/** Cbuffer function to store audio in a packed form.  */
static const cbuffer_functions_t audio_pack_functions = {
    cbuffer_calc_amount_space_in_words,
    packed_space,
    cbuffer_calc_amount_data_in_words,
    pack_audio, /* Only the MSB 16 bit from the audio is packed to the internal buffer. */
    un_pack_audio, /* The MSB 16 bit of the audio is unpacked to the output buffer. */
    advance_packed,
    4,
};

/** Cbuffer functions to handle encoded data. This works for both unpacked
 * and packed formats as the _ex function uses the usable octets. */
static const cbuffer_functions_t data_pack_functions = {
    cbuffer_calc_amount_space_ex,
    cbuffer_calc_amount_space_ex,
    cbuffer_calc_amount_data_ex,
    cbuffer_copy_ex, /* cbuffer_copy_ex will pack the data when needed because it know about the usable octets.*/
    cbuffer_copy_ex, /* same applies for unpacking. */
    cbuffer_advance_read_ptr_ex,
    1
};
#endif /* INSTALL_METADATA */

/****************************************************************************
Private Function Declaration
*/
static void splitter_timer_task(void *timer_data);
static void splitter_process_data_clone(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched);

#ifdef INSTALL_METADATA
static void splitter_process_data_buffering(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched);
static void splitter_metadata_copy(SPLITTER_OP_DATA *splitter, unsigned* data_to_copy, unsigned data_size);
static unsigned amount_to_pack(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer);
static unsigned pack_to_internal(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned data_to_pack);
static bool move_data_from_internal_to_output(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer);
static SPLITTER_OUTPUT_STATE get_output_state(SPLITTER_OP_DATA *splitter, unsigned index);
#endif

/****************************************************************************
Private Function Definitions
*/
/**
 * \brief timer handler to allow internal self-kick
 * This prevents a stall when buffers fill up
 *
 * \param timer_data Pointer to the operator instance data.
 */
static void splitter_timer_task(void *timer_data)
{
    OPERATOR_DATA *op_data = (OPERATOR_DATA*) timer_data;
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);

    splitter->self_kick_timer = TIMER_ID_INVALID;
    /* Raise a bg int to process */
    opmgr_kick_operator(op_data);
}

/**
 * \brief Updates all buffer pointers so that outputs can read any new data and
 * inputs can see any new space created from down stream reads.
 *
 * \param op_data Pointer to the operator instance data.
 */
static void splitter_process_data_clone(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned i, min_new_data, min_new_space;
    int *new_output_write_addr;
    int *new_input_read_addr;

    SPLITTER_CHANNEL_STRUC *channel = splitter->channel_list;
    patch_fn(splitter_process_data);

    timer_cancel_event_atomic(&splitter->self_kick_timer);

    /* This code is very naughty and reaches into the cbuffer structures. It can
     * be done safely because they have to be local, and it's lightning fast as
     * a result. Cbuffer API is subverted because it isn't designed for this.
     */
    min_new_data = min_new_space = UINT_MAX;
    /* Iterate through the list of all active channels. */
    while (NULL != channel)
    {
        int new_data, new_space;
        tCbuffer *out = NULL; /* Initialise to keep the compiler happy. */
        tCbuffer *in = channel->input_buffer;

        for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
        {
            if (channel->output_state[i])
            {
                out = channel->output_buffer[i];

                /* Find out minimum available space. */
                new_space = (char *)out->read_ptr - (char *)in->read_ptr;
                if (new_space < 0)
                {
                    new_space += in->size;
                }

                if (new_space < min_new_space)
                {
                    min_new_space = new_space;
                }
            }
        }

        /* Find out minimum available data. */
        new_data =  (char *)in->write_ptr -  (char *)out->write_ptr;
        if (new_data < 0)
        {
            new_data += out->size;
        }

        if (new_data < min_new_data)
        {
            min_new_data = new_data;
        }

        channel = channel->next;
    }

    /* Typically only one of  min_new_space OR min_new_data are non zero on a
     * given kick so we separate the looping out to reduce the amount of work done.
     */
    if (min_new_space > 0)
    {
        channel = splitter->channel_list;
        while (NULL != channel)
        {
            tCbuffer *in = channel->input_buffer;
            new_input_read_addr = (int *)((char *)in->read_ptr + min_new_space);
            if (new_input_read_addr >= (int *)((char *)in->base_addr + in->size))
            {
                new_input_read_addr = (int *)((char *)new_input_read_addr - in->size);
            }
            in->read_ptr = new_input_read_addr;
            channel = channel->next;
        }
        /* N.B. Because the splitter runs in place and is designed to be
         * cascaded, it is necessary to kick back whenever data is consumed so
         * that the input buffer pointers of a splitter that proceeds this one
         * are updated. */
        touched->sinks = splitter->touched_sinks;
    }

    if (min_new_data > 0)
    {
#ifdef INSTALL_METADATA
        unsigned octets_moved;
        tCbuffer *metadata_ip_buffer;
        tCbuffer *metadata_op_buffer;

        metadata_ip_buffer = get_metadata_buffer(splitter, TRUE, 0);
        metadata_op_buffer = get_metadata_buffer(splitter, FALSE, 0);


        if (buff_has_metadata(metadata_ip_buffer))
        {
            /* Transport all the available metadata. The available tags will be limited by
             * the cbuffer available data. This will eliminate difference between
             * octet and _ex buffers. */
            octets_moved =  buff_metadata_available_octets(metadata_ip_buffer);
        }
        else
        {
            /* There is no metadata for the input buffer, but still update the output
             * buffer metadata write index. Due to the lack of the metadata (so no info on
             * the usable octets) min_new_data is a fair approximation.  */
            octets_moved = min_new_data;
        }

        /* Transport any metadata to the output. */
        metadata_strict_transport(metadata_ip_buffer,
                                    metadata_op_buffer,
                                    octets_moved);
#endif /* INSTALL_METADATA */
        channel = splitter->channel_list;
        while (NULL != channel)
        {
            for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
            {
                if (channel->output_state[i])
                {
                    tCbuffer *out = channel->output_buffer[i];
                    new_output_write_addr = (int *)((char *)out->write_ptr + min_new_data);
                    if (new_output_write_addr >= (int *)((char *)out->base_addr + out->size))
                    {
                        new_output_write_addr = (int *)((char *)new_output_write_addr - out->size);
                    }
                    out->write_ptr = new_output_write_addr;
                }
#ifdef INSTALL_METADATA
                else
                {
                    /* If the output is disabled and connected metadata is created
                     * for the output and not consumed by anyone. Delete those metadata.
                     * NOTE: It would be better not to create it in the first place, but
                     * that involves the splitter to take care of the metadata transport. */
                    tCbuffer *out = channel->output_buffer[i];
                    if (out)
                    {
                        metadata_tag *ret_mtag;
                        unsigned b4idx, afteridx;
                        ret_mtag = buff_metadata_remove(out, octets_moved, &b4idx, &afteridx);
                        buff_metadata_tag_list_delete(ret_mtag);
                    }
                }
#endif /* INSTALL_METADATA */
            }
            channel = channel->next;
        }
        touched->sources = splitter->touched_sources;
    }

    timer_schedule_event_in_atomic(SPLITTER_SELF_KICK_RATIO * stream_if_get_system_kick_period(),
        splitter_timer_task, (void*)op_data, &splitter->self_kick_timer);
}

#ifdef INSTALL_METADATA
/**
 * Copies metadata to the destination without removing it from the source.
 */
static metadata_tag *metadata_copy_without_changing_src(tCbuffer *src, tCbuffer *dest, unsigned data_copied_octets)
{
    unsigned b4idx, afteridx;
    unsigned tag_length;
    metadata_tag *tag_list;
    metadata_tag *new_tag;
    metadata_tag *tail_tag;
    metadata_tag *head_tag;

    /* Check if we have enough data/space. */
    PL_ASSERT(buff_metadata_available_octets(src) >= data_copied_octets);
    PL_ASSERT(buff_metadata_available_space(dest) >= data_copied_octets);

    /* Transport the metadata to the output buffer. */
    tag_list = buff_metadata_peek_ex(src, &b4idx);

    SPLITTER_MSG5("Splitter: buff_metadata_available_space(out) %d data_copied %d octets"
            "\n tag_list 0x%08x  tag_list->next 0x%08x b4idx %d octets " ,
            buff_metadata_available_space(dest), data_copied_octets, tag_list, tag_list->next, b4idx);

    /* Check the before index validity */
    if (tag_list != NULL)
    {
        PL_ASSERT(b4idx < src->metadata->buffer_size);
    }

    /* check if we will consume one tag at all?
     * Note: When tag_list is NULL b4idx is not set by buff_metadata_peek_ex */
    if ((tag_list == NULL) || (data_copied_octets < b4idx))
    {
        /* continuing to consume the previous tag. */
        buff_metadata_append(dest, NULL, 0, data_copied_octets);
        return tag_list;
    }

    /* Decrement the data before the first tag. */
    data_copied_octets -= b4idx;

    tail_tag = NULL;
    head_tag = NULL;
    afteridx = 0;



    while((tag_list != NULL) && (data_copied_octets != 0))
    {

        SPLITTER_MSG2("Splitter tag copy \n tag_list->index 0x%08x \n tag_list->length 0x%08x " ,
                tag_list->index, tag_list->length );

        tag_length = tag_list->length;
        if (data_copied_octets <= tag_length)
        {
            afteridx = data_copied_octets;
            data_copied_octets = 0;
        }
        else
        {
            data_copied_octets -= tag_length;
        }
        /* Create a new tag. */
        new_tag = buff_metadata_copy_tag(tag_list);
        /* Make sure the next pointer is NULL. */
        new_tag->next = NULL;
        /* The new tag will be the head if the list is not created yet */
        if (head_tag == NULL)
        {
            head_tag = new_tag;
        }
        /* Add the tag to the end of the tag list. */
        if (tail_tag != NULL)
        {
            tail_tag->next = new_tag;
        }
        tail_tag = new_tag;

        tag_list = tag_list->next;
    }


    buff_metadata_append(dest, head_tag, b4idx, afteridx);
    return tag_list;
}

/**
 * Copies metadata from the internal buffer to the output buffer.
 */
static void splitter_metadata_copy(SPLITTER_OP_DATA *splitter, unsigned* data_to_copy, unsigned data_size)
{
    unsigned i, output_prev_rd_indexes, before_prev_rd_indexes;
    metadata_tag *out_head_tag, *before_head_tag;
    SPLITTER_CHANNEL_STRUC *metadata_channel;
    tCbuffer *internal;

    metadata_channel = get_metadata_channel(splitter);
    PL_ASSERT(metadata_channel != NULL);

    /* the internal buffer is the metadata source. */
    internal = metadata_channel->internal.buffer;
    PL_ASSERT(internal != NULL);


    /* Save the index value. */
    before_prev_rd_indexes = internal->metadata->prev_rd_index;
    before_head_tag = internal->metadata->tags.head;

    for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
    {

        if ((data_to_copy[i] != 0) && (metadata_channel->output_state[i] == ACTIVE))
        {
            /* Set up the internal buffer read pointers!*/
            output_prev_rd_indexes = metadata_channel->internal.prev_rd_indexes[i];
            internal->metadata->prev_rd_index = output_prev_rd_indexes;

            out_head_tag = metadata_channel->internal.head_tag[i];
            internal->metadata->tags.head = out_head_tag;

            SPLITTER_MSG4("Splitter: output index %d"
                            "\n read_index before 0x%08x\n head tag 0x%08x\n head index 0x%08x",
                            i, output_prev_rd_indexes, out_head_tag, out_head_tag->index);

            out_head_tag = metadata_copy_without_changing_src(
                    internal, metadata_channel->output_buffer[i], data_to_copy[i]*data_size);

            SPLITTER_MSG1("Splitter will continue from tag 0x%08x",
                    out_head_tag);
            metadata_channel->internal.head_tag[i] = out_head_tag;

            /* Calculate the read index of the output metadata. */
            output_prev_rd_indexes += data_to_copy[i] * data_size;
            if (output_prev_rd_indexes >= internal->metadata->buffer_size)
            {
                output_prev_rd_indexes -= internal->metadata->buffer_size;
            }
            /* Update the read index for the output.*/
            metadata_channel->internal.prev_rd_indexes[i] = output_prev_rd_indexes;
        }
    }

    /* restore the index value. */
    internal->metadata->prev_rd_index = before_prev_rd_indexes;
    internal->metadata->tags.head = before_head_tag;

}

/**
 * Helper function to calculate the amount to pack/copy to the internal buffer.
 */
static unsigned amount_to_pack(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer)
{
    SPLITTER_CHANNEL_STRUC *channel;
    unsigned available_data;
    unsigned new_data;
    unsigned internal_space;
    tCbuffer *in;
    tCbuffer *internal;

    available_data = UINT_MAX;
    channel = splitter->channel_list;
    /* We must have at least one channel. */
    PL_ASSERT(channel != NULL);
    internal = channel->internal.buffer;
    /* Calculate the internal space. It is enough to do it for one channel
     * because the internal buffer is synced. Also there is no need to check
     * for the metadata as we keep this buffer synced.*/
    internal_space = cbuffer->packed_space(internal);

    while ((channel != NULL) && (available_data != 0))
    {
        in = channel->input_buffer;
        internal = channel->internal.buffer;

        /* Calculate the minimum available data.*/
        new_data = cbuffer->data(in);
        if (new_data < available_data)
        {
            available_data = new_data;
        }
        /* Calculate the minimum available metadata.*/
        if (buff_has_metadata(in))
        {
            new_data =  buff_metadata_available_octets(in)/cbuffer->data_size;
            if (new_data < available_data)
            {
                SPLITTER_ERRORMSG2("Splitter: Input limited by the available metadata!"
                        "\n limiting %d to %d", available_data, new_data);
                available_data = new_data;
            }
        }
        channel = channel->next;
    }

    /* Limit the data to the available space.*/
    if (internal_space < available_data)
    {
        if (splitter->internal_buffer_full == FALSE)
        {
            splitter->internal_buffer_full = TRUE;
            SPLITTER_ERRORMSG("Splitter Internal buffer full! ");
        }
        available_data = internal_space;
    }
    else
    {
        splitter->internal_buffer_full = FALSE;
    }

    return available_data;
}

/**
 * Transports the metadata form the input to the internal buffer.
 * Very similar to metadata_strict_transport, with the exception that this function sets
 * the new metadata tag head for the channel for tracking reasons.
 */
static void splitter_metadata_transport_to_internal(SPLITTER_OP_DATA *splitter,  unsigned trans_octets)
{
#ifdef METADATA_DEBUG_TRANSPORT
    unsigned return_addr = pl_get_return_addr();
#endif /* METADATA_DEBUG_TRANSPORT */
    tCbuffer *src;
    tCbuffer *dst;
    metadata_tag *ret_mtag;
    unsigned b4idx, afteridx;
    SPLITTER_CHANNEL_STRUC *metadata_channel;
    unsigned i;

    metadata_channel = get_metadata_channel(splitter);
    PL_ASSERT(metadata_channel != NULL);

    src = metadata_channel->input_buffer;
    dst = metadata_channel->internal.buffer;

    if (trans_octets == 0)
    {
        SPLITTER_MSG("splitter_metadata_transport_to_internal: ignoring zero transfer");
        return;
    }

    if (src != NULL)
    {
        /* transport metadata, first (attempt to) consume tag associated with src */
#ifdef METADATA_DEBUG_TRANSPORT
        ret_mtag = buff_metadata_remove_dbg(src, trans_octets, &b4idx,
                                            &afteridx, return_addr);
#else /* METADATA_DEBUG_TRANSPORT */
        ret_mtag = buff_metadata_remove(src, trans_octets, &b4idx, &afteridx);
#endif /* METADATA_DEBUG_TRANSPORT */
    }
    else
    {
        b4idx = 0;
        afteridx = trans_octets;
        ret_mtag = NULL;
    }

    if (splitter->reframe_enabled)
    {
        metadata_tag *mtag_ip, *mtag;
        unsigned new_ttp, base_ttp = 0, sample_offset = 0;
        ttp_status status;

        /* Find the first timestamped tag */
        mtag_ip = ret_mtag;
        while (mtag_ip != NULL)
        {
            if (IS_TIMESTAMPED_TAG(mtag_ip))
            {
                break;
            }
            else
            {
                SPLITTER_ERRORMSG(
                    "splitter_metadata_transport_to_internal: Tag not timestamped!"
                        "\n Splitter cannot reframe timestamped tags.");
                b4idx += mtag_ip->length;
                mtag_ip = mtag_ip->next;
            }
        }

        if ((b4idx == 0) && (mtag_ip != NULL))
        {
            /* If the old tag is already at the start of the encoded frame,
             * Just use its timestamp directly
             */
            base_ttp = mtag_ip->timestamp;
            sample_offset = 0;
        }
        else
        {
            /* Otherwise, use the previously-stashed timestamp.
             * There had better be one ! */
            if (splitter->reframe_data.last_tag_samples != LAST_TAG_SAMPLES_INVALID)
            {
                base_ttp = splitter->reframe_data.last_tag_timestamp;
                sample_offset = splitter->reframe_data.last_tag_samples;
            }
        }

        status.sp_adjustment = splitter->reframe_data.last_tag_spa;
        status.err_offset_id = splitter->reframe_data.last_tag_err_offset_id;
        status.stream_restart = (METADATA_STREAM_START(mtag_ip) != 0);

        /* Create a new tag for the output */
        mtag = buff_metadata_new_tag();

        if (mtag != NULL)
        {
            /* create a tag which covers the newly generated output. */
            mtag->length = trans_octets;

            if (splitter->reframe_data.last_tag_samples != LAST_TAG_SAMPLES_INVALID)
            {
                /* Calculate new TTP from incoming data and sample offset */
                new_ttp = ttp_get_next_timestamp(base_ttp, sample_offset,
                        splitter->reframe_data.sample_rate, splitter->reframe_data.last_tag_spa);
                status.ttp = new_ttp;
                ttp_utils_populate_tag(mtag, &status);
                status.stream_restart = FALSE;
            }
            else
            {
                SPLITTER_ERRORMSG(
                    "splitter_metadata_transport_to_internal: Last tag samples invalid");
            }
        }
        else
        {
            SPLITTER_ERRORMSG(
                "splitter_metadata_transport_to_internal: Failed to allocate tag");
        }

        buff_metadata_append(dst, mtag, 0, trans_octets);

        /* Free all the incoming tags */
        buff_metadata_tag_list_delete(ret_mtag);

        /* Set the created tag as potential new head. */
        ret_mtag = mtag;
    }
    else
    {
        if (dst != NULL)
        {
            /* Even if the src is a NULL buffer we append to dst. It makes no sense
             * for the current configuration. However if another connection is made
             * later to the src which does support metadata the dst metadata write
             * pointer needs to be at the right offset. */
#ifdef METADATA_DEBUG_TRANSPORT
            buff_metadata_append_dbg(dst, ret_mtag, b4idx, afteridx, return_addr);
#else /* METADATA_DEBUG_TRANSPORT */
            buff_metadata_append(dst, ret_mtag, b4idx, afteridx);
#endif /* METADATA_DEBUG_TRANSPORT */
        }
        else
        {
            buff_metadata_tag_list_delete(ret_mtag);
        }
    }

    /* Check if ret_mtag is a new head for any of the output streams. */
    if (ret_mtag != NULL)
    {
        for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
        {
            if ((metadata_channel->output_state[i] == ACTIVE) || (metadata_channel->output_state[i] == HOLD))
            {
                if (metadata_channel->internal.head_tag[i] == NULL)
                {

                    SPLITTER_MSG2("Splitter: Setting head for out % head 0x%08x",
                            i, ret_mtag);
                    metadata_channel->internal.head_tag[i] = ret_mtag;
                }
            }
        }
    }
}

/**
 * Helper function which packs or copies data to the internal buffer.
 */
static unsigned pack_to_internal(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned data_to_pack)
{
    SPLITTER_CHANNEL_STRUC *channel;
    channel = splitter->channel_list;

    SPLITTER_MSG1("Splitter Pack/copy to internal buffer:"
            "data_pack = %d ",
            data_to_pack);

    while (channel != NULL)
    {
        tCbuffer *in = channel->input_buffer;
        tCbuffer *internal = channel->internal.buffer;
        unsigned packed;

        packed = cbuffer->pack(internal, in, data_to_pack);

        /* the min available space of the input data was already calculated,
         * so the limiting must come from the internal buffer.*/
        if (packed < data_to_pack)
        {
            SPLITTER_ERRORMSG1("Splitter: Internal buffer full! Only packed/copied %d octets!", packed * cbuffer->data_size);
            /* This should never happen!! Handle the case anyways.*/
            data_to_pack = packed;
        }

        channel = channel->next;
    }
    /* Transport metadata to the internal buffer. */
    splitter_metadata_transport_to_internal(
            splitter,
            data_to_pack * cbuffer->data_size
    );

    return data_to_pack;
}

/**
 * Helper function to get the amount of data per output streams.
 */
static void internal_buffer_data(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned *data)
{
    SPLITTER_CHANNEL_STRUC *channel;
    int before_prev_rd_indexes;
    tCbuffer *internal;
    unsigned i;

    channel = get_metadata_channel(splitter);
    internal = channel->internal.buffer;
    /* Save the read pointer */
    before_prev_rd_indexes = internal->metadata->prev_rd_index;
    for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
    {

        if (channel->output_state[i] == INACTIVE)
        {
            /* Inactive channels discards data instantaneously.*/
            data[i] = 0;
        }
        else if ((channel->output_state[i] == ACTIVE) || (channel->output_state[i] == HOLD))
        {
            /* Set up the internal buffer metadata read indexes!*/
            internal->metadata->prev_rd_index = channel->internal.prev_rd_indexes[i];

            /* The internal buffer is always synchronised with the metadata
             * so there is no need to check both data and metadata. */
            data[i] = buff_metadata_available_octets(internal) / cbuffer->data_size;
        }

    }/* END of output for loop. */
    internal->metadata->prev_rd_index = before_prev_rd_indexes;
}

/**
 * Helper function to limit the amount of data per output streams which will
 * be unpacked/copied to the output buffers.
 */
static void limit_data_to_move(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned *data)
{
    SPLITTER_CHANNEL_STRUC *channel;
    tCbuffer *internal;
    int* before_read_pointer;
    int before_prev_rd_indexes;
    unsigned i;

    /* Limit the data based on the output space. */
    channel = splitter->channel_list;
    while (channel != NULL)
    {
        internal = channel->internal.buffer;
        /* Save the read pointer */
        before_read_pointer = internal->read_ptr;
        before_prev_rd_indexes = internal->metadata->prev_rd_index;

        for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
        {
            /* Inactive and hold output streams need no limiting. */
            if (channel->output_state[i] == ACTIVE)
            {
                unsigned space;
                tCbuffer *output = channel->output_buffer[i];
                /* Set up the internal buffer read pointers! */
                internal->read_ptr = channel->internal.read_pointers[i];

                space = cbuffer->unpacked_space(output);
                data[i] = MIN(data[i], space);
                /* Now limit to the available metadata. */
                space = buff_metadata_available_space(output) / cbuffer->data_size;
                data[i] = MIN(data[i], space);
            }
            else if (channel->output_state[i] == HOLD)
            {
                unsigned space;
                /* Set up the internal buffer metadata read indexes!*/
                internal->metadata->prev_rd_index = channel->internal.prev_rd_indexes[i];

                /* discard enough data to hold a frame*/
                space = buff_metadata_available_space(internal) / cbuffer->data_size;
                if (space < splitter->frame_size + 3)
                {
                    /* Leave three more words in case we are connected to a SBC decode.*/
                    data[i] = splitter->frame_size - space + 3;
                }
                else
                {
                    data[i] = 0;
                }
            }

        }/* END of output for loop. */
        internal->read_ptr = before_read_pointer;
        internal->metadata->prev_rd_index = before_prev_rd_indexes;

        /* Move to next channel. */
        channel = channel->next;
    }/* END of channel loop. */
}


/**
 * Helper function to copy/unpack from the internal to the output.
 */
static void unpack_data_from_internal_to_output(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned *data)
{
    SPLITTER_CHANNEL_STRUC *channel;
    tCbuffer *internal;
    int* before_read_pointer;
    unsigned i;

    /* copy the metadata to the active output streams */
    splitter_metadata_copy(splitter, data, cbuffer->data_size);

    /* Time to unpack/copy the data from the internal to the output */
    channel = splitter->channel_list;
    while (channel != NULL)
    {
        internal = channel->internal.buffer;

        before_read_pointer = internal->read_ptr;

        for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
        {
            /* Check if anythings needs to be done for the output stream*/
            if ((data[i] != 0) && (channel->output_state[i] == ACTIVE))
            {
                tCbuffer *out = channel->output_buffer[i];
                PL_ASSERT(out != NULL);

                /* Set up the internal buffer read pointers!*/
                internal->read_ptr = channel->internal.read_pointers[i];

                /* Unpack/copy the data to the output. */
                cbuffer->unpack(out, internal, data[i]);

                /* cbuffer->unpack updates the read pointer for the internal buffer.
                 * Update the read pointer/index for the output.*/
                channel->internal.read_pointers[i] = internal->read_ptr;
            }

        }/* END of output for loop. */

        /* Restore the read pointer of the internal buffer.*/
        internal->read_ptr = before_read_pointer;

        /* Move to next channel. */
        channel = channel->next;
    }/* END of channel loop. */
}


/**
 * Helper function to remove unnecessary data from the internal buffer.
 */
static void remove_data_from_internal(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned data_to_remove)
{
    SPLITTER_CHANNEL_STRUC *channel;
    unsigned b4idx, afteridx;
    metadata_tag *ret_mtag;

    /* Remove the unnecessary data from the internal buffer.*/
    channel = splitter->channel_list;
    while (channel != NULL)
    {
            /* Discard the data. */
            cbuffer->advance_rd_ptr(channel->internal.buffer, data_to_remove);

        /* Move to next channel. */
        channel = channel->next;
    }/* END of channel loop. */

    /* remove the associated metadata too. */
    ret_mtag = buff_metadata_remove(
            get_internal_metadata_buffer(splitter),
            data_to_remove*cbuffer->data_size, &b4idx, &afteridx);
    buff_metadata_tag_list_delete(ret_mtag);

    /* Update channels on hold */
    if (splitter->hold_streams != OUT_STREAM__0_OFF__1_OFF)
    {
        tCbuffer *internal;
        unsigned i;

        channel = get_metadata_channel(splitter);
        internal = channel->internal.buffer;
        for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
        {
            if (channel->output_state[i] == HOLD)
            {
                /* Set up the internal buffer metadata read indexes!*/
                channel->internal.prev_rd_indexes[i] = internal->metadata->prev_rd_index;
                channel->internal.head_tag[i] = internal->metadata->tags.head;
            }
        }
    }
}

/**
 * Helper function which moves data from the internal buffer to the output buffers
 */
static bool move_data_from_internal_to_output(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer)
{
    unsigned data_to_move[SPLITTER_MAX_OUTPUTS_PER_CHANNEL];
    unsigned internal_data_before[SPLITTER_MAX_OUTPUTS_PER_CHANNEL];
    unsigned data_to_remove;


    /* bail out early if the internal buffer is empty. */
    if (buff_metadata_available_octets(get_internal_metadata_buffer(splitter)) == 0)
    {
        /* Only display the error message once*/
        if (splitter->internal_buffer_empty == FALSE)
        {
            splitter->internal_buffer_empty = TRUE;
            SPLITTER_MSG("Splitter: Internal buffer is empty, cannot copy to output!");
        }
        return FALSE;
    }
    else
    {
        splitter->internal_buffer_empty = FALSE;
    }

    /* get the available data in the internal buffer */
    internal_buffer_data(splitter, cbuffer, internal_data_before);

    /* Limit the amount to copy to the available data */
    data_to_move[0] = internal_data_before[0];
    data_to_move[1] = internal_data_before[1];

    /* Limit the data to move based on the output space. */
    limit_data_to_move(splitter, cbuffer, data_to_move);

    /* Exit early if there is no place to copy. */
    if (MAX(data_to_move[0], data_to_move[1]) == 0 )
    {
        if (splitter->output_buffer_full == FALSE)
        {
            splitter->output_buffer_full = TRUE;
        }
        return FALSE;
    }
    else
    {
        splitter->output_buffer_full = FALSE;
    }

    /* Time to unpack/copy the data from the internal to the output */
    unpack_data_from_internal_to_output(splitter, cbuffer, data_to_move);
    /*
     * data_to_remove =
     *    MAX(internal_data_before[0], internal_data_before[1]) -
     *    MAX(internal_data_after[0], internal_data_after[1]);
     */
    data_to_remove = MAX(internal_data_before[0], internal_data_before[1]);
    data_to_remove = data_to_remove -  (
            MAX(internal_data_before[0] - data_to_move[0], internal_data_before[1] - data_to_move[1])
        );
    SPLITTER_MSG5("Splitter: "
            "\n out 0 internal data before = %d"
            "\n out 0 internal data after = %d"
            "\n out 1 internal data before = %d"
            "\n out 1 internal data after = %d"
            "\n data_to_remove = %d",
            internal_data_before[0],
            internal_data_before[0] - data_to_move[0],
            internal_data_before[1],
            internal_data_before[1] - data_to_move[1],
            data_to_remove);

    if (data_to_remove !=0)
    {
        remove_data_from_internal(splitter, cbuffer, data_to_remove);
    }

    return TRUE;

}

/**
 * Transition from INACTIVE to ACTIVE.
 *
 * Start copying to the output when the next metadata tag arrives.
 */
static bool splitter_transiton_to_active(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned output_index)
{
    SPLITTER_CHANNEL_STRUC *channel;
    tCbuffer *internal;
    metadata_tag *tag_list;
    unsigned b4idx;
    int *read_pointer;

    /* get the metadata channel and the internal buffer. */
    channel = get_metadata_channel(splitter);
    internal = channel->internal.buffer;
    tag_list = buff_metadata_peek_ex(internal, &b4idx);
    if (tag_list == NULL)
    {
        SPLITTER_ERRORMSG("splitter_transiton_to_active: No tags to align to! Not enough data buffered. ");
        return FALSE;
    }
    else
    {
        SPLITTER_MSG3("splitter_transiton_to_active"
                "\n tag_list 0x%08x  tag_list->index 0x%08x b4idx %d octets " ,
                tag_list, tag_list->index, b4idx);
    }


    if (channel->output_state[output_index] == HOLD)
    {
        /* Go trough the metadata and discard unwanted tags.*/
        unsigned data_to_remove, input_data;
        data_to_remove = b4idx;
        input_data = buff_metadata_available_octets(internal);
        /* Go through the metadata. */
        while(tag_list &&
              (input_data != 0) &&
              IS_TIMESTAMPED_TAG(tag_list) &&
              (tag_list->timestamp < splitter->transition_point))
        {
            unsigned tag_data;
            /* tag_legth = tag_list->length / data_size */
            tag_data = MIN(input_data, tag_list->length);

            if (tag_data != 0)
            {
                SPLITTER_MSG1("Splitter: Metadata tag with timestamp 0x%08x will be discarded.", tag_list->timestamp);
                /* Increase the discarded amount by the tag length. */
                data_to_remove += tag_data;
                /* ... decrease the amount of data. */
                input_data -= tag_data;
            }
            tag_list = tag_list->next;

        }
        data_to_remove = data_to_remove / cbuffer->data_size;
        /* Discard the data and metadata.*/
        if (tag_list == NULL)
        {
            SPLITTER_ERRORMSG("Splitter: No more metadata tag at the input buffer. Probably the buffering is too small. ");
            return FALSE;
        }
        else if (input_data == 0)
        {
            SPLITTER_ERRORMSG("Splitter: No data left to search for timestamp. Probably the buffering is too small.");
            return FALSE;
        }
        else if (!IS_TIMESTAMPED_TAG(tag_list))
        {
            SPLITTER_ERRORMSG("Splitter: Non timestamped tag found. Cannot handle this metadata, switching to passthrough.");
            return FALSE;
        }
        else
        {
            SPLITTER_MSG3("Splitter: Tag with ttp 0x%08x is the first valid tag after transition 0x%08x"
                          "\n Total discarded octets %d",
                    tag_list->timestamp, splitter->transition_point, data_to_remove);
        }

        /* This will remove data from all channels. */
        if (data_to_remove !=0)
        {
            remove_data_from_internal(splitter, cbuffer, data_to_remove);
        }
    }
    /* set the metadata read index and head. */
    channel->internal.prev_rd_indexes[output_index] = tag_list->index;
    channel->internal.head_tag[output_index] = tag_list;

    /* set the read pointer for all channels */
    channel = splitter->channel_list;
    while (channel != NULL)
    {
        /* Change state.*/
        channel->output_state[output_index] = ACTIVE;
        internal = channel->internal.buffer;
        /* convert the index to read index.*/
        if (splitter->packing == PACKED)
        {
            read_pointer= (int *)((uintptr_t)internal->base_addr + tag_list->index/2);
        }
        else
        {
            read_pointer = (int *)((uintptr_t)internal->base_addr + tag_list->index);
        }
        channel->internal.read_pointers[output_index] = read_pointer;
        channel = channel->next;
    }

    return TRUE;
}

static bool splitter_finish_copying_last_tag(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned output_index)
{
    SPLITTER_CHANNEL_STRUC *channel;
    tCbuffer *internal;
    metadata_tag *tag_list, *before_head_tag=NULL;
    unsigned b4idx, before_prev_rd_indexes = 0;
    /* This variable is true if the output index is keeping the most amount of data. */
    bool most_data_buffered;

    /* get the metadata channel and the internal buffer. */
    channel = get_metadata_channel(splitter);
    internal = channel->internal.buffer;

    /* Check the read pointers */
    most_data_buffered = (internal->metadata->prev_rd_index == channel->internal.prev_rd_indexes[output_index]);

    if (!most_data_buffered)
    {
        /* Save the index value. */
        before_prev_rd_indexes = internal->metadata->prev_rd_index;
        before_head_tag = internal->metadata->tags.head;

        /* Set up the internal buffer read pointers!*/
        internal->metadata->prev_rd_index = channel->internal.prev_rd_indexes[output_index];
        internal->metadata->tags.head = channel->internal.head_tag[output_index];
    }
    else
    {
        /* check if the tag head is correct */
        PL_ASSERT(internal->metadata->tags.head == channel->internal.head_tag[output_index]);
    }

    /* b4idx will indicate the octets remaining from the last tag. */
    tag_list = buff_metadata_peek_ex(channel->internal.buffer, &b4idx);

    SPLITTER_MSG3("splitter_transiton_to_inactive"
            "\n tag_list 0x%08x  tag_list->index 0x%08x b4idx %d octets " ,
            tag_list, tag_list->index, b4idx);
    if (tag_list == NULL)
    {
        /* Would be nice to code against this. */
        if (!most_data_buffered)
        {
            /* restore the index value. */
            internal->metadata->prev_rd_index = before_prev_rd_indexes;
            internal->metadata->tags.head = before_head_tag;
        }
        return FALSE;
    }

    if (channel->output_state[output_index] == ACTIVE)
    {
        unsigned data_before[SPLITTER_MAX_OUTPUTS_PER_CHANNEL] = {0,0};
        unsigned data_to_move[SPLITTER_MAX_OUTPUTS_PER_CHANNEL] = {0,0};
        unsigned other_out_index = output_index == 0 ? 1: 0;

        /* Get the amount of date buffered internally. */
        internal_buffer_data(splitter, cbuffer, data_before);

        /* finish copying the current tag. */
        data_to_move[output_index] = b4idx/cbuffer->data_size;

        SPLITTER_MSG4("splitter_transiton_to_inactive:"
                   "\n output_index %d  other_out_index %d data_to_move[0] %d data_to_move[1] %d " ,
                   output_index, other_out_index, data_to_move[0], data_to_move[1]);

        /* Limit the data to move based on the output space. */
        limit_data_to_move(splitter, cbuffer, data_to_move);
        if (data_to_move[output_index] != b4idx/cbuffer->data_size)
        {
            SPLITTER_ERRORMSG("splitter_transiton_to_inactive:"
                       " Not enough space to finish copying tag!" );
            if (!most_data_buffered)
            {
                /* restore the index value. */
                internal->metadata->prev_rd_index = before_prev_rd_indexes;
                internal->metadata->tags.head = before_head_tag;
            }
            return FALSE;
        }
        /* Finish copying last tag. */
        unpack_data_from_internal_to_output(splitter, cbuffer, data_to_move);


        SPLITTER_MSG4("splitter_transiton_to_inactive:"
                   "\n data_before[0] %d data_before[1] %d data to remove %d most_data_buffered %d" ,
                   data_before[0], data_before[1], data_before[output_index] - data_before[other_out_index], most_data_buffered);
        /* Now remove the buffered data from the internal buffer */
        if (data_before[output_index] > data_before[other_out_index] )
        {
            /* Check if the this output stream buffered the most amount of data.*/
            PL_ASSERT(most_data_buffered);
            /* remove the unnecessary data.*/
            remove_data_from_internal(splitter, cbuffer, data_before[output_index]  - data_before[other_out_index]);
        }
    }

    if (!most_data_buffered)
    {
        /* restore the index value. */
        internal->metadata->prev_rd_index = before_prev_rd_indexes;
        internal->metadata->tags.head = before_head_tag;
    }
    return TRUE;

}

/**
 * Transition from any state to INACTIVE.
 *
 */
static bool splitter_transiton_to_inactive(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned output_index)
{
    SPLITTER_CHANNEL_STRUC *channel;
    if (splitter->finish_last_tag)
    {
        /* If copying the last tag failed, return failure */
        if (!splitter_finish_copying_last_tag(splitter, cbuffer, output_index))
        {
            return FALSE;
        }
    }
    /* Deactivate all output streams with index "output_index". */
    channel = splitter->channel_list;
    while (channel != NULL)
    {
        /* Change state.*/
        channel->output_state[output_index] = INACTIVE;
        /* set the metadata read index and head. */
        channel->internal.prev_rd_indexes[output_index] = -1;
        channel->internal.head_tag[output_index] = (metadata_tag *)-1;
        /* convert the idex to read index.*/
        channel->internal.read_pointers[output_index] = (int *)(-1);
        if (!splitter->finish_last_tag)
        {
            /* Expect a new buffer at the output if the tag was not finished. */
            channel->output_buffer[output_index] = NULL;
        }

        channel = channel->next;
    }
    return TRUE;
}



/**
 * Transition to HOLD.
 */
static bool splitter_transiton_to_hold(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer, unsigned output_index)
{
    SPLITTER_CHANNEL_STRUC *channel;
    tCbuffer *internal;

    /* First set the metadata */
    channel = get_metadata_channel(splitter);
    internal = channel->internal.buffer;
    /* set the metadata read index and head. */
    channel->internal.prev_rd_indexes[output_index] = internal->metadata->prev_rd_index;
    channel->internal.head_tag[output_index] = internal->metadata->tags.head;

    /* Put output streams with index "output_index" on hold. */
    channel = splitter->channel_list;
    while (channel != NULL)
    {
        internal = channel->internal.buffer;
        /* Change state.*/
        channel->output_state[output_index] = HOLD;
        /* Set the read pointers.*/
        channel->internal.read_pointers[output_index] = internal->read_ptr;

        channel = channel->next;
    }

    return TRUE;
}

/**
 * Returns the expected state for the give output stream index.
 */
static SPLITTER_OUTPUT_STATE get_output_state(SPLITTER_OP_DATA *splitter, unsigned index)
{
    unsigned channel_mask;
    /* Sanity check the inputs */
    PL_ASSERT(check_output_state_validity(splitter));
    PL_ASSERT(index<SPLITTER_MAX_OUTPUTS_PER_CHANNEL);

    channel_mask = 1<<index;
    if (splitter->active_streams & channel_mask)
    {
        return ACTIVE;
    }
    else if (splitter->inactive_streams & channel_mask)
    {
        return INACTIVE;
    }
    else if (splitter->hold_streams & channel_mask)
    {
        return HOLD;
    }
    else
    {
        /* This cannot happen, but stop the compiler complaining.*/
        return INACTIVE;
    }
}

/**
 * Function which handles the transitions for the splitter capability.
 */
bool splitter_transiton(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer)
{
    SPLITTER_CHANNEL_STRUC *channel;
    unsigned i;
    SPLITTER_OUTPUT_STATE next_state;
    SPLITTER_OUTPUT_STATE old_state;
    bool retval = TRUE;

    channel = get_metadata_channel(splitter);
    if (channel == NULL)
    {
        /* There are no connections, the transition will
         * be handled by connect or channel creation.
         */
        return TRUE;
    }

    for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
    {
        /* Get the new state for the output stream. */
        next_state = get_output_state(splitter, i);
        old_state = channel->output_state[i];

        /* First metadata should be solved.*/
        if (old_state != next_state)
        {
            bool transiton;
            SPLITTER_MSG3("splitter_transiton: Output stream %d transition from %d to %d "
                    "(where 0 INACTIVE, 1 ACTIVE, 2 HOLD)",
                    i, old_state, next_state);

            switch (next_state)
            {
                case INACTIVE:
                {
                    transiton = splitter_transiton_to_inactive(splitter, cbuffer, i);
                    break;
                }
                case ACTIVE:
                {
                    transiton = splitter_transiton_to_active(splitter, cbuffer, i);
                    break;
                }
                case HOLD:
                {
                    transiton = splitter_transiton_to_hold(splitter, cbuffer, i);
                    break;
                }
                default:
                {
                    transiton = FALSE;
                    PL_ASSERT(next_state <= LAST_STATE);
                }
            }
            /* All channel transition must be terminated to
             * finish the overall transition. */
            retval = transiton && retval;
        }/* end of state transition. */
    }
    return retval;
}

/**
 * Helper function to get the cbuffer functions.
 */
const cbuffer_functions_t *get_cbuffer_functions(SPLITTER_OP_DATA *splitter)
{
    /* Select which set of cbuffer functions to use when
     * copying/packing.  */
    switch (splitter->data_format)
    {
        case AUDIO_DATA_FORMAT_FIXP:
        case AUDIO_DATA_FORMAT_FIXP_WITH_METADATA:
            if (splitter->packing == UNPACKED)
            {
                /* Just copy the audio*/
                return &audio_copy_functions;
            }
            else
            {
                /* Pack the audio. */
                return &audio_pack_functions;
            }
            break;
        default:
            /* Data will be packed and unpacked if the
             * usable octets are different. */
            return &data_pack_functions;
            break;
    }
}

/**
 * Helper function which is the core method for internal/external buffering.
 */
static void splitter_process_data_buffering(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    const cbuffer_functions_t *cbuffer = get_cbuffer_functions(splitter);
    /* Data to pack/copy. and the actual copies/packed data.*/
    unsigned data_to_pack;
    bool in_transition;

    patch_fn(splitter_process_data);

    timer_cancel_event_atomic(&splitter->self_kick_timer);

    /* Copy data to the internal buffer */
    /*Calculate the min available data at the input */
    data_to_pack = amount_to_pack(splitter, cbuffer);
    /* Pack/Copy to internal buffer. */
    if (data_to_pack != 0)
    {
        pack_to_internal(splitter, cbuffer, data_to_pack);
        /*  It is necessary to kick back whenever data is consumed so
         * so we can make full use of the internal buffer. */
        touched->sinks = splitter->touched_sinks;
    }

    /* Try to finish the transition.*/
    in_transition = splitter->in_transition;
    if (in_transition)
    {
        if (splitter_transiton(splitter, cbuffer))
        {
            /* This time we managed to finish transitioning.*/
            in_transition = FALSE;
            /* Save the new state. */
            splitter->in_transition = in_transition;
        }
    }

    /* Copy to output if we finished the transition. */
    if (in_transition == FALSE)
    {
        /* Unpack/Copy from internal to the output as much as possible*/
        if (move_data_from_internal_to_output(splitter, cbuffer))
        {
            touched->sources = splitter->touched_sources;
        }
    }

    timer_schedule_event_in_atomic(SPLITTER_SELF_KICK_RATIO * stream_if_get_system_kick_period(),
        splitter_timer_task, (void*)op_data, &splitter->self_kick_timer);
}
#endif


/****************************************************************************
Public Function Definitions
*/
/**
 * \brief Helper to unpack terminal information.
 *
 * \param terminal_id information from connect/disconnect/buffer_details calls.
 * \param buffer given only for connect, NULL otherwhise.
 * \param is_input address where the input type (true if input, false if output)
                       will be stored. [output]
 * \param terminal_num address where the terminal
                       number will be stored. [output]
 * \param buf optional pointer to a CBuffer pointer where
 *            the Cbuffer pointer may be stored. [output]
 *
 * \return TRUE if the call is correct, FALSE otherwise.
 */
bool get_terminal_info(unsigned terminal_id, tCbuffer *buffer, terminal_info_t *terminal_info, bool read_buffer)
{
    terminal_info->terminal_num = terminal_id & TERMINAL_NUM_MASK;
    terminal_info->is_input = terminal_id & TERMINAL_SINK_MASK;

    if(read_buffer)
    {
        terminal_info->buffer = buffer;
    }

    if (terminal_info->is_input)
    {
        if (terminal_info->terminal_num >= SPLITTER_MAX_INPUTS)
        {
            SPLITTER_ERRORMSG1("Splitter: Incorrect terminal number %4d!", terminal_info->terminal_num);
            return FALSE;
        }
        /* For input terminals the channel id is equal to the terminal ID. */
        terminal_info->channel_id = terminal_info->terminal_num;
        /* Input index is always 0 . */
        terminal_info->index = 0;
    }
    else
    {
        if (terminal_info->terminal_num >= SPLITTER_MAX_OUTPUTS)
        {
            SPLITTER_ERRORMSG1("Splitter: Incorrect terminal number %4d!", terminal_info->terminal_num);
            return FALSE;
        }
        /* For outp terminals the channel id is the terminal ID shifted
         * with the number of outputs. */
        terminal_info->channel_id = terminal_info->terminal_num >> 1;

        terminal_info->index = terminal_info->terminal_num & 1;
    }

    return TRUE;
}


/**
 * Creates a channel.
 */
SPLITTER_CHANNEL_STRUC *create_channel(SPLITTER_OP_DATA *splitter, tCbuffer* internal_metadata_buff)
{
    SPLITTER_CHANNEL_STRUC *channel;
    tCbuffer* internal_buff;
    unsigned buffer_size;


    SPLITTER_MSG("Splitter: Creating internal buffer.");

    /* Allocate the channel buffer structure. */
    channel = xzpnew(SPLITTER_CHANNEL_STRUC);
    if (channel == NULL)
    {
        SPLITTER_ERRORMSG("Splitter: Unable to allocate CHANNEL_STRUC!" );
        return NULL;
    }
    /* No need to allocate internal buffer when in cloning mode. */
    if (splitter->working_mode == CLONE_BUFFER)
    {
        return channel;
    }

    /* If the data is packed we only need half of the buffer. */
    if (splitter->packing == PACKED)
    {
        buffer_size = splitter->buffer_size/2;
    }
    else
    {
        buffer_size = splitter->buffer_size;
    }
    /* Allocate the buffer. Metadata flag is set later. */
    if (splitter->location == INTERNAL)
    {
        internal_buff = cbuffer_create_with_malloc(
                buffer_size, BUF_DESC_SW_BUFFER);
    }
    else
    {
        /* Allocate the memory on the external SRAM */
        void *data_location = ext_malloc(sizeof(int) * buffer_size);
        if (data_location != NULL)
        {
            internal_buff = cbuffer_create(data_location,
                    buffer_size, BUF_DESC_SW_BUFFER);
        }
        else
        {
            internal_buff = NULL;
        }
    }

    if (internal_buff == NULL)
    {
        SPLITTER_ERRORMSG("splitter: Unable to allocate internal buffer for the channel!" );
        /* Free up the channel structure.*/
        pdelete(channel);
        return NULL;
    }
#ifdef INSTALL_METADATA
    /* Set up the metadata buffer. */
    if (internal_metadata_buff != NULL)
    {
        internal_buff->metadata = internal_metadata_buff->metadata;
    }
    else
    {
        /* Allocate the metadta buffer. */
        internal_buff->metadata = xzpnew(metadata_list);
        if (internal_buff->metadata == NULL)
        {
            SPLITTER_ERRORMSG("splitter: Unable to allocate metadata for internal buffer.!" );
            /* delete the internal buffer allocated for the channel. */
            cbuffer_destroy(internal_buff);
            /* Free up the channel structure.*/
            pdelete(channel);
            return NULL;
        }
        /* Only one metadat for the internal buffer. */
        internal_buff->metadata->next = internal_buff->metadata;
    }
    /* Set the metadata flag!*/
    internal_buff->descriptor |= BUF_DESC_METADATA_MASK;

    /* Set the usable octets to OCTETS_PER_SAMPLE. This will work for all case:
     * 1) For packed data the data will remain pack.
     * 2) Unpacked data will be packed when copied to the internal buffer
     * 3) Audio will remain the same.
     * 4) Unpacked audio will work because it uses  audio_pack_functions cbuffer
     *    functions. The metadata will work because the metadata buffer size is
     *    doubled to avoid manipulating the tags size.
     *
     * This also sets the metadata buffer size based on the cbuffer buffer size! */
    cbuffer_set_usable_octets(internal_buff,
            OCTETS_PER_SAMPLE);

    /* To avoid manipulating the tag length when packing audio
     * just double the metadata buffer size.
     * internal_metadata_buff->metadata->buffer_size is change by
     * cbuffer_set_usable_octets as all internal buffer has
     * the same metadata*/
    if (splitter->packing == PACKED)
    {
        internal_buff->metadata->buffer_size =
                internal_buff->metadata->buffer_size * 2;
    }

    /* Finally, set the channel internal buffer. */
    channel->internal.buffer = internal_buff;

    /* Transition to hold is instant. No need to have an output buffer. */
    if (splitter->hold_streams != OUT_STREAM__0_OFF__1_OFF)
    {
        unsigned i;
        for(i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
        {
            if (get_output_state(splitter, i)== HOLD)
            {
                channel->output_state[i] = HOLD;
            }
        }
    }
#endif

    return channel;
}

/**
 * Creates a channel.
 */
SPLITTER_CHANNEL_STRUC *get_channel_struct(SPLITTER_OP_DATA *splitter, unsigned channel_id)
{
    SPLITTER_CHANNEL_STRUC *channel = splitter->channel_list;
    while (channel != NULL)
    {
        if (channel->id == channel_id)
        {
            return channel;
        }
        channel = channel->next;
    }

    if(splitter->buffer_size == 0)
    {
        SPLITTER_ERRORMSG("Splitter: Internal buffer size not set cannot connect!" );
        return NULL;
    }

    /* Create a new channel with the id. */
    channel = create_channel(splitter, get_internal_metadata_buffer(splitter));
    if (channel != NULL)
    {
        /* Set the channel ID*/
        channel->id = channel_id;
        /* Add the channel to the channel list. */
        if (splitter->channel_list == NULL)
        {
            /* First Channel. */
            splitter->channel_list = channel;
        }
        else
        {
            /* Add the channel to the end of the list. */
            SPLITTER_CHANNEL_STRUC *temp_channel = splitter->channel_list;
            while (temp_channel->next != NULL)
            {
                temp_channel = temp_channel->next;
            }
            temp_channel->next = channel;
        }
    }
    return channel;
}




/**
 * Deletes the channel.
 */
void delete_channel(SPLITTER_CHANNEL_STRUC *channel, bool last_channel)
{
    SPLITTER_MSG1("splitter: Deleting channel %d !", channel->id);
    if (channel->internal.buffer != NULL)
    {
#ifdef INSTALL_METADATA
        if (last_channel && (channel->internal.buffer->metadata != NULL))
        {
            unsigned b4idx;
            metadata_tag *tag_list;
            tag_list = buff_metadata_peek_ex(channel->internal.buffer, &b4idx);
            buff_metadata_tag_list_delete(tag_list);
            SPLITTER_MSG1("Deleting internal metadata 0x%08x",channel->internal.buffer->metadata);
            /* Delete the internal buffer metadata. */
            pdelete(channel->internal.buffer->metadata);
        }
#endif
        /* delete the internal buffer allocated for the channel. */
        cbuffer_destroy(channel->internal.buffer);
    }
    /* delete the channel structure */
    pdelete(channel);
}


/**
 * Removes and deletes the disconnected channels.
 */
void delete_disconnected_channel(SPLITTER_OP_DATA *splitter)
{
    SPLITTER_CHANNEL_STRUC **channel_ptr = &splitter->channel_list;
    while (*channel_ptr != NULL)
    {
        bool channel_disconnected = TRUE;
        int i;

        /* Check if the channel has connection. */
        if ((*channel_ptr)->input_buffer != NULL)
        {
            channel_disconnected = FALSE;
        }
        else
        {
            for(i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
            {
                if ((*channel_ptr)->output_buffer[i] != NULL)
                {
                    channel_disconnected = FALSE;
                    break;
                }
            }
        }

        if (channel_disconnected)
        {
            SPLITTER_CHANNEL_STRUC *temp = *channel_ptr;
            bool last_channel;

            /* Remove it from the list, */
            *channel_ptr = temp->next;
            last_channel = splitter->channel_list == NULL;
            /* Delete the channel. */
            delete_channel(temp, last_channel);
        }

        channel_ptr = &((*channel_ptr)->next);
    }
}


/**
 * Helper function to get the metadata buffers for the input and outputs.
 */
tCbuffer* get_metadata_buffer(SPLITTER_OP_DATA *splitter, bool is_input, unsigned index)
{
    SPLITTER_CHANNEL_STRUC *channel = get_metadata_channel(splitter);

    if (channel != NULL)
    {
        if (is_input)
        {
            /* Return the medata input buffer. */
            return channel->input_buffer;
        }
        else
        {
            if (splitter->working_mode != CLONE_BUFFER)
            {
                return channel->output_buffer[index];
            }
            else
            {
                unsigned i;
                /* When splitter is in cloning mode each output has the same
                 * metadata so use the one which is connected. */
                for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
                {
                    if (channel->output_buffer[i] != NULL)
                    {
                        return channel->output_buffer[i];
                    }
                }
            }
        }
    }

    /* Couldn't find anything. */
    return NULL;
}


/**
 * Helper function to get the metadata buffer for the internal buffer.
 */
tCbuffer* get_internal_metadata_buffer(SPLITTER_OP_DATA *splitter)
{
    SPLITTER_CHANNEL_STRUC *channel = get_metadata_channel(splitter);

    if (channel != NULL)
    {
        return channel->internal.buffer;
    }

    /* Couldn't find anything. */
    return NULL;
}

/********************    process data   ****************************/
/**
 * \brief The main data processing function only selects the
 *  right data processing based on the working mode.
 *
 */
void splitter_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    /* The comment after the function call helps the ACAT to find the extra operator data 
     * for old implementations where the data type is not save in the elf. */
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data); /* old way = (SPLITTER_OP_DATA *)(op_data->extra_op_data);*/

    switch (splitter->working_mode)
    {
        case CLONE_BUFFER:
            splitter_process_data_clone(op_data, touched);
            break;
#ifdef INSTALL_METADATA
        case BUFFER_DATA:
            splitter_process_data_buffering(op_data, touched);
            break;
#endif
        default:
            SPLITTER_ERRORMSG1("SPLITTER Error, unsupported mode: %d.",splitter->working_mode);
            /* Check if the mode is valid one or is just unsupported.*/
            PL_ASSERT(splitter->working_mode < NR_OF_MODES);
            break;
    }
}
