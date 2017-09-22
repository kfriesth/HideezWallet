#include "main.h"

#if (CONSOLE!=0)

#define STATE_ENABLED   1
#define STATE_SENDING   2
#define STATE_ESC       4
#define STATE_DIRECT    8

#define MAXCMD 0x1f

static char uart_rx_buffer[CONSOLE_RX_BUFFER_SIZE];
static u16 rxpos;
static u8 *dumpaddr;

static char uart_tx_buffer[CONSOLE_TX_BUFFER_SIZE];
static u16 state, txhead, txtail, printmask;

static console_input_cb input_cb;

static void rxhandler(char ch);

void UART0_IRQHandler(void)
{
	// Handle reception
	if (NRF_UART0->EVENTS_RXDRDY)
	{
		u8 c = (u8)NRF_UART0->RXD;
		NRF_UART0->EVENTS_RXDRDY = 0;
		rxhandler(c);
	}

	// Handle transmission.
	if (NRF_UART0->EVENTS_TXDRDY)
	{
		// Clear UART TX event flag.
		NRF_UART0->EVENTS_TXDRDY = 0;
		if (txtail != txhead) {
			NRF_UART0->TXD = uart_tx_buffer[txtail];
			txtail = (txtail + 1) % CONSOLE_TX_BUFFER_SIZE;
		} else {
			NRF_UART0->TASKS_STOPTX = 1;
			state &= ~STATE_SENDING;
		}
	}
}

void console_tx(char c)
{
	if (state & STATE_DIRECT) {
			NRF_UART0->TASKS_STARTTX = 1;
			NRF_UART0->EVENTS_TXDRDY = 0;
			NRF_UART0->TXD = c;
			while (! NRF_UART0->EVENTS_TXDRDY) ;
			return;
	}
	uint16_t newhead = (txhead + 1) % CONSOLE_TX_BUFFER_SIZE;
	if (newhead == txtail) return;
	uart_tx_buffer[txhead] = c;
	txhead = newhead;
	if (! (state & STATE_SENDING)) {
		state |= STATE_SENDING;
		NRF_UART0->TXD = uart_tx_buffer[txtail];
		txtail = (txtail + 1) % CONSOLE_TX_BUFFER_SIZE;
		NRF_UART0->TASKS_STARTTX = 1;
	}
}

static void console_printvalue(int value, int len)
{
	int c, d, sign=1;
	if (value < 0) {
		sign = -1;
		value = -value;
		len--;
	}
	d = 1;
	while (--len > 0 || value / d > 9) d *= 10;
	while (d > 0) {
		if (d > value && sign && d > 1) {
			console_tx(' ');
		} else {
			if (sign < 0) console_tx('-');
			sign = 0;
			c = value / d;
			console_tx(c + 0x30);
			value -= (c * d);
		}
		d /= 10;
	}
}

static void console_printhex(u32 value, int len)
{
	int sh = len * 4;
	u8 c;
	while (sh) {
		sh -= 4;
		c = (value >> sh) & 0xf;
		console_tx((c <= 9) ? c+0x30 : c+0x37);
	}
}

void dprintf(char *fmt, ...)
{
	va_list ap;
	int i, len;
	char c, *s;
	u8 *p;

	if (! (state & (STATE_ENABLED | STATE_DIRECT))) return;

	if (fmt[0] <= 7 && fmt[0] > 0) {
		if (! (printmask & (1 << fmt[0]))) return;
		fmt++;
	}
	va_start(ap, fmt);
	while ((c = *fmt++) != 0) {
		if (c == '%') {
			len = 0;
			while (((c = *fmt++) & 0xf0) == 0x30) {
				len = (len * 10) + (c - 0x30);
			}
			if (c == '*') {
				len = va_arg(ap, int);
				c = *fmt++;
			}
			switch (c) {
				case '%':
					console_tx('%');
					break;
				case 'c':
					console_tx(va_arg(ap, int));
					break;
				case 'd':
					console_printvalue(va_arg(ap, int), len);
					break;
				case 'x':
					console_printhex(va_arg(ap, u32), len ? len : 8);
					break;
				case 'm':
					p = va_arg(ap, u8 *);
					for (i=5; i>=0; i--) {
						console_printhex(p[i], 2);
						if (i > 0) console_tx(':');
					}
					break;
				case 'b':
					p = va_arg(ap, u8 *);
					for (i=0; i<len; i++) {
						console_printhex(p[i], 2);
					}
					break;
				case 's':
					if (! len) len = 65536;
					s = va_arg(ap, char *);
					if (! s) s = "(NULL)";
					while ((c = *s++) != 0 && len-- > 0) console_tx(c);
					break;
			}
		} else {
			if (c == '\n') console_tx('\r');
			console_tx(c);
		}
	}
	va_end(ap);
}

static int parse_value(const char **argptr, int ishex)
{
	int sign = 1;
	int v = 0;
	char *p= (char *) *argptr, c;

	while (*p == ' ') p++;
	if (*p == '-') {
		sign = -1;
		p++;
	}
	if (*p == '0' && *(p+1) == 'x') {
		p += 2;
		ishex = 1;
	}
	while (1) {
		c = *p;
		if (c >= 'a') c -= 0x20;
		if (ishex) {
			if (c < '0' || (c > '9' && c < 'A') || c > 'F') break;
			v = (v << 4) | ((c <= '9') ? c-0x30 : c-0x37);
		} else {
			if (c < '0' || c > '9') break;
			v = (v * 10) + (c-0x30);
		}
		p++;
	}
	*argptr = p;
	return v * sign;
}

static int parse_hexstring(const char **argptr, u8 *buf, int maxlen)
{
	char *p= (char *) *argptr, c;
	u8 v;
	int len = 0;

	while (*p == ' ') p++;
	while (len < maxlen) {
		c = *p++;
		if (c >= 'a') c -= 0x20;
		if (c < '0' || (c > '9' && c < 'A') || c > 'F') break;
		v = ((c <= '9') ? c-0x30 : c-0x37) << 4;
		c = *p++;
		if (c >= 'a') c -= 0x20;
		if (c < '0' || (c > '9' && c < 'A') || c > 'F') break;
		v |= ((c <= '9') ? c-0x30 : c-0x37);
		*buf++ = v;
		len++;
	}
	*argptr = p;
	return len;
}

#define parse_decimal(argp) parse_value(argp, 0)
#define parse_hex(argp) parse_value(argp, 1)

enum { 
	CMD_MD=1, CMD_MR, CMD_MW, CMD_STACK,
	CMD_DISC, CMD_WIPE, CMD_B58ENC
};

static const char cmdlist[] = {

	'm', 'd', CMD_MD,
	'm', 'r', CMD_MR,
	'm', 'w', CMD_MW,
	's', 't', 'a', 'c', 'k', CMD_STACK,
	'd', 'i', 's', 'c', CMD_DISC,
	'w','i','p','e', CMD_WIPE,
	'b','5','8','e','n','c', CMD_B58ENC,
	0

};

void exec_cmd(int cmd, const char *args)
{
	u32 addr, value;
	int i;

	switch (cmd) {
		case CMD_MD:
			console_tx('\r');
			if (*args != 0) {
				dumpaddr = (u8 *) parse_hex(&args);
				console_tx('\n');
			}
			dprintf("%x: ", dumpaddr);
			for (i=0; i<16; i++) {
				dprintf("%2x ", dumpaddr[i]);
			}
			for (i=0; i<16; i++) {
				u8 c = dumpaddr[i];
				console_tx((c >= ' ' && c <= 127) ? c : '.');
			}
			dprintf("\n");
			dumpaddr += 16;
			break;
		case CMD_MR:
			addr = parse_hex(&args);
			dprintf("%x: %x\n", addr, *((u32 *)addr));
			break;
		case CMD_MW:
			addr = parse_hex(&args);
			value = parse_hex(&args);
			*((u32 *)addr) = value;
			break;

		case CMD_STACK:
			for (i=0; i<STACK_SIZE/4; i++) {
				if (__stack_array[i] != 0xface1234) break;
			}
			dprintf("Free: %d bytes\n", i * 4);
			break;

		case CMD_DISC:
		{
			sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			break;
		}

		case CMD_WIPE:
		{
			nvs_delete_record(NV_TABLE_COIN, 0);
			break;
		}

		case CMD_B58ENC:
		{
			u8 hbuf[100];
			char tbuf[140];
			int len = parse_hexstring(&args, hbuf, sizeof(hbuf));
			int tbufsz=140; 
			bool res = b58enc(tbuf, &tbufsz, hbuf, len);
			break;
		}
	}
	dprintf("%% ");
}

#define isalnum(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || ((c) >= '0' && (c) <= '9'))
static void rxhandler(char ch)
{
	const char *p1, *p2, *p0;
	int cmd, tabhint = 0;

	if (input_cb) {
		if (! (input_cb)(ch)) {
			input_cb = NULL;
		}
		return;
	}

	if (ch == 0x03) {
		uart_rx_buffer[0] = 0;
		rxpos = 0;
		ch = 0x0d;
	}
	if (ch == 0x1b) {
		state ^= STATE_ESC;
		return;
	}
	if (state & STATE_ESC) {
		if (ch < 'A' || ch > 'z' || ch == '[') return;
		state &= ~STATE_ESC;
		if (ch == 'A' && rxpos == 0) {
			dprintf("%s", uart_rx_buffer);
			while (uart_rx_buffer[rxpos]) rxpos++;
		}
		return;
	}
	if (ch >= '!' && ch <= '&' && rxpos == 0) {
		// switch debug level
		ch -= ' ';
		printmask ^= (1 << ch);
		dprintf("debug.%d %s\n", ch, (printmask & (1 << ch)) ? "on" : "off");
		return;
	}
	if (ch == '/' && rxpos == 0) {
		// repeat last command
		ch = 0x0e;
	}
	if (ch == 0x0d || ch == 0x0e || ch == 0x09) {
		for (p2 = cmdlist; *p2; p2++) {
			p1 = uart_rx_buffer;
			p0 = p2;
			while (*p2 > MAXCMD && *p1 == *p2) { p1++; p2++; }
			cmd = *p2;
			while (*p2 > MAXCMD) p2++;
			if (ch == 0x09) {
				if (p1 - uart_rx_buffer >= rxpos) {
					if (tabhint++ == 0) dprintf("\n");
					while (*p0 > ' ') console_tx(*p0++);
					console_tx(' ');
				}
				continue;
			}
			if (cmd <= MAXCMD && ! isalnum(*p1)) {
				if (cmd != CMD_MD) {
					dprintf("\n");
					if (ch == 0x0d && rxpos == 0) break;
				}
				const char *args = (rxpos == 0) ? "" : p1;
				while (*args == ' ') args++;
				exec_cmd(cmd, args);
				break;
			}
		}
		if (ch == 0x0d || ch == 0x0e) {
			if (! *p2) dprintf(rxpos ? "\nUnknown command\n%% " : "\n%% ");
			rxpos = 0;
		} else {
			dprintf(tabhint ? "\n%% " : "\r%% ");
			if (ch == 0x09 && rxpos > 0) {
				dprintf("%s", uart_rx_buffer);
			}
		}
	} else if (ch == 0x08 && rxpos > 0) {
		uart_rx_buffer[--rxpos] = 0;
		dprintf("\b \b");
	} else if (ch >= ' ' && ch <= 127 && rxpos < CONSOLE_RX_BUFFER_SIZE-1) {
		uart_rx_buffer[rxpos++] = ch;
		uart_rx_buffer[rxpos] = 0;
		console_tx(ch);
	}
}

void console_read(console_input_cb cb)
{
	input_cb = cb;
}

void console_init(void)
{
	// Configure RX and TX pins.
	nrf_gpio_cfg_output(UART_TX_PIN_NUMBER);
	nrf_gpio_cfg_input(UART_RX_PIN_NUMBER, NRF_GPIO_PIN_PULLUP);

	NRF_UART0->PSELTXD = UART_TX_PIN_NUMBER;
	NRF_UART0->PSELRXD = UART_RX_PIN_NUMBER;
	NRF_UART0->BAUDRATE = 0x01D7E000; // 115200
	NRF_UART0->CONFIG = 0; // no flow, no parity


	NRF_UART0->ENABLE = 0x04; // enable
	NRF_UART0->EVENTS_RXDRDY = 0;
	NRF_UART0->EVENTS_TXDRDY = 0;
	NRF_UART0->TASKS_STARTRX = 1;
	
	NRF_UART0->INTENCLR = 0xffffffffUL;
	NRF_UART0->INTENSET = (1 << 2) | (1 << 7); // txrdy, rxrdy

	NVIC_ClearPendingIRQ(UART0_IRQn);
	NVIC_SetPriority(UART0_IRQn, 3);
	NVIC_EnableIRQ(UART0_IRQn);

	state = STATE_ENABLED;
	dprintf("\n%% ");
}

void HardFaultHandlerProc(u32 *args)
{
	static int fault_state = 0;
	if (! fault_state) {
		fault_state = 1;
		state = STATE_DIRECT;
		dprintf("*FAULT* PC=%x LR=%x PSR=%x [%x %x %x %x]\n",
			args[6], args[5], args[7],
			args[0], args[1], args[2], args[3]
		);
	}
	sd_nvic_SystemReset();
	while (1) ;
}

#else

void HardFaultHandlerProc(u32 *args)
{
}

#endif
