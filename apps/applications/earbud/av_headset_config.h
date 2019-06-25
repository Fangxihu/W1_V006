/*!
\copyright  Copyright (c) 2008 - 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_config.h
\brief      Application configuration file
*/

#ifndef _AV_HEADSET_CONFIG_H_
#define _AV_HEADSET_CONFIG_H_

/*处理回连配对的逻辑*/
#define RECONNECT_HANDSET

/*处理弹窗的问题*/
#define POP_UP

/*连接的反馈处理，hfp和a2dp没连接上，出现CONNECT_FAILED都到这边-ConnectConfirm*/
/*appHfpHandleHfpSlcConnectConfirm、appA2dpHandleA2dpSignallingConnectConfirm*/

/*关闭手机到耳机、耳机到耳机的aac解码*/
#define DISABLE_AAC

/*自动关机处理*/
#define	AUTO_POWER_OFF

/*电量显示百分比调整，和低版本HFP没电量显示问题*/
#define BATTERY_COMPENSATION

/*设备名问题
appHandleClDmLocalNameComplete*/

/*防止同时进配对、同时回连，对对耳进行限制*/
#define LIMIT_PEER

//PEER_SWTICH

/*断开的时候，自动配对的处理*/
#define AUTO_PAIRIING

 /*处理按键多击*/
#define MULTI_TAP

/*dut mode manager*/
#define INCLUDE_DUT

/*用于工厂需求的单耳测试*/
#define INCLUDE_FTSINGLEPEER

/*用于充电管控LED显示*/
#define CHG_FINISH_LED

/*对W1项目的LED管理*/
#define W1_LED

/*低电处理*/
#define BATTERY_LOW

/**/
#define PEER_PAIRING_ALLOW_CONNECT

/*单个使用处理*/
#define SINGLE_PEER

/*同时长按恢复出厂设置*/
#define SYNC_FT_RESET

/*主从切换自动播放*/
#define PEER_SWTICH

/*调试模式管理，包括：设备名、充电不关机、EQ调试开关*/
#define NAME_USER	FALSE

/*voice prompt swtich to tone*/
#define USER_TONE


//#define CHAIN_MIC_SPK

/*BQB需要，关闭tws主机的source role*/
//#define DISABLE_A2DP_SOURCE

/*==============================END==============================*/


/*! Allow 2nd Earbud to connect to TWS+ Handset after pairing */
#define ALLOW_CONNECT_AFTER_PAIRING (TRUE)

/*! Timeout for entering the case after selecting DFU in
    the UI (with no charger). Normally, can't choose UI
    options with a charger attached.
 */
#define APP_DFU_TIMEOUT_TO_PLACE_CASE   (D_SEC(60))

/*! Timeout for DFU mode, entered after a reboot when upgrade completed */
#define APP_DFU_TIMEOUT_REBOOT_TO_CONFIRM_MS (D_SEC(20))

/*! Timeout for DFU mode, entered after a reboot in DFU mode */
#define APP_DFU_TIMEOUT_REBOOT_IN_DFU_MS (D_SEC(15))

/*! Timeout for DFU mode, entered from UI */
#define APP_DFU_TIMEOUT_ENTERED_FROM_UI_MS (30000)

/*! Timeout for DFU mode, entered from GAIA */
#define APP_DFU_TIMEOUT_ENTERED_FROM_GAIA_MS (20000)

/*! Default speaker gain */
#define HFP_SPEAKER_GAIN    (10)

/*! Default microphone gain */
#define HFP_MICROPHONE_GAIN (15)

/*! Auto answer call on connect */
#define HFP_CONNECT_AUTO_ANSWER

/*! Disable - Auto transfer call on connect */
#undef HFP_CONNECT_AUTO_TRANSFER

/*! Enable HF battery indicator */
#define appConfigHfpBatteryIndicatorEnabled() (1)

/*! Use 2 microphones for HFP phone calls */
//#define HFP_USE_2MIC

/*! Disable support for TWS+ */
#define DISABLE_TWS_PLUS

/*! Default BR/EDR Authenticated Payload Timeout */
#define DEFAULT_BR_EDR_AUTHENTICATED_PAYLOAD_TIMEOUT (10000)

/*! Maximum Authenticated Payload Timeout as per HFP spec */
#define HFP_AUTHENTICATED_PAYLOAD_TIMEOUT_SC_MAX_MS (10000)

#ifdef USE_BDADDR_FOR_LEFT_RIGHT
/*! Left and right earbud roles are selected from Bluetooth address. */
/*! TRUE if this is the left earbud (Bluetooth address LAP is odd). */
#define appConfigIsLeft()           (appGetInit()->appInitIsLeft)
/*! TRUE if this is the right earbud (Bluetooth address LAP is even). */
#define appConfigIsRight()          (appConfigIsLeft() ^ 1)
#else
/*! Left and right earbud roles are selected from the state of this PIO */
#define appConfigHandednessPio()    (2)
/*! TRUE if this is the left earbud (the #appConfigHandednessPio state is 1) */
#define appConfigIsLeft()           ((PioGet32Bank(appConfigHandednessPio() / 32) & (1UL << appConfigHandednessPio())) ? 1 : 0)
/*! TRUE if this is the right earbud (the #appConfigHandednessPio state is 0) */
#define appConfigIsRight()          (appConfigIsLeft() ^ 1)
#endif

/*! Number of paired devices that are remembered */
#define appConfigMaxPairedDevices() (4)

/*! Timeout in seconds for user initiated peer pairing */
#define appConfigPeerPairingTimeout()       (120)
/*! Timeout in seconds for user initiated handset pairing */
#define appConfigHandsetPairingTimeout()    (120)
/*! Timeout in seconds to disable page/inquiry scan after entering idle state */
#define appConfigPairingScanDisableDelay()  (5)

/*! Timeout in seconds for automatic peer pairing */
#define appConfigAutoPeerPairingTimeout()       (0)
/*! Timeout in seconds for automatic handset pairing */
#define appConfigAutoHandsetPairingTimeout()    (120)

/*! Qualcomm Bluetooth SIG company ID */
#define appConfigBtSigCompanyId() (0x00AU)
/*! Qualcomm IEEE company ID */
#define appConfigIeeeCompanyId()  (0x00025BUL)

/*! Key ID peer Earbud link-key derivation */
#define appConfigTwsKeyId()       (0x74777332UL)

/*!@{ @name LED pin PIO assignments (chip specific)
      @brief The LED pads can either be controlled by the led_controller hardware
             or driven as PIOs. The following define the PIO numbers used to
             control the LED pads as PIOs.
*/
#define CHIP_LED_0_PIO 66
#define CHIP_LED_1_PIO 67
#define CHIP_LED_2_PIO 68
#define CHIP_LED_3_PIO 69
#define CHIP_LED_4_PIO 70
#define CHIP_LED_5_PIO 71
//!@}

#if defined(HAVE_1_LED)

/*! The number of LEDs av_headset_led will control. If one LED is configured,
    it will use appConfigLed0Pio(), if two LEDs are configured it will use
    appConfigLed0Pio() and appConfigLed1Pio() etc. */
#define appConfigNumberOfLeds()  (1)

/*! PIO to control LED0 */
#define appConfigLed0Pio()       CHIP_LED_0_PIO
/*! PIO to control LED1 (not used) */
#define appConfigLed1Pio()       (0)
/*! PIO to control LED2 (not used) */
#define appConfigLed2Pio()       (0)

#elif defined(HAVE_2_LEDS)

/* The number of LEDs av_headset_led will control. */
#define appConfigNumberOfLeds()  (2)
/*! PIO to control LED0 */
#define appConfigLed0Pio()       CHIP_LED_0_PIO
/*! PIO to control LED1 */
#define appConfigLed1Pio()       CHIP_LED_2_PIO
/*! PIO to control LED2 */
#define appConfigLed2Pio()       (0)

#elif defined(HAVE_3_LEDS)

/* The number of LEDs av_headset_led will control. */
#define appConfigNumberOfLeds()  (3)
/*! PIO to control LED0 */
#define appConfigLed0Pio()       CHIP_LED_0_PIO
/*! PIO to control LED1 */
#define appConfigLed1Pio()       CHIP_LED_1_PIO
/*! PIO to control LED2 */
#define appConfigLed2Pio()       CHIP_LED_2_PIO

#else
#error LED config not correctly defined.
#endif

/*! Returns boolean TRUE if PIO is an LED pin, otherwise FALSE */
#define appConfigPioIsLed(pio) (((pio) >= CHIP_LED_0_PIO) && ((pio <= CHIP_LED_5_PIO)))
/*! Returns the LED number for a LED PIO. Assumes led is an LED PIO. */
#define appConfigPioLedNumber(pio) ((pio) - CHIP_LED_0_PIO)
/*! Product specific range of PIO that can wake the chip from dormant */
#define appConfigPioCanWakeFromDormant(pio) ((pio) >= 1 && ((pio) <= 8))

/*! Allow LED indications when Earbud is in ear */
#define appConfigInEarLedsEnabled() (TRUE)

/*! Page timeout to use as left earbud attempting connection to the right Earbud. */
#define appConfigLeftEarbudPageTimeout()    (0x2000)//(0x1000)

/*! Page timeout to use as right earbud attempting connection to the left Earbud. */
#define appConfigRightEarbudPageTimeout()   (0x2000)

/*! Page timeout to use for connecting to any non-peer Earbud devices. */
#define appConfigDefaultPageTimeout()       (0x0EEE)//(0x1000)

/*! The page timeout multiplier for Earbuds after link-loss.
    Multiplier should be chosen carefully to make sure total page timeout doesn't exceed 0xFFFF */
#define appConfigEarbudLinkLossPageTimeoutMultiplier()  (6)//(3)

/*! The page timeout multiplier for Handsets after link-loss.
    Multiplier should be chosen carefully to make sure total page timeout doesn't exceed 0xFFFF */
#define appConfigHandsetLinkLossPageTimeoutMultiplier() (12)//(4)

/*! Inactivity timeout after which peer signalling channel will be disconnected, 0 to leave connected (in sniff) */
#define appConfigPeerSignallingChannelTimeoutSecs()   (0)


/*! Link supervision timeout for ACL between Earbuds (in milliseconds) */
#define appConfigEarbudLinkSupervisionTimeout()  (5000)

/*! Default link supervision timeout for other ACLs (in milliseconds) */
#define appConfigDefaultLinkSupervisionTimeout()  (5000)


/*! Minimum volume gain in dB */
#define appConfigMinVolumedB() (-45)

/*! Maximum volume gain in dB */
#define appConfigMaxVolumedB() (0)

/*! Default volume gain in dB */
#define appConfigDefaultVolumedB() (0)

/*! The volume setting to use for no gain, when volume is specifed on range of 0-127 */
#define appConfigVolumeNoGain127Step()  (127)

/*! Number of volume steps to use per AV UI volume event. 
 The full volume range is 0-127 */
#define appConfigGetAvVolumeStep()  (8)

/*! Number of volume steps to use per HFP UI volume event. 
 The full volume range is 0-15 */
 #define appConfigGetHfpVolumeStep() (1)

/*! Default MIC gain. */
#define appConfigMicGain()          (18)

/*! The minimum time to play to be added on incoming SCO connections 
    to allow synchronisation. This should represent the total propagation 
    delay in the chain */
#define appConfigScoChainBaseTTP()      (15000)

/*! Time to play to be applied on this earbud, based on the Wesco
    value specified when creating the connection.
    A value of 0 will disable TTP.  */
#define appConfigScoChainTTP(wesco)     0

/*! Should the earbud automatically try to pair with a peer earbud
 * if no pairing exists? */
#define appConfigAutoPeerPairingEnabled()   (TRUE)

/*! Should the earbud automatically try to pair with a handset
 * if no pairing exists? */
#define appConfigAutoHandsetPairingEnabled()   (TRUE)

/*! Should the earbud automatically reconnect to last connected
 * handset on startup? */
#define appConfigAutoReconnectHandset()     (TRUE)

/*! The time before the TTP at which a packet should be transmitted */
#define appConfigTwsTimeBeforeTx()  MAX(70000, TWS_STANDARD_LATENCY_US-200000)

/*! The last time before the TTP at which a packet may be transmitted */
#define appConfigTwsDeadline()      MAX(35000, TWS_STANDARD_LATENCY_US-250000)

/*! Charger configuration */

/*! The time to debounce charger state changes (ms).  
    The charger hardware will have a more limited range. */
#define appConfigChargerStateChangeDebounce()          (128)

/*! Trickle-charge current (mA) */
#define appConfigChargerTrickleCurrent()               (10)

/*! Pre-charge current (mA)*/
#define appConfigChargerPreCurrent()                   (20)

/*! Pre-charge to fast-charge threshold */
#define appConfigChargerPreFastThresholdVoltage()      (3000)

/*! Fast-charge current (mA) */
#define appConfigChargerFastCurrent()                  (50)

/*! Fast-charge (constant voltage) to standby transition point.
    Percentage of the fast charge current */
#define appConfigChargerTerminationCurrent()           (10)

/*! Fast-charge Vfloat voltage */
#define appConfigChargerTerminationVoltage()           (4200)

/*! Standby to fast charge hysteresis (mV) */
#define appConfigChargerStandbyFastVoltageHysteresis() (250)

/* Enable short timeouts for charger/battery platform testing */
#ifdef CF133_BATT
#define CHARGER_PRE_CHARGE_TIMEOUT_MS D_MIN(5)
#define CHARGER_FAST_CHARGE_TIMEOUT_MS D_MIN(15)
#else
#define CHARGER_PRE_CHARGE_TIMEOUT_MS D_MIN(0)
#define CHARGER_FAST_CHARGE_TIMEOUT_MS D_MIN(0)
#endif

/*! The charger will be disabled if the pre-charge time exceeds this limit.
    Following a timeout, the charger will be re-enabled when the charger is detached.
    Set to zero to disable the timeout. */
#define appConfigChargerPreChargeTimeoutMs() CHARGER_PRE_CHARGE_TIMEOUT_MS

/*! The charger will be disabled if the fast-charge time exceeds this limit.
    Following a timeout, the charger will be re-enabled when the charger is detached.
    Set to zero to disable the timeout. */
#define appConfigChargerFastChargeTimeoutMs() CHARGER_FAST_CHARGE_TIMEOUT_MS

//!@{ @name Battery voltage levels in milli-volts
#define appConfigBatteryFullyCharged()      (4200)
#define appConfigBatteryVoltageOk()         (3800)
#define appConfigBatteryVoltageLow()        (3650)
#define appConfigBatteryVoltageCritical()   (3300)
//!@}

//!@{ @name Battery temperature limits in degrees Celsius.
#define appConfigBatteryChargingTemperatureMax() 45
#define appConfigBatteryChargingTemperatureMin() 0
#define appConfigBatteryDischargingTemperatureMax() 60
#define appConfigBatteryDischargingTemperatureMin() -20
//!@}

/*! The interval at which the battery voltage is read. */
#define appConfigBatteryReadPeriodMs() D_SEC(2)

/*! Margin to apply on battery readings before accepting that 
    the level has changed. Units of milli-volts */
#define appConfigSmBatteryHysteresisMargin() (50)

/*! Setting used to indicate that the MIC to use is not configured */
#define NO_MIC  (0xFF)

/*! Define which channel the 'left' audio channel comes out of. */
#define appConfigLeftAudioChannel()              (AUDIO_CHANNEL_A)

/*! Define which audio instance is used for microphone */
#define appConfigMicAudioInstance()              (AUDIO_INSTANCE_0)

/*! Define whether audio should start with or without a soft volume ramp */
#define appConfigEnableSoftVolumeRampOnStart() (FALSE)

/*! Time to wait for successful disconnection of links to peer and handset
 *  before forcing factory reset. */
#define appConfigFactoryResetTimeoutMs()        (5000)

/*! Time to wait for successful disconnection of links to peer and handset
 *  in terminating substate before shutdown/sleep. */
#define appConfigLinkDisconnectionTimeoutTerminatingMs() D_SEC(5)

/*!@{ @name External AMP control
      @brief If required, allows the PIO/bank/masks used to control an external
             amp to be defined.
*/
#if defined(CE821_CF212) || defined(CF376_CF212)

#define appConfigExternalAmpControlRequired()    (TRUE)
#define appConfigExternalAmpControlPio()         (32)
#define appConfigExternalAmpControlPioBank()     (1)
#define appConfigExternalAmpControlEnableMask()  (0 << 0)
#define appConfigExternalAmpControlDisableMask() (1 << (appConfigExternalAmpControlPio() % 32))

#else

#define appConfigExternalAmpControlRequired()    (FALSE)
#define appConfigExternalAmpControlPio()         (0)
#define appConfigExternalAmpControlEnableMask()  (0)
#define appConfigExternalAmpControlDisableMask() (0)

#endif /* defined(CE821_CF212) or defined(CF376_CF212) */
//!@}


#if !defined(CF133)

/*! Only enable LED indications when Earbud is out of ear */
#undef appConfigInEarLedsEnabled()
#define appConfigInEarLedsEnabled() (TRUE)//(FALSE)

#endif /* !defined(CF133) */


/*! Timeout for A2DP audio when earbud removed from ear. */
#define appConfigOutOfEarA2dpTimeoutSecs()      (2)

/*! Timeout within which A2DP audio will be automatically restarted
 *  if placed back in the ear. */
#define appConfigInEarA2dpStartTimeoutSecs()    (10)

/*! Timeout for SCO audio when earbud removed from ear. */
#define appConfigOutOfEarScoTimeoutSecs()      (2)

/*! Time to wait to connect AVRCP after a remotely initiated A2DP connection
    indication if the remote device does not initiate a AVRCP connection */
#define appConfigAvrcpConnectDelayAfterRemoteA2dpConnectMs() D_SEC(3)

/*! Time to wait to connect A2DP media channel after a locally initiated A2DP connection */
#define appConfigA2dpMediaConnectDelayAfterLocalA2dpConnectMs() D_SEC(3)

/*! This timer is active in APP_STATE_OUT_OF_CASE_IDLE if set to a non-zero value.
    On timeout, the SM will allow sleep. */
#define appConfigIdleTimeoutMs()   0//D_SEC(300)

/*! Default DAC disconnection delay (in milliseconds) */
#define appConfigDacDisconnectionDelayMs()    500//(60000)

/*! Microphone path delay variation. */
#define appConfigMicPathDelayVariationUs()      (10000)

/*! Enable or disable voice quality measurements for TWS+. */
#define appConfigVoiceQualityMeasurementEnabled() TRUE

/*! The worst reportable voice quality */
#define appConfigVoiceQualityWorst() 0
/*! The best reportable voice quality */
#define appConfigVoiceQualityBest() 15

/*! The voice quality to report if measurement is disabled. Must be in the
    range appConfigVoiceQualityWorst() to appConfigVoiceQualityBest(). */
#define appConfigVoiceQualityWhenDisabled() appConfigVoiceQualityBest()

/*! This config relates to the behavior of the TWS standard master when AAC sink.
    If TRUE, it will forward the received stereo AAC data to the slave earbud.
    If FALSE, it will transcode one channel to SBC mono and forward. */
#ifdef DISABLE_AAC
#define appConfigAACStereoForwarding() FALSE
#define appConfigAACEnbable() FALSE
#else
#define appConfigAACStereoForwarding() TRUE
#define appConfigAACEnbable() TRUE
#endif

/*! When a voice prompt is played, for this period of time, any repeated prompt
    will not be played. Set to zero to play all prompts, regardless of whether
    the prompt was recently played. */
#define appConfigPromptNoRepeatDelay() D_SEC(2)

#if defined(INCLUDE_PROXIMITY)
#include "av_headset_proximity.h"
/*! The proximity sensor configuration */
extern const proximityConfig proximity_config;
/*! Returns the proximity sensor configuration */
#define appConfigProximity() (&proximity_config)
#endif /* INCLUDE_PROXIMITY */


#if defined(INCLUDE_ACCELEROMETER)
#include "av_headset_accelerometer.h"
/*! The accelerometer configuration */
extern const accelerometerConfig accelerometer_config;
/*! Returns the accelerometer configuration */
#define appConfigAccelerometer() (&accelerometer_config)
#endif /* INCLUDE_ACCELEROMETER */

#if defined(INCLUDE_TEMPERATURE)
#include "av_headset_temperature.h"
/*! The temperature sensor measurement interval in milli-seconds. */
#define appConfigTemperatureMeasurementIntervalMs() D_SEC(10)

#if defined(HAVE_THERMISTOR)
#include "peripherals/thermistor.h"
/*! The thermistor configuration */
extern const thermistorConfig thermistor_config;
/*! Returns the thermistor configration */
#define appConfigThermistor() (&thermistor_config)
#endif  /* HAVE_THERMISTOR */

#endif /* INCLUDE_TEMPERATURE */


#if defined(INCLUDE_SCOFWD)
/*! The time to play delay added in the SCO receive path.  

    A value of 40 will cause some missing / delayed packets in
    good test conditions. 
    It is estimated that 60 is the lowest value that may be
    used in the real world, with random 2.4GHz interference.
    It is recommended that the final value used should be selected 
    based on expected useage, and tolerance for delays vs. errors
    introduced by Packet Loss Concealment. */
#define appConfigScoFwdVoiceTtpMs()         (70)
#endif

#include "av_headset_ui.h"
/*! The audio prompt configuration */
extern const promptConfig prompt_config[];
/*! Return the indexed audio prompt configuration */
#define appConfigGetPromptConfig(index) &prompt_config[index]

#endif /* _AV_HEADSET_CONFIG_H_ */

