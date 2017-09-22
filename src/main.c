#include "main.h"
#include "types.pb.h"
#include "messages.pb.h"
#include "dialog.h"

void* __initial_sp __attribute__((at(RAM_START+RAM_SIZE-4)));
uint32_t __stack_array[STACK_SIZE/4-1]  __attribute__((at(RAM_START+RAM_SIZE-STACK_SIZE)));
uint8_t shared_buffer[SHARED_BUFFER_SIZE];

volatile uint32_t CurrentTime;
volatile uint8_t  HalfSecond;
volatile uint32_t PoweroffTime;

u8 button_event = 0;
rtc_timer_proc rtc_timer_handler[3];

void poweroff(void)
{
	dprintf("poweroff\n");
	nrf_delay_us(10000);
	sd_power_system_off();
}

void update_poweroff_timeout(void)
{
	PoweroffTime = CurrentTime + POWEROFF_TIMEOUT;
}

void SD_EVT_IRQHandler(void)
{
	uint32_t evt_id;

	while (sd_evt_get(&evt_id) != NRF_ERROR_NOT_FOUND) {
	}
}

void softdevice_assert_callback(uint32_t pc, uint16_t line_num, const uint8_t *file_name)
{
	dprintf("*** sd assert: %x %s line %d\n", pc, file_name, line_num);
	while (1);
}

void app_exception(const char *message)
{
	dprintf("*** %s\n", message);
	while (1);
}

void RTC1_IRQHandler(void)
{
	int i;
	u32 ret;

	if (NRF_RTC1->EVENTS_COMPARE[3] == 1) {   				
		NRF_RTC1->EVENTS_COMPARE[3] = 0;
		NRF_RTC1->CC[3] = (NRF_RTC1->CC[3] + 2048) & 0xffffff;
		if (HalfSecond ^= 1) CurrentTime++;
	}
	for (i=0; i<3; i++) {
		if (NRF_RTC1->EVENTS_COMPARE[i] == 1) {
			NRF_RTC1->EVENTS_COMPARE[i] = 0;
			NRF_RTC1->INTENCLR = (1 << (RTC_EVTEN_COMPARE0_Pos + i));
			ret = (rtc_timer_handler[i])(i);
			if (ret) {
				NRF_RTC1->CC[i] = (NRF_RTC1->COUNTER + ret) & 0xffffff;
				NRF_RTC1->INTENSET = (1 << (RTC_EVTEN_COMPARE0_Pos + i));
			}
		}
	}
}

void rtc_init(void)
{
	NVIC_DisableIRQ(RTC1_IRQn);

	NRF_RTC1->TASKS_STOP                = 1;
	NRF_RTC1->TASKS_CLEAR               = 1;
	NRF_RTC1->EVENTS_COMPARE[0] = 0;
	NRF_RTC1->EVENTS_COMPARE[1] = 0;
	NRF_RTC1->EVENTS_COMPARE[2] = 0;
	NRF_RTC1->EVENTS_COMPARE[3] = 0;
	NRF_RTC1->CC[3] = 4096;
	NRF_RTC1->PRESCALER = (8-1); //   (1/4096Hz = 244,14 us)
	NRF_RTC1->INTENCLR = 0xffffffff;
	NRF_RTC1->EVTENCLR = 0xffffffff;
	NRF_RTC1->INTENSET = (1 << (RTC_EVTEN_COMPARE3_Pos));

	NVIC_SetPriority(RTC1_IRQn, 3);
	NVIC_ClearPendingIRQ(RTC1_IRQn);
	NVIC_EnableIRQ(RTC1_IRQn);       // Enable Interrupt for the RTC in the core.
	NRF_RTC1->TASKS_START = 1;
}

void rtc_set_timer(u8 num, rtc_timer_proc proc, u32 delay)
{
	rtc_timer_handler[num] = proc;
	NRF_RTC1->EVENTS_COMPARE[num] = 0;
	NRF_RTC1->CC[num] = (NRF_RTC1->COUNTER + delay) & 0xffffff;
	NRF_RTC1->INTENSET = (1 << (RTC_EVTEN_COMPARE0_Pos + num));
}

void GPIOTE_IRQHandler(void)
{
	// This handler will be run after wakeup from system ON (GPIO wakeup)
	if(NRF_GPIOTE->EVENTS_PORT)
	{
		NRF_GPIOTE->EVENTS_PORT = 0;
		if (NRF_GPIO->IN & (1 << BUTTON_PIN_NUMBER)) {
			nrf_gpio_cfg_sense_input(BUTTON_PIN_NUMBER, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
			ui_button_event(0);
		} else {
			nrf_gpio_cfg_sense_input(BUTTON_PIN_NUMBER, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_HIGH);
			ui_button_event(1);
		}
	}
}

int main(void)
{
	ble_gap_addr_t addr;
	int i;

	for (i=0; i<STACK_SIZE/4-8; i++) __stack_array[i] = 0xface1234;

	nrf_gpio_cfg_input(UART_TX_PIN_NUMBER, NRF_GPIO_PIN_PULLUP);
	nrf_gpio_cfg_input(UART_RX_PIN_NUMBER, NRF_GPIO_PIN_PULLUP);
	nrf_gpio_cfg_sense_input(BUTTON_PIN_NUMBER, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
	nrf_gpio_cfg_output(BUZZ_PIN_NUMBER);
	nrf_gpio_pin_set(BUZZ_PIN_NUMBER);
	nrf_gpio_cfg_output(LED0_PIN_NUMBER);
	nrf_gpio_cfg_output(LED1_PIN_NUMBER);
	nrf_gpio_pin_clear(LED0_PIN_NUMBER);
	nrf_gpio_pin_clear(LED1_PIN_NUMBER);
	
	NRF_CLOCK->TASKS_LFCLKSTOP            = 1;
	NRF_CLOCK->EVENTS_LFCLKSTARTED        = 0;
	NRF_CLOCK->LFCLKSRC                   = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
	NRF_CLOCK->TASKS_LFCLKSTART           = 1;
	//Wait for the low frequency clock to start
	uint32_t toc=100500; //To not to hang.
	while ((NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) && toc--);
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

	#if (CONSOLE!=0)	
		console_init();
	#else
		if(NRF_UART0->ENABLE){ //disable UART if it was enabled in bootloader.
			NRF_UART0->ENABLE=0; 
		}
	#endif 

	CurrentTime = 0;
	update_poweroff_timeout();

	rtc_init();
	nvram_init();

	#define PRINTSIZE(type) dprintf(#type ": %d\n", sizeof(type));
	PRINTSIZE(TxInputType);
	PRINTSIZE(TxOutputType);
	PRINTSIZE(TxOutputBinType);
	PRINTSIZE(TxRequestSerializedType);
	PRINTSIZE(MultisigRedeemScriptType);
	PRINTSIZE(TransactionType);
	PRINTSIZE(GetAddress);
	PRINTSIZE(Address);
	PRINTSIZE(GetPublicKey);
	PRINTSIZE(PublicKey);
	PRINTSIZE(SignTx);
	PRINTSIZE(TxRequest);
	PRINTSIZE(TxAck);

	//dialogConfirm("Test\n", NULL);

	sd_softdevice_enable((uint32_t)NRF_CLOCK_LFCLKSRC_XTAL_75_PPM, softdevice_assert_callback);
	sd_nvic_EnableIRQ(SD_EVT_IRQn);

	ble_init();

	// Set the GPIOTE PORT event as interrupt source, and enable interrupts for GPIOTE
	NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Msk;
	sd_nvic_SetPriority(GPIOTE_IRQn,3);
	sd_nvic_EnableIRQ(GPIOTE_IRQn);

	if(sd_ble_gap_address_get(&addr)==NRF_SUCCESS){
		get_random_bytes(addr.addr, 3);
		addr.addr_type=BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
		sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);	
	}

	ui_init();

	while (1) {
		ble_wait_event();
		if (CurrentTime >= PoweroffTime) poweroff();
	}
}

