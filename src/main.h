#ifndef _PLATFORM_H_
#define _PLATFORM_H_

//EXTERNAL_INCLUDES

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "types.h"

#pragma anon_unions

#include "nrf_soc.h"
#include "nrf_sdm.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_nvmc.h"
#include "ble.h"
#include "ble_hci.h"

#include "config.h"
#include "bt.h"
#include "console.h"
#include "ui.h"
#include "nvram.h"
#include "sha.h"
#include "sound.h"
#include "random.h"
#include "util.h"

#define DBG(...) dprintf(__VA_ARGS__)

#define JIFFIES (NRF_RTC1->COUNTER)
#define TIMER_MAX_VALUE     0xffffff
#define TIMEDIFF(t1, t2)    (((t1)-(t2)) & TIMER_MAX_VALUE)
#define FOREVER             0xffffffff
#define MS2TICKS(n)         (((n) * 4096) / 1000)

#define CHECK_STACK() if (__stack_array[0]!=0xface1234) app_exception("STACK RED")

#define led_on(pin) nrf_gpio_pin_set(pin);
#define led_off(pin) nrf_gpio_pin_clear(pin);
#define led_toggle(pin) nrf_gpio_pin_toggle(pin);

#define is_button_down() (((NRF_GPIO->IN >> BUTTON_PIN_NUMBER) & 1) == BUTTON_ACTIVE_LEVEL)

typedef u32 (*rtc_timer_proc)(u8 num);

void rtc_init(void);
void rtc_set_timer(u8 num, rtc_timer_proc proc, u32 delay);

extern volatile uint32_t CurrentTime;
extern volatile uint8_t  HalfSecond;

extern uint8_t  gs_evt_buf[];

extern uint8_t shared_buffer[SHARED_BUFFER_SIZE];
extern uint32_t __stack_array[STACK_SIZE/4-1];

void     nrf_delay_us(u32 volatile number_of_us);
void     umemcpy(void *dest, const void *src, u32 size);
uint32_t getSP(void);
void     app_exception(const char *message);

void poweroff(void);
void update_poweroff_timeout(void);

void check_stack(void);

#endif
