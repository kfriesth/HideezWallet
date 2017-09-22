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

#include <string.h>
#include <stdint.h>

#include "main.h"

#include "messages.pb.h"

#include "sha2.h"
#include "bip32.h"
#include "hmac.h"
#include "curves.h"
#include "dialog.h"
#include "storage.h"

#define SDBG(s...) dprintf(s)

static bool sessionSeedCached;

static uint8_t sessionSeed[64];

static bool sessionPinCached;

static void seed_from_entropy(uint8_t *seed, const uint8_t *entropy)
{
	hmac_sha512((const uint8_t *)"entropy", 7, entropy, 32, seed);
	SDBG("Seed: [%64b]\n", seed);
}

void session_clear(bool clear_pin)
{
	sessionSeedCached = false;
	memset(&sessionSeed, 0, sizeof(sessionSeed));
	if (clear_pin) {
		sessionPinCached = false;
	}
}

void storage_setEntropy(uint8_t *entropy)
{
	SDBG("New entropy: [%32b]\n", entropy);
	nvs_write_record(NV_TABLE_COIN, NV_KEY_ENTROPY, entropy, 32);
}

const uint8_t *storage_getSeed(void)
{
	const u8 *entropy;
	int entropy_len;

	if (sessionSeedCached) return sessionSeed;

	entropy = nvs_read_record(NV_TABLE_COIN, NV_KEY_ENTROPY, &entropy_len);
	if (entropy && entropy_len == 32) {
		//TODO: decrypt with passphrase
		seed_from_entropy(sessionSeed, entropy);
		sessionSeedCached = true;
		return sessionSeed;
	} else {
		return NULL;
	}
}

bool storage_getRootNode(HDNode *node, const char *curve)
{
	const uint8_t *seed = storage_getSeed();
	if (! seed) return false;
	return hdnode_from_seed(seed, 64, curve, node);
}

/* Check whether pin matches storage.  The pin must be
 * a null-terminated string with at most 9 characters.
 */
bool storage_containsPin(const char *pin)
{
#ifdef USE_DIALOG
	/* The execution time of the following code only depends on the
	 * (public) input.  This avoids timing attacks.
	 */
	const char *cpin = nvs_read_string(NV_TABLE_COIN, NV_KEY_PIN);
	if (! cpin) return false;
	char diff = 0;
	uint32_t i = 0;
	while (pin[i]) {
		diff |= cpin[i] - pin[i];
		i++;
	}
	diff |= cpin[i];
	return (diff == 0);
#else
	return true;
#endif
}

bool storage_hasPin(void)
{
	return (nvs_read_string(NV_TABLE_COIN, NV_KEY_PIN) != NULL);
}

void storage_setPin(const char *pin)
{
	if (pin && pin[0]) {
		nvs_write_string(NV_TABLE_COIN, NV_KEY_PIN, pin);
	} else {
		nvs_delete_record(NV_TABLE_COIN, NV_KEY_PIN);
	}
	sessionPinCached = false;
}

void session_cachePin(void)
{
	sessionPinCached = true;
}

bool session_isPinCached(void)
{
	return sessionPinCached;
}

void storage_resetPinFails(void)
{
	nvs_delete_record(NV_TABLE_COIN, NV_KEY_PINFAILS);
}

void storage_increasePinFails(void)
{
	int v = nvs_read_value(NV_TABLE_COIN, NV_KEY_PINFAILS, 0);
	nvs_write_value(NV_TABLE_COIN, NV_KEY_PINFAILS, v+1);
}

uint32_t storage_getPinFails(void)
{
	return (uint32_t) nvs_read_value(NV_TABLE_COIN, NV_KEY_PINFAILS, 0);
}

bool storage_isInitialized(void)
{
	return (nvs_read_record(NV_TABLE_COIN, NV_KEY_ENTROPY, NULL) != NULL);
}

bool storage_needsBackup(void)
{
	return (nvs_read_value(NV_TABLE_COIN, NV_KEY_BACKUP, 0) == 0);
}

void storage_applyFlags(uint32_t flags)
{
	uint32_t cflags = (uint32_t) nvs_read_value(NV_TABLE_COIN, NV_KEY_FLAGS, 0);
	if ((cflags | flags) != cflags) {
		cflags |= flags;
		nvs_write_value(NV_TABLE_COIN, NV_KEY_FLAGS, cflags);
	}
}

uint32_t storage_getFlags(void)
{
	return (uint32_t) nvs_read_value(NV_TABLE_COIN, NV_KEY_FLAGS, 0);
}

const char *storage_getUUID(void)
{
	const char *uuid = nvs_read_string(NV_TABLE_COIN, NV_KEY_UUID);
	if (! uuid) {
		uint8_t bytes[12];
		char new_uuid[25];
		// generate new
		get_random_bytes(bytes, 12);
		data2hex(bytes, 12, new_uuid);
		SDBG("New UUID: [%s]\n", new_uuid);
		nvs_write_string(NV_TABLE_COIN, NV_KEY_UUID, new_uuid);
		uuid = nvs_read_string(NV_TABLE_COIN, NV_KEY_UUID);
	}
	return uuid;
}

void storage_wipe(void)
{
	SDBG("WIPE\n");
	nvs_delete_record(NV_TABLE_COIN, 0);
	session_clear(true);
}
