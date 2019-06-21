/*!
\copyright  Copyright (c) 2008 - 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_ui.c
\brief      Application User Interface
*/

#include <panic.h>
#include <ps.h>
#include <boot.h>
#include <input_event_manager.h>

#include "av_headset.h"
#include "av_headset_ui.h"
#include "av_headset_sm.h"
#include "av_headset_hfp.h"
#include "av_headset_power.h"
#include "av_headset_log.h"

/*! Include the correct button header based on the number of buttons available to the UI */
#if defined(HAVE_9_BUTTONS)
#include "9_buttons.h"
#elif defined(HAVE_6_BUTTONS)
#include "6_buttons.h"
#elif defined(HAVE_1_BUTTON)
#include "1_button.h"
#else
#error "No buttons define found"
#endif

/*! User interface internal messasges */
enum ui_internal_messages
{
    /*! Message sent later when a prompt is played. Until this message is delivered
        repeat prompts will not be played */
    UI_INTERNAL_CLEAR_LAST_PROMPT = 0x20,
#ifdef MULTI_TAP
    UI_INTERNAL_MULTI_TAP,
#endif
#ifdef INCLUDE_DUT
    UI_INTERNAL_DUT,
#endif

};

/*! At the end of every tone, add a short rest to make sure tone mxing in the DSP doens't truncate the tone */
#define RINGTONE_STOP  RINGTONE_NOTE(REST, HEMIDEMISEMIQUAVER), RINGTONE_END

/*!@{ \name Definition of LEDs, and basic colour combinations

    The basic handling for LEDs is similar, whether there are
    3 separate LEDs, a tri-color LED, or just a single LED.
 */

#if (appConfigNumberOfLeds() == 3)
#define LED_0_STATE  (1 << 0)
#define LED_1_STATE  (1 << 1)
#define LED_2_STATE  (1 << 2)
#define LED_ALL  (LED_0_STATE | LED_1_STATE | LED_2_STATE)
#elif (appConfigNumberOfLeds() == 2)
/* We only have 2 LED so map all control to the same LED */
#define LED_0_STATE  (1 << 0)
#define LED_1_STATE  (1 << 1)
#define LED_2_STATE  (1 << 1)
#else
/* We only have 1 LED so map all control to the same LED */
#define LED_0_STATE  (1 << 0)
#define LED_1_STATE  (1 << 0)
#define LED_2_STATE  (1 << 0)
#endif

#define LED_RED     (LED_0_STATE)
#define LED_BLUE   (LED_1_STATE)
#define LED_WHITE  (LED_RED | LED_BLUE)

/*!@} */

/*! \brief An LED filter used for battery low

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_battery_low(uint16 led_state)
{
    return (led_state) ? LED_RED : 0;
}

/*! \brief An LED filter used for low charging level

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_charging_low(uint16 led_state)
{
    UNUSED(led_state);
    return LED_RED;
}

/*! \brief An LED filter used for charging level OK

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_charging_ok(uint16 led_state)
{
    UNUSED(led_state);
    return LED_RED;
}

/*! \brief An LED filter used for charging complete 

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_charging_complete(uint16 led_state)
{
    UNUSED(led_state);
#if (appConfigNumberOfLeds() == 1)
    return LED_RED;
#else
    return LED_BLUE;
#endif
}

#ifdef CHG_FINISH_LED
/*! \brief An LED filter used for charging finish 

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_charging_finish(uint16 led_state)
{
    UNUSED(led_state);
    return 0;
}
#endif

#ifdef INCLUDE_DUT
uint16 app_led_filter_DUT(uint16 led_state)
{
    UNUSED(led_state);
    return (LED_BLUE | LED_RED);
}
#endif

/*! \cond led_patterns_well_named
    No need to document these. The public interface is
    from public functions such as appUiPowerOn()
 */

const ledPattern app_led_pattern_power_on[] = 
{
    LED_LOCK,
#if (appConfigNumberOfLeds() == 3)
    LED_ON(LED_ALL),    LED_WAIT(100),
    LED_OFF(LED_ALL),  LED_WAIT(100),
#elif (appConfigNumberOfLeds() == 1)
    LED_ON(LED_RED),    LED_WAIT(200),
    LED_OFF(LED_RED),  LED_WAIT(200),
#endif
    LED_UNLOCK,
    LED_END
};

const ledPattern app_led_pattern_power_off[] = 
{
    LED_LOCK,
    LED_ON(LED_WHITE), LED_WAIT(100), LED_OFF(LED_WHITE), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
};

const ledPattern app_led_pattern_error[] = 
{
#ifdef W1_LED
    LED_LOCK,
    LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_END
#else
    LED_LOCK,
    LED_ON(LED_RED), LED_WAIT(100), LED_OFF(LED_RED), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
#endif
};

const ledPattern app_led_pattern_idle[] = 
{
#ifdef W1_LED
    LED_LOCK,
    LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_END
#else
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(100), LED_OFF(LED_BLUE),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
#endif
};

const ledPattern app_led_pattern_idle_connected[] = 
{
#ifdef W1_LED
    LED_LOCK,
    LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_END
#else
    LED_SYNC(1000),
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(100), LED_OFF(LED_BLUE),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
#endif
};

const ledPattern app_led_pattern_pairing[] = 
{
    LED_LOCK,
#if (appConfigNumberOfLeds() == 1)
    LED_ON(LED_RED), LED_WAIT(400), LED_OFF(LED_RED), LED_WAIT(600),
#else
    LED_ON(LED_BLUE), LED_WAIT(150), LED_OFF(LED_BLUE), LED_WAIT(150),
    LED_ON(LED_RED), LED_WAIT(150), LED_OFF(LED_RED), LED_WAIT(150),
#endif
    LED_UNLOCK,
    LED_REPEAT(0, 0)
};
#ifdef RECONNECT_HANDSET
const ledPattern app_led_pattern_reconnecting[] = 
{
    LED_SYNC(1000),
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(200), LED_OFF(LED_BLUE),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
};
#endif

const ledPattern app_led_pattern_pairing_deleted[] = 
{
#ifdef W1_LED
    LED_LOCK,
    LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_END
#else
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(100), LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
#endif
};

const ledPattern app_led_pattern_peer_pairing[] =
{
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(100), LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_REPEAT(0, 0)
};

#ifdef INCLUDE_DFU
const ledPattern app_led_pattern_dfu[] = 
{
    LED_LOCK,
    LED_ON(LED_RED), LED_WAIT(200), LED_OFF(LED_RED), LED_WAIT(800),
    LED_UNLOCK,
    LED_REPEAT(0, 0)
};
#endif

#ifdef INCLUDE_AV
const ledPattern app_led_pattern_streaming[] =
{
#ifdef W1_LED
    LED_LOCK,
    LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_END
#else
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(50), LED_OFF(LED_BLUE), LED_WAIT(50),
    LED_REPEAT(2, 2),
    LED_WAIT(500),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
#endif
};
#endif

#ifdef INCLUDE_AV
const ledPattern app_led_pattern_streaming_aptx[] =
{
#ifdef W1_LED
    LED_LOCK,
    LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_END
#else
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(50), LED_OFF(LED_BLUE), LED_WAIT(50),
    LED_REPEAT(2, 2),
    LED_WAIT(500),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
#endif
};
#endif

const ledPattern app_led_pattern_sco[] = 
{
#ifdef W1_LED
    LED_LOCK,
    LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_END
#else
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(50), LED_OFF(LED_BLUE), LED_WAIT(50),
    LED_REPEAT(2, 1),
    LED_WAIT(500),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
#endif
};

const ledPattern app_led_pattern_call_incoming[] = 
{
#ifdef W1_LED
	LED_SYNC(2000),
	LED_LOCK,
	LED_ON(LED_WHITE), LED_WAIT(50), LED_OFF(LED_WHITE), LED_WAIT(50),
	LED_REPEAT(2, 1),
	LED_WAIT(500),
	LED_UNLOCK,
	LED_REPEAT(0, 0),
#else
    LED_LOCK,
    LED_SYNC(1000),
    LED_ON(LED_WHITE), LED_WAIT(50), LED_OFF(LED_WHITE), LED_WAIT(50),
    LED_REPEAT(2, 1),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
#endif
};

const ledPattern app_led_pattern_battery_empty[] = 
{
#ifndef BATTERY_LOW
    LED_LOCK,
    LED_ON(LED_RED),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
#else
    LED_LOCK,
    LED_ON(LED_RED), LED_WAIT(100), LED_OFF(LED_RED), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_WAIT(1000),
    LED_UNLOCK,
    LED_REPEAT(0, 0)
#endif
};
/*! \endcond led_patterns_well_named
 */


/*! \cond constant_well_named_tones 
    No Need to document these tones. Their access through functions such as
    appUiIdleActive() is the public interface.
 */
 
const ringtone_note app_tone_button[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_button_2[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_button_3[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_button_4[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

#ifdef INCLUDE_DFU
const ringtone_note app_tone_button_dfu[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(A7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};
#endif

const ringtone_note app_tone_button_factory_reset[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(A7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(C7, SEMIQUAVER),
    RINGTONE_NOTE(B7, SEMIQUAVER),
    RINGTONE_STOP
};

#ifdef INCLUDE_AV
const ringtone_note app_tone_av_connect[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_disconnect[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_remote_control[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_connected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_disconnected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_link_loss[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_STOP
};
#endif

const ringtone_note app_tone_hfp_connect[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_connected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_disconnected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_link_loss[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_STOP
};
        
const ringtone_note app_tone_hfp_sco_connected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(AS5, DEMISEMIQUAVER),
    RINGTONE_NOTE(DS6, DEMISEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_sco_disconnected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(DS6, DEMISEMIQUAVER),
    RINGTONE_NOTE(AS5, DEMISEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_mute_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_sco_unencrypted_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_ring[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_NOTE(REST, SEMIQUAVER),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_ring_caller_id[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_voice_dial[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_voice_dial_disable[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_answer[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_hangup[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_mute_active[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(CS7, SEMIQUAVER),
    RINGTONE_NOTE(DS7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_mute_inactive[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(DS7, SEMIQUAVER),
    RINGTONE_NOTE(CS7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_talk_long_press[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_pairing[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_paired[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A6, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_pairing_deleted[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(A6, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_volume[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(8),
    RINGTONE_NOTE(B6, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_volume_limit[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(F7, SEMIQUAVER),
    RINGTONE_NOTE(F7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_error[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_battery_empty[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6, SEMIQUAVER),
    RINGTONE_NOTE(B6, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_power_on[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(CS5, SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_power_off[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(CS5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_paging_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_peer_pairing[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_peer_pairing_error[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_peer_pairing_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

#ifdef INCLUDE_DFU
const ringtone_note app_tone_dfu[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(CS5, SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_STOP
};
#endif

/*! \endcond constant_well_named_tones */

/*! \brief Play tone.
    \param tone The tone to play.
    \param interruptible If TRUE, always play to completion, if FALSE, the tone may be
    interrupted before completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
    in client_lock when the tone finishes - either on completion, when interrupted,
    or if the tone is not played at all, because the UI is not currently playing tones.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appUiPlayToneCore(const ringtone_note *tone, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask)
{
#ifndef INCLUDE_TONES
    UNUSED(tone);
    UNUSED(interruptible);
#else
    /* Only play tone if it can be heard */
    if (PHY_STATE_IN_EAR == appPhyStateGetState())
    {
        appKymeraTonePlay(tone, interruptible, client_lock, client_lock_mask);
    }
    else
#endif
    {
        if (client_lock)
        {
            *client_lock &= ~client_lock_mask;
        }
    }
}

/*! \brief Play prompt.
    \param prompt The prompt to play.
    \param interruptible If TRUE, always play to completion, if FALSE, the prompt may be
    interrupted before completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
    in client_lock when the prompt finishes - either on completion, when interrupted,
    or if the prompt is not played at all, because the UI is not currently playing prompts.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appUiPlayPromptCore(voicePromptName prompt, bool interruptible,
                         uint16 *client_lock, uint16 client_lock_mask)
{
#ifndef INCLUDE_PROMPTS
    UNUSED(prompt);
    UNUSED(interruptible);
#else
    uiTaskData *theUi = appGetUi();
    PanicFalse(prompt < NUMBER_OF_PROMPTS);
    /* Only play prompt if it can be heard */
    if ((PHY_STATE_IN_EAR == appPhyStateGetState()) && (prompt != theUi->prompt_last))
    {
        const promptConfig *config = appConfigGetPromptConfig(prompt);
        FILE_INDEX *index = theUi->prompt_file_indexes + prompt;
        if (*index == FILE_NONE)
        {
            const char* name = config->filename;
            *index = FileFind(FILE_ROOT, name, strlen(name));
            /* Prompt not found */
            PanicFalse(*index != FILE_NONE);
        }
        appKymeraPromptPlay(*index, config->format, config->rate,
                            interruptible, client_lock, client_lock_mask);

        if (appConfigPromptNoRepeatDelay())
        {
            MessageCancelFirst(&theUi->task, UI_INTERNAL_CLEAR_LAST_PROMPT);
            MessageSendLater(&theUi->task, UI_INTERNAL_CLEAR_LAST_PROMPT, NULL,
                             appConfigPromptNoRepeatDelay());
            theUi->prompt_last = prompt;
        }
    }
    else
#endif
    {
        if (client_lock)
        {
            *client_lock &= ~client_lock_mask;
        }
    }
}

/*! \brief Report a generic error on LEDs and play tone */
void appUiError(void)
{
    appUiPlayTone(app_tone_error);
    appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
}

/*! \brief Play HFP error tone and set LED error pattern.
    \param silent If TRUE the error is not presented on the UI.
*/
void appUiHfpError(bool silent)
{
    if (!silent)
    {
#if 0
        appUiPlayTone(app_tone_error);
        appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
#endif
    }
}

/*! \brief Play AV error tone and set LED error pattern.
    \param silent If TRUE the error is not presented on the UI.
*/
void appUiAvError(bool silent)
{
    if (!silent)
    {
#if 0
        appUiPlayTone(app_tone_error);
        appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
#endif
    }
}

/*! \brief Play power on prompt and LED pattern */
void appUiPowerOn(void)
{
    /* Enable LEDs */
    appLedEnable(TRUE);
    
    appLedSetPattern(app_led_pattern_power_on, LED_PRI_EVENT);
    appUiPlayPrompt(PROMPT_POWER_ON);
}

/*! \brief Play power off prompt and LED pattern.
    \param lock The caller's lock, may be NULL.
    \param lock_mask Set bits in lock_mask will be cleared in lock when the UI completes.
 */
void appUiPowerOff(uint16 *lock, uint16 lock_mask)
{
    appLedSetPattern(app_led_pattern_power_off, LED_PRI_EVENT);
    appUiPlayPromptClearLock(PROMPT_POWER_OFF, lock, lock_mask);

    /* Disable LEDs */
    appLedEnable(FALSE);
}

/*! \brief Prepare UI for sleep.
    \note If in future, this function is modified to play a tone, it should
    be modified to resemble #appUiPowerOff, so the caller's lock is cleared when
    the tone is completed. */
void appUiSleep(void)
{
    appLedSetPattern(app_led_pattern_power_off, LED_PRI_EVENT);
    appLedEnable(FALSE);
}

#ifdef INCLUDE_FTSINGLEPEER
bool appUiFTSingleGet(void)
{
    uiTaskData *theUi = appGetUi();

    return theUi->ftsingle_flag;
}

void appUiFTSingleSet(bool value)
{
    uiTaskData *theUi = appGetUi();

    theUi->ftsingle_flag = value;
}

#endif

/**/
#ifdef MULTI_TAP
static void appTapFunction(void)
{
    uiTaskData *theUi = appGetUi();
    theUi->tap_count = theUi->tap_count + 1;
    MessageCancelAll(&theUi->task, UI_INTERNAL_MULTI_TAP);
    MessageSendLater(&theUi->task, UI_INTERNAL_MULTI_TAP, NULL, 500);
    DEBUG_LOGF("tap_count = %d", theUi->tap_count);
}

static void appUiMultiTapHandle(void)
{
	uiTaskData *theUi = appGetUi();
	uint8 key_count = theUi->tap_count;
	theUi->tap_count = 0;
	MessageCancelAll(&theUi->task, UI_INTERNAL_MULTI_TAP);
	
	switch(key_count)
	{
		case 1:
			DEBUG_LOG("key single!!!");
			if (appSmIsOutOfCase())
			{
                /* If incoming voice call, accept */
                /*else */if (appHfpIsCallIncoming())
                    appHfpCallAccept();
                else if (appScoFwdIsCallIncoming())
                    appScoFwdCallAccept();
                /* If AVRCP to handset connected, send play or pause */
                else if ((appDeviceIsHandsetAvrcpConnected())/*&& appDeviceIsHandsetA2dpConnected()*/)/*add by fang 20190121*/
                    appAvPlayToggle(TRUE);
                /* If AVRCP is peer is connected and peer is connected to handset, send play or pause */
                else if (appDeviceIsPeerAvrcpConnectedForAv() && appPeerSyncIsComplete() && appPeerSyncIsPeerHandsetAvrcpConnected())
                    appAvPlayToggle(TRUE);
                else if (appDeviceIsHandsetHfpConnected() && appDeviceIsHandsetA2dpConnected())
                    appUiError();
                else
                {
                    appSmConnectHandset();
                    appUiAvConnect();
                }
            }
		break;
			
		case 2:
		{
			DEBUG_LOG("key double!!!_forward");
	            if (appSmIsOutOfCase())
	            {
					/* If voice call active, hangup */
					if (appHfpIsCallActive())
						appHfpCallHangup();
						/* Sco Forward can be streaming a ring tone */
					else if (appScoFwdIsReceiving() && !appScoFwdIsCallIncoming())
						appScoFwdCallHangup();
					/* If outgoing voice call, hangup */
					else if (appHfpIsCallOutgoing())
						appHfpCallHangup();
	                else if (appHfpIsCallIncoming())
	                    appHfpCallReject();
	                else if (appScoFwdIsCallIncoming())
	                    appScoFwdCallReject();
	                else
	                	{
		                	if(appConfigIsLeft())
	                		{
								appAvBackward();
	                		}
							else
								appAvForward();
						}
	            }
		}
		break;
		
		case 3:
		{
			DEBUG_LOG("key three!!!_backward");
#ifdef INCLUDE_FTSINGLEPEER
			if ((PHY_STATE_IN_CASE != appPhyStateGetState()) && (!(theUi->ftsingle_flag))\
				&& ((appGetState() == APP_STATE_STARTUP) || (appGetState() == APP_STATE_PEER_PAIRING)))
			{
				DEBUG_LOG("DUT_modle!!!");
				appUiFTSingleSet(TRUE);
				Dut_User_Exit_Peer_Pairing();
			}
#endif
			if (appSmIsOutOfCase())
			{
				appHfpCallVoice();
			}
		}
		break;
		
		case 4:
		{
			DEBUG_LOG("key four!!!_forward");
#ifdef EQ_TUNING
			if (appSmIsOutOfCase())
				appTestPhyStateInCaseEvent();
			else if (appSmIsInCase())
				appTestPhyStateOutOfCaseEvent();
#endif
		}
		break;
		
		case 5:
		{
			DEBUG_LOG("key five!!!_dfu");
#ifdef INCLUDE_DUT
            if ((PHY_STATE_IN_CASE != appPhyStateGetState()) && (!(theUi->dut_flag)))
            {
                DEBUG_LOG("DUT_modle!!!");
                appUiDut();
                Dut_User_Exit_Peer_Pairing();
                MessageSendLater(&theUi->task, UI_INTERNAL_DUT, NULL, 1000);
            }
#endif
		}
		break;
		
		case 7:
		{
			DEBUG_LOG("key 7!!!_ft");
#ifdef INCLUDE_DFU
            if (appSmIsOutOfCase() && appUpgradeUiDfuRequest())
                appUiDfuRequested();
#else
#endif /* INCLUDE_DFU */
		}
		break;
		
		case 9:
		{
			DEBUG_LOG("key nine!!!_power off");
			/*appPowerOffRequest();*/
            if (appSmIsOutOfCase())
        	{
				appSmFactoryReset();
        	}
		}
		break;
		
		case 15:
		{
            DEBUG_LOG("key 15!!!_DUT");
		}
		break;

		default:
			break;
	}
}
#endif

/*! \brief Message Handler

    This function is the main message handler for the UI module, all user button
    presses are handled by this function.

    NOTE - only a single button config is currently defined for both earbuds.
    The following defines could be used to split config amongst the buttons on
    two earbuds.

        APP_RIGHT_CONFIG
        APP_SINGLE_CONFIG
        APP_LEFT_CONFIG
*/    
static void appUiHandleMessage(Task task, MessageId id, Message message)
{
    uiTaskData *theUi = (uiTaskData *)task;
    UNUSED(message);

    switch (id)
    {
        case UI_INTERNAL_CLEAR_LAST_PROMPT:
            theUi->prompt_last = PROMPT_NONE;
        break;
		
#ifdef MULTI_TAP
        case UI_INTERNAL_MULTI_TAP:
            appUiMultiTapHandle();
        break;
#endif

#ifdef INCLUDE_DUT
        case UI_INTERNAL_DUT:
		MessageCancelAll(&theUi->task, UI_INTERNAL_DUT);
		theUi->dut_flag = TRUE;
		ConnectionEnterDutMode();
        break;
#endif

        /* HFP call/reject & A2DP play/pause */
        case APP_MFB_BUTTON_PRESS:
        {
            DEBUG_LOG("APP_MFB_BUTTON_PRESS");
#ifdef MULTI_TAP
			appTapFunction();
#endif
        }
        break;
			
        case APP_MFB_BUTTON_1_SECOND:
        {
            DEBUG_LOG("APP_MFB_BUTTON_1_SECOND");
        }
        break;
		
        case APP_MFB_BUTTON_HELD_1:
            DEBUG_LOG("APP_(MFB)_BUTTON_HELD_1");
            break;

        case APP_MFB_BUTTON_HELD_2:
            DEBUG_LOG("APP_MFB_BUTTON_HELD_3");
            if (appSmIsOutOfCase())
        	{
#ifdef INCLUDE_DUT
				if(theUi->dut_flag)
				{
					appSmReboot();
				}
#endif
				{
				int16 num_steps, hfp_change, av_change;
				if(appConfigIsLeft())
					num_steps = -1; 		
				else
					num_steps = 1;			
		
				hfp_change = (appConfigGetHfpVolumeStep() * num_steps);
				av_change = (appConfigGetAvVolumeStep() * num_steps);

				if (appHfpIsScoActive())
				{
					appHfpVolumeStart(hfp_change);
				}
				else if (appScoFwdIsReceiving())
				{
					appScoFwdVolumeStart(hfp_change);
				}
#ifdef INCLUDE_AV
				else if (appAvIsStreaming())
				{
					appAvVolumeStart(av_change);
				}
#endif
				else if (appHfpIsConnected())
				{
					appHfpVolumeStart(hfp_change);
				}
				else if (appScoFwdIsConnected())
				{
					appScoFwdVolumeStart(hfp_change);
				}
				else
				{
					appUiHfpError(FALSE);
				}
				}
        	}
			else if(PHY_STATE_IN_CASE != appPhyStateGetState())
			{
#ifdef INCLUDE_DUT
				if(theUi->dut_flag)
				{
					appSmReboot();
				}
#endif
			}
            break;
			
        case APP_MFB_BUTTON_2_SECOND:
        {
			int16 vol_dir, hfp_change_vol, av_change_vol;
			if(appConfigIsLeft())
				vol_dir = -1; 		
			else
				vol_dir = 1;	
			hfp_change_vol = (appConfigGetHfpVolumeStep() * vol_dir);
			av_change_vol = (appConfigGetAvVolumeStep() * vol_dir);
			
            DEBUG_LOG("APP_MFB_BUTTON_3_SECOND");
			if (appHfpIsScoActive())
				appHfpVolumeStop(hfp_change_vol);
			else if (appScoFwdIsReceiving())
				appScoFwdVolumeStop(-appConfigGetHfpVolumeStep());
#ifdef INCLUDE_AV
			else if (appAvIsStreaming())
				appAvVolumeStop(av_change_vol);
#endif
			else if (appHfpIsConnected())
				appHfpVolumeStop(hfp_change_vol);
			else if (appScoFwdIsConnected())
				appScoFwdVolumeStop(-appConfigGetHfpVolumeStep());
			/*! \todo why no else with UiHfpError() ? */
        }
        break;

        case APP_MFB_BUTTON_6_SECOND:
        {
            DEBUG_LOG("APP_MFB_BUTTON_6_SECOND");
#ifdef SYNC_FT_RESET
			appScoFwdSyncFactoryResetSet(FALSE);
#endif
        }
        break;
		
        case APP_MFB_BUTTON_HELD_6:
            DEBUG_LOG("APP_MFB_BUTTON_HELD_6");
            if (appSmIsOutOfCase())
        	{
#ifdef SYNC_FT_RESET
        		if(appConfigIsRight())
    			{
					appScoFwdSyncFactoryReset();
				}
#endif
        	}
        break;

		case APP_MFB_BUTTON_7_SECOND:
		{
			DEBUG_LOG("APP_MFB_BUTTON_7_SECOND");
#ifdef SYNC_FT_RESET
			appScoFwdSyncFactoryResetSet(FALSE);
#endif
		}
		break;
		
		case APP_MFB_BUTTON_HELD_7:
			DEBUG_LOG("APP_MFB_BUTTON_HELD_7");
			if (appSmIsOutOfCase())
			{
#ifdef SYNC_FT_RESET
				if(appConfigIsLeft())
				{
					if(appScoFwdSyncFactoryResetGet())
					{
						appScoFwdSyncFactoryReset();
						appSmFactoryReset();
					}
				}
				else
				{
					appScoFwdSyncFactoryReset();
				}
#endif
			}
		break;
    }
}

/*! brief Initialise UI module */
void appUiInit(void)
{
    uiTaskData *theUi = appGetUi();
    
    /* Set up task handler */
    theUi->task.handler = appUiHandleMessage;
    
    /* Initialise input event manager with auto-generated tables for
     * the target platform */
    theUi->input_event_task = InputEventManagerInit(appGetUiTask(), InputEventActions,
                                                    sizeof(InputEventActions),
                                                    &InputEventConfig);

    memset(theUi->prompt_file_indexes, FILE_NONE, sizeof(theUi->prompt_file_indexes));

    theUi->prompt_last = PROMPT_NONE;
#ifdef MULTI_TAP
    theUi->tap_count = 0;
#endif
#ifdef INCLUDE_DUT
    theUi->dut_flag = FALSE;
#endif
#ifdef INCLUDE_FTSINGLEPEER
	theUi->ftsingle_flag = FALSE;
#endif
}
