/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define ARDUINO_MAIN
#include "Arduino.h"

#include "usb.h"

/* USB VID/PID if not defined, tinyusb to use default value
 * Note: different class combination e.g CDC and (CDC + MSC) should have different
 * PID since Host OS will "remembered" device driver after the first plug */
#define USB_VID  0x239A
#define USB_PID  0x8029

// DEBUG Level 1
#if CFG_DEBUG
// weak function to avoid compilation error with
// non-Bluefruit library sketch such as ADC read test
void Bluefruit_printInfo() __attribute__((weak));
void Bluefruit_printInfo() {}
#endif

// DEBUG Level 3
#if CFG_DEBUG >= 3
#include "SEGGER_SYSVIEW.h"
#endif

static TaskHandle_t  _loopHandle;


// Weak empty variant initialization function.
// May be redefined by variant files.
void initVariant() __attribute__((weak));
void initVariant() { }

#define LOOP_STACK_SZ   (512*3)
#define USBD_STACK_SZ   (150)

static void loop_task(void* arg)
{
  (void) arg;

  setup();

#if CFG_DEBUG
  // If Serial is not begin(), call it to avoid hard fault
  if ( !Serial ) Serial.begin(115200);
  dbgPrintVersion();
  // dbgMemInfo();
  Bluefruit_printInfo();
#endif

  while (1)
  {
    loop();

#ifdef NRF52840_XXAA
    tud_cdc_write_flush();
#endif

    // Serial events
    if (serialEvent && serialEventRun) serialEventRun();
  }
}

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
static void usb_device_task(void* param)
{
  (void) param;

  // RTOS forever loop
  while (1)
  {
    // tinyusb device task
    tud_task();
  }
}

// \brief Main entry point of Arduino application
int main( void )
{
  init();
  initVariant();

#ifdef NRF52840_XXAA
  USBDevice.addInterface( (Adafruit_USBDev_Interface&) Serial);
  USBDevice.begin(USB_VID, USB_PID);
  usb_init();

  // Create a task for tinyusb device stack
  xTaskCreate( usb_device_task, "usbd", USBD_STACK_SZ, NULL, TASK_PRIO_HIGH, NULL);
#endif

#if CFG_DEBUG >= 3
  SEGGER_SYSVIEW_Conf();
#endif

  // Create a task for loop()
  xTaskCreate( loop_task, "loop", LOOP_STACK_SZ, NULL, TASK_PRIO_LOW, &_loopHandle);

  // Initialize callback task
  ada_callback_init();

  // Start FreeRTOS scheduler.
  vTaskStartScheduler();

  NVIC_SystemReset();

  return 0;
}

void suspendLoop(void)
{
  vTaskSuspend(_loopHandle);
}

extern "C"
{

// nanolib printf() retarget
int _write (int fd, const void *buf, size_t count)
{
  (void) fd;

  if ( Serial )
  {
    return Serial.write( (const uint8_t *) buf, count);
  }
  return 0;
}

}

