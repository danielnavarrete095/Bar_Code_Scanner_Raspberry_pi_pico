/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "hardware/watchdog.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
// #define DEBUG
// TODO: Create several buffers
#define BUFF_SIZE 500
#define SEND_TIME 10000

void led_blinking_task(void);
void sendTask(void);
void sendToServer();
void cleanBuffer();

void debug_msg(const char *message, const char *value);
void debug(const char *message);
void debug_ch(const char c);

extern void cdc_task(void);
extern void hid_app_task(void);

char buffer[BUFF_SIZE];
uint8_t _index = 0;
unsigned long send_timer = 0;
unsigned long extraTime = 0;
bool sendUrgent = false;
bool connectedAlert = false;
bool disconnectedAlert = false;
bool reset = false;


/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  // sleep_ms(12000);
  // debug("TinyUSB Host CDC MSC HID Example");

  tusb_init();

  if (watchdog_caused_reboot()) {
      printf("Rasp Rebooted by Watchdog!\n");
  } else {
      printf("Rasp Clean boot\n");
  }
  watchdog_enable(3000, 1);

  while (1)
  {
    // tinyusb host task
    tuh_task();
    led_blinking_task();
    sendTask();
    if(!reset) watchdog_update();
  }

  return 0;
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
#if CFG_TUH_CDC
CFG_TUSB_MEM_SECTION static char serial_in_buffer[64] = { 0 };

void tuh_mount_cb(uint8_t dev_addr)
{
  // application set-up
  // printf("A device with address %d is mounted\r\n", dev_addr);

  tuh_cdc_receive(dev_addr, serial_in_buffer, sizeof(serial_in_buffer), true); // schedule first transfer
}

void tuh_umount_cb(uint8_t dev_addr)
{
  // application tear-down
  // printf("A device with address %d is unmounted \r\n", dev_addr);
}

// invoked ISR context
void tuh_cdc_xfer_isr(uint8_t dev_addr, xfer_result_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes)
{
  (void) event;
  (void) pipe_id;
  (void) xferred_bytes;

  printf(serial_in_buffer);
  tu_memclr(serial_in_buffer, sizeof(serial_in_buffer));

  tuh_cdc_receive(dev_addr, serial_in_buffer, sizeof(serial_in_buffer), true); // waiting for next data
}

void cdc_task(void)
{

}

#endif
//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// Send Task
//--------------------------------------------------------------------+
void sendTask() {
  if( send_timer == 0 ) send_timer = board_millis();
  if(connectedAlert) {
    strcpy(buffer, "Scanner Connected");
    sendUrgent = true;
    connectedAlert = false;
  } else if(disconnectedAlert) {
    strcpy(buffer, "Scanner Disconnected");
    sendUrgent = true;
    disconnectedAlert = false;
    reset = true;
  }
  if(sendUrgent) {
    sendToServer();
    send_timer = 0;
    sendUrgent = false;
  }
  // If there's nothing in buffer, dont send
  if( _index <= 0) {
    send_timer = 0;
    return;
  }
  // If enough time passed, send the buffer and reset
  if( board_millis() > (send_timer + SEND_TIME + extraTime)) {
    // debug("Time passed!");
    // debug_msg("Buffer", buffer);
    // buffer should end with '|', if not, give it another second
    if (buffer[_index - 1] =! '|') {
      // debug("Extra time!");
      extraTime = 100;
      return;
    } else extraTime = 0;
    sendToServer();
    send_timer = 0;
  }
  // If we already filled the buffer, send it and reset
}

//--------------------------------------------------------------------+
// Send to Arduino
//--------------------------------------------------------------------+
void sendToServer() {
#ifdef DEBUG
  debug_msg("Sending", buffer);
#else
  printf("%s\n", buffer);
#endif
  cleanBuffer();
}

void fillBuffer(char c) {
  // If an \n arrived, copy temp buffer to buffer
  if(c == '\r') {
    buffer[_index] =  '|';
    _index++;
    return;
  }
  // Avoid overfilling the buffer
  if (_index > BUFF_SIZE - 1) {
    debug("Buffer is full!");
    return;
  }
  buffer[_index] = c;
  _index++;
}

void cleanBuffer() {
  // Set buffer to 0
  memset(buffer, '\0', BUFF_SIZE);
  // index starts at the end of IMEI = 16
  _index = 0;
  debug_msg("Debug", "Cleaning buffer");
}

#ifdef DEBUG
void debug_msg(const char *message, const char *value) {
  printf("%s: %s\n", message, value);
}
void debug(const char *message) {
  printf("%s\n", message);
}
void debug_ch(const char c) {
  putchar(c);
}
#else
void debug_msg(const char *message, const char *value) {
  return;
}
void debug(const char *message) {
  return;
}
void debug_ch(const char c) {
  return;
}
#endif
