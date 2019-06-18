#ifndef BUTTON_CONFIG_H
#define BUTTON_CONFIG_H

#include "input_event_manager.h"
extern const InputEventConfig_t InputEventConfig;
extern const InputActionMessage_t InputEventActions[13];

#define MFB_BUTTON           (1UL <<  0)

#define APP_MFB_BUTTON_6_SECOND                  1006
#define APP_BUTTON_DFU                           1010
#define APP_BUTTON_HELD_FACTORY_RESET            1011
#define APP_BUTTON_POWER_OFF                     1004
#define APP_BUTTON_FACTORY_RESET                 1012
#define APP_MFB_BUTTON_HELD_2                    1003
#define APP_MFB_BUTTON_8_SECOND                  1008
#define APP_MFB_BUTTON_1_SECOND                  1002
#define APP_MFB_BUTTON_HELD_3                    1005
#define APP_MFB_BUTTON_HELD_1                    1001
#define APP_MFB_BUTTON_PRESS                     1000
#define APP_MFB_BUTTON_HELD_4                    1007
#define APP_BUTTON_HELD_DFU                      1009

#endif

