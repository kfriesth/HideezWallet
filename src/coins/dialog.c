/*
 * This file is part of the TREZOR project.
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "main.h"

#include "dialog.h"
#include "storage.h"
#include "string.h"

static void format_uint64(uint64_t v, char *buffer)
{
	char value[20];
	char *p = &value[19];
	*p = 0;

	do {
		*(--p) = (char)(v % 10UL) + '0';
		v /= 10UL;
	} while (v);

	strcpy(buffer, p);
}

void dialogClear(void)
{
	dprintf("\n");
}

static volatile bool dialog_inprogress = false;
static bool dialog_result;
static char *pinptr;
static int pinlen;

static bool confirm_cb(char c)
{
	if (c == 'Y' || c == 'y') { dialog_result = true; dialog_inprogress = false; return false; }
	if (c == 'N' || c == 'n') { dialog_result = false; dialog_inprogress = false; return false; }
	return true;
}

bool dialogConfirm(const char *s, ...)
{
#ifdef USE_DIALOG

	va_list ap;
	const char *nexts = s;

	va_start(ap, s);
	while (nexts) {
		dprintf("%s", nexts);
		nexts = va_arg(ap, char *);
	}
	va_end(ap);

	dprintf("\n[Confirm: Y/N] ");
	dialog_inprogress = true;
	console_read(confirm_cb);
	while (dialog_inprogress) ;
	dprintf("\n");
	return dialog_result;
#else
	return ui_useraction(1);
#endif
}

static bool enterpin_cb(char c)
{
	if (c == 3 || c == 0x1b) { dialog_result = false; dialog_inprogress = false; return false; }
	if ((c == '\r' || c == '\n') && pinlen > 0) { dialog_result = true; dialog_inprogress = false; return false; }
	if ((c >= '0' && c <= '9') && pinlen < 9) { pinptr[pinlen++] = c; dprintf("%c", c); }
	if (c == 8 && pinptr > 0) { pinptr[--pinlen] = 0; dprintf("\b"); }
	return true;
}

bool dialogEnterPin(char *pin, bool is_new)
{
#ifdef USE_DIALOG
	dprintf("Enter %s pin: ", is_new ? "new" : "current");
	dialog_inprogress = true;
	memset(pin, 0, 10);
	pinptr = pin;
	pinlen = 0;
	console_read(enterpin_cb);
	while (dialog_inprogress) ;
	dprintf("\n");
	return dialog_result;
#else
	strcpy(pin, "1234");
	return true;
#endif
}

void dialogProgress(const char *desc, int permil)
{
	dprintf("%s [%d%%]...     \r", desc, permil/10);
}

bool dialogConfirmOutput(const CoinType *coin, const TxOutputType *out)
{
	char str_out[20];
	const char *units = coin->has_coin_shortcut ? coin->coin_shortcut : "";
	format_uint64(out->amount, str_out);
	return dialogConfirm("Confirm sending ", str_out, " ", units, " to ", out->address, NULL);
}

bool dialogConfirmTx(const CoinType *coin, uint64_t amount_out, uint64_t amount_fee)
{
	char str_out[20], str_fee[20];
	const char *units = coin->has_coin_shortcut ? coin->coin_shortcut : "";
	format_uint64(amount_out, str_out);
	format_uint64(amount_fee, str_fee);
	return dialogConfirm("Really send ", str_out, " ", units, " from your wallet? Fee included: ", str_fee, NULL);
}

bool dialogFeeOverThreshold(const CoinType *coin, uint64_t fee)
{
	char str_fee[20];
	const char *units = coin->has_coin_shortcut ? coin->coin_shortcut : "";
	format_uint64(fee, str_fee);
	return dialogConfirm("Fee ", str_fee, " ", units, " is unexpectedly high. Send anyway?", NULL);
}

bool dialogSignMessage(const uint8_t *msg, uint32_t len)
{
	return dialogConfirm("[", msg, "]\nSign message?", NULL);
}

bool dialogCheckAddress(const char *desc, const char *address)
{
	return dialogConfirm(desc, address, NULL);
}

bool dialogVerifyMessage(const uint8_t *msg, uint32_t len)
{
	return dialogConfirm("Verified message: ", msg, "\n", NULL);
}

void dialogShowPublicKey(const uint8_t *key)
{
	dprintf("Public key: [%33b]\n", key);
}

