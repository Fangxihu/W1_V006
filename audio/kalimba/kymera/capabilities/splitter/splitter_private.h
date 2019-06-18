/****************************************************************************
 * Copyright (c) 2015 - 2017 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup splitter
 * \file  splitter.h
 * \ingroup capabilities
 *
 * Splitter operator public header file. <br>
 *
 */

#ifndef SPLITTER_PRIVATE_H
#define SPLITTER_PRIVATE_H


#include "splitter.h"
#include "buffer.h"
#include "pmalloc/pl_malloc.h"
#include "util.h"
#include "audio_log/audio_log.h"
#include "platform/pl_intrinsics.h"
#include "platform/pl_assert.h"
#include "op_msg_helpers.h"
#include "pl_timers/pl_timers.h"
#include "patch/patch.h"
#include "ttp/ttp.h"
#include "ttp_utilities.h"


/****************************************************************************
Private Constant Definitions
*/

/** The minimum required transform buffer size.  */
#define SPLITTER_TRANSFORM_BUFFER_SIZE 128

/** Splitters frame size.  */
#define SPLITTER_DEFAULT_FRAME_SIZE 500

/** Maximum number of splitter channels */
#define SPLITTER_MAX_CHANS 4
/** The maximum number of streams to split the input channel into.
 * Only two outputs are supported! */
#define SPLITTER_MAX_OUTPUTS_PER_CHANNEL 2
/** Maximum number of splitter input channels */
#define SPLITTER_MAX_INPUTS SPLITTER_MAX_CHANS
/** Maximum number of splitter channels */
#define SPLITTER_MAX_OUTPUTS (SPLITTER_MAX_OUTPUTS_PER_CHANNEL * SPLITTER_MAX_CHANS)
/** The block size of the splitter reported to streams */
#define SPLITTER_BLOCK_SIZE 1
/** Value indicating an invalid terminal on the splitter */
#define SPLITTER_INVALID_TERMINAL UINT_MAX

#ifdef CAPABILITY_DOWNLOAD_BUILD
#define SPLITTER_CAP_ID CAP_ID_DOWNLOAD_SPLITTER
#else
#define SPLITTER_CAP_ID CAP_ID_SPLITTER
#endif

/** Splitter self-kick time, as a multiple of system kick period */
#define SPLITTER_SELF_KICK_RATIO 2

#define LAST_TAG_SAMPLES_INVALID    ((unsigned)-1)
#define LAST_TAG_TIMESTAMP_INVALID  ((unsigned)-1)

/****************************************************************************
Private Type Definitions
*/
typedef struct internal_buffer_t
{
    /* The buffer which holds the data. */
    tCbuffer* buffer;

    /* Each output will have a different progress in the buffer.
     * This progress is saved in the form of a read pointer to the
     * internal buffer. */
    int* read_pointers[SPLITTER_MAX_OUTPUTS_PER_CHANNEL];

    /* Just as with the read pointers the same applies to the metadata read pointer. */
    unsigned prev_rd_indexes[SPLITTER_MAX_OUTPUTS_PER_CHANNEL];

    /* Pointer to the last processed tag for the output. */
    metadata_tag *head_tag[SPLITTER_MAX_OUTPUTS_PER_CHANNEL];

}internal_buffer_t;

/* The enum holds all the possible output state. */
typedef enum
{
    /* Inactive streams will discard data. */
    INACTIVE = 0,

    /* Active streams will pass data.*/
    ACTIVE,

    /* Streams on hold will buffer data. */
    HOLD,

    LAST_STATE = HOLD,
}SPLITTER_OUTPUT_STATE;

/* Splitter channel descriptor  */
typedef struct splitter_channel_struct
{
   /* Next channel in the list. */
   struct splitter_channel_struct *next;

   /* Channel ID*/
   unsigned id;

   /* Input buffer of the stream. */
   tCbuffer *input_buffer;

   /* Output buffers of the stream */
   tCbuffer *output_buffer[SPLITTER_MAX_OUTPUTS_PER_CHANNEL];

   /* internal buffering related data. */
   internal_buffer_t internal;

   /* Output state.*/
   SPLITTER_OUTPUT_STATE output_state[SPLITTER_MAX_OUTPUTS_PER_CHANNEL];
} SPLITTER_CHANNEL_STRUC;

/**
 * Enum for the splitter workings modes.
 */
typedef enum
{
    /* The input buffer is cloned at the outputs. */
    CLONE_BUFFER = OPMSG_SPLITTER_CLONE_BUFFER,

    /* The data is buffer in a separete buffer. */
    BUFFER_DATA = OPMSG_SPLITTER_BUFFER_DATA,

    /* For sanity checks. */
    NR_OF_MODES
}SPLITTER_MODES;

/**
 * Enum for the splitter buffering locations.
 */
typedef enum
{
    /* The data is buffered in an internal buffer. */
    INTERNAL = OPMSG_SPLITTER_INTERNALLY,

    /* The data is buffer in an external SRAM. */
    SRAM = OPMSG_SPLITTER_SRAM,

    /* For sanity checks. */
    NR_OF_LOCATIONS
}SPLITTER_BUFFER_LOCATION;

/**
 * Enum to indicate if splitter is packing data.
 */
typedef enum
{
    /* Data will not be packed. */
    UNPACKED = OPMSG_SPLITTER_UNPACKED,

    /* Audio data will be packed*/
    PACKED = OPMSG_SPLITTER_PACKED,

    NR_OF_PACKING
}SPLITTER_PACKING;

/**
 * Enum to hold all the possible output stream states.
 * The current implementation only allows two output streams.
 */
typedef enum
{
    OUT_STREAM__0_OFF__1_OFF = 0,
    OUT_STREAM__0_ON___1_OFF,
    OUT_STREAM__0_OFF__1_ON,
    OUT_STREAM__0_ON___1_ON
}SPLITTER_STREAMS;

typedef struct
{
    /** The number of samples consumed */
    unsigned samples_consumed;

    /** The number of samples consumed */
    unsigned samples_produced;

    /** The input sample rate */
    uint32 sample_rate;

    /** Timestamp from the last metadata tag processed */
    TIME last_tag_timestamp;

    /** Sample period adjustment value from the last metadata tag */
    unsigned last_tag_spa;

    /** Samples read since the last metadata tag */
    unsigned last_tag_samples;

    /* Error offset ID from last tag */
    unsigned last_tag_err_offset_id;
}SPLITTER_METADATA_REFRAME;

typedef struct
{
    /** List of splitter channels */
    SPLITTER_CHANNEL_STRUC *channel_list;

    /* The working mode of the splitter operator. */
    SPLITTER_MODES working_mode;

    /* Represents the location of the buffered data.
     * This field is ignored when the working mode is
     * CLONE_BUFFER*/
    SPLITTER_BUFFER_LOCATION location;

    /* Represents the location of the buffered data.
     * This field is ignored when the working mode is
     * CLONE_BUFFER or when the data format is different
     * than AUDIO_DATA_FORMAT_FIXP or
     * AUDIO_DATA_FORMAT_FIXP_WITH_METADATA */
    SPLITTER_PACKING packing;

    /** The internal buffer size of the capability. */
    unsigned buffer_size;

    /**
     * Activate this to finish last tag when deactivating an output stream.
     * When an output stream is transitioning to inactive mode the last tag
     * at the output can remain unfinished. When the inactive stream is not
     * connected to another buffer this can cause metadata transport corruption.
     */
    bool finish_last_tag;

    /** The data format to advertise at connect */
    AUDIO_DATA_FORMAT data_format;

    /** The sinks to kick on when kicking backwards */
    unsigned touched_sinks;

    /** The sources to kick on when kicking forwards */
    unsigned touched_sources;

    /** Bitfield indicating which output streams are active. */
    SPLITTER_STREAMS active_streams:SPLITTER_MAX_OUTPUTS_PER_CHANNEL;

    /** Bitfield indicating which output streams are inactive. */
    SPLITTER_STREAMS inactive_streams:SPLITTER_MAX_OUTPUTS_PER_CHANNEL;

    /** Bitfield indicating which output streams are are buffering */
    SPLITTER_STREAMS hold_streams:SPLITTER_MAX_OUTPUTS_PER_CHANNEL;

    /** If true, the operator din't finish the transiton yet. This can happen
     * when transitioning to active, but there is not enough data in the
     * internal buffer.  */
    bool in_transition;

    /** Buffer that was supplied by buffer_details and should be provided by the
     * connect call. This is mainly stored in case connect fails before splitter_connect()
     * is called and we need to release this memory at a later time. */
    tCbuffer *curr_connecting;

    /** Timer ID used to kick operator to ensure data keeps flowing */
    tTimerId self_kick_timer;

    /* Timestamp where the operator will transition from consume to pass-through  */
    TIME_INTERVAL transition_point;

    /** If the operator reframes the tags the tag length will be equal with
     * the frame size. Furthermore when an output is on hold the operator
     * leaves a frame worth of space free in the internal/ external buffer. */
    unsigned frame_size;

    /** Reframe data */
    SPLITTER_METADATA_REFRAME reframe_data;

    /** True if splitter reframes the incoming timestamps. */
    bool reframe_enabled:1;

    /* Debug variables */
    bool internal_buffer_empty:1;
    bool internal_buffer_full:1;
    bool output_buffer_full:1;
} SPLITTER_OP_DATA;

/* A structure containing a set of cbuffer processing function pointers */
typedef struct
{
    unsigned (*unpacked_space)(tCbuffer *cbuffer);
    unsigned (*packed_space)(tCbuffer *cbuffer);
    unsigned (*data)(tCbuffer *cbuffer);
    unsigned (*pack)(tCbuffer *dst, tCbuffer *src, unsigned to_copy);
    unsigned (*unpack)(tCbuffer *dst, tCbuffer *src, unsigned to_copy);
    void (*advance_rd_ptr)(tCbuffer *cbuffer, unsigned to_advance);
    unsigned data_size;
}cbuffer_functions_t;

typedef struct
{
    unsigned channel_id;
    bool is_input;
    unsigned terminal_num;
    unsigned index;
    tCbuffer *buffer;
}terminal_info_t;


/****************************************************************************
Public Macro Declarations
*/
/**
 * Splitter debug messages.
 */
#if defined(SPLITTER_DEBUG)
#define SPLITTER_MSG(x)                 L2_DBG_MSG(x)
#define SPLITTER_MSG1(x, a)             L2_DBG_MSG1(x, a)
#define SPLITTER_MSG2(x, a, b)          L2_DBG_MSG2(x, a, b)
#define SPLITTER_MSG3(x, a, b, c)       L2_DBG_MSG3(x, a, b, c)
#define SPLITTER_MSG4(x, a, b, c, d)    L2_DBG_MSG4(x, a, b, c, d)
#define SPLITTER_MSG5(x, a, b, c, d, e) L2_DBG_MSG5(x, a, b, c, d, e)
#else
#define SPLITTER_MSG(x)                 ((void)0)
#define SPLITTER_MSG1(x, a)             ((void)0)
#define SPLITTER_MSG2(x, a, b)          ((void)0)
#define SPLITTER_MSG3(x, a, b, c)       ((void)0)
#define SPLITTER_MSG4(x, a, b, c, d)    ((void)0)
#define SPLITTER_MSG5(x, a, b, c, d, e) ((void)0)
#endif /* SPLITTER_DEBUG */

#define SPLITTER_ERRORMSG(x)                 L1_DBG_MSG(x)
#define SPLITTER_ERRORMSG1(x, a)             L1_DBG_MSG1(x, a)
#define SPLITTER_ERRORMSG2(x, a, b)          L1_DBG_MSG2(x, a, b)
#define SPLITTER_ERRORMSG3(x, a, b, c)       L1_DBG_MSG3(x, a, b, c)
#define SPLITTER_ERRORMSG4(x, a, b, c, d)    L1_DBG_MSG4(x, a, b, c, d)
#define SPLITTER_ERRORMSG5(x, a, b, c, d, e) L1_DBG_MSG5(x, a, b, c, d, e)

/****************************************************************************
Public Function Declarations
*/
bool get_terminal_info(unsigned terminal_id, tCbuffer *buffer, terminal_info_t *terminal_info, bool read_buffer);
SPLITTER_CHANNEL_STRUC *create_channel(SPLITTER_OP_DATA *splitter, tCbuffer* internal_metadata_buff);
SPLITTER_CHANNEL_STRUC *get_channel_struct(SPLITTER_OP_DATA *splitter, unsigned channel_id);
void delete_channel(SPLITTER_CHANNEL_STRUC *channel, bool last_channel);
void delete_disconnected_channel(SPLITTER_OP_DATA *splitter);
tCbuffer* get_internal_metadata_buffer(SPLITTER_OP_DATA *splitter);
tCbuffer* get_metadata_buffer(SPLITTER_OP_DATA *splitter, bool is_input, unsigned index);

const cbuffer_functions_t *get_cbuffer_functions(SPLITTER_OP_DATA *splitter);
bool splitter_transiton(SPLITTER_OP_DATA *splitter, const cbuffer_functions_t *cbuffer);

#ifdef INSTALL_CBUFFER_EX
void cbuffer_copy_packed_to_audio_shift(tCbuffer * dst, tCbuffer *src, unsigned num_octets);
void cbuffer_copy_audio_shift_to_packed(tCbuffer * dst, tCbuffer *src, unsigned num_octets);

unsigned un_pack_audio(tCbuffer * dst, tCbuffer *src, unsigned num_samples);
unsigned pack_audio(tCbuffer * dst, tCbuffer *src, unsigned num_samples);

void advance_packed(tCbuffer *cbuffer, unsigned num_samples);
unsigned packed_space(tCbuffer *cbuffer);
#endif

/****************************************************************************
Private Function Definitions
*/
/**
 * Helper function to get the channel which is used to transport metadata.
 */
static inline  SPLITTER_CHANNEL_STRUC* get_metadata_channel(SPLITTER_OP_DATA *splitter)
{
    return splitter->channel_list;
}

static inline SPLITTER_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (SPLITTER_OP_DATA *) base_op_get_instance_data(op_data);
}

/**
 * Checks if the bitfield received is within range.
 */
static inline bool invalid_stream_setting(unsigned stream_bitfield)
{
    return (stream_bitfield > (OPMSG_SPLITTER_STREAM_0 | OPMSG_SPLITTER_STREAM_1));
}

static inline bool check_output_state_validity(SPLITTER_OP_DATA *splitter)
{
    /* Check if flags for the output stream indexes are only set for state only. */
    return (
            splitter->active_streams ^ splitter->inactive_streams ^ splitter->hold_streams
           ) == (
            (1<<SPLITTER_MAX_OUTPUTS_PER_CHANNEL) - 1
           );
}

#endif /* SPLITTER_PRIVATE_H */
