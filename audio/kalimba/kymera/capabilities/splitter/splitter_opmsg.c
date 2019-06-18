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

/****************************************************************************
Public Constant Declarations
*/
/****************************************************************************
Private Function Declaration
*/
/* Checks if the operator state is valid together with the bitfiled. */
static bool valid_input_and_splitter_state(SPLITTER_OP_DATA *splitter, unsigned bitfield)
{
    if(splitter->working_mode != BUFFER_DATA)
    {
        SPLITTER_ERRORMSG1("Splitter: Wrong working mode %d (0 Clone, 1 Buffer Data).",
                splitter->working_mode);
        return FALSE;
    }

    if (invalid_stream_setting(bitfield))
    {
        SPLITTER_ERRORMSG("Splitter: Invalid bitfield.");
        return FALSE;
    }

    if (splitter->in_transition)
    {
        SPLITTER_ERRORMSG("Splitter: Still in transition cannot change state.");
        return FALSE;
    }
    return TRUE;
}

/* Checks if the operator state is valid. Self kicks the operator where needed.*/
static bool valid_new_splitter_state(OPERATOR_DATA *op_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    SPLITTER_MSG3("splitter: New state: "
            "\nActive %d\n Inactive %d\n Hold %d!",
            splitter->active_streams, splitter->inactive_streams, splitter->hold_streams);

    if(!check_output_state_validity(splitter))
    {
        SPLITTER_ERRORMSG("Splitter: Wrong splitter state!");
        return FALSE;
    }

    /* No need to transition if the splitter is not connected yet. */
    if (splitter->channel_list != NULL)
    {
        splitter->in_transition = TRUE;
    }

    if (opmgr_op_is_running(op_data))
    {
        /* A switch is needed. Try to kick the operator to perform the switch. */
        opmgr_kick_operator(op_data);
    }
    return TRUE;
}

/* Check if the outputs are connected for the given bitfield. */
static bool splitter_output_stream_connected(SPLITTER_OP_DATA *splitter, SPLITTER_STREAMS streams)
{
    unsigned channel_mask, i;
    SPLITTER_CHANNEL_STRUC *channel;

    channel = splitter->channel_list;
    while (channel != NULL)
    {
        for (i=0; i<SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
        {
            channel_mask = 1<<i;
            if ((streams & channel_mask) && (channel->output_buffer[i] == NULL))
            {
                SPLITTER_ERRORMSG2("Splitter: Channel %d output %d not connected", channel->id ,i);
                return FALSE;
            }
        }

        channel = channel->next;
    }

    /* All output channels are connected. */
    return TRUE;
}

/****************************************************************************
Public Function Definitions
*/
/**
 * \brief Configures the splitter defaults.
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the create request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    if (!base_op_create(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    splitter->buffer_size = 0;
    splitter->data_format = AUDIO_DATA_FORMAT_FIXP_WITH_METADATA;
    splitter->active_streams = OPMSG_SPLITTER_STREAM_0 | OPMSG_SPLITTER_STREAM_1;
    splitter->self_kick_timer = TIMER_ID_INVALID;
    splitter->working_mode = CLONE_BUFFER;

    splitter->frame_size = SPLITTER_DEFAULT_FRAME_SIZE;
    splitter->reframe_data.sample_rate            = stream_if_get_system_sampling_rate();
    splitter->reframe_data.last_tag_samples       = LAST_TAG_SAMPLES_INVALID;
    splitter->reframe_data.last_tag_err_offset_id = INFO_ID_INVALID;
    splitter->reframe_enabled = FALSE;

    return TRUE;
}

/**
 * \brief Reports the buffer requirements of the splitter terminal
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the buffer size request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_buffer_details(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_BUF_DETAILS_TERMINAL_ID(message_data);
    OP_BUF_DETAILS_RSP *buf_details;
    terminal_info_t terminal_info;
    SPLITTER_CHANNEL_STRUC *channel;

    if (!base_op_buffer_details(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    if (!get_terminal_info(terminal_id, NULL, &terminal_info, FALSE))
    {
        base_op_change_response_status(response_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Alias the void* to the actual structure to make it easier to work with */
    buf_details = (OP_BUF_DETAILS_RSP*)*response_data;
#ifdef INSTALL_METADATA
    buf_details->supports_metadata = TRUE;
    buf_details->metadata_buffer = get_metadata_buffer(splitter, terminal_info.is_input, terminal_info.index);
#endif
    channel = get_channel_struct(splitter, terminal_info.channel_id);
    if (channel == NULL)
    {
        /* Unable to find/create the channel.*/
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if (splitter->working_mode == CLONE_BUFFER)
    {
        if (terminal_info.is_input)
        {
            if (splitter->buffer_size != 0)
            {
                /* Override the calculated buffer size if one has been set */
                buf_details->b.buffer_size = splitter->buffer_size;
            }
        }
        else
        {
            if (channel->input_buffer == NULL)
            {
                /* Input needs to be connected prior to output */
                base_op_change_response_status(response_data, STATUS_CMD_FAILED);
                return TRUE;
            }
            else
            {
                tCbuffer *buffer_to_wrap = channel->input_buffer;

                buf_details->supplies_buffer = TRUE;

                if (channel->output_buffer[terminal_info.index] != NULL)
                {
                    /* If this terminal is already connected then make sure the buffer that
                     * is in use is returned to the caller. */
                    buf_details->b.buffer = channel->output_buffer[terminal_info.index];
                }
                else
                {
                    /* If buffer details is requested during connect for the same channel
                     * return the buffer being used for connect */
                    if (splitter->curr_connecting == NULL || splitter->curr_connecting->base_addr != buffer_to_wrap->base_addr)
                    {
                        /* A buffer orphaned in a previously aborted connect might exist, free it
                         * before we create a new buffer that might get orphaned that we want to track. */
                        cbuffer_destroy_struct(splitter->curr_connecting);

                        /* Store the buffer created here in case it gets orphaned by connect failing */
                        splitter->curr_connecting = cbuffer_create(buffer_to_wrap->base_addr,
                            cbuffer_get_size_in_words(buffer_to_wrap), BUF_DESC_SW_BUFFER);

                        if (splitter->curr_connecting != NULL)
                        {
                            splitter->curr_connecting->write_ptr = buffer_to_wrap->write_ptr;
                            splitter->curr_connecting->read_ptr = buffer_to_wrap->write_ptr;
                        }
                        else
                        {
                            /* There wasn't enough RAM so we have to fail */
                            base_op_change_response_status(response_data, STATUS_CMD_FAILED);
                            return TRUE;
                        }
                    }
                    buf_details->b.buffer = splitter->curr_connecting;
                }
            }
        }

    }
    else
    {
        buf_details->supplies_buffer = FALSE;
        /* The splitter capability will have an internal or external buffer.
         * Use the smallest transform buffer*/
        buf_details->b.buffer_size = SPLITTER_TRANSFORM_BUFFER_SIZE;
    }


    return TRUE;
}

/**
 * \brief Returns the splitter terminal audio data format.
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the get_block_size request message payload
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_get_data_format(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    *response_id = OPCMD_DATA_FORMAT;
    /* Set up the a default success response information */
    if (base_op_build_std_response_ex(op_data, STATUS_OK, response_data))
    {
        ((OP_STD_RSP*)*response_data)->resp_data.data = splitter->data_format;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/**
 * \brief Returns the splitter block size
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the get_block_size request message payload
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_get_sched_info(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    OP_SCHED_INFO_RSP* resp;
    *response_id = OPCMD_GET_SCHED_INFO;
    resp = base_op_get_sched_info_ex(op_data, message_data, response_id);
    if (resp == NULL)
    {
        return base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, response_data);
    }
    *response_data = resp;

    /* Same buffer size for sink and source.
       No additional verification needed.*/
    resp->block_size = SPLITTER_BLOCK_SIZE;

    return TRUE;
}

/**
 * \brief Connects a splitter terminal to a buffer. This capability requires
 * that a channel's input terminal is connected before its output terminals.
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the connect request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_connect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    tCbuffer* buffer = OPMGR_GET_OP_CONNECT_BUFFER(message_data);
    terminal_info_t terminal_info;
    SPLITTER_CHANNEL_STRUC *channel;

    *response_id = OPCMD_CONNECT;
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, response_data))
    {
        return FALSE;
    }

    if (!get_terminal_info(terminal_id, buffer, &terminal_info, TRUE))
    {
        base_op_change_response_status(response_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    channel = get_channel_struct(splitter, terminal_info.channel_id);
    if (channel == NULL)
    {
        /* Unable to find/create the channel.*/
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if (terminal_info.is_input)
    {
        SPLITTER_MSG2("Splitter: Connecting sink terminal %4d to buff 0x%08x!", terminal_info.terminal_num, terminal_info.buffer);
        if (channel->input_buffer == NULL)
        {
#ifdef INSTALL_METADATA
            tCbuffer *metadata_buf = get_metadata_buffer(splitter, terminal_info.is_input, terminal_info.index);

            if ((metadata_buf != NULL) && (metadata_buf->metadata != terminal_info.buffer->metadata))
            {
                base_op_change_response_status(response_data, STATUS_CMD_FAILED);
                SPLITTER_ERRORMSG("Splitter: The two input has different metadata!");
                return TRUE;
            }
#endif
            channel->input_buffer = terminal_info.buffer;

            /* Set the active sinks. */
            splitter->touched_sinks |= TOUCHED_SINK_0 << terminal_info.terminal_num;
        }
    }
    else
    {
        SPLITTER_MSG2("Splitter: Connecting source terminal %4d to buff 0x%08x!", terminal_info.terminal_num, terminal_info.buffer);
        if ((channel->output_buffer[terminal_info.index] == NULL) && (channel->output_state[terminal_info.index] != ACTIVE))
        {
            channel->output_buffer[terminal_info.index] = terminal_info.buffer;

            if(splitter->working_mode == CLONE_BUFFER)
            {
                /* This should be the curr_connecting buffer that buffer_details
                 * stashed. If it isn't fail the request as something went wrong,
                 * otherwise it's not in danger of being orphaned any more. */
                PL_ASSERT(terminal_info.buffer == splitter->curr_connecting);
                splitter->curr_connecting = NULL;
            }
            else
            {
                /* Set the outputs read pointer and the head to the internal buffer!! */
                channel->internal.read_pointers[terminal_info.index] = channel->internal.buffer->write_ptr;
#ifdef INSTALL_METADATA
                channel->internal.head_tag[terminal_info.index] = channel->internal.buffer->metadata->tags.head;
#endif
            }

#ifdef INSTALL_METADATA
            if (buff_has_metadata(terminal_info.buffer))
            {
                /* Set the usable octet */
                cbuffer_set_usable_octets(channel->output_buffer[terminal_info.index],
                        cbuffer_get_usable_octets(channel->input_buffer));
            }
#endif
            /* Activate the stream if needed if the operator is not running. */
            if ((splitter->active_streams & (1<<terminal_info.index)) != 0)
            {
                PL_ASSERT(!opmgr_op_is_running(op_data));
                channel->output_state[terminal_info.index] = ACTIVE;
            }
            /* Check if the channel is already on hold. */
            if ((splitter->hold_streams & (1<<terminal_info.index)) != 0)
            {
                PL_ASSERT(channel->output_state[terminal_info.index] == HOLD);
            }
            /* Set the active sources.*/
            splitter->touched_sources |= TOUCHED_SOURCE_0 << terminal_info.terminal_num;
        }
        else
        {
            base_op_change_response_status(response_data, STATUS_CMD_FAILED);
            SPLITTER_ERRORMSG2("Splitter: Channel %d Output %d already connected or active !",
                    terminal_info.channel_id, terminal_info.index);
            return TRUE;
        }

    }

    return TRUE;
}

/**
 * \brief Starts the splitter capability so that data will flow through this
 * capablity.
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_start(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);// = (SPLITTER_OP_DATA *)(op_data->extra_op_data);

    if (!base_op_start(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }

    if (opmgr_op_is_running(op_data))
    {
        return TRUE;
    }

    if (splitter->channel_list == NULL)
    {
        /* At least one input and its corresponding output(s)
           need to be connected */
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
    }

    return TRUE;
}

/**
 * \brief Disconnects the splitter terminal from a buffer.
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the disconnect request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_disconnect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_DISCONNECT_TERMINAL_ID(message_data);
    unsigned i;
    terminal_info_t terminal_info;
    SPLITTER_CHANNEL_STRUC *channel;

    *response_id = OPCMD_DISCONNECT;
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, response_data))
    {
        return FALSE;
    }

    if (!get_terminal_info(terminal_id, NULL, &terminal_info, FALSE))
    {
        base_op_change_response_status(response_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    channel = get_channel_struct(splitter, terminal_info.channel_id);
    if (channel == NULL)
    {
        /* Unable to find/create the channel.*/
        base_op_change_response_status(response_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if (opmgr_op_is_running(op_data))
    {
        bool error = FALSE;
        if (splitter->working_mode == CLONE_BUFFER)
        {
            SPLITTER_ERRORMSG("Splitter: Cannot disconnect from a running operator when buffers are cloned.");
            error = TRUE;
        }
        else
        {
            if (terminal_info.is_input)
            {
                SPLITTER_ERRORMSG("Splitter: Input buffers cannot be disconnected even if the data is buffered.");
                error = TRUE;
            }
            else
            {
                if (channel->output_state[terminal_info.index] == ACTIVE)
                {
                    SPLITTER_ERRORMSG("Splitter: Active outputs cannot be disconnected.");
                    error = TRUE;
                }
            }
        }
        if (error)
        {
            base_op_change_response_status(response_data, STATUS_CMD_FAILED);
            return TRUE;
        }
    }


    if (terminal_info.is_input)
    {
        SPLITTER_MSG1("Splitter: Disconnecting sink terminal %4d!", terminal_info.terminal_num);
        channel->input_buffer = NULL;
        splitter->touched_sinks &= ~(TOUCHED_SINK_0 << terminal_info.terminal_num);

        if (splitter->working_mode == CLONE_BUFFER)
        {
            /* If there are outputs connected we need to make them look empty.
             * This shouldn't be done by the user, but we can't reject the
             * scenario when we're stopped as destroy operator needs to
             * disconnect all terminals before the operator are destroyed. */
            opmgr_op_suspend_processing(op_data);
            for (i = 0; i < SPLITTER_MAX_OUTPUTS_PER_CHANNEL; i++)
            {
                tCbuffer *out = channel->output_buffer[i];
                if (out != NULL)
                {
                    out->write_ptr = out->read_ptr;
                }
            }
            opmgr_op_resume_processing(op_data);
        }
    }
    else
    {
        SPLITTER_MSG1("Splitter: Disconnecting source terminal %4d!", terminal_info.terminal_num);
        splitter->touched_sinks &= ~(TOUCHED_SINK_0 << terminal_info.terminal_num);

        if (splitter->working_mode == CLONE_BUFFER)
        {
            tCbuffer* buff;
            buff = channel->output_buffer[terminal_info.index];
            cbuffer_destroy_struct(buff);
        }
        channel->output_buffer[terminal_info.index] = NULL;
    }

    delete_disconnected_channel(splitter);
    return TRUE;
}

/**
 * \brief Clears up the splitter operator if it's in the correct state.
 *
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_destroy(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);

    timer_cancel_event_atomic(&splitter->self_kick_timer);

    if (splitter->curr_connecting)
    {
        /* Clear up the capability specific data. */
        /* A buffer may have been orphaned during a failed connect make sure it
         * gets freed. */
        cbuffer_destroy_struct(splitter->curr_connecting);
    }

    return base_op_destroy(op_data, message_data, response_id, response_data);
}

/**
 * \brief Handles the opmsg that changes the buffer size requested by the splitter
 * at connect.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_buffer_size(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    if (opmgr_op_is_running(op_data))
    {
        SPLITTER_ERRORMSG("splitter_set_buffer_size: Operator running!");
        return FALSE;
    }
    if  (splitter->channel_list != NULL)
    {
        SPLITTER_ERRORMSG("splitter_set_buffer_size: Cannot set the buffer size for an operator with connected terminals!");
        return FALSE;
    }

    splitter->buffer_size = OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_BUFFER_SIZE, BUFFER_SIZE);
    SPLITTER_MSG1("splitter_set_buffer_size: Internal buffer size set to %d words!", splitter->buffer_size);
    return TRUE;
}

/**
 * \brief Handles the opmsg that changes the working mode for the splitter operator.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_working_mode(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    SPLITTER_MODES working_mode;
    if (opmgr_op_is_running(op_data))
    {
        SPLITTER_ERRORMSG("splitter_set_working_mode: Cannot configure working mode while running!");
        return FALSE;
    }
    if  (splitter->channel_list != NULL)
    {
        SPLITTER_ERRORMSG("splitter_set_working_mode: Cannot set working mode with connected terminals!");
        return FALSE;
    }
    working_mode = (SPLITTER_MODES)OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_SET_WORKING_MODE, WORKING_MODE);
    if  (working_mode >= NR_OF_MODES)
    {
        SPLITTER_ERRORMSG1("splitter_set_working_mode: %d is an unknonw working mode!", working_mode);
        return FALSE;
    }
    if(working_mode == BUFFER_DATA)
    {
        /* Deactivate all output streams. */
        splitter->active_streams = OUT_STREAM__0_OFF__1_OFF;
        splitter->inactive_streams = OUT_STREAM__0_ON___1_ON;
        splitter->hold_streams = OUT_STREAM__0_OFF__1_OFF;
    }
    splitter->working_mode = working_mode;
    SPLITTER_MSG1("splitter: Working mode set to %d (0 CLONE_BUFFER, 1 BUFFER_DATA)!",working_mode);
    return TRUE;
}

/**
 * \brief Handles the opmsg that changes location of the latency buffer.
 *     Functionality only available when the working mode is BUFFERING.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_location(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    SPLITTER_BUFFER_LOCATION location;
    if (opmgr_op_is_running(op_data))
    {
        SPLITTER_ERRORMSG("splitter_set_location: Cannot configure buffering location mode while running!");
        return FALSE;
    }
    if (splitter->working_mode != BUFFER_DATA)
    {
        SPLITTER_ERRORMSG("splitter_set_location: Cannot set location when input buffer is cloned!");
        return FALSE;
    }
    location = (SPLITTER_BUFFER_LOCATION)OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_SET_LOCATION, LOCATION);
    if (location >= NR_OF_LOCATIONS)
    {
        SPLITTER_ERRORMSG1("splitter_set_location: %d is a unknown location for SPLITTER_BUFFER_LOCATION!", location);
        return FALSE;
    }

    splitter->location = location;
    SPLITTER_MSG1("splitter_set_location: Location set to %d (0 INTERNAL, 1 SRAM)!", location);
    return TRUE;
}

/**
 * \brief Handles the opmsg that changes packing mode of the latency buffer.
 *     Functionality only available when the working mode is BUFFERING.
 *     Note that packing is only usefull for audio input where the decoder
 *     only produces 16bit audio.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_packing(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    SPLITTER_PACKING packing;
    if (opmgr_op_is_running(op_data))
    {
        SPLITTER_ERRORMSG("splitter_set_packing: Cannot configure working packing while running!");
        return FALSE;
    }
    if (splitter->working_mode != BUFFER_DATA)
    {
        SPLITTER_ERRORMSG("splitter_set_packing: Cannot set packing when input buffer is cloned!");
        return FALSE;
    }
    if ((splitter->data_format != AUDIO_DATA_FORMAT_FIXP) && (splitter->data_format != AUDIO_DATA_FORMAT_FIXP_WITH_METADATA))
    {
        SPLITTER_ERRORMSG("splitter_set_packing: Cannot set packing when data format is different than audio!");
        return FALSE;
    }
    packing = (SPLITTER_MODES)OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_SET_PACKING, PACKING);
    if (packing >= NR_OF_PACKING)
    {
        SPLITTER_ERRORMSG1("splitter_set_packing: %d is an invalid packing!", packing);
        return FALSE;
    }

    splitter->packing = packing;
    SPLITTER_MSG1("splitter_set_packing: Packing set to %d (0 UNPACKED, 1 PACKED)!", packing);
    return TRUE;
}

/**
 * \brief Handles the opmsg that changes the data format advertised by the splitter
 * at connect.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_data_format(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);

    if (opmgr_op_is_running(op_data))
    {
        SPLITTER_ERRORMSG("splitter: Cannot set data format while running!");
        return FALSE;
    }
    splitter->data_format = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_SET_DATA_FORMAT, DATA_TYPE);
    SPLITTER_MSG1("splitter: Data format set to %d!", splitter->data_format);
    return TRUE;
}


/**
 * \brief Handles the opmsg that configures which output streams are to be
 * considered as running/active
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the set_running_streams request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_running_streams(OPERATOR_DATA *op_data, void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned bitfield = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_ACTIVATE_STREAMS, STREAMS);

    if (invalid_stream_setting(bitfield))
    {
        return FALSE;
    }


    if (splitter->working_mode == CLONE_BUFFER)
    {

        unsigned    cur_channel, new_channel;
        SPLITTER_CHANNEL_STRUC *channel;
        /* Before setting the stream active make all the channels buffer pointers
         * look empty like it's just been connected, BUT aligned with the write
         * pointer of the other stream so that there is no chance of stalls. */
        if ((splitter->active_streams ^ bitfield) == OPMSG_SPLITTER_STREAM_1)
        {
            new_channel = 1;
            cur_channel = 0;
        }
        else
        {
            new_channel = 0;
            cur_channel = 1;
        }

        /* Prevent processing while the read and write ptrs for the stream that is
         * coming online is aligned to the current write pointer of the running
         * stream.
         */
        opmgr_op_suspend_processing(op_data);
        channel = splitter->channel_list;
        while (channel)
        {
            tCbuffer *new_out_buffer = channel->output_buffer[new_channel];
            if (new_out_buffer != NULL)
            {
                tCbuffer *cur_out_buffer = channel->output_buffer[cur_channel];
                new_out_buffer->read_ptr = cur_out_buffer->write_ptr;
                new_out_buffer->write_ptr = cur_out_buffer->write_ptr;

#ifdef INSTALL_METADATA
                {
                    /* Set both metadata read/write indices on the new stream
                     * to match the write index for the running stream.
                     * This ensures that new tags have the same indices on both
                     * streams.
                     */
                    metadata_list *new_metadata = new_out_buffer->metadata;
                    metadata_list *cur_metadata = cur_out_buffer->metadata;
                    new_metadata->prev_wr_index = cur_metadata->prev_wr_index;
                    new_metadata->prev_rd_index = cur_metadata->prev_wr_index;
                }
#endif /* INSTALL_METADATA */
            }
            channel = channel->next;
        }
        opmgr_op_resume_processing(op_data);
        splitter->active_streams = (SPLITTER_STREAMS)bitfield;

        return TRUE;
    }
    else
    {
        SPLITTER_STREAMS streams;
        if (!valid_input_and_splitter_state(splitter, bitfield))
        {
            return FALSE;
        }
        streams = (SPLITTER_STREAMS)bitfield;
        if (splitter->hold_streams != OUT_STREAM__0_OFF__1_OFF)
        {
            SPLITTER_ERRORMSG3("splitter_set_running_streams: Streams on hold! cannot use this command."
                    "\n Active %d\n Inactive %d\n Hold %d",
                    splitter->active_streams, splitter->inactive_streams, splitter->hold_streams);
            return FALSE;
        }
        /* Check if all output terminals are connected. */
        if (opmgr_op_is_running(op_data) && !splitter_output_stream_connected(splitter, streams))
        {
            return FALSE;
        }
        /* Change the three main stream. */
        splitter->active_streams = streams;
        splitter->inactive_streams = ~streams;

        /* verify the new state validity. */
        return valid_new_splitter_state(op_data);
    }
}

/**
 * \brief Handles the opmsg that changes the active streams for the splitter.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_activate_streams(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned bitfield;
    SPLITTER_STREAMS streams;

    bitfield = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_ACTIVATE_STREAMS_AFTER_TIMESTAMP, STREAMS);

    if (!valid_input_and_splitter_state(splitter, bitfield))
    {
        return FALSE;
    }
    streams = (SPLITTER_STREAMS)bitfield;
    if ((splitter->inactive_streams & streams) != streams)
    {
        SPLITTER_ERRORMSG3("splitter_activate_streams: Only inactive streams can be activated."
                "\n Active %d\n Inactive %d\n Hold %d",
                splitter->active_streams, splitter->inactive_streams, splitter->hold_streams);
        return FALSE;
    }

    /* Check if all output terminals are connected. */
    if (opmgr_op_is_running(op_data) && !splitter_output_stream_connected(splitter, streams))
    {
        return FALSE;
    }
    /* Activate streams. */
    splitter->active_streams = splitter->active_streams | streams;
    /* Update the inactive output streams to reflect the changes. */
    splitter->inactive_streams = splitter->inactive_streams & ~streams;

    /* verify the new state validity. */
    return valid_new_splitter_state(op_data);
}

/**
 * \brief Handles the opmsg that changes the active streams after a timestamp.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_activate_streams_after_timestamp(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned bitfield;
    SPLITTER_STREAMS streams;
    bitfield = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_ACTIVATE_STREAMS_AFTER_TIMESTAMP, STREAMS);
    splitter->transition_point = (TIME_INTERVAL)OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_ACTIVATE_STREAMS_AFTER_TIMESTAMP, TIMESTAMP_LS) +
                                 ((TIME_INTERVAL)OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_ACTIVATE_STREAMS_AFTER_TIMESTAMP, TIMESTAMP_MS) << 16);


    if (!valid_input_and_splitter_state(splitter, bitfield))
    {
        return FALSE;
    }

    streams = (SPLITTER_STREAMS)bitfield;
    if ((splitter->hold_streams & streams) != streams)
    {
        SPLITTER_ERRORMSG3("splitter_activate_streams_after_timestamp: Only streams on hold (streams buffered) can be activated."
                "\n Active %d\n Inactive %d\n Hold %d",
                splitter->active_streams, splitter->inactive_streams, splitter->hold_streams);
        return FALSE;
    }

    /* Check if all output terminals are connected. */
    if (opmgr_op_is_running(op_data) && !splitter_output_stream_connected(splitter, streams))
    {
        return FALSE;
    }
    /* Activate streams after timestamp. */
    splitter->active_streams = splitter->active_streams | streams;
    /* Update the hold output streams to reflect the changes. */
    splitter->hold_streams = splitter->hold_streams & ~streams;

    /* verify the new state validity. */
    return valid_new_splitter_state(op_data);
}

/**
 * \brief Handles the opmsg that deactivates streams.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_deactivate_streams(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned bitfield;
    SPLITTER_STREAMS streams;
    bitfield = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_DEACTIVATE_STREAMS, STREAMS);


    streams = (SPLITTER_STREAMS)bitfield;
    if (splitter->inactive_streams == streams)
    {
        /* No need to do anything. */
        return TRUE;
    }
    /* Exit from transition if switching to active did not finish. */
    if (splitter->in_transition)
    {
        if ((splitter->active_streams & streams) == streams)
        {
            splitter->in_transition = FALSE;
        }
        else
        {
            /* Other transitions should be instant. */
            return FALSE;
        }
    }
    if (!valid_input_and_splitter_state(splitter, bitfield))
    {
        return FALSE;
    }
    /* Deactivate streams. */
    splitter->inactive_streams = splitter->inactive_streams | streams;
    /* Update the active and hold output streams to reflect the changes. */
    splitter->active_streams = splitter->active_streams & ~streams;
    splitter->hold_streams = splitter->hold_streams & ~streams;

    /* verify the new state validity. */
    if (valid_new_splitter_state(op_data))
    {
#ifdef INSTALL_METADATA
        /* Switching to inactive is instant */
        return splitter_transiton(splitter, get_cbuffer_functions(splitter));
#else
        return FALSE;
#endif
    }
    else
    {
        return FALSE;
    }
}

/**
 * \brief Handles the opmsg that marks streams for buffering.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_buffer_streams(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned bitfield;
    SPLITTER_STREAMS streams;
    bitfield = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_DEACTIVATE_STREAMS, STREAMS);

    if (!valid_input_and_splitter_state(splitter, bitfield))
    {
        return FALSE;
    }

    streams = (SPLITTER_STREAMS)bitfield;
    if (splitter->hold_streams == streams)
    {
        /* No need to do anything. */
        return TRUE;
    }
    if ((splitter->inactive_streams & streams) != streams)
    {
        SPLITTER_ERRORMSG3("splitter_buffer_streams: Only inactive streams can be put on hold (buffered)."
                "\n Active %d\n Inactive %d\n Hold %d",
                splitter->active_streams, splitter->inactive_streams, splitter->hold_streams);
        return FALSE;
    }

    /* Terminals on hold can stay unconnected. */
    /* Hold streams. */
    splitter->hold_streams = splitter->hold_streams | streams;
    /* Update the inactive output streams to reflect the changes. */
    splitter->inactive_streams = splitter->inactive_streams & ~streams;

    /* verify the new state validity. */
    if (valid_new_splitter_state(op_data))
    {
#ifdef INSTALL_METADATA
        /* Switching to hold is instant */
        return splitter_transiton(splitter, get_cbuffer_functions(splitter));
#else
        return FALSE;
#endif
    }
    else
    {
        return FALSE;
    }
}

/**
 * \brief Handles the opmsg that sets the reframing.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_reframing(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    bool enabled;
    unsigned size;

    enabled = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_SET_REFRAMING, MODE) == OPMSG_SPLITTER_REFRAMING_ENABLED;
    size = OPMSG_FIELD_GET(message_data, OPMSG_SPLITTER_SET_REFRAMING, SIZE);

    if (enabled)
    {
        /* Make sure at least for frames can be saved at the internal buffer. */
        if (size > splitter->buffer_size / 4)
        {
            SPLITTER_ERRORMSG1("splitter_set_reframing: Frame %d size too big!",size);
            return FALSE;
        }
        if (splitter->reframe_data.sample_rate == 0)
        {
            SPLITTER_ERRORMSG("splitter_set_reframing: Sampling rate not set!");
            return FALSE;
        }
    }

    splitter->reframe_enabled = enabled;
    splitter->frame_size = size;
    return TRUE;
}

/**
 * \brief Handles the opmsg that sets the sample rate.
 *
 * \param op_data Pointer to the operator instance data
 * \param message_data Pointer to the destroy request message
 * \param response_id Location to write the response message id
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool splitter_set_sample_rate(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    SPLITTER_OP_DATA *splitter = get_instance_data(op_data);
    unsigned sample_rate = 25 * (OPMSG_FIELD_GET(message_data, OPMSG_COMMON_MSG_SET_SAMPLE_RATE, SAMPLE_RATE));

    if ((sample_rate < 8000) || (sample_rate > 192000))
    {
        SPLITTER_ERRORMSG1("splitter_set_reframing: Invalid sampling rate %d!", sample_rate);
        return FALSE;
    }
    splitter->reframe_data.sample_rate = sample_rate;
    return TRUE;
}
