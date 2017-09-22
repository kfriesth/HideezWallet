#include "main.h"

#define BUZZER_TICKS_IN_PERIOD 1000000

//frequency - in Hz
//duty_cycle 1-99
static void set_timer(int frequency, u8 duty_cycle){
	//dprintf("buzz(%d)\n", frequency);
		nrf_gpiote_unconfig(PPI_CH_BUZZ);
		nrf_gpiote_unconfig(PPI_CH_BUZZ_2);
		sd_ppi_channel_enable_clr(1 << PPI_CH_BUZZ);
		sd_ppi_channel_enable_clr(1 << PPI_CH_BUZZ_2);

		nrf_gpio_pin_set(BUZZ_PIN_NUMBER);

		NRF_TIMER2->TASKS_STOP     = 1;	
		NRF_TIMER2->TASKS_CLEAR    = 1;	
		/*PAN73 workaround when stopping timer2 */
		*(uint32_t *)0x4000AC0C = 0; //for Timer 2

		NVIC_DisableIRQ(TIMER2_IRQn);
	
		if ((duty_cycle<1)||(duty_cycle>99)) return;
		if (! frequency) return;
		
		NRF_TIMER2->MODE = TIMER_MODE_MODE_Timer;
		NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;
		NRF_TIMER2->PRESCALER = 4; // 1 MHz
	
		// Configure PPI channel 0 to toggle PWM_OUTPUT_PIN on every TIMER2 COMPARE[0] match.
		sd_ppi_channel_assign(PPI_CH_BUZZ, &NRF_TIMER2->EVENTS_COMPARE[0], &NRF_GPIOTE->TASKS_OUT[GPIOTE_CH_BUZZ]);
		sd_ppi_channel_assign(PPI_CH_BUZZ_2, &NRF_TIMER2->EVENTS_COMPARE[1], &NRF_GPIOTE->TASKS_OUT[GPIOTE_CH_BUZZ]);
		sd_ppi_channel_enable_set(1 << PPI_CH_BUZZ);
		sd_ppi_channel_enable_set(1 << PPI_CH_BUZZ_2);

		// Configure GPIOTE channel 0 to toggle the PWM pin state
		// @note Only one GPIOTE task can be connected to an output pin.
		nrf_gpiote_task_config(GPIOTE_CH_BUZZ, 	 BUZZ_PIN_NUMBER, NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_HIGH);

		uint32_t duty=BUZZER_TICKS_IN_PERIOD*duty_cycle/100;
		NRF_TIMER2->CC[0] = 1;
		NRF_TIMER2->CC[1] = duty/frequency;
		NRF_TIMER2->CC[3] = BUZZER_TICKS_IN_PERIOD/frequency;
		
		/* Create an Event-Task shortcut to clear TIMER2 on COMPARE[2] event. */
		NRF_TIMER2->SHORTS = (TIMER_SHORTS_COMPARE3_CLEAR_Enabled << TIMER_SHORTS_COMPARE3_CLEAR_Pos);
		
		/* PAN73 workaround when starting timer2 */
		*(uint32_t *)0x4000AC0C = 1; //for Timer 2
		NRF_TIMER2->TASKS_START = 1;	
}


void buzzer_play_freq(int frequency, u8 duty_cycle)
{
	set_timer(frequency,duty_cycle);
}
