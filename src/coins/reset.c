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

#include "main.h"

#include "reset.h"
#include "storage.h"
#include "sha2.h"
#include "messages.h"
#include "fsm.h"
#include "dialog.h"
#include "types.pb.h"

static bool     awaiting_entropy = false;
static bool     skip_backup = false;

void reset_init(bool pin_protection, bool _skip_backup)
{
	skip_backup = _skip_backup;

	char newpin[10];
	if (pin_protection && ! dialogEnterPin(newpin, true)) {
		fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
		return;
	}
	storage_setPin(newpin);
		
	EntropyRequest resp;
	memset(&resp, 0, sizeof(EntropyRequest));
	msg_write(MessageType_MessageType_EntropyRequest, &resp);
	awaiting_entropy = true;
}

void reset_entropy(const uint8_t *ext_entropy, uint32_t len)
{
	uint8_t  int_entropy[32];

	if (!awaiting_entropy) {
		fsm_sendFailure(FailureType_Failure_UnexpectedMessage, "Not in Reset mode");
		return;
	}

	get_random_bytes(int_entropy, 32);

	SHA256_CTX ctx;
	sha256_Init(&ctx);
	sha256_Update(&ctx, int_entropy, 32);
	sha256_Update(&ctx, ext_entropy, len);
	sha256_Final(&ctx, int_entropy);
	awaiting_entropy = false;

	storage_setEntropy(int_entropy);
	memset(int_entropy, 0, 32);

	//if (skip_backup) {
		fsm_sendSuccess("Device successfully initialized");
		dialogClear();
	//} else {
	//	reset_backup(false);
	//}

}

