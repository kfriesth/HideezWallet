#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "nrf_soc.h"
#include "nrf_sdm.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"

/*
 *  Asynchronous debugging console implementation
 *
 *  (c) Dmitry Zakharov
 *
 *
 *  There are three variants of console configuration selected by CONSOLE define:
 *    0: no console at all
 *    1: output only
 *    2: input and output
 */


typedef bool (*console_input_cb)(char ch);

#if (CONSOLE!=0)

void console_init(void);
void console_tx(char c);
void dprintf(char *fmt, ...);
void console_read(console_input_cb cb);

#else

#define console_init()
#define console_tx(c)
#define dprintf(fmt...)
#define console_tx(c)
#define console_read(cb)

#endif

#define dprintf1(s...) dprintf("\1" s)
#define dprintf2(s...) dprintf("\2" s)
#define dprintf3(s...) dprintf("\3" s)
#define dprintf4(s...) dprintf("\4" s)
#define dprintf5(s...) dprintf("\5" s)
#define dprintf6(s...) dprintf("\6" s)
#define dprintf7(s...) dprintf("\7" s)

#endif
