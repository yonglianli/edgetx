/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include "definitions.h"
#include "edgetx_constants.h"

#include "board_common.h"
#include "hal.h"
#include "hal/serial_port.h"
#include "hal/watchdog_driver.h"

#define FLASHSIZE                       0x200000
#define FLASH_PAGESIZE                  256
#define BOOTLOADER_ADDRESS              0x08000000
#define BOOTLOADER_SIZE                 0x10000
#define FIRMWARE_ADDRESS                0x70000000
#define FIRMWARE_LEN(fsize)             (fsize)
#define FIRMWARE_MAX_LEN                (FLASHSIZE)
#define APP_START_ADDRESS               (uint32_t)(FIRMWARE_ADDRESS)

#define MB                              *1024*1024
#define LUA_MEM_EXTRA_MAX               (2 MB)    // max allowed memory usage for Lua bitmaps (in bytes)
#define LUA_MEM_MAX                     (6 MB)    // max allowed memory usage for complete Lua  (in bytes), 0 means unlimited

extern uint16_t sessionTimer;

#define SLAVE_MODE()                    (g_model.trainerData.mode == TRAINER_MODE_SLAVE)

// Board driver
void boardInit();
void boardOff();

// CPU Unique ID
#define LEN_CPU_UID                     (3*8+2)
void getCPUUniqueID(char * s);

// Pulses driver
#if !defined(SIMU)

#define INTERNAL_MODULE_ON()            gpio_set(INTMODULE_PWR_GPIO)
#define INTERNAL_MODULE_OFF()           gpio_clear(INTMODULE_PWR_GPIO);
#define EXTERNAL_MODULE_ON()            gpio_set(EXTMODULE_PWR_GPIO)
#define EXTERNAL_MODULE_OFF()           gpio_clear(EXTMODULE_PWR_GPIO)
#define EXTERNAL_MODULE_PWR_OFF         EXTERNAL_MODULE_OFF
#define BLUETOOTH_MODULE_ON()           gpio_clear(BLUETOOTH_ON_GPIO)
#define BLUETOOTH_MODULE_OFF()          gpio_set(BLUETOOTH_ON_GPIO)
#define IS_INTERNAL_MODULE_ON()         (false)
#define IS_EXTERNAL_MODULE_ON()         (GPIO_ReadInputDataBit(EXTMODULE_PWR_GPIO, EXTMODULE_PWR_GPIO_PIN) == Bit_SET)

#else

#define INTERNAL_MODULE_OFF()
#define INTERNAL_MODULE_ON()
#define EXTERNAL_MODULE_ON()
#define EXTERNAL_MODULE_OFF()
#define BLUETOOTH_MODULE_ON()
#define BLUETOOTH_MODULE_OFF()
#define IS_INTERNAL_MODULE_ON()         (false)
#define IS_EXTERNAL_MODULE_ON()         (false)

#endif // defined(SIMU)

#if !defined(NUM_FUNCTIONS_SWITCHES)
#define NUM_FUNCTIONS_SWITCHES        0
#endif

#define NUM_TRIMS                       6
#define DEFAULT_STICK_DEADZONE          2

#define BATTERY_WARN                  37 // 3.7V
#define BATTERY_MIN                   35 // 3.4V
#define BATTERY_MAX                   84 // 8.4V
#define BATTERY_DIVIDER             1000

#if defined(__cplusplus) && !defined(SIMU)
extern "C" {
#endif

// Power driver
#define SOFT_PWR_CTRL
#define POWER_ON_DELAY               10 // 1s
void pwrInit();
void extModuleInit();
uint32_t pwrCheck();
uint32_t lowPowerCheck();

void pwrOn();
void pwrSoftReboot();
void pwrOff();
void pwrResetHandler();
bool pwrPressed();
bool pwrOffPressed();
#if defined(PWR_EXTRA_SWITCH_GPIO)
  bool pwrForcePressed();
#else
  #define pwrForcePressed() false
#endif
uint32_t pwrPressedDuration();;
  
const etx_serial_port_t* auxSerialGetPort(int port_nr);
#define AUX_SERIAL_POWER_ON()
#define AUX_SERIAL_POWER_OFF()

// LED driver
void ledInit();
void ledOff();
void ledRed();
void ledBlue();
void ledGreen();

// LCD driver
void lcdSetInitalFrameBuffer(void* fbAddress);
void lcdInit();
void lcdCopy(void * dest, void * src);

void lcdOff();
void lcdOn();

#define lcdRefreshWait(...)

// Backlight driver
#define BACKLIGHT_LEVEL_MAX             100
#define BACKLIGHT_FORCED_ON             BACKLIGHT_LEVEL_MAX + 1
#define BACKLIGHT_LEVEL_MIN             1

extern bool boardBacklightOn;
void backlightLowInit( void );
void backlightInit();
void backlightEnable(uint8_t dutyCycle);
void backlightFullOn();
bool isBacklightEnabled();

#define BACKLIGHT_ENABLE()                                         \
  {                                                                \
    boardBacklightOn = true;                                       \
    backlightEnable(BACKLIGHT_LEVEL_MAX - currentBacklightBright); \
  }

#define BACKLIGHT_DISABLE()                                               \
  {                                                                       \
    boardBacklightOn = false;                                             \
    backlightEnable(((g_eeGeneral.blOffBright == BACKLIGHT_LEVEL_MIN) &&  \
                     (g_eeGeneral.backlightMode != e_backlight_mode_off)) \
                        ? 0                                               \
                        : g_eeGeneral.blOffBright);                       \
  }

#if defined(RADIO_PL18EV)
#define USB_NAME                        "FlySky PL18EV"
#define USB_MANUFACTURER                'F', 'l', 'y', 'S', 'k', 'y', ' ', ' '  /* 8 bytes */
#define USB_PRODUCT                     'P', 'L', '1', '8', 'E', 'V', ' ', ' '  /* 8 Bytes */
#else
#define USB_NAME                        "FlySky PL18"
#define USB_MANUFACTURER                'F', 'l', 'y', 'S', 'k', 'y', ' ', ' '  /* 8 bytes */
#define USB_PRODUCT                     'P', 'L', '1', '8', ' ', ' ', ' ', ' '  /* 8 Bytes */
#endif

#if defined(__cplusplus) && !defined(SIMU)
}
#endif

// Audio driver
int audioInit();
void audioConsumeCurrentBuffer();

// Telemetry driver
#define INTMODULE_FIFO_SIZE            512
#define TELEMETRY_FIFO_SIZE            512

// Haptic driver
void hapticInit();
void hapticDone();
void hapticOff();
void hapticOn(uint32_t pwmPercent);

// Second serial port driver
//#define AUX_SERIAL
#define DEBUG_BAUDRATE                  115200
#define LUA_DEFAULT_BAUDRATE            115200

extern uint8_t currentTrainerMode;
void checkTrainerSettings();

// Touch panel driver
bool touchPanelEventOccured();
struct TouchState touchPanelRead();
struct TouchState getInternalTouchState();

#define IS_FIRMWARE_COMPATIBLE_WITH_BOARD() true

#endif // _BOARD_H_
