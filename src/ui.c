#include "main.h"

#define K(a, b) (((a) << 8) | (b))

enum {
	UI_STATE_IDLE,
	UI_STATE_INIT,
	UI_STATE_DISCONNECTED,
	UI_STATE_CONNECTED,
	UI_STATE_USERACTION,
	UI_STATE_PINCODE,
};

static u8 ui_state = UI_STATE_IDLE;
static u8 ui_substate = 0;

static bool button_ok = false;
static bool cancel_flag = false;

static u16 pinmask[2];
static u8 digit;

static u32 ui_timer_handler(u8 num)
{
	switch (K(ui_state, ui_substate)) {

		case K(UI_STATE_IDLE, 0):
			led_on(LED0_PIN_NUMBER);
			ui_substate = 1;
			return ble_is_connected() ? 50 : 500;

		case K(UI_STATE_IDLE, 1):
			led_off(LED0_PIN_NUMBER);
			ui_substate = 0;
			return ble_is_connected() ? 950 : 500;

		case K(UI_STATE_INIT, 0):
			led_on(LED0_PIN_NUMBER);
			buzzer_play_freq(3000, 5);
			ui_substate = 1;
			return MS2TICKS(100);

		case K(UI_STATE_INIT, 1):
			buzzer_play_freq(0, 0);
			ui_substate = 2;
			return MS2TICKS(100);

		case K(UI_STATE_INIT, 2):
			buzzer_play_freq(3000, 5);
			ui_substate = 3;
			return MS2TICKS(100);

		case K(UI_STATE_INIT, 3):
			buzzer_play_freq(0, 0);
			ui_state = UI_STATE_IDLE;
			return 10;

		case K(UI_STATE_CONNECTED, 0):
			led_on(LED0_PIN_NUMBER);
			buzzer_play_freq(2400, 5);
			ui_substate = 1;
			return MS2TICKS(50);

		case K(UI_STATE_CONNECTED, 1):
			buzzer_play_freq(2700, 5);
			ui_substate = 2;
			return MS2TICKS(50);

		case K(UI_STATE_CONNECTED, 2):
			led_off(LED0_PIN_NUMBER);
			buzzer_play_freq(0, 0);
			ui_state = UI_STATE_IDLE;
			ui_substate = 0;
			return MS2TICKS(10);

		case K(UI_STATE_DISCONNECTED, 0):
			led_on(LED0_PIN_NUMBER);
			buzzer_play_freq(2700, 5);
			ui_substate = 1;
			return MS2TICKS(50);

		case K(UI_STATE_DISCONNECTED, 1):
			buzzer_play_freq(2400, 5);
			ui_substate = 2;
			return MS2TICKS(50);

		case K(UI_STATE_DISCONNECTED, 2):
			led_off(LED0_PIN_NUMBER);
			buzzer_play_freq(0, 0);
			ui_state = UI_STATE_IDLE;
			ui_substate = 0;
			return MS2TICKS(10);

		case K(UI_STATE_USERACTION, 0):
			led_off(LED0_PIN_NUMBER);
			led_on(LED1_PIN_NUMBER);
			buzzer_play_freq(2400, 5);
			ui_substate = 1;
			return MS2TICKS(100);

		case K(UI_STATE_USERACTION, 1):
			led_off(LED0_PIN_NUMBER);
			led_off(LED1_PIN_NUMBER);
			buzzer_play_freq(0, 0);
			ui_substate = 0;
			return MS2TICKS(250);

		case K(UI_STATE_PINCODE, 0):
			// start entering green pincode
			led_off(LED1_PIN_NUMBER);
			led_on(LED0_PIN_NUMBER);
			ui_substate = 1;
			return MS2TICKS(PINCODE_TIMEOUT);

		case K(UI_STATE_PINCODE, 1):
			// timeout
			led_off(LED0_PIN_NUMBER);
			led_off(LED1_PIN_NUMBER);
			cancel_flag = true;
			return 0;

		case K(UI_STATE_PINCODE, 2):
			// bit entered
			ui_substate = 3;
			return MS2TICKS(PINCODE_DIGIT_TIMEOUT);

		case K(UI_STATE_PINCODE, 3):
			// digit entered
			if (digit == 0) {
				digit = 1;
				console_tx(' ');
				ui_substate = 1;
				led_off(LED0_PIN_NUMBER);
				led_on(LED1_PIN_NUMBER);
				return PINCODE_TIMEOUT;
			} else {
				led_off(LED0_PIN_NUMBER);
				led_off(LED1_PIN_NUMBER);
				button_ok = true;
				ui_state = UI_STATE_IDLE;
				return 0;
			}			

		default:
			return 0;
	}
}

static void ui_set_state(u8 state, u8 substate)
{
	ui_state = state;
	ui_substate = substate;
	rtc_set_timer(RTC_TIMER_UI, ui_timer_handler, 10);
}

void ui_button_event(u8 bstate)
{
	static u32 tdown;

	dprintf("Button:%d\n", bstate);
	if (ui_state == UI_STATE_USERACTION) {
		button_ok = true;
	} else if (ui_state == UI_STATE_PINCODE) {
		if (bstate == 1) {
			tdown = JIFFIES;
		} else {
			u32 c = (TIMEDIFF(JIFFIES,tdown) >= PINCODE_LONG) ? 1 : 0;
			console_tx(c ? '-' : '.');
			pinmask[digit] = (pinmask[digit] << 1) | c;
			ui_set_state(UI_STATE_PINCODE, 2);
		}
	}
}

void ui_init(void)
{
	dprintf("[Init]\n");
	ui_set_state(UI_STATE_INIT, 0);
}

void ui_connected(void)
{
	ui_set_state(UI_STATE_CONNECTED, 0);
}

void ui_disconnected(void)
{
	ui_set_state(UI_STATE_DISCONNECTED, 0);
}

bool ui_useraction(int code)
{

	bool ret = false;
	u32 t = CurrentTime;

	dprintf("[Confirm action] ");

	ui_set_state(UI_STATE_USERACTION, 0);
	cancel_flag = false;
	button_ok = false;

	while (CurrentTime - t < CONFIRM_TIMEOUT && ! cancel_flag) {
		ble_check_event();
		if (! ble_is_connected()) break;
		if (button_ok) {
			ret = true;
			break;
		}
	}
	led_off(LED1_PIN_NUMBER);
	buzzer_play_freq(0, 0);
	ui_set_state(UI_STATE_IDLE, 0);
	dprintf(ret ? "ok\n" : "canceled\n");
	return ret;

}

bool ui_pincode_enter(char *buffer)
{
	bool ret = false;
	u32 t = CurrentTime;
	int i;

	dprintf("[Enter pin code] ");

	ui_set_state(UI_STATE_PINCODE, 0);
	cancel_flag = false;
	button_ok = false;
	pinmask[0] = pinmask[1] = 1;
	digit = 0;

	while (! cancel_flag) {
		ble_check_event();
		if (! ble_is_connected()) break;
		if (button_ok) {
			ret = true;
			break;
		}
	}

	buffer[0] = 0;
	if (ret) {
		for (i=0; i<4; i++) {
			buffer[0+i] = '0' + (pinmask[0] % 10);
			pinmask[0] /= 10;
			buffer[5+i] = '0' + (pinmask[1] % 10);
			pinmask[1] /= 10;
		}
		buffer[10] = 0;
		dprintf(" \"%s\" ", buffer);
	}
	dprintf(ret ? "ok\n" : "canceled\n");
	ui_set_state(UI_STATE_IDLE, 0);
	return ret;
}

void ui_cancel(void)
{
	cancel_flag = true;
}
