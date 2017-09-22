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

#ifndef __STORAGE_H__
#define __STORAGE_H__

#include "types.pb.h"
#include "messages.pb.h"
#include "bip32.h"


void session_clear(bool clear_pin);
void session_cachePin(void);
bool session_isPinCached(void);

bool storage_isInitialized(void);
bool storage_needsBackup(void);
const char *storage_getUUID(void);

void storage_setEntropy(uint8_t *entropy);
const uint8_t *storage_getSeed(void);
bool storage_getRootNode(HDNode *node, const char *curve);

bool storage_hasPin(void);
bool storage_containsPin(const char *pin);
void storage_setPin(const char *pin);

void storage_resetPinFails(void);
void storage_increasePinFails(void);
uint32_t storage_getPinFails(void);

void storage_applyFlags(uint32_t flags);
uint32_t storage_getFlags(void);

void storage_wipe(void);

#endif
