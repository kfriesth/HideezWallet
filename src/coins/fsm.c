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
 * GNU Lesser General Public Licedialognse for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "main.h"
#include "console.h"

#include "fsm.h"
#include "messages.h"
#include "bip32.h"
#include "storage.h"
#include "coins.h"
#include "transaction.h"
#include "storage.h"
#include "dialog.h"
#include "address.h"
#include "ecdsa.h"
#include "reset.h"
#include "signing.h"
#include "hmac.h"
#include "crypto.h"
#include "ripemd160.h"
#include "curves.h"
#include "secp256k1.h"

#define FSMDBG(s...) dprintf(s)

// message methods

#define CHECK_INITIALIZED \
	if (!storage_isInitialized()) { \
		fsm_sendFailure(FailureType_Failure_NotInitialized, NULL); \
		return; \
	}

#define CHECK_NOT_INITIALIZED \
	if (storage_isInitialized()) { \
		fsm_sendFailure(FailureType_Failure_UnexpectedMessage, "Device is already initialized. Use Wipe first."); \
		return; \
	}

#define CHECK_PARAM(cond, errormsg) \
	if (!(cond)) { \
		fsm_sendFailure(FailureType_Failure_DataError, (errormsg)); \
		dialogClear(); \
		return; \
	}

#define REQ(INTYPE) resp_init(#INTYPE, sizeof(INTYPE), 0);
#define RESP(INTYPE,OUTTYPE) OUTTYPE *resp = (OUTTYPE *) resp_init(#INTYPE, sizeof(INTYPE), sizeof(OUTTYPE));

void *resp_init(char *type, int insize, int outsize)
{
	FSMDBG("> %s (%d,%d)\n", type, insize, outsize);
	if (insize+outsize > SHARED_BUFFER_SIZE) app_exception("shared buffer overflow\n");
	memset(shared_buffer + (SHARED_BUFFER_SIZE - outsize) , 0, outsize);
	return (void *)(shared_buffer + (SHARED_BUFFER_SIZE - outsize));
}

static bool check_pin(bool use_cached)
{
	char pin[10];

	if (use_cached && session_isPinCached()) return true;

	if (! dialogEnterPin(pin, false)) {
		fsm_sendFailure(FailureType_Failure_PinCancelled, NULL);
		dialogClear();
		return false;
	}
	if (storage_containsPin(pin)) {
		session_cachePin();
		storage_resetPinFails();
		return true;
	} else {
		fsm_sendFailure(FailureType_Failure_PinInvalid, NULL);
		return false;
	}
}

void fsm_sendSuccess(const char *text)
{
	Success *resp = (Success *) shared_buffer;
	memset(resp, 0, sizeof(Success));

	if (text) {
		resp->has_message = true;
		strlcpy(resp->message, text, sizeof(resp->message));
	}
	FSMDBG("Success: %s\n", text);
	msg_write(MessageType_MessageType_Success, resp);
}

void fsm_sendFailure(FailureType code, const char *text)
{
	Failure *resp = (Failure *) shared_buffer;
	memset(resp, 0, sizeof(Failure));

	resp->has_code = true;
	resp->code = code;
	if (!text) {
		switch (code) {
			case FailureType_Failure_UnexpectedMessage:
				text = "Unexpected message";
				break;
			case FailureType_Failure_ButtonExpected:
				text = "Button expected";
				break;
			case FailureType_Failure_DataError:
				text = "Data error";
				break;
			case FailureType_Failure_ActionCancelled:
				text = "Action cancelled by user";
				break;
			case FailureType_Failure_PinExpected:
				text = "PIN expected";
				break;
			case FailureType_Failure_PinCancelled:
				text = "PIN cancelled";
				break;
			case FailureType_Failure_PinInvalid:
				text = "PIN invalid";
				break;
			case FailureType_Failure_InvalidSignature:
				text = "Invalid signature";
				break;
			case FailureType_Failure_ProcessError:
				text = "Process error";
				break;
			case FailureType_Failure_NotEnoughFunds:
				text = "Not enough funds";
				break;
			case FailureType_Failure_NotInitialized:
				text = "Device not initialized";
				break;
			case FailureType_Failure_FirmwareError:
				text = "Firmware error";
				break;
		}
	}
	if (text) {
		resp->has_message = true;
		strlcpy(resp->message, text, sizeof(resp->message));
	}
	FSMDBG("Failure: %s\n", text);
	msg_write(MessageType_MessageType_Failure, resp);
}

static const CoinType *fsm_getCoin(bool has_name, const char *name)
{
	const CoinType *coin;
	if (has_name) {
		coin = coinByName(name);
	} else {
		coin = coinByName("Bitcoin");
	}
	if (!coin) {
		fsm_sendFailure(FailureType_Failure_DataError, "Invalid coin name");
		dialogClear();
		return 0;
	}
	return coin;
}

static HDNode *fsm_getDerivedNode(const char *curve, uint32_t *address_n, size_t address_n_count)
{
	static HDNode node;
	if (!storage_getRootNode(&node, curve)) {
		fsm_sendFailure(FailureType_Failure_NotInitialized, "Device not initialized or passphrase request cancelled or unsupported curve");
		dialogClear();
		return 0;
	}
	if (!address_n || address_n_count == 0) {
		return &node;
	}
	if (hdnode_private_ckd_cached(&node, address_n, address_n_count, NULL) == 0) {
		fsm_sendFailure(FailureType_Failure_ProcessError, "Failed to derive private key");
		dialogClear();
		return 0;
	}
	return &node;
}

void fsm_msgInitialize(Initialize *msg)
{
	(void)msg;
	FSMDBG("*** Initialize ***\n");
	//recovery_abort();
	signing_abort();
	session_clear(false); // do not clear PIN
	dialogClear();
	fsm_msgGetFeatures(0);
}

void fsm_msgGetFeatures(GetFeatures *msg)
{
	(void)msg;
	RESP(GetFeatures, Features);
	resp->has_vendor = true;         strlcpy(resp->vendor, "bitcointrezor.com", sizeof(resp->vendor));
	resp->has_major_version = true;  resp->major_version = VERSION_MAJOR;
	resp->has_minor_version = true;  resp->minor_version = VERSION_MINOR;
	resp->has_patch_version = true;  resp->patch_version = VERSION_PATCH;
	resp->has_device_id = true;      strlcpy(resp->device_id, storage_getUUID(), sizeof(resp->device_id));
	resp->has_pin_protection = true; resp->pin_protection = storage_hasPin();
	resp->has_passphrase_protection = true; resp->passphrase_protection = false;
#ifdef SCM_REVISION
	int len = sizeof(SCM_REVISION) - 1;
	resp->has_revision = true; memcpy(resp->revision.bytes, SCM_REVISION, len); resp->revision.size = len;
#endif
#if 0
	resp->has_bootloader_hash = true; resp->bootloader_hash.size = memory_bootloader_hash(resp->bootloader_hash.bytes);
#endif
	resp->coins_count = COINS_COUNT;
	memcpy(resp->coins, coins, COINS_COUNT * sizeof(CoinType));
	resp->has_initialized = true; resp->initialized = storage_isInitialized();
	resp->has_imported = true; resp->imported = false;
	resp->has_pin_cached = true; resp->pin_cached = session_isPinCached();
	resp->has_passphrase_cached = true; resp->passphrase_cached = false;
	resp->has_needs_backup = true; resp->needs_backup = storage_needsBackup();
	resp->has_flags = true; resp->flags = storage_getFlags();
	msg_write(MessageType_MessageType_Features, resp);
}

void fsm_msgPing(Ping *msg)
{
	RESP(Ping, Success);

	if (msg->has_button_protection && msg->button_protection) {
		if (! dialogConfirm("Do you really want to answer to ping?", NULL)) {
			fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
			dialogClear();
			return;
		}
	}

	if (msg->has_pin_protection && msg->pin_protection) {
		if (! check_pin(true)) return;
	}

	/*
	if (msg->has_passphrase_protection && msg->passphrase_protection) {
		if (!protectPassphrase()) {
			fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
			return;
		}
	}
	*/

	if (msg->has_message) {
		resp->has_message = true;
		memcpy(&(resp->message), &(msg->message), sizeof(resp->message));
	}
	msg_write(MessageType_MessageType_Success, resp);
	dialogClear();
}

void fsm_msgChangePin(ChangePin *msg)
{
	REQ(ChangePin);
	bool removal = msg->has_remove && msg->remove;
	if (removal) {
		if (storage_hasPin()) {
			if (! dialogConfirm("Do you really want to remove current PIN?", NULL)) {
				fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
				dialogClear();
				return;
			}
		} else {
			fsm_sendSuccess("PIN removed");
			return;
		}
	} else {
		char *s = storage_hasPin() ? "Do you really want to change current PIN?" : "Do you really want to set new PIN?";
		if (! dialogConfirm(s, NULL)) {
			fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
			dialogClear();
			return;
		}
	}

	if (! check_pin(false)) return;

	if (removal) {
		storage_setPin(NULL);
		fsm_sendSuccess("PIN removed");
	} else {
		char newpin[10];
		if (dialogEnterPin(newpin, true)) {
			storage_setPin(newpin);
			fsm_sendSuccess("PIN changed");
		} else {
			fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
		}
	}
	dialogClear();
}

void fsm_msgWipeDevice(WipeDevice *msg)
{
	(void)msg;
	update_poweroff_timeout();
	REQ(WipeDevice);
	if (! dialogConfirm("Do you really want to wipe the device?\nAll data will be lost.", NULL)) {
		fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
		dialogClear();
		return;
	}
	storage_wipe();
	// the following does not work on Mac anyway :-/ Linux/Windows are fine, so it is not needed
	// usbReconnect(); // force re-enumeration because of the serial number change
	fsm_sendSuccess("Device wiped");
	dialogClear();
}

void fsm_msgGetEntropy(GetEntropy *msg)
{
	RESP(GetEntropy, Entropy);
	if (! dialogConfirm("Do you really want to send entropy?", NULL)) {
		fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
		dialogClear();
		return;
	}
	uint32_t len = msg->size;
	if (len > 1024) {
		len = 1024;
	}
	resp->entropy.size = len;
	get_random_bytes(resp->entropy.bytes, len);
	msg_write(MessageType_MessageType_Entropy, resp);
	dialogClear();
}

void fsm_msgGetPublicKey(GetPublicKey *msg)
{
	RESP(GetPublicKey, PublicKey);

	CHECK_INITIALIZED

	if (! check_pin(true)) return;

	const CoinType *coin = fsm_getCoin(msg->has_coin_name, msg->coin_name);
	if (!coin) return;

	const char *curve = SECP256K1_NAME;
	if (msg->has_ecdsa_curve_name) {
		curve = msg->ecdsa_curve_name;
	}
	uint32_t fingerprint;
	HDNode *node;
	if (msg->address_n_count == 0) {
		/* get master node */
		fingerprint = 0;
		node = fsm_getDerivedNode(curve, msg->address_n, 0);
	} else {
		/* get parent node */
		node = fsm_getDerivedNode(curve, msg->address_n, msg->address_n_count - 1);
		if (!node) return;
		fingerprint = hdnode_fingerprint(node);
		/* get child */
		hdnode_private_ckd(node, msg->address_n[msg->address_n_count - 1]);
	}
	hdnode_fill_public_key(node);

	//if (msg->has_show_display && msg->show_display) {
		dialogShowPublicKey(node->public_key);
		// here was button check, but there is only "confirm" button...
	//}

	resp->node.depth = node->depth;
	resp->node.fingerprint = fingerprint;
	resp->node.child_num = node->child_num;
	resp->node.chain_code.size = 32;
	memcpy(resp->node.chain_code.bytes, node->chain_code, 32);
	resp->node.has_private_key = false;
	resp->node.has_public_key = true;
	resp->node.public_key.size = 33;
	memcpy(resp->node.public_key.bytes, node->public_key, 33);
	if (node->public_key[0] == 1) {
		/* ed25519 public key */
		resp->node.public_key.bytes[0] = 0;
	}
	resp->has_xpub = true;
	hdnode_serialize_public(node, fingerprint, coin->xpub_magic, resp->xpub, sizeof(resp->xpub));
	msg_write(MessageType_MessageType_PublicKey, resp);
	dialogClear();
}

void fsm_msgResetDevice(ResetDevice *msg)
{
	update_poweroff_timeout();
	REQ(ResetDevice);
	CHECK_NOT_INITIALIZED

	reset_init(
		msg->has_pin_protection && msg->pin_protection,
		msg->has_skip_backup ? msg->skip_backup : false
	);
}

void fsm_msgBackupDevice(BackupDevice *msg)
{
	update_poweroff_timeout();
	RESP(BackupDevice, Success);

	CHECK_INITIALIZED
	if (! check_pin(false)) return;

}

void fsm_msgSignTx(SignTx *msg)
{
	update_poweroff_timeout();
	RESP(SignTx, TxRequest);
	CHECK_INITIALIZED

	CHECK_PARAM(msg->inputs_count > 0, "Transaction must have at least one input");
	CHECK_PARAM(msg->outputs_count > 0, "Transaction must have at least one output");

	if (! check_pin(true)) return;

	const CoinType *coin = fsm_getCoin(msg->has_coin_name, msg->coin_name);
	if (!coin) return;
	const HDNode *node = fsm_getDerivedNode(SECP256K1_NAME, 0, 0);
	if (!node) return;

	signing_init(resp, msg->inputs_count, msg->outputs_count, coin, node, msg->version, msg->lock_time);
}

void fsm_msgTxAck(TxAck *msg)
{
	update_poweroff_timeout();
	RESP(TxAck, TxRequest);
	CHECK_PARAM(msg->has_tx, "No transaction provided");
	// tx: TransactionType

	signing_txack(resp, &(msg->tx));
}

void fsm_msgCancel(Cancel *msg)
{
	(void)msg;
	REQ(Cancel);
	//recovery_abort();
	signing_abort();
	//ethereum_signing_abort();
	fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
}

void fsm_msgClearSession(ClearSession *msg)
{
	(void)msg;
	REQ(ClearSession);
	session_clear(true); // clear PIN as well
	dialogClear();
	fsm_sendSuccess("Session cleared");
}

void fsm_msgApplySettings(ApplySettings *msg)
{
	update_poweroff_timeout();
	REQ(ApplySettings);
	fsm_sendSuccess("Settings applied");
}

void fsm_msgApplyFlags(ApplyFlags *msg)
{
	update_poweroff_timeout();
	REQ(ApplyFlags);
	if (msg->has_flags) {
		storage_applyFlags(msg->flags);
	}
	fsm_sendSuccess("Flags applied");
}

void fsm_msgGetAddress(GetAddress *msg)
{
	RESP(GetAddress, Address);

	CHECK_INITIALIZED

	if (! check_pin(true)) return;

	const CoinType *coin = fsm_getCoin(msg->has_coin_name, msg->coin_name);
	if (!coin) return;
	HDNode *node = fsm_getDerivedNode(SECP256K1_NAME, msg->address_n, msg->address_n_count);
	if (!node) return;
	hdnode_fill_public_key(node);

	char address[MAX_ADDR_SIZE];
	dialogProgress("Computing address", 0);
	if (!compute_address(coin, msg->script_type, node, msg->has_multisig, &msg->multisig, address)) {
		fsm_sendFailure(FailureType_Failure_DataError, "Can't encode address");
		dialogClear();
		return;
	}

	if (msg->has_show_display && msg->show_display) {
		char desc[16];
		if (msg->has_multisig) {
			#ifdef SUPPORT_EXTENDED_TYPES
			strlcpy(desc, "Msig __ of __:", sizeof(desc));
			const uint32_t m = msg->multisig.m;
			const uint32_t n = msg->multisig.pubkeys_count;
			desc[5] = (m < 10) ? ' ': ('0' + (m / 10));
			desc[6] = '0' + (m % 10);
			desc[11] = (n < 10) ? ' ': ('0' + (n / 10));
			desc[12] = '0' + (n % 10);
			#else
			fsm_sendFailure(FailureType_Failure_DataError, "Multisig not supported");
			#endif
		} else {
			strlcpy(desc, "Address:", sizeof(desc));
		}
		dialogCheckAddress(desc, address);
	}

	msg_write(MessageType_MessageType_Address, resp);
	dialogClear();
}

void fsm_msgEntropyAck(EntropyAck *msg)
{
	REQ(EntropyAck);
	if (msg->has_entropy) {
		reset_entropy(msg->entropy.bytes, msg->entropy.size);
	} else {
		reset_entropy(0, 0);
	}
}

void fsm_msgSignMessage(SignMessage *msg)
{
	update_poweroff_timeout();
	RESP(SignMessage, MessageSignature);

	CHECK_INITIALIZED

	if (! dialogSignMessage(msg->message.bytes, msg->message.size)) {
		fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
		dialogClear();
		return;
	}

	if (! check_pin(true)) return;

	const CoinType *coin = fsm_getCoin(msg->has_coin_name, msg->coin_name);
	if (!coin) return;
	HDNode *node = fsm_getDerivedNode(SECP256K1_NAME, msg->address_n, msg->address_n_count);
	if (!node) return;

	dialogProgress("Signing", 0);
	if (cryptoMessageSign(coin, node, msg->script_type, msg->message.bytes, msg->message.size, resp->signature.bytes) == 0) {
		resp->has_address = true;
		hdnode_fill_public_key(node);
		if (!compute_address(coin, msg->script_type, node, false, NULL, resp->address)) {
			fsm_sendFailure(FailureType_Failure_ProcessError, "Error computing address");
			dialogClear();
			return;
		}
		resp->has_signature = true;
		resp->signature.size = 65;
		msg_write(MessageType_MessageType_MessageSignature, resp);
	} else {
		fsm_sendFailure(FailureType_Failure_ProcessError, "Error signing message");
	}
	dialogClear();
}

void fsm_msgVerifyMessage(VerifyMessage *msg)
{
	update_poweroff_timeout();
	REQ(VerifyMessage);
	CHECK_PARAM(msg->has_address, "No address provided");
	CHECK_PARAM(msg->has_message, "No message provided");

	const CoinType *coin = fsm_getCoin(msg->has_coin_name, msg->coin_name);
	if (!coin) return;
	uint8_t addr_raw[MAX_ADDR_RAW_SIZE];
	uint32_t address_type;
	if (!coinExtractAddressType(coin, msg->address, &address_type) || !ecdsa_address_decode(msg->address, address_type, addr_raw)) {
		fsm_sendFailure(FailureType_Failure_DataError, "Invalid address");
		return;
	}
	dialogProgress("Verifying", 0);
	if (msg->signature.size == 65 && cryptoMessageVerify(coin, msg->message.bytes, msg->message.size, address_type, addr_raw, msg->signature.bytes) == 0) {
		if (! dialogCheckAddress("Signed by:", msg->address)) {
			fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
			dialogClear();
			return;
		}
		if (! dialogVerifyMessage(msg->message.bytes, msg->message.size)) {
			fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
			dialogClear();
			return;
		}
		fsm_sendSuccess("Message verified");
	} else {
		fsm_sendFailure(FailureType_Failure_DataError, "Invalid signature");
	}
	dialogClear();
}

void fsm_msgSignIdentity(SignIdentity *msg)
{
#if 0
	update_poweroff_timeout();
	REQ(SignIdentity, SignedIdentity);

	CHECK_INITIALIZED

	dialogSignIdentity(&(msg->identity), msg->has_challenge_visual ? msg->challenge_visual : 0);
	if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
		fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
		dialogClear();
		return;
	}

	if (! check_pin(true)) return;

	uint8_t hash[32];
	if (!msg->has_identity || cryptoIdentityFingerprint(&(msg->identity), hash) == 0) {
		fsm_sendFailure(FailureType_Failure_DataError, "Invalid identity");
		dialogClear();
		return;
	}

	uint32_t address_n[5];
	address_n[0] = 0x80000000 | 13;
	address_n[1] = 0x80000000 | hash[ 0] | (hash[ 1] << 8) | (hash[ 2] << 16) | (hash[ 3] << 24);
	address_n[2] = 0x80000000 | hash[ 4] | (hash[ 5] << 8) | (hash[ 6] << 16) | (hash[ 7] << 24);
	address_n[3] = 0x80000000 | hash[ 8] | (hash[ 9] << 8) | (hash[10] << 16) | (hash[11] << 24);
	address_n[4] = 0x80000000 | hash[12] | (hash[13] << 8) | (hash[14] << 16) | (hash[15] << 24);

	const char *curve = SECP256K1_NAME;
	if (msg->has_ecdsa_curve_name) {
		curve = msg->ecdsa_curve_name;
	}
	HDNode *node = fsm_getDerivedNode(curve, address_n, 5);
	if (!node) return;

	bool sign_ssh = msg->identity.has_proto && (strcmp(msg->identity.proto, "ssh") == 0);
	bool sign_gpg = msg->identity.has_proto && (strcmp(msg->identity.proto, "gpg") == 0);

	int result = 0;
	dialogProgress("Signing", 0);
	if (sign_ssh) { // SSH does not sign visual challenge
		result = sshMessageSign(node, msg->challenge_hidden.bytes, msg->challenge_hidden.size, resp->signature.bytes);
	} else if (sign_gpg) { // GPG should sign a message digest
		result = gpgMessageSign(node, msg->challenge_hidden.bytes, msg->challenge_hidden.size, resp->signature.bytes);
	} else {
		uint8_t digest[64];
		sha256_Raw(msg->challenge_hidden.bytes, msg->challenge_hidden.size, digest);
		sha256_Raw((const uint8_t *)msg->challenge_visual, strlen(msg->challenge_visual), digest + 32);
		result = cryptoMessageSign(&(coins[0]), node, InputScriptType_SPENDADDRESS, digest, 64, resp->signature.bytes);
	}

	if (result == 0) {
		hdnode_fill_public_key(node);
		if (strcmp(curve, SECP256K1_NAME) != 0) {
			resp->has_address = false;
		} else {
			resp->has_address = true;
			hdnode_get_address(node, 0x00, resp->address, sizeof(resp->address)); // hardcoded Bitcoin address type
		}
		resp->has_public_key = true;
		resp->public_key.size = 33;
		memcpy(resp->public_key.bytes, node->public_key, 33);
		if (node->public_key[0] == 1) {
			/* ed25519 public key */
			resp->public_key.bytes[0] = 0;
		}
		resp->has_signature = true;
		resp->signature.size = 65;
		msg_write(MessageType_MessageType_SignedIdentity, resp);
	} else {
		fsm_sendFailure(FailureType_Failure_ProcessError, "Error signing identity");
	}
	dialogClear();
#endif
}

void fsm_msgGetECDHSessionKey(GetECDHSessionKey *msg)
{
#if 0
	RESP(GetECDHSessionKey, ECDHSessionKey);

	CHECK_INITIALIZED

	if (! dialogDecryptIdentity(&msg->identity)) {
		fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
		dialogClear();
		return;
	}

	if (! check_pin(true)) return;

	uint8_t hash[32];
	if (!msg->has_identity || cryptoIdentityFingerprint(&(msg->identity), hash) == 0) {
		fsm_sendFailure(FailureType_Failure_DataError, "Invalid identity");
		dialogClear();
		return;
	}

	uint32_t address_n[5];
	address_n[0] = 0x80000000 | 17;
	address_n[1] = 0x80000000 | hash[ 0] | (hash[ 1] << 8) | (hash[ 2] << 16) | (hash[ 3] << 24);
	address_n[2] = 0x80000000 | hash[ 4] | (hash[ 5] << 8) | (hash[ 6] << 16) | (hash[ 7] << 24);
	address_n[3] = 0x80000000 | hash[ 8] | (hash[ 9] << 8) | (hash[10] << 16) | (hash[11] << 24);
	address_n[4] = 0x80000000 | hash[12] | (hash[13] << 8) | (hash[14] << 16) | (hash[15] << 24);

	const char *curve = SECP256K1_NAME;
	if (msg->has_ecdsa_curve_name) {
		curve = msg->ecdsa_curve_name;
	}

	const HDNode *node = fsm_getDerivedNode(curve, address_n, 5);
	if (!node) return;

	int result_size = 0;
	if (hdnode_get_shared_key(node, msg->peer_public_key.bytes, resp->session_key.bytes, &result_size) == 0) {
		resp->has_session_key = true;
		resp->session_key.size = result_size;
		msg_write(MessageType_MessageType_ECDHSessionKey, resp);
	} else {
		fsm_sendFailure(FailureType_Failure_ProcessError, "Error getting ECDH session key");
	}
	dialogClear();
#endif
}

void fsm_msgRecoveryDevice(RecoveryDevice *msg)
{
	update_poweroff_timeout();
	REQ(RecoveryDevice);
}
