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

#ifndef __DIALOG_H__
#define __DIALOG_H__

#include "types.pb.h"

void dialogClear(void);
bool dialogConfirm(const char *s, ...);
bool dialogEnterPin(char *pin, bool is_new);
void dialogProgress(const char *desc, int permil);
bool dialogConfirmOutput(const CoinType *coin, const TxOutputType *out);
bool dialogConfirmTx(const CoinType *coin, uint64_t amount_out, uint64_t amount_fee);
bool dialogFeeOverThreshold(const CoinType *coin, uint64_t fee);
bool dialogSignMessage(const uint8_t *msg, uint32_t len);
bool dialogCheckAddress(const char *desc, const char *address);
bool dialogVerifyMessage(const uint8_t *msg, uint32_t len);
void dialogShowPublicKey(const uint8_t *pubkey);

#endif
