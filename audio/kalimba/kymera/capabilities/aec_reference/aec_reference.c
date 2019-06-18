/**
 * Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
 * \file  aec_reference.c
 * \ingroup  capabilities
 *
 *  AEC Reference
 *
 */

/****************************************************************************
Include Files
*/
#include "aec_reference_cap_c.h"

#include "adaptor/adaptor.h"

#include "platform/pl_intrinsics.h"
#include "platform/pl_fractional.h"
#include "platform/pl_interrupt.h"
#include "ps/ps.h"
#include "obpm_prim.h"
#include "mem_utils/shared_memory_ids.h"
#include "aec_reference_config.h"
#include "op_msg_helpers.h"
#include "opmgr/opmgr_for_ops.h"
#include "patch/patch.h"
#include "pl_assert.h"

/****************************************************************************
Public Type Declarations
*/

/* Period of Timer Task */
#ifndef CHIP_BASE_A7DA_KAS
#define AEC_REFERENCE_TIME_PERIOD   1000
#define AEC_REFERENCE_FRACTIONAL_PERIOD  FRACTIONAL(0.001)
#else
#define AEC_REFERENCE_TIME_PERIOD   900
#define AEC_REFERENCE_FRACTIONAL_PERIOD  FRACTIONAL(0.001)
#endif

#define AECREF_TICKS_PER_SECOND  1000

#ifdef CAPABILITY_DOWNLOAD_BUILD
#define AEC_REFERENCE_CAP_ID CAP_ID_DOWNLOAD_AEC_REFERENCE
#else
#define AEC_REFERENCE_CAP_ID CAP_ID_AEC_REFERENCE
#endif

/****************************************************************************
Private Constant Definitions
*/

/* Macro to enable debug logs for creating
 * graphs.
 * Note: this shouldn't be enabled permanently
 */
#define DEBUG_GRAPHS_BUILDx
#ifdef DEBUG_GRAPHS_BUILD
#define DEBUG_GRAPHS(x) L2_DBG_MSG1(x " - time = 0x%08x", hal_get_time())
#else
#define DEBUG_GRAPHS(x) ((void)0)
#endif

/* Message handlers */

/** The aec_reference capability function handler table */
const handler_lookup_struct aec_reference_handler_table =
{
    aec_reference_create,          /* OPCMD_CREATE */
    aec_reference_destroy,         /* OPCMD_DESTROY */
    aec_reference_start,           /* OPCMD_START */
    aec_reference_stop,            /* OPCMD_STOP */
    aec_reference_reset,           /* OPCMD_RESET */
    aec_reference_connect,         /* OPCMD_CONNECT */
    aec_reference_disconnect,      /* OPCMD_DISCONNECT */
    aec_reference_buffer_details,  /* OPCMD_BUFFER_DETAILS */
    aec_reference_get_data_format, /* OPCMD_DATA_FORMAT */
    aec_reference_get_sched_info   /* OPCMD_GET_SCHED_INFO */
};

/* Null terminated operator message handler table */

const opmsg_handler_lookup_table_entry aec_reference_opmsg_handler_table[] =
{{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION,           base_op_opmsg_get_capability_version},

 {OPMSG_COMMON_ID_SET_CONTROL,                       aec_reference_opmsg_obpm_set_control},
 {OPMSG_COMMON_ID_GET_PARAMS,                        aec_reference_opmsg_obpm_get_params},
 {OPMSG_COMMON_ID_GET_DEFAULTS,                      aec_reference_opmsg_obpm_get_defaults},
 {OPMSG_COMMON_ID_SET_PARAMS,                        aec_reference_opmsg_obpm_set_params},
 {OPMSG_COMMON_ID_GET_STATUS,                        aec_reference_opmsg_obpm_get_status},

 {OPMSG_COMMON_GET_CONFIGURATION,                    aec_reference_opmsg_ep_get_config},
 {OPMSG_COMMON_CONFIGURE,                            aec_reference_opmsg_ep_configure},
 {OPMSG_COMMON_GET_CLOCK_ID,                         aec_reference_opmsg_ep_clock_id},

 {OPMSG_AEC_REFERENCE_ID_SET_SAMPLE_RATES,           aec_reference_set_rates},

 {OPMSG_COMMON_ID_SET_UCID,                          aec_reference_opmsg_set_ucid},
 {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,                 aec_reference_opmsg_get_ps_id},

 {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,                 aec_reference_opmsg_get_ps_id},
 {OPMSG_AEC_REFERENCE_ID_MUTE_MIC_OUTPUT,            aec_reference_opmsg_mute_mic_output},
#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
 {OPMSG_COMMON_SET_TTP_LATENCY,                      aec_reference_opmsg_set_ttp_latency},
 {OPMSG_COMMON_SET_LATENCY_LIMITS,                   aec_reference_opmsg_set_latency_limits},
 {OPMSG_COMMON_SET_TTP_PARAMS,                       aec_reference_opmsg_set_ttp_params},
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */
 {OPMSG_AEC_REFERENCE_ID_SAME_INPUT_OUTPUT_CLK_SOURCE, aec_reference_opmsg_enable_mic_sync},
 {OPMSG_COMMON_ID_SET_TERMINAL_BUFFER_SIZE, aec_reference_opmsg_set_buffer_size},

 {0, NULL}};


/* Supports up to 8 MICs, 2 SPKRS, and AEC Reference */

const CAPABILITY_DATA aec_reference_cap_data =
{
    AEC_REFERENCE_CAP_ID,              /* Capability ID */
    AEC_REFERENCE_AECREF_VERSION_MAJOR, AEC_REFERENCE_CAP_VERSION_MINOR, /* Version information - hi and lo parts */
    AEC_REF_NUM_SINK_TERMINALS, AEC_REF_NUM_SOURCE_TERMINALS,            /* Max number of sinks/inputs and sources/outputs */
    &aec_reference_handler_table,       /* Pointer to message handler function table */
    aec_reference_opmsg_handler_table,  /* Pointer to operator message handler function table */
    base_op_process_data,               /* Pointer to data processing function */
    0,                                  /* TODO - Processing time information */
    sizeof(AEC_REFERENCE_OP_DATA)       /* Size of capability-specific per-instance data */
};


/****************************************************************************
Public Function Declarations
*/

#define SIDETONE_ENABLE_FLAG     0x02
#define SIDETONE_MIC_SPKR_FLAG   0x01
#define USE_SIDETONE_FLAG        0x03

/* ********************************** API functions ************************************* */
#if defined(IO_DEBUG)
aec_latency_common *aec_latency_ptr = NULL;
#endif


bool aec_reference_create(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    patch_fn_shared(aec_reference);

    /* Setup Response to Creation Request.   Assume Failure*/
    *response_id = OPCMD_CREATE;
    if(!base_op_build_std_response(STATUS_CMD_FAILED,op_data->id,response_data))
    {
        return(FALSE);
    }

    /* Processing is in timer task.  This prevents kicks from
       propagating to the main processing function */
    op_data->stop_chain_kicks = TRUE;

    /* Initialize extended data for operator.  Assume intialized to zero*/
    op_extra_data->cap_id = op_data->cap_data->id;
    op_extra_data->ReInitFlag = TRUE;
    op_extra_data->Cur_mode = AEC_REFERENCE_SYSMODE_FULL;
    op_extra_data->kick_id = TIMER_ID_INVALID;

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* set minimum tag length for mic output metadata tags */
    op_extra_data->mic_metadata_min_tag_len = AEC_REFERENCE_MIC_METADATA_MIN_TAG_LEN;

    /* create time to play with default params */
    op_extra_data->mic_time_to_play = ttp_init();
    if (op_extra_data->mic_time_to_play != NULL)
    {
        ttp_params params;
        ttp_get_default_params(&params, TTP_TYPE_PCM);
        ttp_configure_params(op_extra_data->mic_time_to_play, &params);
    }
    else
    {
        goto aFailed;
    }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

    /* Task Period as fraction - this field is currently a fixed constant,
       but is not declared as such because we may want it to be configurable in the future */

    /* For Atlas this must be less than for equal to the ping/pong period.
       Also set AEC_REFERENCE_TIME_PERIOD
    */
    op_extra_data->task_period = AEC_REFERENCE_FRACTIONAL_PERIOD;
    op_extra_data->mic_rate_ability  = RATEMATCHING_SUPPORT_NONE;
    op_extra_data->spkr_rate_ability = RATEMATCHING_SUPPORT_NONE;
    op_extra_data->mic_shift  = 8;
    op_extra_data->spkr_shift = -8;

    /* ATlas does not use sidetone so don't add it */
#if !defined(CHIP_BASE_A7DA_KAS)
    op_extra_data->using_sidetone = SIDETONE_ENABLE_FLAG;
#else
    op_extra_data->using_sidetone = 0;
#endif

    /* Note:  sample rate config must be sent before the operator's terminals may be connected
       input_rate and output_rate are initialized to zero and checked in the connect operation */

    /*allocate the volume control shared memory */
    op_extra_data->shared_volume_ptr = allocate_shared_volume_cntrl();
    if(!op_extra_data->shared_volume_ptr)
    {
        goto aFailed;
    }

    if(!cpsInitParameters(&op_extra_data->parms_def,(unsigned*)AEC_REFERENCE_GetDefaults(op_extra_data->cap_id),(unsigned*)&op_extra_data->params,sizeof(AEC_REFERENCE_PARAMETERS)))
    {
        goto aFailed;
    }

#if defined(IO_DEBUG)
    aec_latency_ptr = &op_extra_data->sync_block;
#endif

    /* operator state variable set to not running state. based on base_op.c */
    op_data->state = OP_NOT_RUNNING;

    /* chance to fix up */
    patch_fn_shared(aec_reference);

    base_op_change_response_status(response_data,STATUS_OK);
    return TRUE;
  aFailed:
    if(op_extra_data->shared_volume_ptr)
    {
        release_shared_volume_cntrl(op_extra_data->shared_volume_ptr);
        op_extra_data->shared_volume_ptr = NULL;
    }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* free it if we created time to play context for mic */
    if (op_extra_data->mic_time_to_play != NULL)
    {
        ttp_free(op_extra_data->mic_time_to_play);
        op_extra_data->mic_time_to_play = NULL;
    }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

    base_op_change_response_status(response_data, STATUS_CMD_FAILED);
    return TRUE;
}


void aec_reference_set_mic_gains(OPERATOR_DATA *op_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*) op_data->extra_op_data;
    unsigned i, *lpadcgains = (unsigned*)&op_extra_data->params.OFFSET_ADC_GAIN1;

    patch_fn_shared(aec_reference);

    for(i=0;i<MAX_NUMBER_MICS;i++)
    {
        if(op_extra_data->input_stream[MicrophoneTerminalByIndex(i)])
        {
            uint32 config_value = (uint32)(lpadcgains[i]);
            set_override_ep_gain(get_overriden_endpoint(op_data->id,MicrophoneTerminalByIndex(i) | TERMINAL_SINK_MASK),config_value);
        }
    }
}

#if defined(IO_DEBUG)
cbops_op *insert_op  = NULL;
cbops_op *st_disgard_op = NULL;
#endif
/**
 * aec_reference_cleanup
 * \brief clean up the aec-reference operator internal states
 *
 * \param op_data Pointer to the AEC reference operator data.
 *
 * Note: This function is the same as aec_reference_cleanup_graphs
 *       except that it will reset the entire channel status so any new attempt
 *       to build the graphs will rebuild everything from scratch.
 */
void aec_reference_cleanup(OPERATOR_DATA *op_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    patch_fn_shared(aec_reference);

    /* cleanup all the graphs */
    aec_reference_cleanup_graphs(op_extra_data);

    /* reset channel status, so any new trying
     * of building graphs will rebuild every thing
     * from scratch
     */
    op_extra_data->channel_status = 0;
}

bool aec_reference_destroy(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    patch_fn_shared(aec_reference);

    /* Setup Response to Destroy Request.*/
    if(!base_op_destroy(op_data, message_data, response_id, response_data))
    {
        return(FALSE);
    }

    /* calling the "destroy" assembly function - this frees up all the capability-internal memory */
    /*free volume control shared memory*/
    release_shared_volume_cntrl(op_extra_data->shared_volume_ptr);
    op_extra_data->shared_volume_ptr = NULL;

    /* Make sure everything is cleared */
    aec_reference_cleanup(op_data);

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* delete mic time-to-play object */
    if(op_extra_data->mic_time_to_play != NULL)
    {
        ttp_free(op_extra_data->mic_time_to_play);
        op_extra_data->mic_time_to_play = NULL;
    }
#endif

    base_op_change_response_status(response_data,STATUS_OK);
    return(TRUE);
}

/**
 * build_mic_graph
 * \brief builds cbops graph for microphone path based on the relevant
 *        connections and configurations. Th microphone graph is built
 *        whenever there is a change in microphone channel status.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool build_mic_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    cbops_graph *mic_graph;
    cbops_op *op_ptr;
    unsigned i, num_mics = op_extra_data->num_mic_channels,j,num_io;
    unsigned* idxs;
    unsigned  out_idx,intern_idx;
    unsigned  st_idx=0,st_rs_idx=0;

    patch_fn_shared(aec_reference);

    /*********************************************************************

    MICs --> RM --> DC_REMOVE -+--> resample --> OUT
                               |
                            ST filter
                               |
                             resample
                               |
                             SIDETONE

    **********************************************************************/

    /* Is there a MIC path */
    if(!(op_extra_data->channel_status&CHAN_STATE_MIC_MASK))
    {
        return(TRUE);
    }

    /* see if we need sidetone path */
    if((op_extra_data->num_mic_channels > 0) && (op_extra_data->num_spkr_channels > 0))
    {
        op_extra_data->using_sidetone |= SIDETONE_MIC_SPKR_FLAG;
    }
    else
    {
        op_extra_data->using_sidetone &= SIDETONE_ENABLE_FLAG;
    }

    /* create shared sidetone buffer */
    if(op_extra_data->using_sidetone == USE_SIDETONE_FLAG)
    {
        /* we don't expect SidetoneOA buffer already existing at this point */
        PL_ASSERT(op_extra_data->sidetone_buf == NULL);

        /* Allocate Buffer between cbops Graphs */
        op_extra_data->sidetone_buf = cbuffer_create_with_malloc_fast(3*frac_mult(op_extra_data->spkr_rate, AEC_REFERENCE_FRACTIONAL_PERIOD), BUF_DESC_SW_BUFFER);
        if(!op_extra_data->sidetone_buf)
        {
            /* Not going ahead with creating mic graph if we
             * cannot create shared buffer for sidetone path.
             */
            return FALSE;
        }
    }
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
    /* see if mic graph wants to use an external rate adjust operator */
    aec_reference_mic_check_external_rate_adjust_op(op_extra_data);
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */

    /* number of io buffers
       num_mic*[IN,INTERN,OUT] plus SideTONE

       Buffer Order
       MIC Inputs
       MIC Output
       MIC Internal
       SIDETONE OUT
    */

    out_idx    = num_mics;
    intern_idx = out_idx;
    num_io     = 2*num_mics;

    if(op_extra_data->using_sidetone==USE_SIDETONE_FLAG)
    {
        /* Need internal buffer between input and output */
        intern_idx = num_io;
        num_io    += num_mics;

        /* sidetone output */
        st_idx        = num_io;
        num_io++;

        st_rs_idx = st_idx;
        if(op_extra_data->mic_rate!=op_extra_data->spkr_rate)
        {
            /* Sidetone is inplace */
            st_rs_idx = intern_idx;
        }
    }
    else if( (op_extra_data->mic_rate_ability==RATEMATCHING_SUPPORT_SW) &&
             (op_extra_data->mic_rate!=op_extra_data->output_rate))
    {
        /* Need internal buffer between input and output */
        intern_idx = num_io;
        num_io    += num_mics;
    }
    else if (op_extra_data->mic_rate!=op_extra_data->output_rate)
    {
        /* No ratematching or sidetone.  Just resampling  */
        intern_idx = 0;
    }

    idxs = create_default_indexes(num_io);
    if(idxs == NULL)
    {
        return(FALSE);
    }

    /* Allocate mic graph */
    mic_graph = cbops_alloc_graph(num_io);
    if(!mic_graph)
    {
        goto aFailed;
    }
    op_extra_data->mic_graph = mic_graph;

    /* Setup IO Buffers*/
    for(j=0,i=0;(j<MAX_NUMBER_MICS);j++)
    {
        tCbuffer    *buffer_ptr_src,*buffer_ptr_snk;
        /* MICs may not be consecutive */
        buffer_ptr_src = op_extra_data->input_stream[MicrophoneTerminalByIndex(j)];
        buffer_ptr_snk = op_extra_data->output_stream[OutputTerminalByIndex(j)];
        if(buffer_ptr_src && buffer_ptr_snk && (i < num_mics))
        {
            /* Inputs */
            cbops_set_input_io_buffer(mic_graph,i,0,buffer_ptr_src);
            /* Outputs */
            cbops_set_output_io_buffer(mic_graph,out_idx+i,out_idx,buffer_ptr_snk);
            i++;
        }
    }

    if(intern_idx > out_idx)
    {
        for(i=0;i<num_mics;i++)
        {
            cbops_set_internal_io_buffer(mic_graph,intern_idx+i,intern_idx,op_extra_data->scratch_bufs[i]);
        }
    }

    op_extra_data->mic_rate_adjustment = 0;
#ifndef CHIP_BASE_A7DA_KAS
    /* Monitor rate */
    op_ptr = create_rate_monitor_op(AECREF_TICKS_PER_SECOND,0);

    if(!op_ptr)
    {
        goto aFailed;
    }

    if(op_extra_data->mic_rate_ability==RATEMATCHING_SUPPORT_HW)
    {
        rate_monitor_op_initiatlize(op_ptr,op_extra_data->mic_rate,TRUE,3*MS_PER_SEC);
    }
    else
    {
        rate_monitor_op_initiatlize(op_ptr,op_extra_data->mic_rate,FALSE,5*MS_PER_SEC);
    }

    op_extra_data->mic_rate_monitor_op = op_ptr;

    cbops_append_operator_to_graph(mic_graph,op_ptr);
#endif

    /* Handle input */
    if(op_extra_data->mic_rate_ability == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
       && (0 == op_extra_data->mic_ext_rate_adjust_op)
#endif
       )
    {
        /* Apply Software Rate Adjustment */
        op_ptr = create_sw_rate_adj_op(num_mics, idxs, &idxs[intern_idx],
                                       CBOPS_RATEADJUST_COEFFS,
                                       &op_extra_data->mic_rate_adjustment, op_extra_data->mic_shift);
        if(!op_ptr)
        {
            goto aFailed;
        }

        op_extra_data->mic_sw_rateadj_op=op_ptr;
        cbops_rateadjust_passthrough_mode(op_ptr,(op_extra_data->mic_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);

        cbops_append_operator_to_graph(mic_graph,op_ptr);

        /* Early DC remove on mic path. Before Sidetone split so the signal split
           to the speaker doesn't have DC */
        op_ptr = create_dc_remove_op(num_mics, &idxs[intern_idx], &idxs[intern_idx]);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);

    }
    else if(intern_idx != 0)
    {
        /* Otherwise, just copy data to next section */
        op_ptr = create_shift_op(num_mics, idxs, &idxs[intern_idx], op_extra_data->mic_shift);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);

        /* Early DC remove on mic path. Before Sidetone split so the signal split
           to the speaker doesn't have DC */
        op_ptr = create_dc_remove_op(num_mics, &idxs[intern_idx], &idxs[intern_idx]);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }

    /* Handle output */
    if(op_extra_data->mic_rate!=op_extra_data->output_rate)
    {
        /* If only operator then shift input to output */
        unsigned shift_amount = (intern_idx != 0) ? 0 :  op_extra_data->mic_shift;

        /*  Add Resampler Operators per channel (mic_rate --> output_rate) */
        op_ptr = create_iir_resamplerv2_op(num_mics, &idxs[intern_idx], &idxs[out_idx],
                                           op_extra_data->mic_rate,op_extra_data->output_rate,
                                           AEC_REF_RESAMPLER_TEMP_BUF_SIZE,
                                           op_extra_data->resampler_temp_buffer, shift_amount, 0, 0);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }
    else if (st_idx != 0)
    {
        /* Need to copy internal buffer to output before sidetone */
        op_ptr = create_shift_op(num_mics,&idxs[intern_idx], &idxs[out_idx],0);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }

    /* Handle sidetone */
    if(st_idx != 0)
    {
        /* Minimum space needed in buffer */
        unsigned threshold = frac_mult(op_extra_data->spkr_rate,op_extra_data->task_period)+1;

        cbops_set_output_io_buffer(mic_graph,st_idx,st_idx,op_extra_data->sidetone_buf);

        /*  Add Sidetone operators:
            Sidetone Apply Operator
            Note:  Sidetone is before resampler.
            Better solution is to place it at lowest sample rate */

        op_ptr = create_sidetone_filter_op(intern_idx, st_rs_idx, 3,
                                           (cbops_sidetone_params*)&op_extra_data->params.OFFSET_ST_CLIP_POINT,
                                           (void*)&op_extra_data->params.OFFSET_ST_PEQ_CONFIG);

        op_extra_data->mic_sidetone_op = op_ptr;
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);

        if(st_rs_idx!=st_idx)
        {
            // create resampler only for one in & out channel
            op_ptr = create_iir_resamplerv2_op(1, &idxs[st_rs_idx], &idxs[st_idx], op_extra_data->mic_rate, op_extra_data->spkr_rate,
                                               AEC_REF_RESAMPLER_TEMP_BUF_SIZE, op_extra_data->resampler_temp_buffer, 0, 0, 0);
            if(!op_ptr)
            {
                goto aFailed;
            }
            cbops_append_operator_to_graph(mic_graph,op_ptr);
        }

        /* Add in disgard on sidetone */

        op_ptr = create_sink_overflow_disgard_op(1,&idxs[st_idx],threshold);
#if defined(IO_DEBUG)
        st_disgard_op = op_ptr;
#endif
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(mic_graph,op_ptr);
    }

    /* add mute operator, it can be controlled by
     * message to the operator
     */
    op_ptr = create_mute_op(num_mics, &idxs[out_idx]);
    if(!op_ptr)
    {
        goto aFailed;
    }
    cbops_append_operator_to_graph(mic_graph,op_ptr);
    op_extra_data->mic_mute_op = op_ptr;

    /* Insert latency operator */
    op_ptr = create_mic_latency_op(out_idx,&op_extra_data->sync_block);
    if(!op_ptr)
    {
        goto aFailed;
    }
    cbops_append_operator_to_graph(mic_graph,op_ptr);

    pfree(idxs);

    /* Each mic input has a corresponding output,
     * so it's safe to purge all channels now
     */
    aec_ref_purge_mics(mic_graph,num_mics);

    DEBUG_GRAPHS("AEC REFERENCE: Building mic graph, Done!" );

    return(TRUE);
  aFailed:
    DEBUG_GRAPHS("AEC REFERENCE: Building mic graph, Failed!");

    pfree(idxs);
    return(FALSE);
}

/**
 * build_spkr_graph
 * \brief builds cbops graph for speaker path based on the relevant
 *        connections and configurations. Th speaker graph is rebuilt
 *        whenever there is a change in speaker channel status.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 *
 * Note: Speaker path can have reference sub-path and/or side-tone mix
 *       depending on the connections and microphone state, however they
 *       aren't included by this function, instead they are added to the
 *       speaker graph later.
 */
bool build_spkr_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    unsigned    i,j,k,num_spkrs,num_inputs;
    cbops_graph *spkr_graph;
    cbops_op    *op_ptr;
    cbops_op    *overrid_op_ptr;
    unsigned    num_io;
    tCbuffer    *buffer_ptr;
    unsigned    *idxs;
    unsigned    out_indx,intern_ins_idx,intern_rs_idx,intern_rm_idx;
    unsigned    spkr_channel_status = GetSpkrChannelStatus(op_extra_data);

    patch_fn_shared(aec_reference);

    /*********************************************************************

                                           RM ---> resample --> REF
                                             |
    IN --> INSERT --> DC_REMOVE -->  MIXER --+--> resample --> Sidetone Mix --> RM ----------> SPKR
   (0)                                   (intern_ins_idx)    (intern_rs_idx)  (intern_rm_idx)  (out_indx)

   Note: Addition of "Reference sub-path" and "Sidetone Mix" aren't done in
         this function. These two are inserted into the speaker graph later
         if required. Both of these two can also be dynamically removed from the graph
         when they are no longer required.

         "Sidetone Mix" is added/removed to/from the graph by aec_reference_spkr_include_sidetone function.
         "Reference sub-path" is added/removed to/from the graph by aec_reference_spkr_include_ref_path function.

    **********************************************************************/
    /* Is there a Speaker path */
    if(!(op_extra_data->channel_status&CHAN_STATE_SPKR_MASK))
    {
        return(TRUE);
    }

#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
    /* see if speaker graph wants to use an external rate adjust operator */
    aec_reference_spkr_check_external_rate_adjust_op(op_extra_data);
#endif

    /* Determine IO configuration */
    num_spkrs  = op_extra_data->num_spkr_channels;
    num_inputs = op_extra_data->num_spkr_channels;
    if(!(spkr_channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_PARA))
    {
        if(spkr_channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_MIX)
        {
            /* Multiple inputs, mono output */
            num_spkrs=1;
        }
        else
        {
            /* Mono input, multiple outputs */
            num_inputs=1;
        }
    }

    /* outputs follow inputs */
    /* Internal buffers for output of insert follow outputs */
    out_indx       = num_inputs;

    /* Count the buffers needed */
    num_io         = num_inputs+num_spkrs;

    /* Assume no resampler or rate matching */
    intern_rs_idx = 0;
    intern_rm_idx = 0;
    intern_ins_idx = 0;

    /* reserve one index for sidetone input buffer */
    op_extra_data->spkr_st_in_idx = num_io;
    num_io++;

    /* Check if resampler is required */
    if(op_extra_data->input_rate!=op_extra_data->spkr_rate)
    {
        intern_rs_idx = out_indx;

        if(op_extra_data->spkr_rate_ability==RATEMATCHING_SUPPORT_SW)
        {
            /* Need buffers following resampler */
            intern_rs_idx    = num_io;
            if(num_spkrs>num_inputs)
            {
                /* Resampler is Mono.  Feeds shift */
                num_io++;
            }
            else
            {
                /* Resampler is multi channel */
                num_io += num_inputs;
            }
        }
    }

    /* Check if sw rate matching is required */
    if(op_extra_data->spkr_rate_ability == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
       /* not required if having access to external rate adjust op */
       && (0 == op_extra_data->spkr_ext_rate_adjust_op)
#endif
       )
    {
        intern_rm_idx = out_indx;
        /* Split needs buffer following rate matching (Mono)*/
        if(num_spkrs>num_inputs)
        {
            intern_rm_idx = num_io;
            num_io++;
        }
    }

    /* reserve 2 indexes for reference sub-path,
     * one is for reference output buffer, the
     * other one for scratch buffer for possible
     * resampler in the sub-path.
     */
    op_extra_data->spkr_ref_idx  = num_io;
    num_io +=2;

    /* Allocate buffer index array for easy setup */
    if(num_inputs<num_spkrs)
    {
        idxs = create_default_indexes(num_io+num_spkrs);
    }
    else
    {
        idxs = create_default_indexes(num_io);
    }
    if(idxs == NULL)
    {
        return(FALSE);
    }

    /* Allocate num_io buffers in spkr graph */
    spkr_graph = cbops_alloc_graph(num_io);
    if(!spkr_graph)
    {
        goto aFailed;
    }
    op_extra_data->spkr_graph = spkr_graph;

    /* Setup IO Buffers
       Buffer Order
       Inputs
       SPKR Outputs
       Internal (after insert)          : scratch[0:num_inputs-1]
       Internal (after resample)
       Internal (after rate match)
       Internal (after ref resample)    : scratch[num_inputs]
       REF OUT
    */

    /* Input, Output, and insert_op buffers */
    for(j=0,i=0,k=0;j<MAX_NUMBER_SPEAKERS;j++)
    {
        /* Inputs may not be contiguous */
        buffer_ptr =  op_extra_data->input_stream[SpeakerInputTerminalByIndex(j)];
        if(buffer_ptr)
        {
            /* Inputs */
            cbops_set_input_io_buffer(spkr_graph,i,0,buffer_ptr);
            i++;
        }
        /* Outputs may not be contiguous */
        buffer_ptr =  op_extra_data->output_stream[SpeakerTerminalByIndex(j)];
        if(buffer_ptr)
        {
            cbops_set_output_io_buffer(spkr_graph,out_indx + k,out_indx,buffer_ptr);
            k++;
        }
    }

    /* DC remove before reference tap */
    op_ptr = create_dc_remove_op(num_inputs, idxs, idxs);
    if(!op_ptr)
    {
        goto aFailed;
    }
    cbops_append_operator_to_graph(spkr_graph,op_ptr);

    /* Thresholds for Insertion, keep a copy in main structure */
    op_extra_data->spkr_in_threshold  = frac_mult(op_extra_data->input_rate,op_extra_data->task_period)+1;
    op_extra_data->spkr_out_threshold = frac_mult(op_extra_data->spkr_rate,op_extra_data->task_period)+1;

    overrid_op_ptr = create_aec_ref_spkr_op(num_inputs, idxs,
                                            op_extra_data->spkr_in_threshold,
                                            num_spkrs,&idxs[out_indx],
                                            op_extra_data->spkr_out_threshold);
#if defined(IO_DEBUG)
    insert_op  = overrid_op_ptr;
#endif
    if(!overrid_op_ptr)
    {
        goto aFailed;
    }

    cbops_set_override_operator(spkr_graph,overrid_op_ptr);

    /* Add Mixer to section #1 if needed*/
    if(num_inputs > num_spkrs)
    {
        for(i=1;i<num_inputs;i++)
        {
            /* NOTE: left scratch input reused for output (inplace) */
            /*  - Add Mix Operator - stereo to mono (left = (left+right)/2 */
            op_ptr = create_mixer_op(intern_ins_idx, intern_ins_idx+i, intern_ins_idx, 0, FRACTIONAL(0.5));
            if(!op_ptr)
            {
                goto aFailed;
            }
            cbops_append_operator_to_graph(spkr_graph,op_ptr);
        }
        /* Input is now mono */
        num_inputs = 1;
    }

    /* This is the point where we take input for the
     * reference sub-path, store information about
     * where in the graph the reference sub-path
     * should be inserted to.
     */
    op_extra_data->spkr_ref_input_idx = intern_ins_idx;
    op_extra_data->spkr_ref_point_op = op_ptr;
    op_extra_data->spkr_ref_scratch_idx = num_inputs;

    /* Add sample rate conversion per channel [num_inputs] (input_rate --> spkr_rate) */
    if(intern_rs_idx!=0)
    {
        int shift_amount = op_extra_data->spkr_shift;

        if(intern_rs_idx!=out_indx)
        {
            /* Buffers for output of resampler. */
            for(i=0; i < num_inputs ; i++)
            {
                cbops_set_internal_io_buffer(spkr_graph,intern_rs_idx+i,intern_rs_idx,op_extra_data->scratch_bufs[num_inputs+i]);
            }
            shift_amount=0;
        }

        op_ptr = create_iir_resamplerv2_op(num_inputs, &idxs[intern_ins_idx], &idxs[intern_rs_idx],
                                           op_extra_data->input_rate, op_extra_data->spkr_rate,
                                           AEC_REF_RESAMPLER_TEMP_BUF_SIZE,
                                           op_extra_data->resampler_temp_buffer, shift_amount, 0, 0);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(spkr_graph,op_ptr);

        /* Move next input to output of resampler */
        intern_ins_idx = intern_rs_idx;
    }

    /* This is the point where we insert possible sidetone
     * mix operator, store both the operator and the input
     * buffer index for the operator. Sidetone mixing
     * will always be done in-place.
     */
    op_extra_data->spkr_st_point_op = op_ptr;
    op_extra_data->spkr_stmix_in_idx = intern_ins_idx;

    /* Optional section for SW rate matching */
    if(intern_rm_idx != 0)
    {
        int shift_amount =  op_extra_data->spkr_shift;

        if(intern_rm_idx!=out_indx)
        {
            /* Use alternate set of scratch buffers from resampler */
            unsigned scratch_offset = (intern_rs_idx!=0)? 0 : num_inputs;

            /* Buffers for output of ratematch if it exists */
            for(i=0; i < num_inputs; i++)
            {
                cbops_set_internal_io_buffer(spkr_graph,intern_rm_idx+i,intern_rm_idx,op_extra_data->scratch_bufs[scratch_offset+i]);
            }
            shift_amount=0;
        }

        /*  SW rate adjustment per channel [num_inputs] */
        op_ptr = create_sw_rate_adj_op(num_inputs, &idxs[intern_ins_idx], &idxs[intern_rm_idx],
                                       CBOPS_RATEADJUST_COEFFS,
                                       &op_extra_data->spkr_rate_adjustment,shift_amount);
        if(!op_ptr)
        {
            goto aFailed;
        }

        op_extra_data->spkr_sw_rateadj_op=op_ptr;
        cbops_rateadjust_passthrough_mode(op_ptr,(op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);

        cbops_append_operator_to_graph(spkr_graph,op_ptr);

        /* Move next input to output of ratematch */
        intern_ins_idx = intern_rm_idx;
    }


    /* Did previous operator terminate route? */
    if(intern_ins_idx!=out_indx)
    {
        /* Check for Mono to multi-channel */
        if(num_inputs<num_spkrs)
        {
            for(i=0; i < num_spkrs; i++)
            {
                idxs[num_io+i] = intern_ins_idx;
            }
            intern_ins_idx = num_io;
        }

        op_ptr = create_shift_op(num_spkrs, &idxs[intern_ins_idx], &idxs[out_indx], op_extra_data->spkr_shift);
        if(!op_ptr)
        {
            goto aFailed;
        }
        cbops_append_operator_to_graph(spkr_graph,op_ptr);
    }

    op_extra_data->spkr_rate_adjustment=0;

/* Atlas is going to transfer the DAC SW buffers into a ping/pong
   DRAM buffer at the ping/pong rate.   There is no ability
   to control the latency.  That is handled by the size of the
   ping/pong buffer */
#ifndef CHIP_BASE_A7DA_KAS
    /* Create Rate monitoring operator */
    op_ptr = create_rate_monitor_op(AECREF_TICKS_PER_SECOND,out_indx);
    if(!op_ptr)
    {
        goto aFailed;
    }

    if(op_extra_data->spkr_rate_ability==RATEMATCHING_SUPPORT_HW)
    {
        rate_monitor_op_initiatlize(op_ptr,op_extra_data->spkr_rate,TRUE,3*MS_PER_SEC);
    }
    else
    {
        rate_monitor_op_initiatlize(op_ptr,op_extra_data->spkr_rate,FALSE,5*MS_PER_SEC);
    }

    op_extra_data->spkr_rate_monitor_op=op_ptr;

    cbops_append_operator_to_graph(spkr_graph,op_ptr);
#endif

    pfree(idxs);

    DEBUG_GRAPHS("AEC REFERENCE: Building speaker graph, Done!" );
    return TRUE;
  aFailed:
    DEBUG_GRAPHS("AEC REFERENCE: Building speaker graph, Failed!" );
    pfree(idxs);
    return FALSE;
}

/**
 * validate_channels_and_build
 * \brief checks all the connections for both speaker and microphone
 *        paths and rebuild the cbops graphs for each path when needed.
 *
 * \param op_data Pointer to the AEC reference operator data.
 */
bool validate_channels_and_build(OPERATOR_DATA *op_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    /* check changes in speaker, microphone and reference paths */
    bool spkr_changed = aec_reference_update_spkr_channel_status(op_extra_data);
    bool mic_changed = aec_reference_update_mic_channel_status(op_extra_data);
    bool ref_changed = aec_reference_update_ref_channel_status(op_extra_data);
    bool interrupts_blocked = FALSE;

    patch_fn_shared(aec_reference);

    /* see if graph builds need to be done with
     * interrupts blocked.
     */
    if(op_data->state == OP_RUNNING &&
       !spkr_changed &&
       (mic_changed || ref_changed))
    {
        /* if we have a change in mic/ref paths while
         * speaker graph is already running, then we block the
         * interrupts so new additions wont disrupt speaker
         * path.
         */
        LOCK_INTERRUPTS;
        interrupts_blocked = TRUE;
    }

    /* rebuild graphs if there is change in
     * speaker or microphone path
     */
    if(!aec_reference_build_graphs(op_extra_data, spkr_changed, mic_changed))
    {
        goto aFailed;
    }

    /* also if there is changes in reference path, then
     * apply that change to speaker graph
     */
    if(ref_changed)
    {
        if(!aec_reference_spkr_include_ref_path(op_extra_data, GetRefChannelStatus(op_extra_data)))
        {
            goto aFailed;
        }
    }

    /* if interrupts blocked for re-build
     * then unblock them now
     */
    if(interrupts_blocked)
    {
        UNLOCK_INTERRUPTS;
    }

    /* start running the graphs if not already running
     */
    if(op_extra_data->kick_id == TIMER_ID_INVALID)
    {
        op_extra_data->kick_id = timer_schedule_event_in(AEC_REFERENCE_TIME_PERIOD,aec_reference_timer_task,(void*)op_data);
    }

    return TRUE;

  aFailed:
    /* it failed to complete the build for some reason,
     * clean up all the graphs.
     */
    aec_reference_cleanup(op_data);
    if(interrupts_blocked)
    {
        UNLOCK_INTERRUPTS;
    }
    return FALSE;
}

/**
 * aec_reference_update_spkr_channel_status
 * \brief updates speaker channel status based on the latest connections
 *        affecting speaker path.
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \return whether there is a change in speaker channel status that requires
 *         rebuilding of the speaker graph.
 */
bool aec_reference_update_spkr_channel_status(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    unsigned i;
    unsigned spkr_channel_status=0;
    unsigned number_spkrs=0;
    tCbuffer *inputBuf,*outputBuf;

    patch_fn_shared(aec_reference);

    /* Speakers are connected */
    op_extra_data->sink_kicks   = 0;
    if(op_extra_data->input_stream[AEC_REF_INPUT_TERMINAL1] && op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])
    {
        int in_count=1;
        int out_count=1;

        /* Master Channel is connected */
        spkr_channel_status |= AEC_REFERENCE_CONSTANT_CONN_SPKR_1;

        /* Primary sink is connected */
        op_extra_data->sink_kicks = (1<<AEC_REF_INPUT_TERMINAL1);

        for(i=1;i<MAX_NUMBER_SPEAKERS;i++)
        {
            int sink_idx = SpeakerInputTerminalByIndex(i);

            /* Old terminals kept for backwards compatibility */
            outputBuf = op_extra_data->output_stream[SpeakerTerminalByIndex(i)];
            inputBuf  = op_extra_data->input_stream[sink_idx];

            if(inputBuf || outputBuf)
            {
                spkr_channel_status |= (AEC_REFERENCE_CONSTANT_CONN_SPKR_1<<i);

                if(inputBuf)
                {
                    op_extra_data->sink_kicks |= (1<<sink_idx);
                    in_count++;
                }
                else if(in_count>1)
                {
                    /* Special case allows mono input split to multiple outputs*/
                    return(FALSE);
                }

                if(outputBuf)
                {
                    out_count++;
                }
                else if(out_count>1)
                {
                    /* Special case allows mono output mixed from multiple inputs*/
                    return(FALSE);
                }
            }
        }
        /* Verify complete channel setup */
        number_spkrs = out_count;
        if(in_count==out_count)
        {
            /* Parallel channels */
            spkr_channel_status |= AEC_REFERENCE_CONSTANT_CONN_TYPE_PARA;
        }
        else if(out_count==1)
        {
            /* Mono Output.  Mix inputs */
            spkr_channel_status |= AEC_REFERENCE_CONSTANT_CONN_TYPE_MIX;
            number_spkrs = in_count;
        }
        else
        {
            /* Mono Input.  Split outputs */
            PL_ASSERT(in_count==1);
        }
    }

    if((GetSpkrChannelStatus(op_extra_data)) == spkr_channel_status)
    {
        /* No change in the speaker channel status */
        return FALSE;
    }

    /* update number of speaker channels */
    op_extra_data->num_spkr_channels = number_spkrs;

    /* update speaker channel status */
    SetSpkrChannelStatus(op_extra_data, spkr_channel_status);
    return TRUE;

}

/**
 * aec_reference_update_ref_channel_status
 * \brief updates ref channel status based on the reference output connection
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \return whether there is a change in ref channel status that requires
 *         insertion/deletion of the reference sub-path into/from speaker graph.
 */
bool aec_reference_update_ref_channel_status(AEC_REFERENCE_OP_DATA *op_extra_data)
{

    unsigned ref_channel_status = 0;
    patch_fn_shared(aec_reference);

    /* Check AEC reference */
    if(op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL] && (op_extra_data->channel_status&AEC_REFERENCE_CONSTANT_CONN_MIKE_1))
    {
        op_extra_data->source_kicks |= 1<<AEC_REF_REFERENCE_TERMINAL;
        ref_channel_status = AEC_REFERENCE_CONSTANT_CONN_TYPE_REF;
    }

    if(ref_channel_status == (GetRefChannelStatus(op_extra_data)))
    {
        /* No change in reference path */
        return FALSE;
    }

    /* reference path has changed, update
     * channel status.
     */
    SetRefChannelStatus(op_extra_data, ref_channel_status);
    return TRUE;
}

/**
 * aec_reference_update_mic_channel_status
 * \brief updates mic channel status based on the latest connections
 *        affecting mic path.
 *
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 * \return whether there is a change in speaker channel status that requires
 *         rebuilding of the speaker graph.
 */
bool aec_reference_update_mic_channel_status(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference);
    unsigned i;
    unsigned mic_channel_status=0;
    unsigned number_mics=0;
    tCbuffer *inputBuf,*outputBuf;

    patch_fn_shared(aec_reference);

    /* Microphones are connected */
    op_extra_data->source_kicks = 0;
    if(op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1] && op_extra_data->output_stream[AEC_REF_OUTPUT_TERMINAL1])
    {
        bool all_mic_channels = TRUE;
        for(i=0;i<MAX_NUMBER_MICS;i++)
        {
            inputBuf  = op_extra_data->input_stream[MicrophoneTerminalByIndex(i)];
            outputBuf = op_extra_data->output_stream[OutputTerminalByIndex(i)];

            if(inputBuf && outputBuf)
            {
                number_mics++;

                if(i<4)
                {
                    mic_channel_status |= (AEC_REFERENCE_CONSTANT_CONN_MIKE_1<<i);
                }
                else
                {
                    mic_channel_status |= ((AEC_REFERENCE_CONSTANT_CONN_MIKE_5>>4)<<i);
                }
                op_extra_data->source_kicks |= (1 << OutputTerminalByIndex(i) );
            }
            else if(inputBuf || outputBuf)
            {
                all_mic_channels=FALSE;
                break;
            }
        }
        if(!all_mic_channels)
        {
            number_mics=0;
            mic_channel_status = 0;
            op_extra_data->source_kicks = 0;
        }
    }

    if((GetMicChannelStatus(op_extra_data)) == mic_channel_status)
    {
        /* No change in the mic path */
        return FALSE;
    }

    /* update number of mics */
    op_extra_data->num_mic_channels = number_mics;

    /* update mic channel status */
    SetMicChannelStatus(op_extra_data, mic_channel_status);

    return TRUE;
}

/**
 * aec_reference_spkr_include_sidetone
 * \brief updates speaker graph to include/exclude side tone mixing
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool aec_reference_spkr_include_sidetone(AEC_REFERENCE_OP_DATA* op_extra_data, bool include_sidetone)
{
    cbops_graph *spkr_graph = op_extra_data->spkr_graph;
    cbops_op    *op_ptr;

    /* we need to have a speaker graph */
    if(NULL == op_extra_data->spkr_graph)
    {
        /* No action if we don't have a speaker graph
         * however in that case we expect the sidetone is
         * not active.
         */
        PL_ASSERT(op_extra_data->spkr_sidetone_active == FALSE);
        return TRUE;
    }

    if(!include_sidetone == !op_extra_data->spkr_sidetone_active)
    {
        /* Also no action if new request is the same
         * as current state.
         */
        return TRUE;
    }

    if(include_sidetone)
    {
        /* Add sidetone mixer to resampler section,  Master channel only */
        cbops_set_input_io_buffer(spkr_graph,
                                  op_extra_data->spkr_st_in_idx,
                                  op_extra_data->spkr_st_in_idx,
                                  op_extra_data->sidetone_buf);

        /* Add sidetone mix operator [left input] (inplace, so pass it num_inputs as in & out index);
         * it is as per multi-channel model, but only ever works on one single channel
         */
        op_ptr = create_sidetone_mix_op(op_extra_data->spkr_stmix_in_idx,
                                        op_extra_data->spkr_stmix_in_idx,
                                        op_extra_data->spkr_st_in_idx,
                                        op_extra_data->spkr_out_threshold);
        if(op_ptr == NULL)
        {
            return FALSE;
        }

        /* insert sidetone mix operator into speaker graph */
        cbops_insert_operator_into_graph(spkr_graph, op_ptr, op_extra_data->spkr_st_point_op);

        /* save the sidetone mix operator */
        op_extra_data->spkr_stmix_op = op_ptr;

        /* now speaker graph has sidetone mix operator */
        op_extra_data->spkr_sidetone_active = TRUE;

        DEBUG_GRAPHS("AEC REFERENCE: Side tone path added!" );
    }
    else
    {
        /* remove the sidetone mix operator from speaker graph */
        cbops_remove_operator_from_graph(spkr_graph, op_extra_data->spkr_stmix_op);
        op_extra_data->spkr_stmix_op = NULL;

        /* also tell the cbops not to care about sidetone buffer any more */
        cbops_unset_buffer(spkr_graph, op_extra_data->spkr_st_in_idx);

        /* speaker graph no longer has sidetone mix operator*/
        op_extra_data->spkr_sidetone_active = FALSE;

        DEBUG_GRAPHS("AEC REFERENCE: Side tone path removed!" );
    }

    return TRUE;
}

/**
 * aec_reference_spkr_include_ref_path
 * \brief updates speaker graph to include/exclude path for reference output
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool aec_reference_spkr_include_ref_path(AEC_REFERENCE_OP_DATA* op_extra_data, bool include_ref_path)
{
    cbops_graph *spkr_graph = op_extra_data->spkr_graph;
    cbops_op    *op_ptr;
    unsigned ref_idx = op_extra_data->spkr_ref_idx;

    /* No action if no speaker graph */
    if(NULL == spkr_graph)
    {
        /* nothing to do but we expect no reference output */
        PL_ASSERT(op_extra_data->spkr_ref_active == FALSE);
        return TRUE;
    }

    if(!include_ref_path == !op_extra_data->spkr_ref_active)
    {
        /* Also no action if new request is the same
         * as current state.
         */
        return TRUE;
    }

    if(include_ref_path)
    {
        /* Adding Reference path to speaker graph */

        unsigned ref_input_idx = op_extra_data->spkr_ref_input_idx;
        unsigned ref_rm_in_idx = ref_input_idx;
        cbops_op *after = op_extra_data->spkr_ref_point_op;
        unsigned spkr_num_ref_ops = 0;

        /* set buffer index for reference output in cbops graph */
        cbops_set_output_io_buffer(spkr_graph,
                                   ref_idx,
                                   ref_idx,
                                   op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL]);

        /* see if resampler is needed in reference path */
        if(op_extra_data->input_rate != op_extra_data->output_rate)
        {
            /* resampler outputs into internal buffer */
            ref_rm_in_idx = ref_idx+1;

            /* NOTE: left scratch input is input to resampler */

            cbops_set_internal_io_buffer(spkr_graph,
                                         ref_rm_in_idx,
                                         ref_rm_in_idx,
                                         op_extra_data->scratch_bufs[op_extra_data->spkr_ref_scratch_idx]);

            /* Add reference sample rate conversion (input_rate --> output_rate) - for 1 channel only */
            op_ptr = create_iir_resamplerv2_op(1, &ref_input_idx, &ref_rm_in_idx,
                                               op_extra_data->input_rate, op_extra_data->output_rate,
                                               AEC_REF_RESAMPLER_TEMP_BUF_SIZE,
                                               op_extra_data->resampler_temp_buffer, 0, 0, 0);

            if(op_ptr == NULL)
            {
                return FALSE;
            }

            /* save the resampler op and it's scratch buffer index */
            op_extra_data->spkr_ref_rs_op = op_ptr;
            op_extra_data->spkr_ref_rs_idx = ref_rm_in_idx;

            /* insert resamler op into speaker cbops graph */
            cbops_insert_operator_into_graph(spkr_graph,op_ptr, after);
            after = op_ptr;
            spkr_num_ref_ops++;
        }

        /*  SW rate adjustment for reference */
        op_ptr = create_sw_rate_adj_op(1, &ref_rm_in_idx, &ref_idx,
                                       CBOPS_RATEADJUST_COEFFS,
                                       &op_extra_data->sync_block.rm_adjustment, 0);
        if(op_ptr == NULL)
        {
            return FALSE;
        }

        if(op_extra_data->mic_sync_enable)
        {
            /* if input and output are in the same clock, then mic output
             * can get sychronised to ref, and ref won't need rate adjustment.
             * The operator will be doing simple copy here.
             */
            cbops_rateadjust_passthrough_mode(op_ptr, TRUE);
        }

        /* store reference rate adjust operator */
        op_extra_data->ref_sw_rateadj_op = op_ptr;

        /* insert rate adjust op into speaker cbops graph*/
        cbops_insert_operator_into_graph(spkr_graph, op_ptr, after);
        after = op_ptr;
        spkr_num_ref_ops++;

        /* speaker latency cbops operator */
        op_ptr = create_speaker_latency_op(ref_idx, &op_extra_data->sync_block);
        if(op_ptr == NULL)
        {
            return FALSE;
        }
        /* insert speaker latency op into speaker cbops graph*/
        cbops_insert_operator_into_graph(spkr_graph, op_ptr, after);
        spkr_num_ref_ops++;

        /* save last operator in the reference sub-path and
         * also the number of operator in the sub-path,
         * these will be required when removing the
         * reference path
         */
        op_extra_data->spkr_ref_last_op = op_ptr;
        op_extra_data->spkr_num_ref_ops = spkr_num_ref_ops;

        /* Now speaker graph includes reference sub path */
        op_extra_data->spkr_ref_active = TRUE;

        DEBUG_GRAPHS("AEC REFERENCE: Reference path added!" );
    }
    else
    {
        /* removing sub-graph starts from its last operator */
        cbops_op *op = op_extra_data->spkr_ref_last_op;
        unsigned i;

        /* remove all the ops in the reference sub-path */
        for (i=0; i < op_extra_data->spkr_num_ref_ops; ++i)
        {
            cbops_op *prev_op = op->prev_operator_addr;
            cbops_remove_operator_from_graph(spkr_graph, op);
            op = prev_op;
        }

        /*  tell the cbops not to care about reference buffer any more */
        cbops_unset_buffer(spkr_graph, ref_idx);

        if(op_extra_data->spkr_ref_rs_op != NULL)
        {
            /* if we have resampler in the reference path
             * then also the reserved index buffer should
             * get unset.
             */
            cbops_unset_buffer(spkr_graph, op_extra_data->spkr_ref_rs_idx);
            op_extra_data->spkr_ref_rs_op = NULL;
        }

        op_extra_data->spkr_num_ref_ops = 0;
        op_extra_data->spkr_ref_last_op = NULL;

        /* speaker graph no longer has reference sub-path */
        op_extra_data->spkr_ref_active = FALSE;
        op_extra_data->ref_sw_rateadj_op = NULL;
        DEBUG_GRAPHS("AEC REFERENCE: Reference path removed!" );
    }
    return TRUE;
}

/**
 * aec_reference_cleanup_graphs
 * \brief clean up all the cbops graphs
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_cleanup_graphs(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    unsigned i;

    patch_fn_shared(aec_reference);

    /* Kill timer Task */
    /* This needs to be atomic, to make sure the right timer gets cancelled */
    LOCK_INTERRUPTS;
    if(op_extra_data->kick_id!=TIMER_ID_INVALID)
    {
        timer_cancel_event(op_extra_data->kick_id);
        op_extra_data->kick_id=TIMER_ID_INVALID;
    }
    UNLOCK_INTERRUPTS;

    /* clean mic graph */
    aec_reference_cleanup_mic_graph(op_extra_data);

    /* clean speaker graph */
    aec_reference_cleanup_spkr_graph(op_extra_data);

    /* Free Internal buffers */
    for(i=0;i<AEC_NUM_SCRATCH_BUFFERS;i++)
    {
        if(op_extra_data->scratch_bufs[i] != NULL)
        {
            cbuffer_destroy(op_extra_data->scratch_bufs[i]);
            op_extra_data->scratch_bufs[i] = NULL;
        }
    }

    /* clear scratch buffer used by resampler operator */
    if(op_extra_data->resampler_temp_buffer != NULL)
    {
        pfree(op_extra_data->resampler_temp_buffer);
        op_extra_data->resampler_temp_buffer = NULL;
    }

    DEBUG_GRAPHS("AEC REFERENCE: Full graphs cleanup done!" );
}

/**
 * aec_reference_cleanup_mic_graph
 * \brief clean up mic graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_cleanup_mic_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference);

    /* Clear links to graphs */
    op_extra_data->mic_rate_monitor_op = NULL;
    op_extra_data->mic_sidetone_op = NULL;
    op_extra_data->mic_sw_rateadj_op = NULL;
    op_extra_data->mic_mute_op = NULL;

    /* Free cbops mic graph */
    if(op_extra_data->mic_graph != NULL)
    {
        destroy_graph(op_extra_data->mic_graph);
        op_extra_data->mic_graph = NULL;
    }

    /* if we have active sidetone path then remove
     * it from speaker graph
     */
    if(op_extra_data->spkr_sidetone_active)
    {
        aec_reference_spkr_include_sidetone(op_extra_data, FALSE);
    }

    /* Free Sidetone buffers, this must be
     * done after removing sidetone mix from
     * speaker graph.
     */
    if(op_extra_data->sidetone_buf != NULL)
    {
        cbuffer_destroy(op_extra_data->sidetone_buf);
        op_extra_data->sidetone_buf = NULL;
    }

#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
    if(op_extra_data->mic_ext_rate_adjust_op != 0)
    {
        /* set external op to passthrough mode */
        stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->mic_ext_rate_adjust_op, TRUE);
        op_extra_data->mic_ext_rate_adjust_op = 0;
    }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */
    /* reset the state of sidetone
     * ATlas does not use sidetone so don't add it */
#if !defined(CHIP_BASE_A7DA_KAS)
    op_extra_data->using_sidetone = SIDETONE_ENABLE_FLAG;
#else
    op_extra_data->using_sidetone = 0;
#endif

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    op_extra_data->mic_metadata_tag_left_words = 0;
#endif

    DEBUG_GRAPHS("AEC REFERENCE: MIC graph's cleanup done!" );
}

/**
 * aec_reference_cleanup_spkr_graph
 * \brief clean up speaker graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
void aec_reference_cleanup_spkr_graph(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    patch_fn_shared(aec_reference);

#if defined(IO_DEBUG)
    insert_op  = NULL;
    st_disgard_op = NULL;
#endif

    /* Clear links to graphs */
    op_extra_data->spkr_rate_monitor_op = NULL;
    op_extra_data->spkr_sw_rateadj_op = NULL;
    op_extra_data->spkr_ref_point_op = NULL;
    op_extra_data->spkr_st_point_op = NULL;
    op_extra_data->spkr_ref_last_op = NULL;
    op_extra_data->spkr_stmix_op =  NULL;
    op_extra_data->spkr_ref_rs_op = NULL;
    op_extra_data->ref_sw_rateadj_op = NULL;

    /* destroy speaker graph */
    if(op_extra_data->spkr_graph != NULL)
    {
        destroy_graph(op_extra_data->spkr_graph);
        op_extra_data->spkr_graph = NULL;
    }

    /* clear flag for reference path */
    op_extra_data->spkr_ref_active = FALSE;

    /* clear flag for sidetone path */
    op_extra_data->spkr_sidetone_active = FALSE;
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
    if(op_extra_data->spkr_ext_rate_adjust_op != 0)
    {
        /* set external op to passthrough mode */
        stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->spkr_ext_rate_adjust_op, TRUE);
        op_extra_data->spkr_ext_rate_adjust_op = 0;
    }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */

#ifdef AEC_REFERENCE_SPKR_TTP
    /* destroy any structure allocated for ttp playback */
    aec_reference_spkr_ttp_terminate(op_extra_data);
#endif /* AEC_REFERENCE_SPKR_TTP */

    /* TODO - Fill speakers with silence */

    DEBUG_GRAPHS("AEC REFERENCE: Speaker graph's cleanup done!" );
}

/**
 * aec_reference_cleanup_spkr_graph
 * \brief clean up speaker graph
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 */
bool aec_reference_build_graphs(AEC_REFERENCE_OP_DATA *op_extra_data, bool spkr_changed, bool mic_changed)
{
    unsigned i;
    unsigned num_scratch_buffs;
    unsigned size = 100;

    patch_fn_shared(aec_reference);

    /* nothing to do if neither speaker nor mic has changed */
    if(!spkr_changed && !mic_changed)
    {
        return TRUE;
    }

    if(spkr_changed)
    {
        /* any change in main speaker path will cause full
         * rebuild of every thing from scratch
         */
        aec_reference_cleanup_graphs(op_extra_data);
    }
    else if(mic_changed)
    {
        /* if only mic has changed, then only mic graph will be rebuilt */
        aec_reference_cleanup_mic_graph(op_extra_data);
    }

    /* allocate buffer needed for resampler */
    if(op_extra_data->resampler_temp_buffer == NULL)
    {
        op_extra_data->resampler_temp_buffer =
            (unsigned*)xzpmalloc(AEC_REF_RESAMPLER_TEMP_BUF_SIZE*sizeof(unsigned));
        if(op_extra_data->resampler_temp_buffer == NULL)
        {
            return FALSE;
        }
    }

    /* get the number of required scratch buffers based
     * on the number of speakers and mics that we have.
     */
    num_scratch_buffs = (MAX(op_extra_data->num_spkr_channels,
                             op_extra_data->num_mic_channels))*2;

    /* create scratch buffers, some of them might
     * already have been created */
    for(i=0;i<num_scratch_buffs;i++)
    {
        if(NULL == op_extra_data->scratch_bufs[i])
        {
            op_extra_data->scratch_bufs[i] = cbuffer_create_with_malloc_fast(size, BUF_DESC_SW_BUFFER);
            if(!op_extra_data->scratch_bufs[i])
            {
                return FALSE;
            }
        }
    }

    /** Setup Latency Control */
    {
    	unsigned jitter = frac_mult(op_extra_data->output_rate,op_extra_data->task_period);

	    op_extra_data->sync_block.jitter     = 	jitter + (jitter>>1);				/* 1.5 times task period */
	    op_extra_data->sync_block.ref_delay  = frac_mult(op_extra_data->output_rate,FRACTIONAL(0.001)); 	 /* 1.0 msec */
	    op_extra_data->sync_block.block_sync = 0;
	    op_extra_data->sync_block.rm_adjustment = 0;
        op_extra_data->sync_block.frm_count = 0;
    }

    if(spkr_changed)
    {
        /* rebuild speaker graph */
        if(!build_spkr_graph(op_extra_data))
        {
            return FALSE;
        }
    }

    /* mic graph is rebuilt if there is a change
     * in either mic path or speaker path */
    if(!build_mic_graph(op_extra_data))
    {
        return FALSE;
    }

    /* update speaker graph to include sidetone mix if required */
    aec_reference_spkr_include_sidetone(op_extra_data, op_extra_data->using_sidetone==USE_SIDETONE_FLAG);
#ifdef CHIP_BASE_A7DA_KAS
    interrupt_block();
    /* Make sure the output is purged before starting graphs.  This assists latency/wrap protection  */
    /* Block interrupts because there could be an interrupt between clearing of two channels
       which may cause channels to go out of phase */
    for(i=0;i<MAX_NUMBER_SPEAKERS;i++)
    {
        outputBuf = op_extra_data->output_stream[SpeakerTerminalByIndex(i)];
        if(outputBuf)
        {
            cbuffer_move_write_to_read_point(outputBuf,0);
        }
    }
    interrupt_unblock();
#endif

    /* save a flag showing mic and speaker have same clock source */
    op_extra_data->spkr_mic_in_same_clock =
        override_endpoints_have_same_clock_source(op_extra_data->spkr_endpoint,
                                                  op_extra_data->mic_endpoint);

    /*  Re-init */
    op_extra_data->ReInitFlag = TRUE;

    return TRUE;
}

bool aec_reference_start(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    patch_fn_shared(aec_reference);

    /* Setup Response to Start Request.   Assume Failure*/
    *response_id = OPCMD_START;
    if(!base_op_build_std_response(STATUS_CMD_FAILED,op_data->id,response_data))
    {
        return(FALSE);
    }
    /* do something only if the current state is "connected" */
    if(op_data->state == OP_RUNNING)
    {
        base_op_change_response_status(response_data,STATUS_OK);
        return(TRUE);
    }

    /* Validate channel configuration */
    if(!validate_channels_and_build(op_data))
    {
        return(TRUE);
    }
#ifdef AEC_REFERENCE_SPKR_TTP
    {
        AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
        /* initialisation for timed playback in speaker path */
        aec_reference_spkr_ttp_init(op_extra_data);
    }
#endif /* AEC_REFERENCE_SPKR_TTP */
    /* set internal capability state variable to "running" */
    /* operator state variable set to not running state. based on base_op.c */
    op_data->state = OP_RUNNING;

    base_op_change_response_status(response_data,STATUS_OK);
    return TRUE;
}

bool aec_reference_stop_reset(OPERATOR_DATA *op_data,void **response_data)
{
    patch_fn_shared(aec_reference);

    if(!base_op_build_std_response(STATUS_CMD_FAILED,op_data->id,response_data))
    {
        return(FALSE);
    }
    /* do something only if the current state is "running" */
    if(op_data->state == OP_RUNNING)
    {
        aec_reference_cleanup(op_data);
    }

    /* operator state variable set to not running state. based on base_op.c */
    op_data->state = OP_NOT_RUNNING;
    base_op_change_response_status(response_data,STATUS_OK);
    return TRUE;
}

bool aec_reference_stop(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    /* Setup Response to Stop Request.   Assume Failure*/
    *response_id = OPCMD_STOP;
    return(aec_reference_stop_reset(op_data,response_data));
}

bool aec_reference_reset(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    /* Setup Response to Reset Request.   Assume Failure*/
    *response_id = OPCMD_RESET;
    return(aec_reference_stop_reset(op_data,response_data));
}


bool aec_reference_connect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    unsigned terminal_id    = ((unsigned*)message_data)[0];    /* extract the terminal_id */
    tCbuffer* pterminal_buf = (tCbuffer*)(uintptr_t)(((unsigned *)message_data)[1]);

    patch_fn_shared(aec_reference);

    L3_DBG_MSG1("AEC REFERENCE --- connect TID = %x", terminal_id);


    /* Setup Response to Connection Request.   Assume Failure*/
    *response_id = OPCMD_CONNECT;
    if(!base_op_build_std_response(STATUS_CMD_FAILED,op_data->id,response_data))
    {
        return(FALSE);
    }

    /* Only allow connection if operator has been configured */
    if((op_extra_data->input_rate==0) || (op_extra_data->output_rate==0) )
    {
        return(TRUE);
    }

    /* (i)  check if the terminal ID is valid . The number has to be less than the maximum number of sinks or sources .  */
    /* (ii) check if we are connecting to the right type . It has to be a buffer pointer and not endpoint connection */
    if( !base_op_is_terminal_valid(op_data, terminal_id) || !pterminal_buf)
    {
        base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* check if the terminal is already connected and if not , connect the terminal */
    if(terminal_id & TERMINAL_SINK_MASK)
    {
        terminal_id &= ~ TERMINAL_SINK_MASK;

        if(terminal_id==AEC_REF_MIC_TERMINAL1)
        {
            uint32 config_value;

            /* get info about overridden endpoints */
            op_extra_data->mic_endpoint = get_overriden_endpoint(op_data->id,AEC_REF_MIC_TERMINAL1 | TERMINAL_SINK_MASK);

            if(!get_override_ep_ratematch_ability(op_extra_data->mic_endpoint,&config_value))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }
#if defined(ENABLE_FORCE_SW_RATEMATCH)
            /* Save rate matching ability HW or SW*/
            op_extra_data->mic_rate_ability   = RATEMATCHING_SUPPORT_SW;
            op_extra_data->mic_rate_enactment = RATEMATCHING_SUPPORT_SW;
#else
            /* Save rate matching ability HW or SW*/
            op_extra_data->mic_rate_ability   = (unsigned)config_value;
            op_extra_data->mic_rate_enactment = RATEMATCHING_SUPPORT_NONE;
#endif

            if(op_extra_data->mic_sync_enable)
            {
                /* Mic output will be synchronised to speaker input, SW or HW depends on
                 * it's ability.
                 */
                op_extra_data->mic_rate_enactment = op_extra_data->mic_rate_ability;
                if(op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
                {
                    set_override_ep_ratematch_enacting(op_extra_data->mic_endpoint, TRUE);
                }
            }

            if(!get_override_ep_sample_rate(op_extra_data->mic_endpoint,&config_value))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }
            op_extra_data->mic_rate = (unsigned)config_value;

        }
        else if(terminal_id==AEC_REF_INPUT_TERMINAL1)
        {
            op_extra_data->spkr_in_endpoint = get_overriden_endpoint(op_data->id,AEC_REF_INPUT_TERMINAL1 | TERMINAL_SINK_MASK);
        }

        if(op_extra_data->input_stream[terminal_id])
        {
            /* Should never try to connect an already connected terminal*/
            panic_diatribe(PANIC_AUDIO_ALREADY_CONNECTED_OPERATOR_TERMINAL, op_data->id);
        }
        else
        {
            op_extra_data->input_stream[terminal_id] = pterminal_buf;
        }
#ifdef AEC_REFERENCE_SUPPORT_METADATA
        /* Metadata might be enabled for speaker graph inputs
         * in multi input cases we expect all use same metadata
         * buffer.
         */
        if(IsSpeakerInputTerminal(terminal_id) &&
           buff_has_metadata(pterminal_buf))
        {
            /* set metadata buffer if it hasn't been set already */
            if(NULL == op_extra_data->spkr_input_metadata_buffer)
            {
                op_extra_data->spkr_input_metadata_buffer = pterminal_buf;
            }
        }
#endif /* AEC_REFERENCE_SUPPORT_METADATA */
    }
    else
    {
        if(terminal_id==AEC_REF_SPKR_TERMINAL1)
        {
            uint32 config_value;

            op_extra_data->spkr_endpoint = get_overriden_endpoint(op_data->id,AEC_REF_SPKR_TERMINAL1);

            if(!get_override_ep_ratematch_ability(op_extra_data->spkr_endpoint,&config_value))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }

#if defined(ENABLE_FORCE_SW_RATEMATCH)
            op_extra_data->spkr_rate_ability   = RATEMATCHING_SUPPORT_SW;
            op_extra_data->spkr_rate_enactment = RATEMATCHING_SUPPORT_SW;
#else
            op_extra_data->spkr_rate_ability   = (unsigned)config_value;
            op_extra_data->spkr_rate_enactment = RATEMATCHING_SUPPORT_NONE;
#endif

            if(!get_override_ep_sample_rate(op_extra_data->spkr_endpoint,&config_value))
            {
                /* Should never fail */
                base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
                return TRUE;
            }
            op_extra_data->spkr_rate = (unsigned)config_value;

        }
        else if (terminal_id==AEC_REF_OUTPUT_TERMINAL1)
        {
            op_extra_data->mic_out_endpoint = get_overriden_endpoint(op_data->id,AEC_REF_OUTPUT_TERMINAL1);
            op_extra_data->sync_block.mic_data = 0;
        }
        else if(terminal_id==AEC_REF_REFERENCE_TERMINAL)
        {
            op_extra_data->sync_block.speaker_data = 0;
        }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
       /* set metadata buffer for mic outputs */
       if(IsMicrophoneOutputTerminal(terminal_id) &&
          buff_has_metadata(pterminal_buf))
       {
           if(NULL == op_extra_data->mic_metadata_buffer)
           {   /* first connected mic output buffer with metadata */
               op_extra_data->mic_metadata_buffer = pterminal_buf;
           }
       }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

        if(op_extra_data->output_stream[terminal_id])
        {
            /* Should never try to connect an already connected terminal*/
            panic_diatribe(PANIC_AUDIO_ALREADY_CONNECTED_OPERATOR_TERMINAL, op_data->id);
        }
        else
        {
            op_extra_data->output_stream[terminal_id] = pterminal_buf;
        }
    }

    /* OP not running, accept connections */
    /* OP is running, validate terminals and reset config */
    if(op_data->state != OP_RUNNING)
    {
        base_op_change_response_status(response_data,STATUS_OK);
    }
    else if(validate_channels_and_build(op_data))
    {
        base_op_change_response_status(response_data,STATUS_OK);
    }
    return TRUE;
}

bool aec_reference_disconnect(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    unsigned terminal_id = *((unsigned*)message_data);

    patch_fn_shared(aec_reference);

    L3_DBG_MSG1("AEC REFERENCE --- disconnect TID = %x", terminal_id);

    /* Setup Response to Disconnection Request. Assume Failure*/
    *response_id = OPCMD_DISCONNECT;
    if(!base_op_build_std_response(STATUS_CMD_FAILED,op_data->id,response_data))
    {
        return(FALSE);
    }

    /* check if the terminal ID is valid . The number has to be less than the maximum number of sinks or sources.  */
    if(!base_op_is_terminal_valid(op_data, terminal_id))
    {
        base_op_change_response_status(response_data,STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* check if the terminal is connected and if so , disconnect the terminal */
    if(terminal_id & TERMINAL_SINK_MASK)
    {
        terminal_id &= ~ TERMINAL_SINK_MASK;

#ifdef AEC_REFERENCE_SUPPORT_METADATA
        if(IsSpeakerInputTerminal(terminal_id))
        {
            tCbuffer *this_buf = op_extra_data->input_stream[terminal_id];
            if(this_buf == op_extra_data->spkr_input_metadata_buffer)
            {
                /* disconnecting buffer is the metadata buffer,
                 * change the metadata buffer to another connected
                 * buffer with metadata, if there is any.
                 */
                tCbuffer *new_metadata_buf = NULL;
                int idx;
                for(idx=0; idx < MAX_NUMBER_SPEAKERS; idx++)
                {
                    tCbuffer *inp_buf = op_extra_data->input_stream[SpeakerInputTerminalByIndex(idx)];
                    if(inp_buf != NULL &&
                       inp_buf != this_buf &&
                       buff_has_metadata(inp_buf))
                    {
                        new_metadata_buf = inp_buf;
                        break;
                    }
                }
                op_extra_data->spkr_input_metadata_buffer = new_metadata_buf;
            }
        }
#endif /* AEC_REFERENCE_SUPPORT_METADATA */

        if(terminal_id==AEC_REF_MIC_TERMINAL1)
        {
            op_extra_data->mic_endpoint=NULL;
        }
        else if(terminal_id==AEC_REF_INPUT_TERMINAL1)
        {
            op_extra_data->spkr_in_endpoint=NULL;
        }

        if(op_extra_data->input_stream[terminal_id])
        {
            op_extra_data->input_stream[terminal_id] = NULL;
        }
        else
        {
            /* Should never try to disconnect an already disconnected terminal*/
            panic_diatribe(PANIC_AUDIO_ALREADY_DISCONNECTED_OPERATOR_TERMINAL, op_data->id);
        }
    }
    else
    {
#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
        if(IsMicrophoneOutputTerminal(terminal_id))
        {
            tCbuffer *this_buf = op_extra_data->output_stream[terminal_id];
            if(this_buf == op_extra_data->mic_metadata_buffer)
            {
                /* disconnecting buffer is the mic metadata buffer,
                 * change the metadata buffer to another connected
                 * buffer with metadata, if there is any.
                 */
                tCbuffer *new_metadata_buf = NULL;
                int idx;
                for(idx=0; idx < MAX_NUMBER_MICS; idx++)
                {
                    tCbuffer *out_buf = op_extra_data->output_stream[MicrophoneOutputTerminalByIndex(idx)];
                    if(out_buf != NULL &&
                       out_buf != this_buf &&
                       buff_has_metadata(out_buf))
                    {
                        new_metadata_buf = out_buf;
                        break;
                    }
                }
                op_extra_data->mic_metadata_buffer = new_metadata_buf;
            }
        }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

        if(terminal_id==AEC_REF_SPKR_TERMINAL1)
        {
            op_extra_data->spkr_endpoint=NULL;
        }
        else if(terminal_id==AEC_REF_OUTPUT_TERMINAL1)
        {
            op_extra_data->mic_out_endpoint=NULL;
        }

        if(op_extra_data->output_stream[terminal_id])
        {
            op_extra_data->output_stream[terminal_id] = NULL;
        }
        else
        {
            /* Streams should not have reached this point of sending us a connect for already existing connection */
            panic_diatribe(PANIC_AUDIO_ALREADY_DISCONNECTED_OPERATOR_TERMINAL, op_data->id);
        }
    }

    /* OP not running, accept disconnections */
    /* OP is running, validate terminals and reset config */
    if(op_data->state != OP_RUNNING)
    {
        base_op_change_response_status(response_data,STATUS_OK);
    }
    else if(validate_channels_and_build(op_data))
    {
        base_op_change_response_status(response_data,STATUS_OK);
    }

    return TRUE;
}

bool aec_reference_buffer_details(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    unsigned term_id =  *((unsigned *)message_data);
    OP_BUF_DETAILS_RSP *resp;

    patch_fn_shared(aec_reference);

    if(!base_op_buffer_details(op_data, message_data, response_id, response_data))
    {
        return FALSE;
    }
    resp = (OP_BUF_DETAILS_RSP*)*response_data;

    if ( term_id & TERMINAL_SINK_MASK)
    {
        if(IsMicrophoneTerminal(term_id))
        {
            /* override MIC endpoints */
            resp->needs_override = TRUE;
            resp->b.buffer_size  = 0;
        }
        else
        {
            if(op_extra_data->input_buffer_size != 0)
            {
                /* buffer size based on user configuration */
                resp->b.buffer_size = op_extra_data->input_buffer_size;
            }
            else
            {
                /* buffer size based on sample rate. 3ms worth */
                resp->b.buffer_size  = frac_mult(op_extra_data->input_rate,FRACTIONAL(0.0030));
            }

#ifdef AEC_REFERENCE_SUPPORT_METADATA
            /* currently metadata is supported only for
             * speaker input channels.
             */
            L3_DBG_MSG("AEC_REFERENCE: metadata is supported for speaker inputs");
            resp->metadata_buffer = op_extra_data->spkr_input_metadata_buffer;
            resp->supports_metadata = TRUE;
#endif /* SCO_RX_OP_GENERATE_METADATA */
        }
    }
    else
    {
        if(IsSpeakerTerminal(term_id))
        {
            /* override SPKR endpoints */
            resp->needs_override = TRUE;
            resp->b.buffer_size  = 0;
        }
        else
        {
            if(op_extra_data->output_buffer_size != 0)
            {
                /* buffer size based on user configuration */
                resp->b.buffer_size = op_extra_data->output_buffer_size;
                /* TODO: if this is for REFERENCE output then add small extra (~1.5m), so
                 * user doesn't have to add that to all output terminals.
                 */
            }
            else
            {

                /* buffer size based on sample rate */
                resp->b.buffer_size  = frac_mult(op_extra_data->output_rate,FRACTIONAL(0.0087));
            }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
            /* Support metadta for microphone output channels (reference channel not included)
             * if enabled by the user.
             */
            if(op_extra_data->mic_metadata_enable &&
               IsMicrophoneOutputTerminal(term_id))
            {
                resp->metadata_buffer = op_extra_data->mic_metadata_buffer;
                resp->supports_metadata = TRUE;
            }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */
        }
    }

    return TRUE;
}

bool aec_reference_get_sched_info(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    OP_SCHED_INFO_RSP* resp;
    unsigned terminal_id;

    patch_fn_shared(aec_reference);


    if (!base_op_get_sched_info(op_data, message_data, response_id, response_data))
    {
        return base_op_build_std_response(STATUS_CMD_FAILED, op_data->id, response_data);
    }

    /* Populate the response */
    base_op_change_response_status(response_data, STATUS_OK);
    resp = *response_data;
    resp->op_id = op_data->id;

    /* block_size set to 1 if ep marked as "real", else it can be any arbitrary
     * value so simplifying the code by choosing 1. */
    resp->block_size = 1;

    /* The real endpoints are locally clocked if the respective overridden
     * endpoint is locally clocked. */
    terminal_id = *((unsigned int*)message_data);
    if( (terminal_id&TERMINAL_SINK_MASK) && !IsMicrophoneTerminal(terminal_id) )
    {
        resp->locally_clocked = is_override_ep_locally_clocked(op_extra_data->spkr_endpoint);
    }
    else if (((terminal_id & TERMINAL_SINK_MASK) == 0) && !IsSpeakerTerminal(terminal_id))
    {
        resp->locally_clocked = is_override_ep_locally_clocked(op_extra_data->mic_endpoint );
    }
    else
    {
        resp->locally_clocked = TRUE;
    }

    return TRUE;
}

bool aec_reference_get_data_format(OPERATOR_DATA *op_data, void *message_data, unsigned *response_id, void **response_data)
{
    /* we allocate a standard response, and another 3 fields: number of connected terminals, terminal ID and block size */
    unsigned* resp = (unsigned*)xpmalloc(3*sizeof(unsigned));

    patch_fn_shared(aec_reference);

    *response_id = OPCMD_DATA_FORMAT;
    if((*response_data = (void*)resp) == NULL)
    {
        return(FALSE);
    }
    resp[0] = op_data->id;
    resp[1] = STATUS_OK;
    resp[2] = AUDIO_DATA_FORMAT_FIXP;
    return TRUE;
}



/* ************************************* Data processing-related functions and wrappers **********************************/


/* **************************** Operator message handlers ******************************** */

bool aec_reference_opmsg_obpm_set_control(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    patch_fn_shared(aec_reference);

    /* In the case of this capability, nothing is done for control message. Just follow protocol and ignore any content. */
    return cps_control_setup(message_data, resp_length, resp_data,NULL);
}

bool aec_reference_opmsg_obpm_get_params(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    patch_fn_shared(aec_reference);

    return cpsGetParameterMsgHandler(&op_extra_data->parms_def ,message_data, resp_length,resp_data);
}

bool aec_reference_opmsg_obpm_get_defaults(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    patch_fn_shared(aec_reference);

    return cpsGetDefaultsMsgHandler(&op_extra_data->parms_def ,message_data, resp_length,resp_data);
}

bool aec_reference_opmsg_obpm_set_params(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    bool retval;

    patch_fn_shared(aec_reference);

    retval = cpsSetParameterMsgHandler(&op_extra_data->parms_def ,message_data, resp_length,resp_data);

    /* Set the Reinit flag after setting the parameters */
    op_extra_data->ReInitFlag = TRUE;

    return retval;
}

#if defined(IO_DEBUG)
unsigned ref_spkr_refdrop=0;
unsigned ref_micref_delay=0;
unsigned ref_st_drop=0;
unsigned ref_inserts=0;
unsigned ref_last_inserts_total=0;
unsigned ref_last_inserts_insert=0;
unsigned ref_last_inserts_wrap=0;
#endif

bool aec_reference_opmsg_obpm_get_status(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    unsigned  *resp;

#if defined(IO_DEBUG)
    unsigned num_inserts_total  = 0;
    unsigned num_inserts_insert = 0;
    unsigned num_inserts_wrap   = 0;

    unsigned last_acc_mic = 0;
    unsigned last_acc_spkr = 0;
#endif

    patch_fn_shared(aec_reference);

    if(!common_obpm_status_helper(message_data,resp_length,resp_data,sizeof(AEC_REFERENCE_STATISTICS) ,&resp))
    {
        return FALSE;
    }

    if(resp)
    {
        unsigned config_flag = 0;
        unsigned volume=op_extra_data->shared_volume_ptr->current_volume_level;

        if(op_extra_data->using_sidetone==USE_SIDETONE_FLAG)
        {
            config_flag = flag_uses_SIDETONE;
        }
        if(op_extra_data->channel_status&AEC_REFERENCE_CONSTANT_CONN_TYPE_REF)
        {
            config_flag += flag_uses_AEC_REFERENCE;
        }

        resp = cpsPackWords(&op_extra_data->Cur_mode,&op_extra_data->Ovr_Control, resp);
        resp = cpsPackWords(&config_flag, &volume, resp);
        resp = cpsPackWords(&op_extra_data->channel_status, &op_extra_data->mic_rate, resp);
        resp = cpsPackWords(&op_extra_data->output_rate, &op_extra_data->input_rate, resp);
        resp = cpsPackWords(&op_extra_data->spkr_rate,NULL, resp);

        /* Rate Matching statistics */
        /* TODO: Make these on demand instead of always ON */

#if defined(IO_DEBUG)
        if(st_disgard_op)
        {
            ref_st_drop = get_sink_overflow_disgard_drops(st_disgard_op);
        }
#ifdef AEC_REFERENCE_LATENCY_DEBUG
        ref_spkr_refdrop = op_extra_data->sync_block.speaker_drops + op_extra_data->sync_block.speaker_inserts;
        ref_micref_delay = op_extra_data->sync_block.speaker_delay;
#endif
        if(insert_op)
        {
            num_inserts_total  = get_aec_ref_cbops_inserts_total(insert_op);
            num_inserts_insert = get_aec_ref_cbops_insert_op_insert_total(insert_op);
            num_inserts_wrap   = get_aec_ref_cbops_wrap_op_insert_total(insert_op);

            if(num_inserts_total!=ref_last_inserts_total)
            {
                ref_last_inserts_total=num_inserts_total;
                ref_last_inserts_insert=num_inserts_insert;
                ref_last_inserts_wrap=num_inserts_wrap;
                ref_inserts++;
            }
        }

        /* get last acc for mic and speaker,
         * Note: it's fine if rate_monitor_op not existing, it will return 0 */
        last_acc_mic = get_rate_monitor_last_acc(op_extra_data->mic_rate_monitor_op);
        last_acc_spkr = get_rate_monitor_last_acc(op_extra_data->spkr_rate_monitor_op);

        resp = cpsPackWords(&op_extra_data->mic_rate_enactment,&op_extra_data->spkr_rate_enactment, resp);
        resp = cpsPackWords((unsigned*)&last_acc_mic,(unsigned*)&last_acc_spkr, resp);
        resp = cpsPackWords(&op_extra_data->mic_rate_adjustment,&op_extra_data->spkr_rate_adjustment, resp);
        resp = cpsPackWords(&op_extra_data->sync_block.rm_adjustment,(unsigned*)&ref_last_inserts_total, resp);
        resp = cpsPackWords((unsigned*)&ref_spkr_refdrop,(unsigned*)&ref_st_drop, resp);
        cpsPackWords((unsigned*)&ref_micref_delay,NULL, resp);
#endif

    }

    return TRUE;
}

/* Callback function for getting parameters from persistent store */
bool ups_params_aec(void* instance_data,PS_KEY_TYPE key,PERSISTENCE_RANK rank,
                    uint16 length, unsigned* data, STATUS_KYMERA status,uint16 extra_status_info)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)((OPERATOR_DATA*)instance_data)->extra_op_data;

    patch_fn_shared(aec_reference);

    cpsSetParameterFromPsStore(&op_extra_data->parms_def,length,data,status);

    /* Set the Reinit flag after setting the parameters */
    op_extra_data->ReInitFlag = TRUE;

    return(TRUE);
}

bool aec_reference_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    PS_KEY_TYPE key;
    bool retval;

    patch_fn_shared(aec_reference);

    retval = cpsSetUcidMsgHandler(&op_extra_data->parms_def,message_data,resp_length,resp_data);

    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(op_extra_data->cap_id,op_extra_data->parms_def.ucid,OPMSG_P_STORE_PARAMETER_SUB_ID);
    ps_entry_read((void*)op_data,key,PERSIST_ANY,ups_params_aec);

    return retval;
}

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
/**
 * aec_reference_opmsg_set_ttp_latency
 * \brief message handler to set ttp target latency for mic output buffers
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_ttp_latency(OPERATOR_DATA *op_data, void *message_data,
                                     unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    /* We cant change this setting while running */
    if(op_data->state == OP_RUNNING)

    {
        return FALSE;
    }

    /* get the latency from payload */
    op_extra_data->mic_target_latency = ttp_get_msg_latency(message_data);

    /* configure latency */
    ttp_configure_latency(op_extra_data->mic_time_to_play, op_extra_data->mic_target_latency);

    /* receiving this message (with latenct != 0) will enable metadata generation */
    op_extra_data->mic_metadata_enable = op_extra_data->mic_target_latency != 0;

    return TRUE;
}

/**
 * aec_reference_opmsg_set_latency_limits
 * \brief message handler to set ttp latency limits for mic output channels
 */
bool aec_reference_opmsg_set_latency_limits(OPERATOR_DATA *op_data, void *message_data,
                                        unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    TIME_INTERVAL min_latency, max_latency;

    /* get the limits from the message payload and configure the time to play limits */
    ttp_get_msg_latency_limits(message_data, &min_latency, &max_latency);
    ttp_configure_latency_limits(op_extra_data->mic_time_to_play, min_latency, max_latency);

    return TRUE;
}

/**
 * aec_reference_opmsg_set_ttp_params
 * \brief message handler to set ttp parameters for mic output channels
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_ttp_params(OPERATOR_DATA *op_data, void *message_data,
                                    unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{

    AEC_REFERENCE_OP_DATA *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    ttp_params params;

    /* We cant change this setting while running */
    if(op_data->state == OP_RUNNING)
    {
        return FALSE;
    }

    /* configure ttp params */
    ttp_get_msg_params(&params, message_data);
    ttp_configure_params(op_extra_data->mic_time_to_play, &params);

    return TRUE;
}

/**
 * aec_reference_mic_generate_metadata_with_ttp
 * \brief generates metadata for microphone output channels
 * \param op_extra_data Pointer to AEC_REFERENCE operator specific data
 * \param samples number of samples in the to-be-copied chunk
 */
void aec_reference_mic_generate_metadata_with_ttp(AEC_REFERENCE_OP_DATA *op_extra_data, unsigned samples)
{
    metadata_tag *mtag;
    unsigned b4idx, afteridx;
    tCbuffer *met_buf = op_extra_data->mic_metadata_buffer;

    /* no update needed if no new samples arrived */
    if(samples == 0)
    {
        return;
    }

    /* if previous tag was incomplete, we need first to complete the tag */
    if(op_extra_data->mic_metadata_tag_left_words > 0)
    {
        /* last written tag was incomplete, we keep adding
         * Null tag until full length of incomplete tag is
         * covered.
         */
        unsigned null_tag_len = op_extra_data->mic_metadata_tag_left_words;
        if(null_tag_len > samples)
        {
            null_tag_len = samples;
        }

        /* append Null tag, with length = null_tag_len */
        b4idx = 0;
        afteridx = null_tag_len*OCTETS_PER_SAMPLE;
        buff_metadata_append(met_buf, NULL, b4idx, afteridx);

        /* update amount left */
        op_extra_data->mic_metadata_tag_left_words -= null_tag_len;
        samples -= null_tag_len;
        if(samples == 0)
        {
            /* all new words used for completing old tag */
            return;
        }
    }

    /* create a new tag to append */
    b4idx = 0;
    afteridx = samples*OCTETS_PER_SAMPLE;
    mtag = buff_metadata_new_tag();
    if (mtag != NULL)
    {
        /* calculating time of arrival for first sample of the tag,
         * we do that by looking how many samples are in the mic buffer,
         * this could be short by up to 1 sample as our reading isn't aligned.
         */
        TIME current_time = hal_get_time();

        /* amount of data in the mic buffer */
        unsigned amount_in_buffer =
            cbuffer_calc_amount_data_in_words(op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1]);

        /* convert samples to time,
         * Note: some inaccuracies because of rate adjustment, but jitter will be filtered by ttp filter */
        INTERVAL time_passed = (INTERVAL) (((uint48)(amount_in_buffer)*SECOND)/op_extra_data->mic_rate) +
            (INTERVAL) (((uint48)(samples)*SECOND)/op_extra_data->output_rate);

        TIME time_of_arrival = time_sub(current_time, time_passed);

        /* see if we have minimum amount for tag */
        if(samples >= op_extra_data->mic_metadata_min_tag_len)
        {
            /* we have enough new samples to append a complete tag */
            mtag->length = samples*OCTETS_PER_SAMPLE;
        }
        else
        {
            /* new samples aren't enough to form a
             * new complete tag, we append a new tag with
             * minimum length, this tag is incomplete and
             * will be completed in next calls when we receive
             * new samples by appending Null tags.
             */
            mtag->length = op_extra_data->mic_metadata_min_tag_len*OCTETS_PER_SAMPLE;
            op_extra_data->mic_metadata_tag_left_words = op_extra_data->mic_metadata_min_tag_len - samples;
        }

        ttp_status status;
        ttp_update_ttp(op_extra_data->mic_time_to_play, time_of_arrival,
                       mtag->length/OCTETS_PER_SAMPLE, &status);
        /* Populate the metadata tag from the TTP status */
        ttp_utils_populate_tag(mtag, &status);
    }
    /* append generated metadata to the output buffer */
    buff_metadata_append(met_buf, mtag, b4idx, afteridx);
}
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */

bool aec_reference_opmsg_enable_mic_sync(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data =
        (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    patch_fn_shared(aec_reference);

    if((NULL != op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1]) ||
       (NULL != op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL]) ||
       (NULL != op_extra_data->input_stream[AEC_REF_OUTPUT_TERMINAL1]))
    {
        /* This message can be handled only if MIC and REF are disconnected */
        return FALSE;
    }

    /* read the enable field */
    op_extra_data->mic_sync_enable = (OPMSG_FIELD_GET(message_data, OPMSG_AEC_SAME_INPUT_OUTPUT_CLOCK_SOURCE, SAME_CLK_SRC)) != 0;
    L2_DBG_MSG1("ACE REFERENCE input-output use same clock: ", op_extra_data->mic_sync_enable);

    return TRUE;
}

/**
 * aec_reference_opmsg_set_buffer_size
 * \brief message handler for OPMSG_COMMON_ID_SET_TERMINAL_BUFFER_SIZE message,
 *        can be used to configure required buffer size for input and output terminals.
 * \param op_data Pointer to the operator instance data.
 * \param message_data Pointer to the start request message
 * \param resp_length pointer to location to write the response message length
 * \param response_data Location to write a pointer to the response message
 *
 * \return Whether the response_data field has been populated with a valid
 * response
 */
bool aec_reference_opmsg_set_buffer_size(OPERATOR_DATA *op_data, void *message_data,
                                         unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{

    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    /* get the required buffer size */
    unsigned buffer_size = OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_TERMINAL_BUFFER_SIZE, BUFFER_SIZE);
    /* get the sink terminals that need configuration */
    unsigned sinks = OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_TERMINAL_BUFFER_SIZE,
                                     SINKS);
    /* get the source terminals that need configuration */
    unsigned sources = OPMSG_FIELD_GET(message_data, OPMSG_COMMON_SET_TERMINAL_BUFFER_SIZE,
                                       SOURCES);

    /* All input terminals will report same requied
     * buffer size, so only look at the first input.
     */
    sinks &= (1<<AEC_REF_INPUT_TERMINAL1);

    /* All output terminals will report same requied
     * buffer size, so only look at the first output.
     * This includes REFERENCE buffer.
     */
    sources &= (1<<AEC_REF_OUTPUT_TERMINAL1);

    /* Output buffer size is allowd to change if none of outputs are connected,
     * Note: We allow buffer size change while the operator is running, only the
     * relevant path must be not running.
     */
    if(sources != 0)
    {
        unsigned idx;

        /* No output must be connected */
        for(idx=0;idx<MAX_NUMBER_MICS;idx++)
        {
            if(NULL != op_extra_data->output_stream[OutputTerminalByIndex(idx)])
            {
                return FALSE;
            }
        }
        /* Also REFERENCE must also be disconnected */
        if(NULL != op_extra_data->output_stream[AEC_REF_REFERENCE_TERMINAL])
        {
            return FALSE;
        }

        /* Output can change */
    }

    /* Input buffer size is allowd to change if none of inputs are connected,
     * Note: We allow buffer size change while the operator is running, only the
     * relevant path must be not running.
     */
    if(sinks != 0)
    {
        unsigned idx;

        /* No input must be connected */
        for(idx=0; idx < MAX_NUMBER_SPEAKERS; idx++)
        {
            if(NULL != op_extra_data->input_stream[SpeakerInputTerminalByIndex(idx)])
            {
                return FALSE;
            }
        }
    }

    if(sources != 0)
    {
        /* set the output buffer size */
        op_extra_data->output_buffer_size = buffer_size;
        L2_DBG_MSG1("AEC_REFERENCE: minimum output buffer size set to %d words ", buffer_size);
    }

    if(sinks != 0)
    {
        op_extra_data->input_buffer_size = buffer_size;
        L2_DBG_MSG1("AEC_REFERENCE: minimum input buffer size set to %d words ", buffer_size);
    }

    return TRUE;
}

bool aec_reference_opmsg_mute_mic_output(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    unsigned mute_enable;

    patch_fn_shared(aec_reference);

    if(NULL == op_extra_data->mic_mute_op)
    {
        /* No mute operator found */
        return FALSE;
    }

    /* read the enable field */
    mute_enable  = OPMSG_FIELD_GET(message_data, OPMSG_AEC_MUTE_MIC_OUTPUT, ENABLE);
    L2_DBG_MSG1("ACE REFERENCE muting mic, mute=%d", mute_enable);

    /* configure mute operator */
    cbops_mute_enable(op_extra_data->mic_mute_op, mute_enable != 0);

    return TRUE;
}

bool aec_reference_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;

    patch_fn_shared(aec_reference);

    return cpsGetUcidMsgHandler(&op_extra_data->parms_def,op_extra_data->cap_id,message_data,resp_length,resp_data);
}

bool aec_reference_set_rates(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    unsigned irate,orate;
    bool rebuild = FALSE;

    patch_fn_shared(aec_reference);

    /* Rates needed for creating cbops and for "aec_reference_buffer_details" */
    irate  = OPMSG_FIELD_GET(message_data, OPMSG_AEC_SET_SAMPLE_RATES, INPUT_RATE);
    orate  = OPMSG_FIELD_GET(message_data, OPMSG_AEC_SET_SAMPLE_RATES, OUTPUT_RATE);

    L2_DBG_MSG2("AEC REFERENCE: set rates = %d %d", irate, orate);

    if(op_extra_data->input_rate!=irate)
    {
        op_extra_data->input_rate = irate;
        if(op_data->state == OP_RUNNING)
        {
            /* if the operator is running, any change in input
             * rate should trigger a rebuild of speaker graph
             */
            SetSpkrChannelStatus(op_extra_data, 0);
            rebuild = TRUE;
        }
    }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
    /* configure sample rate for MIC ttp */
    ttp_configure_rate(op_extra_data->mic_time_to_play, orate);
#endif

    if(op_extra_data->output_rate!=orate)
    {
        op_extra_data->output_rate = orate;
        if(op_data->state == OP_RUNNING)
        {
            /* if the operator is running, any change in output
             * rate should trigger a rebuild of microphone graph
             */
            SetMicChannelStatus(op_extra_data, 0);
            rebuild = TRUE;
        }
    }

    if(rebuild)
    {
        /* At least one of input or output rate has changed while the operator
         * is running. This will require fresh rebuild of the affected cbops graphs.
         * No need to do this when operator isn't running since this will be done
         * at start point.
         */
        if(!validate_channels_and_build(op_data))
        {
            /* Not the best way to indicate failure, but best we can do */
            return FALSE;
        }
    }

    return TRUE;
}



static unsigned aecref_get_rate_measurment(AEC_REFERENCE_OP_DATA *op_extra_data,bool bSink)
{
    /* 1.0 in Qx.22 independent of word width */
    unsigned rt_meas = 1<<STREAM_RATEMATCHING_FIX_POINT_SHIFT;
    cbops_op    *rate_monitor_op=NULL;

    rate_monitor_op = op_extra_data->mic_rate_monitor_op;

    if(bSink)
    {
        /* If we are not using HW rate enactment and mic speaker are on
           the same clock then use the microphone rate measurement for speaker
        */
        if((rate_monitor_op==NULL) || (!op_extra_data->spkr_mic_in_same_clock) ||
           (op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_HW) ||
           (op_extra_data->mic_rate_enactment==RATEMATCHING_SUPPORT_HW))
        {
            rate_monitor_op = op_extra_data->spkr_rate_monitor_op;
        }
    }

    if (rate_monitor_op)
    {
        rt_meas = rate_monitor_op_get_rate(rate_monitor_op,0);
    }

    return rt_meas;
}

/**
 * aec_reference_update_mic_reference_sync
 * \brief keeps mic and ref syncronised by updatin mic or reference path warp value
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 *
 * NOTE: Assumes ENABLE_FORCE_SW_RATEMATCH has been defined
 */
void aec_reference_update_mic_reference_sync( AEC_REFERENCE_OP_DATA * op_extra_data)
{
    int mic_ra = 0;
    int spkr_ra = 0;
    unsigned mic_rt;
    unsigned spkr_rt;

    /* This is run in main loop, decimate update as it won't be
     * needed to get updated that often
     */
    op_extra_data->ref_update_counter++;
    if(op_extra_data->ref_update_counter >= AEC_REFERENCE_REF_RATE_UPDATE_PERIOD)
    {
        op_extra_data->ref_update_counter = 0;
#if !defined(ENABLE_FORCE_SW_RATEMATCH)
        if(op_extra_data->spkr_rate_enactment == RATEMATCHING_SUPPORT_HW)
        {
            /* speaker is using HW rate adjustment, read the latest HW warp rate,
             * when using HW rate adjust pretend it is applied in SW and speaker rate
             * itself is perfect.
             * TODO: might need a constant offset for 44.1kHz-like speaker rates
             */
            uint32 val;
            if(get_override_ep_current_hw_warp(op_extra_data->spkr_endpoint, &val))
            {
                spkr_ra = (int)val;
            }
            spkr_rt = (1<<STREAM_RATEMATCHING_FIX_POINT_SHIFT);
        }
        else
#endif /* ENABLE_FORCE_SW_RATEMATCH */
        {
            /* get the latest speaker rate measurement */
            spkr_rt = aecref_get_rate_measurment(op_extra_data, TRUE);
            if(op_extra_data->spkr_rate_enactment == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
               /* if using standalone operator, no built-in adjust rate is applied */
               &&(0 == op_extra_data->spkr_ext_rate_adjust_op)
#endif
               )
            {
                /* if SW rate adjustment is used, get the current value */
                spkr_ra = cbops_sra_get_current_rate_adjust(op_extra_data->spkr_sw_rateadj_op);
            }
        }
#if !defined(ENABLE_FORCE_SW_RATEMATCH)
        if(op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
        {
            /* MIC is using HW rate adjustment, read the latest HW warp rate,
             * when using HW rate adjust pretend it is applied in SW and mic rate
             * itself is perfect.
             * TODO: might need a constant offset for 44.1khz-like mic rates
             */
            uint32 val;
            if(get_override_ep_current_hw_warp(op_extra_data->mic_endpoint, &val))
            {
                mic_ra = -(int)val;
            }
            mic_rt = (1<<STREAM_RATEMATCHING_FIX_POINT_SHIFT);
        }
        else
#endif /* ENABLE_FORCE_SW_RATEMATCH */
        {
            /* get the latest speaker rate measurement */
            mic_rt = aecref_get_rate_measurment(op_extra_data, FALSE);
            if(op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_SW
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
               /* if using standalone operator, no built-in adjust rate is applied */
               &&(0 == op_extra_data->mic_ext_rate_adjust_op)
#endif
               )
            {
                mic_ra = cbops_sra_get_current_rate_adjust(op_extra_data->mic_sw_rateadj_op);
            }
        }

        if(op_extra_data->mic_sync_enable)
        {
            /* if we are syncronising MIC to REF then calculate the rate needs
             * to be applied to the mic path, so it will be syncronised to
             * REFERENCE output(i.e. speaker input)*/
            int new_mic_ra = aecref_calc_sync_mic_rate(spkr_ra,spkr_rt,mic_rt);
            int diff = new_mic_ra - mic_ra;
            if(diff != 0)
            {
#if !defined(ENABLE_FORCE_SW_RATEMATCH)
                if(op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
                {
                    /* HW rate adjustment, apply the change only */
                    set_override_ep_ratematch_adjustment(op_extra_data->mic_endpoint, diff);
                }
                else
#endif /* ENABLE_FORCE_SW_RATEMATCH */
                {
                    /* apply new SW rate adjustment */
                    cbops_sra_set_rate_adjust(op_extra_data->mic_sw_rateadj_op,
                                              op_extra_data->num_mic_channels,
                                              new_mic_ra);
                }
            }
            op_extra_data->mic_rate_adjustment = new_mic_ra;
        }
        else
        {

            /* We are synchronising REFERENCE to MIC output,
             * Update reference SW rate adjustment.
             */
            op_extra_data->sync_block.rm_adjustment = aecref_calc_ref_rate(mic_rt,mic_ra,spkr_rt,spkr_ra);

            /* update rate adjust for reference path */
            cbops_sra_set_rate_adjust(op_extra_data->ref_sw_rateadj_op,
                                      1,
                                      op_extra_data->sync_block.rm_adjustment);
        }
        /* Clear frm_count to disable backup rate adjustment
         *  when rate adjustment is enacted
         */
        op_extra_data->sync_block.frm_count = 0;
    }
}

void aec_reference_timer_task(void *kick_object)
{
    OPERATOR_DATA           *op_data = (OPERATOR_DATA*)kick_object;
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*) op_data->extra_op_data;
    TIME next_fire_time;
    unsigned                 sink_kicks=0,source_kicks=0;

#ifdef PROFILER_ON
    if (op_data->profiler != NULL)
    {
        PROFILER_START(op_data->profiler);
    }
#endif

    patch_fn_shared(aec_reference_run);

    if(op_extra_data->ReInitFlag==TRUE)
    {
        op_extra_data->ReInitFlag=FALSE;

        /* Handle Reinitialize */
        if(op_extra_data->mic_sidetone_op)
        {
            initialize_sidetone_filter_op(op_extra_data->mic_sidetone_op);
        }

        aec_reference_set_mic_gains(op_data);
    }

    if(op_extra_data->mic_sidetone_op)
    {
        update_sidetone_filter_op(op_extra_data->mic_sidetone_op,
                                  op_extra_data->params.OFFSET_CONFIG & AEC_REFERENCE_CONFIG_SIDETONEENA,
                                  op_extra_data->shared_volume_ptr->ndvc_filter_sum_lpdnz);
    }

    /*  - Run MIC cbops */
    if(op_extra_data->mic_graph)
    {
#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
        unsigned b4_space = 0;
        tCbuffer *met_buf = op_extra_data->mic_metadata_buffer;
        if(met_buf!= NULL && buff_has_metadata(met_buf))
        {
            b4_space = cbuffer_calc_amount_space_in_words(met_buf);
        }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */
        if(op_extra_data->spkr_ref_active
           || op_extra_data->mic_sync_enable)
        {
            /* update reference path adjust rate to keep it synchronised with MIC */
            aec_reference_update_mic_reference_sync(op_extra_data);
        }

#ifdef CHIP_BASE_A7DA_KAS
        if(cbuffer_calc_amount_data_in_words(op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1])>0)
#endif

        {
            cbops_process_data(op_extra_data->mic_graph, CBOPS_MAX_COPY_SIZE-1);
        }

#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
        if(met_buf!= NULL && buff_has_metadata(met_buf))
        {
            /* see how many samples produced for mic output buffers and
             * generate metadata for them
             */
            unsigned after_space = cbuffer_calc_amount_space_in_words(met_buf);
			/* Don't tolerate cbops writing more than available space */
            PL_ASSERT(b4_space >= after_space);
            aec_reference_mic_generate_metadata_with_ttp(op_extra_data, b4_space - after_space);
        }
#endif /* AEC_REFERENCE_GENERATE_MIC_TIMESTAMP */
    }

    /*  - Run SPKR cbops */
    if(op_extra_data->spkr_graph)
    {

#ifdef CHIP_BASE_A7DA_KAS
        if(cbuffer_calc_amount_space_in_words(op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])>0)
#endif
        {
#ifdef AEC_REFERENCE_SUPPORT_METADATA
            unsigned max_to_process = CBOPS_MAX_COPY_SIZE-1;
            tCbuffer *met_buf = op_extra_data->spkr_input_metadata_buffer;
            unsigned before_amount = 0;
            if(met_buf!= NULL && buff_has_metadata(met_buf))
            {
                /* amount of metadata available */
                unsigned meta_data_available = buff_metadata_available_octets(met_buf)/OCTETS_PER_SAMPLE;

                /* get amount in the buffer before running cbops */
                before_amount = cbuffer_calc_amount_data_in_words(met_buf);

                /* if we have metadata enabled then limit the amount to
                 * process to the amount of available metadata
                 */
                max_to_process = MIN(max_to_process, meta_data_available);
#ifdef AEC_REFERENCE_SPKR_TTP
                /* Run TTP error control for speaker graph */
                aec_reference_spkr_ttp_run(op_extra_data, &max_to_process);
#endif
            }

            /* run cbops process */
            cbops_process_data(op_extra_data->spkr_graph, max_to_process);

            if(met_buf!= NULL && buff_has_metadata(met_buf))
            {
                /* calculate how much input has been consumed,
                 * The assumption is that the write pointer of
                 * input buffer isn't changed during cbops
                 * process.
                 */
                unsigned amount_processed;
                unsigned after_amount = cbuffer_calc_amount_data_in_words(met_buf);
#ifdef TODO_AEC_REFERENCE_TTP
                /* for the moment don't tolerate cbops doing anything wrong */
                PL_ASSERT(after_amount <= before_amount);
                amount_processed = before_amount - after_amount;
#else /* TODO_AEC_REFERENCE_TTP */
                if(after_amount <= before_amount)
                {
                    amount_processed = before_amount - after_amount;
                }
                else
                {
                    /* This shall never happen, cbops will never
                     * consume more than available data in input buffer.
                     */
                    unsigned buff_size = cbuffer_get_size_in_words(met_buf);
                    amount_processed = buff_size + before_amount - after_amount;
                }
#endif /* TODO_AEC_REFERENCE_TTP */
                if(amount_processed > 0)
                {
                    /* delete metadata tags for consumed input */
                    unsigned b4idx, afteridx;
                    buff_metadata_tag_list_delete(
                        buff_metadata_remove(met_buf, OCTETS_PER_SAMPLE * amount_processed, &b4idx, &afteridx));
                    /* update timestamp */
                    aec_reference_spkr_ttp_update_last_timestamp(op_extra_data, amount_processed);
                }
            }
#else /* AEC_REFERENCE_SUPPORT_METADATA */
            cbops_process_data(op_extra_data->spkr_graph, CBOPS_MAX_COPY_SIZE-1);
#endif /* AEC_REFERENCE_SUPPORT_METADATA*/
#ifdef PROFILER_ON
            if (op_data->profiler != NULL)
            {
                op_data->profiler->kick_inc++;
            }
#endif
        }
    }

    /* Check for Kicks (outputs).   Use Output 1 available data*/
    if(op_extra_data->sync_block.block_sync)
    {
        source_kicks = op_extra_data->source_kicks;
        op_extra_data->sync_block.block_sync = 0;
    }

    /* Check for Kick (inputs).   Use Input 1 available space */
    if(op_extra_data->input_stream[AEC_REF_INPUT_TERMINAL1])
    {
        int available_space = cbuffer_calc_amount_space_in_words(op_extra_data->input_stream[AEC_REF_INPUT_TERMINAL1]);

        if(available_space >= op_extra_data->spkr_kick_size)
        {
            sink_kicks = op_extra_data->sink_kicks;
        }
    }


    if(sink_kicks || source_kicks)
    {
        opmgr_kick_from_operator(op_data,source_kicks,sink_kicks);
    }

    /* Next Timer Event */
    next_fire_time = time_add(get_last_fire_time(),AEC_REFERENCE_TIME_PERIOD);
    op_extra_data->kick_id = timer_schedule_event_at(next_fire_time,
                                                     aec_reference_timer_task, (void*)op_data);
#ifdef PROFILER_ON
    if (op_data->profiler != NULL)
    {
        PROFILER_STOP(op_data->profiler);
    }
#endif
}

#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
/**
 * aec_reference_spkr_check_external_rate_adjust_op
 *
 * \brief Checks whether speaker path has access to a standalone RATE_ADJUST operator.
 *        If there is one then speaker graph will use that instead on built-in rate
 *        adjustment. Note that reference sub-path still will use it's own built-in
 *        rate adjust operator.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 *
 */
void aec_reference_spkr_check_external_rate_adjust_op(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    if(op_extra_data->spkr_rate_enactment != RATEMATCHING_SUPPORT_NONE)
    {
        uint32 val;
        if(get_override_ep_rate_adjust_op(op_extra_data->spkr_endpoint, &val) && val != 0)
        {
            /* Speaker graph will use an external RATE_ADJUST operator for
             * performing rate adjustment.
             */
            op_extra_data->spkr_ext_rate_adjust_op = val;

            /* Speaker path will use this, so not in pass-through mode */
            stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->spkr_ext_rate_adjust_op, FALSE);

            L2_DBG_MSG1("AEC_REFERENCE: Speaker path will use standalone rate adjust: opid=0x%x", val);

            return;
        }
    }

    /* No external operator found or needed */
    op_extra_data->spkr_ext_rate_adjust_op = 0;

    return;
}

/**
 * aec_reference_mic_check_external_rate_adjust_op
 *
 * \brief Checks whether microphone path has access to a standalone RATE_ADJUST operator.
 *        If there is one then speaker graph will use that instead on built-in rate
 *        adjustment.
 *
 * \param op_extra_data Pointer to the AEC reference operator specific data.
 *
 * \return TRUE if there is a standalone rate adjust operator linked to the microphne path
 */
void aec_reference_mic_check_external_rate_adjust_op(AEC_REFERENCE_OP_DATA *op_extra_data)
{
    if(op_extra_data->mic_rate_enactment != RATEMATCHING_SUPPORT_NONE)
    {
        uint32 val;
        if(get_override_ep_rate_adjust_op(op_extra_data->mic_endpoint, &val) && val != 0)
        {
            /* If we have been told to synchronise mic to speaker input then
             * we shouldn't have been told to use standalone rate adjust.
             */
            PL_ASSERT(!op_extra_data->mic_sync_enable);

            /* Microphone graph will use an external RATE_ADJUST operator for
             * performing rate adjustment.
             */
            op_extra_data->mic_ext_rate_adjust_op = val;

            /* Microphone path will use this, so not in pass-through mode */
            stream_delegate_rate_adjust_set_passthrough_mode(op_extra_data->mic_ext_rate_adjust_op, FALSE);

            L2_DBG_MSG1("AEC_REFERENCE: Microphone path will use standalone rate adjust: opid=0x%x", val);

            return;
        }
    }

    /* No external operator found or needed */
    op_extra_data->mic_ext_rate_adjust_op = 0;

    return;
}
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */

bool aec_reference_opmsg_ep_get_config(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    OPMSG_GET_CONFIG *msg = message_data;
    OPMSG_GET_CONFIG_RESULT *result = msg->result;
    unsigned term_idx = msg->header.cmd_header.client_id;

    patch_fn_shared(aec_reference);

    /* msg->value - Pointer which will be populated with the asked configuration value
       msg->cmd_header.client_id - Terminal ID (includes TERMINAL_SINK_MASK for sinks)
       msg->key - Parameter Key to return value for */

    switch(msg->key)
    {
        case OPMSG_OP_TERMINAL_DETAILS:
            /* Return a uint32 - Is Terminal emulating a real endpoint. Called at operator endpoint creation.
             */
            if(term_idx & TERMINAL_SINK_MASK)
            {
                result->value = (uint32)(IsMicrophoneTerminal(term_idx)?
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_NONE:
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_REAL);
            }
            else
            {
                result->value = (uint32)(IsSpeakerTerminal(term_idx)?
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_NONE:
                                         OPMSG_GET_CONFIG_TERMINAL_DETAILS_REAL);
            }
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_ABILITY: /* uint32 */
            if(term_idx == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                result->value = (uint32)op_extra_data->spkr_rate_ability;
#ifdef ENABLE_FORCE_ENACTING_BY_AEC_REFERENCE
                if(RATEMATCHING_SUPPORT_SW == result->value)
                {
                    /* Advertise HW despite planning to do it in SW
                     * this will make sure that enacting will be granted
                     * to this end.
                     */
                    result->value = RATEMATCHING_SUPPORT_HW;
                }
#endif
            }
            else if (term_idx == AEC_REF_OUTPUT_TERMINAL1)
            {

                if(op_extra_data->mic_sync_enable)
                {
                    /* if we are syncing mic to speaker output, then
                     * report AUTO so no rate match pair is created for
                     * mic path.
                     */
                    result->value = (uint32)RATEMATCHING_SUPPORT_AUTO;
                }
                else
                {

                    result->value = (uint32)op_extra_data->mic_rate_ability;
#ifdef ENABLE_FORCE_ENACTING_BY_AEC_REFERENCE
                    if(RATEMATCHING_SUPPORT_SW == result->value)
                    {
                        /* Advertise HW despite planning to do it in SW
                         * this will make sure that enacting will be granted
                         * to this end.
                         */
                        result->value = RATEMATCHING_SUPPORT_HW;
                    }
#endif
                }

            }
            else
            {
                result->value = (uint32)RATEMATCHING_SUPPORT_AUTO;
            }
            break;
        case OPMSG_OP_TERMINAL_KICK_PERIOD:       /* uint32 */
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_RATE:    /* uint32 */

            if(term_idx == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
#if defined(CHIP_BASE_A7DA_KAS)
                get_override_ep_ratematch_rate(op_extra_data->spkr_endpoint,&result->value);
#else
                result->value = aecref_get_rate_measurment(op_extra_data,TRUE);
#endif
                patch_fn_shared(aec_reference);

                op_extra_data->spkr_rate_meas = (unsigned)(result->value);

            }
            else if (term_idx == AEC_REF_OUTPUT_TERMINAL1)
            {
#if defined(CHIP_BASE_A7DA_KAS)
                get_override_ep_ratematch_rate(op_extra_data->mic_endpoint,&result->value);
#else
                result->value = aecref_get_rate_measurment(op_extra_data,FALSE);
#endif
                patch_fn_shared(aec_reference);

                op_extra_data->mic_rate_meas = (unsigned)(result->value);
            }
            else
            {
                /* 1.0 in Qx.22 independent of word width */
                result->value = 1<<STREAM_RATEMATCHING_FIX_POINT_SHIFT;
            }

            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_MEASUREMENT:
            /* TODO */
            result->rm_measurement.sp_deviation = 0;
            result->rm_measurement.measurement.valid = FALSE;
            break;
        case OPMSG_OP_TERMINAL_BLOCK_SIZE:        /* uint32 */
        case OPMSG_OP_TERMINAL_PROC_TIME:         /* uint32 */
        default:
            return FALSE;
    }

    return TRUE;
}


bool aec_reference_opmsg_ep_configure(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
    OPMSG_CONFIGURE  *msg = message_data;
    unsigned terminal_id = msg->header.cmd_header.client_id;
    uint32      value = msg->value;

    patch_fn_shared(aec_reference);

    /* msg->value - Pointer or Value for Key
       msg->cmd_header.client_id - Terminal ID (includes TERMINAL_SINK_MASK for sinks)
       msg->key - Parameter Key to return value for */

    switch(msg->key)
    {
        case OPMSG_OP_TERMINAL_DATA_FORMAT:
            /* value is data type */
            if( ((AUDIO_DATA_FORMAT)msg->value)!=AUDIO_DATA_FORMAT_FIXP )
            {
                return(FALSE);
            }
            break;
        case OPMSG_OP_TERMINAL_KICK_PERIOD:
            /* uint32 polling period in usec - ignore */
            break;
        case OPMSG_OP_TERMINAL_PROC_TIME:
            /* uint32 - N/A an operator will never receive this (has_deadline always FALSE for operators)   */
            break;

        case OPMSG_OP_TERMINAL_CBOPS_PARAMETERS:
            /* value is CBOPS_PARAMETERS *
               TODO - Really need to know type of endpoint: ADC/DAC, I2S, Digital MIC

               CBOPS_DC_REMOVE | CBOPS_SHIFT (set data format --> AUDIO_DATA_FORMAT_FIXP)
               CBOPS_RATEADJUST (EP_RATEMATCH_ENACTING) */

            if(terminal_id == (AEC_REF_MIC_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                op_extra_data->mic_shift = get_shift_from_cbops_parameters((CBOPS_PARAMETERS *)(uintptr_t)value);
            }
            else if (terminal_id == AEC_REF_SPKR_TERMINAL1)
            {
                op_extra_data->spkr_shift = get_shift_from_cbops_parameters((CBOPS_PARAMETERS *)(uintptr_t)value);
            }
            break;
        case OPMSG_OP_TERMINAL_BLOCK_SIZE:
            /* uint32 expected block size per period

               endpoint->state.audio.block_size = (unsigned int)value;
               endpoint->state.audio.kick_period =  (unsigned int)(value * (unsigned long)STREAM_KICK_PERIOD_FROM_USECS(1000000UL) /
               endpoint->state.audio.sample_rate); */
            if(terminal_id == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                op_extra_data->spkr_kick_size = (unsigned)value;
            }
            else if (terminal_id == AEC_REF_OUTPUT_TERMINAL1)
            {
                unsigned block_size     = (unsigned)value;
                unsigned min_block_size = frac_mult(op_extra_data->output_rate,op_extra_data->task_period)*6;

                /* Validate block size for sync logic.   Output buffers size will be at least 8.7 msec of data */
                if(block_size && (block_size < min_block_size) )
                {
                   /* Disable sync logic if block_size is to small */
                   block_size = 0;
                }
                op_extra_data->sync_block.block_size = block_size;
#ifdef AEC_REFERENCE_GENERATE_MIC_TIMESTAMP
                {
                    /* update minimum len metadata tags for mic output */
                    op_extra_data->mic_metadata_min_tag_len =
                        MAX(op_extra_data->mic_metadata_min_tag_len,
                            op_extra_data->sync_block.block_size);
                }
#endif
            }
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_ENACTING:
            /* uint32 TRUE or FALSE.   Operator should perform rate matching if TRUE  */

            if(terminal_id == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
                set_override_ep_ratematch_enacting(op_extra_data->spkr_endpoint,value);
                if(value==(uint32)FALSE)
                {
                    op_extra_data->spkr_rate_enactment = RATEMATCHING_SUPPORT_NONE;
                }
                else
                {
                    op_extra_data->spkr_rate_enactment = op_extra_data->spkr_rate_ability;
                }

#ifndef CHIP_BASE_A7DA_KAS
                if((op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_HW) && (op_extra_data->spkr_rate_monitor_op))
                {
                    rate_monitor_op_initiatlize(op_extra_data->spkr_rate_monitor_op,op_extra_data->spkr_rate,TRUE,3*MS_PER_SEC);
                }
#endif

                if(op_extra_data->spkr_sw_rateadj_op)
                {
                    cbops_rateadjust_passthrough_mode(op_extra_data->spkr_sw_rateadj_op,(op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);

                }
            }
            else if (terminal_id == AEC_REF_OUTPUT_TERMINAL1
                     && !op_extra_data->mic_sync_enable)
            {
                set_override_ep_ratematch_enacting(op_extra_data->mic_endpoint,value);
                if(value==(uint32)FALSE)
                {
                    op_extra_data->mic_rate_enactment = RATEMATCHING_SUPPORT_NONE;
                }
                else
                {
                    op_extra_data->mic_rate_enactment = op_extra_data->mic_rate_ability;
                }
#ifndef CHIP_BASE_A7DA_KAS
                if((op_extra_data->mic_rate_enactment==RATEMATCHING_SUPPORT_HW) && (op_extra_data->mic_rate_monitor_op))
                {
                    rate_monitor_op_initiatlize(op_extra_data->mic_rate_monitor_op,op_extra_data->mic_rate,TRUE,3*MS_PER_SEC);
                }
#endif

                if(op_extra_data->mic_sw_rateadj_op)
                {
                    cbops_rateadjust_passthrough_mode(op_extra_data->mic_sw_rateadj_op,(op_extra_data->mic_rate_enactment==RATEMATCHING_SUPPORT_NONE)?TRUE:FALSE);
                }
            }
            break;
        case OPMSG_OP_TERMINAL_RATEMATCH_ADJUSTMENT:
        {
            int svalue = (int)value;
            /*  See BlueCore audio real endpoint function "adjust_audio_rate" for details */
            if(terminal_id == (AEC_REF_INPUT_TERMINAL1|TERMINAL_SINK_MASK) )
            {
#ifdef AEC_REFERENCE_SPKR_TTP
                if(op_extra_data->spkr_timed_playback_mode)
                {
                    /* ignore this message when in timed playback mode,
                     * rate adjustment is managed by speaker ttp */
                    break;
                }
#endif /* AEC_REFERENCE_SPKR_TTP */
                /* Send Rate Adjustment to hardware */
                if(op_extra_data->spkr_rate_enactment==RATEMATCHING_SUPPORT_HW)
                {
                    if(op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])
                    {
#ifndef CHIP_BASE_A7DA_KAS
                        if((op_extra_data->spkr_rate_monitor_op) && (rate_monitor_op_is_complete(op_extra_data->spkr_rate_monitor_op)))
                        {
                            op_extra_data->spkr_rate_adjustment = svalue;
                            value = op_extra_data->spkr_rate_adjustment;
                            set_override_ep_ratematch_adjustment(op_extra_data->spkr_endpoint,value);
                            rate_monitor_op_restart(op_extra_data->spkr_rate_monitor_op);

                        }
#else
                        op_extra_data->spkr_rate_adjustment = svalue;
#endif
                    }
                }
                else
                {
                    op_extra_data->spkr_rate_adjustment = svalue;
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
                    if(0 != op_extra_data->spkr_ext_rate_adjust_op)
                    {

                        /* set the target rate value, this will be ignored if speaker path is doing TTP. */
                        stream_delegate_rate_adjust_set_target_rate(op_extra_data->spkr_ext_rate_adjust_op,
                                                                    op_extra_data->spkr_rate_adjustment);
                    }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */
                }
            }
            else if (terminal_id == AEC_REF_OUTPUT_TERMINAL1
                     /* if we are syncing mic to speaker input we shouldn't
                      * receive this message, but ignore it if we received.
                      */
                     && !op_extra_data->mic_sync_enable)
            {
                /* Send Rate Adjustment to hardware */
                if(op_extra_data->mic_rate_enactment == RATEMATCHING_SUPPORT_HW)
                {
                    if(op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1])
                    {
#ifndef CHIP_BASE_A7DA_KAS
                        if((op_extra_data->mic_rate_monitor_op) && (rate_monitor_op_is_complete(op_extra_data->mic_rate_monitor_op)))
                        {
                            op_extra_data->mic_rate_adjustment = svalue;
                            value = op_extra_data->mic_rate_adjustment;
                            set_override_ep_ratematch_adjustment(op_extra_data->mic_endpoint,value);
                            rate_monitor_op_restart(op_extra_data->mic_rate_monitor_op);
                        }
#else
                        op_extra_data->mic_rate_adjustment = svalue;
#endif
                    }
                }
                else
                {
                    op_extra_data->mic_rate_adjustment = svalue;
#ifdef INSTALL_DELEGATE_RATE_ADJUST_SUPPORT
                    if(0 != op_extra_data->mic_ext_rate_adjust_op)
                    {

                        /* set the target rate */
                        stream_delegate_rate_adjust_set_target_rate(op_extra_data->mic_ext_rate_adjust_op,
                                                                    op_extra_data->mic_rate_adjustment);
                    }
#endif /* INSTALL_DELEGATE_RATE_ADJUST_SUPPORT */
                }
            }

        }
        break;
        case OPMSG_OP_TERMINAL_RATEMATCH_REFERENCE:
            /* TODO */
            /* break; */
        default:
            return(FALSE);
    }

    return(TRUE);
}

/* With the change of setting rate match ability for all terminals but
   AEC_REF_INPUT_TERMINAL1 and AEC_REF_OUTPUT_TERMINAL1 to AUTO
   this function is never called.

   TODO.  Remove OPMSG_COMMON_GET_CLOCK_ID from table and delete this function
*/

bool aec_reference_opmsg_ep_clock_id(OPERATOR_DATA *op_data, void *message_data, unsigned *resp_length, OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    unsigned *resp;
    OP_MSG_REQ *msg = message_data;
    unsigned terminal_id = msg->header.cmd_header.client_id;

    patch_fn_shared(aec_reference);

    /* There is a maximum of 2 possible rates at real terminals. All real sources
     * have a rate tied to the Mic rate. All real sinks have a rate tied to the
     * Spkr rate.
     *
     * If the Mic and Spkr share the same clock source then all real terminals
     * share the same rate.
     */

    /* Payload is a single word containing the clock ID */
    resp = xpmalloc(sizeof(unsigned));
    if (!resp)
    {
        return FALSE;
    }
    *resp_data = (OP_OPMSG_RSP_PAYLOAD*)resp;
    *resp_length = 1;

    /*
      This function is only called for terminals marked as real;AEC_REF_INPUT_TERMINAL[1:8] (sink),
      AEC_REF_OUTPUT_TERMINAL[1:8] (source), or AEC_REF_REFERENCE_TERMINAL (source).  Otherwise,
      clock ID will be reported as zero before this operation is called.

      If MIC and SPKR real endpoints are not connected then we report the same clock source.
    */

    if (terminal_id & TERMINAL_SINK_MASK)
    {
        AEC_REFERENCE_OP_DATA   *op_extra_data = (AEC_REFERENCE_OP_DATA*)op_data->extra_op_data;
        /* This is only relevant for the input sinks. If the speaker has the
         * same clock source as the mic then report the same clock source of
         * the op id. If they differ then report op_id and 1 << 7 as the op id
         * is 7 bits long.
         */
        if(!op_extra_data->input_stream[AEC_REF_MIC_TERMINAL1] || !op_extra_data->output_stream[AEC_REF_SPKR_TERMINAL1])
        {
            *resp = op_data->id;
        }
        else if (override_endpoints_have_same_clock_source(op_extra_data->spkr_endpoint,
                                                           op_extra_data->mic_endpoint))
        {
            *resp = op_data->id;
        }
        else
        {
            *resp = op_data->id | (1 << 7);
        }
    }
    else
    {
        /* The sources that this is relevant to is Outputs and Reference. These
         * all have the same clock source. Report default clock source as the
         * op id.
         */
        *resp = op_data->id;
    }

    return TRUE;
}

/*  LocalWords:  metadata
 */
