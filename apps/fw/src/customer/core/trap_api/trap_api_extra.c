/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   Part of 6.3.0 */
/**
 * \file
 *
 * Contains trap implementations that appear inline in syscalls.in
 */

#include "trap_api/trap_api_private.h"

#include <message.h>
#include <test.h>
#include <pio.h>
#include <stream.h>
#include <source.h>
#include <sink.h>

static void cleanup_handler(Task t, MessageId id, Message payload)
{
    UNUSED(t);
    if(id == MESSAGE_STREAM_DISCONNECT)
    {
        const MessageStreamDisconnect *m = (const MessageStreamDisconnect *)payload;
        if(m && m->source)
        {
            (void)MessageStreamTaskFromSource(m->source, NULL);
            (void)StreamConnectDispose(m->source);
        }
    }
}

static TaskData cleanup = { cleanup_handler };


/*!
  @brief Send a message to the corresponding task immediately.
  The message will be passed to free after delivery.

  @param task The task to deliver the message to.
  @param id The message type identifier.
  @param message The message data (if any).
*/
void MessageSend(Task task, MessageId id, void *message)
{
  MessageSendLater(task, id, message, D_IMMEDIATE);
}

/*!
  @brief Make an automatic connection between a source and sink, or dispose it.

  @param source The Source data will be taken from.
  @param sink The Sink data will be written to.
  @return TRUE if the connection was made between \e source and \e sink;
    FALSE if the initial connection failed (in which case, if \e source was
    valid, it will have been immediately passed to StreamConnectDispose()).

  Like StreamConnect(), but if the connection could not be made then the
  source will be passed to StreamConnectDispose(). Similarly, if the
  connection is subsequently broken using StreamDisconnect() or by the
  sink being closed the source will be passed to StreamConnectDispose().

  The end result is that the source will be tidied up correctly, no
  matter what happens after this call.

  Note that the task associated with the source will be
  changed. Messages related to the source will no longer be sent to
  the task previously associated with it.
*/
bool StreamConnectAndDispose(Source source, Sink sink)
{
    if(source)
    {
        /* Cast to remove const-ness is okay here */
        (void)MessageStreamTaskFromSource(source, (Task)&cleanup);
        if (StreamConnect(source, sink))
        {
            return TRUE;
        }
        (void)MessageStreamTaskFromSource(source, NULL);
        (void)StreamConnectDispose(source);
    }
    return FALSE;
}


/*!
   @brief  Sends a message (BCCMD) for BT radio test.
   @param  len Command length.
   @param  args The command payload.
   @return TRUE if successful, otherwise FALSE.
*/
static bool test_perform_bt(uint16 len, const uint16 * args)
{
    IPC_TESTTRAP_BT_REQ *ipc_prim;
    IPC_BOOL_RSP resp;
    uint16 ipc_prim_len;

    ipc_prim_len = (uint16)(offsetof(IPC_TESTTRAP_BT_REQ, cmd) +
                                                     len * sizeof(uint16));
    if((ipc_prim = (IPC_TESTTRAP_BT_REQ *)xpmalloc(ipc_prim_len)) == NULL)
    {
        return FALSE;
    }

    ipc_prim->len = len;
    /*lint -e419 struct hack used in ipc_prim. */
    memcpy(&ipc_prim->cmd[0], args, len * sizeof(uint16));
    ipc_send(IPC_SIGNAL_ID_TESTTRAP_BT_REQ, ipc_prim, ipc_prim_len);
    (void)ipc_recv(IPC_SIGNAL_ID_TESTTRAP_BT_RSP, &resp);
    xpfree(ipc_prim);

    return resp.ret;
}

/*!
    @brief Radio test pause.
*/
bool TestPause(void)
{
  static const uint16 args[1] = { 0 /* RADIOTEST_PAUSE */ };
  return test_perform_bt(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief Radio test tx start.
   @param lo_freq  The lo_freq to use.
   @param level The level to use. The lower 8 bits are the internal gain.
   The upper 8 bits are the external gain.
   @param mod_freq The mod_freq to use.
*/
bool TestTxStart(uint16 lo_freq, uint16 level, uint16 mod_freq)
{
  uint16 args[4];
  args[0] = 1; /* RADIOTEST_TXSTART */
  args[1] = lo_freq;
  args[2] = level;
  args[3] = mod_freq;
  return test_perform_bt(sizeof(args)/sizeof(*args), args);
}

/*!
  @brief Radio test tx data 1.
  @param lo_freq  Local oscillator frequency to use.
  @param level    Transmit level to use. The lower 8 bits are the internal gain.
  The upper 8 bits are the external gain.
*/
bool TestTxData1(uint16 lo_freq, uint16 level)
{
  uint16 args[3];
  args[0] = 4; /* RADIOTEST_TXDATA1 */
  args[1] = lo_freq;
  args[2] = level;
  return test_perform_bt(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief Radio test tx data 2.
   @param cc The cc to use.
   @param level The level to use. The lower 8 bits are the internal gain.
   The upper 8 bits are the external gain.
*/
bool TestTxData2(uint16 cc, uint16 level)
{
  uint16 args[3];
  args[0] = 5; /* RADIOTEST_TXDATA2 */
  args[1] = cc;
  args[2] = level;
  return test_perform_bt(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief Radio test rx.
   @param lo_freq The lo_freq to use.
   @param highside The high side to use.
   @param attn The attn to use.
*/
bool TestRxStart(uint16 lo_freq, uint16 highside, uint16 attn)
{
  uint16 args[4];
  args[0] = 2; /* RADIOTEST_RXSTART1 */
  args[1] = lo_freq;
  args[2] = highside;
  args[3] = attn;
  return test_perform_bt(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief Radio test deep sleep.
*/
bool TestDeepSleep(void)
{
  static const uint16 args[1] = { 10 /* RADIOTEST_DEEP_SLEEP */ };
  return TestPerform_(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief  Configure hardware loopback for PCM port.
   @param  pcm_mode  Chosen loopback mode. Valid values: 0, 1, 2
*/
bool TestPcmLb(uint16 pcm_mode)
{
  return TestPcmLbIf(pcm_mode, FIRST_PCM_IF);
}

/*!
   @brief  Radio test loop back.
   @param lo_freq The lo_freq to use.
   @param level The level to use.
*/
bool TestLoopback(uint16 lo_freq, uint16 level)
{
  uint16 args[3];
  args[0] = 21; /* RADIOTEST_LOOP_BACK */
  args[1] = lo_freq;
  args[2] = level;
  return test_perform_bt(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief  Configure external hardware loopback for PCM port.
           A block of random data is written to the PCM output
           port and is read back again on the PCM input port.
   @param pcm_mode  Chosen loopback mode. Valid values: 0, 1, 2
*/
bool TestPcmExtLb(uint16 pcm_mode)
{
  return TestPcmExtLbIf(pcm_mode, FIRST_PCM_IF);
}

/*!
   @brief  Radio test for configuring the crystal trim value.
   @param  xtal_ftrim Selected crystal trim value.
*/
bool TestCfgXtalFtrim(uint16 xtal_ftrim)
{
  uint16 args[2];
  args[0] = 29; /* RADIOTEST_CFG_XTAL_FTRIM */
  args[1] = xtal_ftrim;
  return TestPerform_(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief  Play a constant tone on the PCM port (or the codec for
           BC02 with PSKEY_HOSTIO_MAP_SCO_CODEC set).
   @param  freq  Chosen frequency.
   @param  ampl  Chosen amplitude.
   @param  dc    Specifies a constant offset to add to the audio data.
*/
bool TestPcmTone(uint16 freq, uint16 ampl, uint16 dc)
{
  return TestPcmToneIf(freq, ampl, dc, FIRST_PCM_IF);
}

/*!
   @brief  Turn on codec hardware for stereo loopback
   @param  samp_rate     Sampling rate. Valid values: 8000, 11025,
                         16000, 22050, 24000, 32000 and 44100
   @param  reroute_optn  Routing option. Valid values: 0, 1, 2, 3
*/
bool TestCodecStereoLb(uint16 samp_rate, uint16 reroute_optn)
{
  uint16 args[3];
  args[0] = 48; /* RADIOTEST_CODEC_STEREO_LB */
  args[1] = samp_rate;
  args[2] = reroute_optn;
  return TestPerform_(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief  Play a constant tone on the PCM port (or the codec for
           BC02 with PSKEY_HOSTIO_MAP_SCO_CODEC set).
   @param  freq  Chosen frequency. Valid range: 0 (low) to 3 (high).
   @param  ampl  Chosen amplitude. Valid range: 0 (minimum) to 8 (maximum).
   @param  dc    Specifies a constant offset to add to the audio data.
   @param  interface Chosen PCM interface. A value from the \#audio_instance enumeration.
   @return TRUE if successful, else FALSE.
*/
bool TestPcmToneIf(uint16 freq, uint16 ampl, uint16 dc, uint16 interface)
{
  uint16 args[5];
  args[0] = 54; /* RADIOTEST_PCM_TONE_INTERFACE */
  args[1] = freq;
  args[2] = ampl;
  args[3] = dc;
  args[4] = interface;
  return TestPerform_(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief  Configure hardware loopback for PCM port.
   @param  pcm_mode  Chosen mode. Valid values: 0 (slave), 1 (master), 2 (Manchester slave)
   @param  interface Chosen PCM interface. A value from the \#audio_instance enumeration.
   @return TRUE if successful, else FALSE.
*/
bool TestPcmLbIf(uint16 pcm_mode, uint16 interface)
{
  uint16 args[3];
  args[0] = 53; /* RADIOTEST_PCM_LB_INTERFACE */
  args[1] = pcm_mode;
  args[2] = interface;
  return TestPerform_(sizeof(args)/sizeof(*args), args);
}

/*!
   @brief  Configure external hardware loopback for PCM port.
           A block of random data is written to the PCM output
           port and is read back again on the PCM input port.
   @param  pcm_mode  Chosen mode. Valid values: 0 (slave), 1 (master), 2 (Manchester slave)
   @param  interface Chosen PCM interface. A value from the \#audio_instance enumeration.
   @return TRUE if successful, else FALSE.
*/
bool TestPcmExtLbIf(uint16 pcm_mode, uint16 interface)
{
  uint16 args[3];
  args[0] = 55; /* RADIOTEST_PCM_EXT_LB_INTERFACE */
  args[1] = pcm_mode;
  args[2] = interface;
  return TestPerform_(sizeof(args)/sizeof(*args), args);
}

#if TRAPSET_PIOEX1
/*!
    @brief Grabs PIOs for use by the LCD block.
    @param mask These are the required LCD segments. b0-PIO0, b31=PIO31.
    @param pins These should be set to one.
    @param common_pin The PIO that will be connected to the LCD common.
    @return zero if successful, otherwise bad bits returned and nothing done.
*/
uint32 PioSetLcdPins( uint32 mask, uint32 pins, uint16 common_pin )
{
    static uint32 lcd_segments_in_use32 = 0;
    uint32 invalid_pios;
    uint32 common32;
    uint32 pios_of_interest32;    /* segments plus common */
    uint16 upper16, lower16, pio;
    uint32 seg_mask32;

    common32             = 0x00000001L << common_pin;
    pios_of_interest32   = mask | common32;

    /* do some input parameter validity checks */
    /* we should whinge about calls doing nothing, fail hard. */
    if( mask == 0x00000000L )
    {
        return (0xffffffffUL);
    }

    /* and we should whinge if requested common is not in range, 0-31 incl */
    if( common32 == 0 )
    {
        /* out-of-range request for common, fail hard */
        return (0xffffffffUL);
    }

    /* not allowed to grab pins if we've already grabbed some.
     * User must make this call exactly once, fail hard.
     */
    if ( lcd_segments_in_use32 != 0L )
    {
        return (0xffffffffUL);
    }

    /* can't use pins that don't physically exist, bagsied PIOs or
     *  PIOs that are in use by other VM apps
     */
    invalid_pios = ~(uint32)PioGetUnusedPins32();   /* all invalid pins */
    invalid_pios &= pios_of_interest32;             /* just the invalid ones
                                                        we're interested in */
    if ( invalid_pios )
    {
        return ( invalid_pios );
    }

    /* the common connection can never be a segment connection */
    invalid_pios = common32 & mask ;
    if ( invalid_pios )
    {
        return ( invalid_pios );
    }

    /* releasing pios is not allowed. */
    if( ( mask & ~pins ) != 0L )
    {
        return ( mask & ~pins );
    }

   /* ensure any LCD output isn't mapped to two PIOs
     * LCD Block has 16 outputs (incl common) which are connected to 32
     * fixed PIOs as follows:
     * LCD[0] -> PIO0 and PIO16
     *        thru
     * LCD[15] -> PIO15 and PIO31
     * It is not allowed (API decision) for one LCD o/p to drive two
     * segments or segment+common
     */
    upper16 = (uint16)((mask | common32) >> 16);
    lower16 = (uint16)((mask | common32) & 0x0000ffffL);

    /* invalid to have bit[n+16] and bit[n] both set */
    if ( upper16 & lower16 )
    {
        /* construct 32-bit error value for returning */
        invalid_pios |= ( upper16 & lower16 );                   /* lower 16 */
        invalid_pios |= ((uint32)( upper16 & lower16 ) << 16 );  /* upper 16 */
        return ( invalid_pios );
    }

    /* we're here because request is valid, so do it */
    /* set the PIO muxes for all required segments and common */
    /* firstly, do the segments */
    pio = 0;
    seg_mask32 = mask;
    while ( seg_mask32 != 0L )
    {
        if ( seg_mask32 & 0x00000001L )
        {
            if( !PioSetFunction( pio, LCD_SEGMENT ) )
                invalid_pios |= (0x00000001L << pio);
        }

        seg_mask32 >>= 1;
        pio++;
    }

    /* and do the common pin */
    if ( !PioSetFunction( common_pin, LCD_COMMON ) )
    {
        invalid_pios |= common32;
    }

    /* Note: controlling the clocking, LCD block enabling and the
     * LCD block's segment and common registers is done in LcdSet()
     */

    /* remember the successfully-set segments and common connections so we can
     * check for illegal calls in the future */
    lcd_segments_in_use32 = mask & pins & ~invalid_pios;

    return ( invalid_pios );
}
#endif /* TRAPSET_PIOEX1 */
