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

#include "main.h"

#include "messages.h"
#include "fsm.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "messages.pb.h"

#define MDBG(s...) dprintf(s)

//#define MDBG2(s...) dprintf(s)
#define MDBG2(s...)

static uint8_t msg_outbuf[2+HID_BLOCK_SIZE];
static uint32_t msg_outlen = 0;
static uint32_t msg_outstatus;

static uint8_t msg_inbuf[2+HID_BLOCK_SIZE];
static uint32_t msg_inpos = 0;
static uint32_t msg_inlen = 0;
static uint32_t msg_instatus;

typedef void (*mproc)(void *);

struct MessagesMap_t
{
	char type;	// n = normal, d = debug
	char dir; 	// i = in, o = out
	uint16_t msg_id;
	const pb_field_t *fields;
	void (*process_func)(void *ptr);
};

static const struct MessagesMap_t inMessagesMap[] =
{
	// in messages

	{ 'n', 'i', MessageType_MessageType_Initialize,        Initialize_fields,        (mproc) fsm_msgInitialize },
	{ 'n', 'i', MessageType_MessageType_Ping,              Ping_fields,              (mproc) fsm_msgPing },
	{ 'n', 'i', MessageType_MessageType_ChangePin,         ChangePin_fields,         (mproc) fsm_msgChangePin },
	{ 'n', 'i', MessageType_MessageType_WipeDevice,        WipeDevice_fields,        (mproc) fsm_msgWipeDevice },
	{ 'n', 'i', MessageType_MessageType_GetEntropy,        GetEntropy_fields,        (mproc) fsm_msgGetEntropy },
	{ 'n', 'i', MessageType_MessageType_GetPublicKey,      GetPublicKey_fields,      (mproc) fsm_msgGetPublicKey },
	{ 'n', 'i', MessageType_MessageType_ResetDevice,       ResetDevice_fields,       (mproc) fsm_msgResetDevice },
	{ 'n', 'i', MessageType_MessageType_SignTx,            SignTx_fields,            (mproc) fsm_msgSignTx },
	{ 'n', 'i', MessageType_MessageType_Cancel,            Cancel_fields,            (mproc) fsm_msgCancel },
	{ 'n', 'i', MessageType_MessageType_TxAck,             TxAck_fields,             (mproc) fsm_msgTxAck },
	{ 'n', 'i', MessageType_MessageType_ClearSession,      ClearSession_fields,      (mproc) fsm_msgClearSession },
	{ 'n', 'i', MessageType_MessageType_ApplySettings,     ApplySettings_fields,     (mproc) fsm_msgApplySettings },
	{ 'n', 'i', MessageType_MessageType_ApplyFlags,        ApplyFlags_fields,        (mproc) fsm_msgApplyFlags },
	{ 'n', 'i', MessageType_MessageType_GetAddress,        GetAddress_fields,        (mproc) fsm_msgGetAddress },
	{ 'n', 'i', MessageType_MessageType_BackupDevice,      BackupDevice_fields,      (mproc) fsm_msgBackupDevice },
	{ 'n', 'i', MessageType_MessageType_EntropyAck,        EntropyAck_fields,        (mproc) fsm_msgEntropyAck },
	{ 'n', 'i', MessageType_MessageType_SignMessage,       SignMessage_fields,       (mproc) fsm_msgSignMessage },
	{ 'n', 'i', MessageType_MessageType_VerifyMessage,     VerifyMessage_fields,     (mproc) fsm_msgVerifyMessage },
	{ 'n', 'i', MessageType_MessageType_SignIdentity,      SignIdentity_fields,      (mproc) fsm_msgSignIdentity },
	{ 'n', 'i', MessageType_MessageType_GetFeatures,       GetFeatures_fields,       (mproc) fsm_msgGetFeatures },
	{ 'n', 'i', MessageType_MessageType_GetECDHSessionKey, GetECDHSessionKey_fields, (mproc) fsm_msgGetECDHSessionKey },
	{0, 0, 0, 0, 0}
};

static const struct MessagesMap_t outMessagesMap[] =
{
	// out messages

	{ 'n', 'o', MessageType_MessageType_Success,           Success_fields,           0 },
	{ 'n', 'o', MessageType_MessageType_Failure,           Failure_fields,           0 },
	{ 'n', 'o', MessageType_MessageType_Entropy,           Entropy_fields,           0 },
	{ 'n', 'o', MessageType_MessageType_PublicKey,         PublicKey_fields,         0 },
	{ 'n', 'o', MessageType_MessageType_Features,          Features_fields,          0 },
	{ 'n', 'o', MessageType_MessageType_PinMatrixRequest,  PinMatrixRequest_fields,  0 },
	{ 'n', 'o', MessageType_MessageType_TxRequest,         TxRequest_fields,         0 },
	{ 'n', 'o', MessageType_MessageType_ButtonRequest,     ButtonRequest_fields,     0 },
	{ 'n', 'o', MessageType_MessageType_Address,           Address_fields,           0 },
	{ 'n', 'o', MessageType_MessageType_EntropyRequest,    EntropyRequest_fields,    0 },
	{ 'n', 'o', MessageType_MessageType_MessageSignature,  MessageSignature_fields,  0 },
	{ 'n', 'o', MessageType_MessageType_PassphraseRequest, PassphraseRequest_fields, 0 },
	{ 'n', 'o', MessageType_MessageType_SignedIdentity,    SignedIdentity_fields,    0 },
	{ 'n', 'o', MessageType_MessageType_ECDHSessionKey,    ECDHSessionKey_fields,    0 },
	{0, 0, 0, 0, 0}
};

void msg_process(void);

void hid_message_receive(uint8_t *data, int len)
{
	static bool decoding = false;;

	// store hid message to buffer, handle later
	MDBG2("R %d [%*b]\n", len, len, data);
	if (data[0] == 0x00) {
		// extra byte
		data++;
		len--;
	}
	if (len < 1+HID_BLOCK_SIZE) return;
	if (data[0] != HID_BLOCK_SIZE) return;
	memcpy(msg_inbuf, data+1, HID_BLOCK_SIZE);
	msg_inpos = 0;
	msg_inlen = HID_BLOCK_SIZE;
	// call msg_process() for first message, otherwise we're called from pb_callback
	if (! decoding) {
		decoding = true;
		msg_process();
		decoding = false;
	}
}

uint32_t hid_message_send(uint8_t *data, u16 len)
{
	uint32_t ret;
	uint32_t t = JIFFIES;
	while (TIMEDIFF(JIFFIES, t) < HID_IO_TIMEOUT) {
		ret = ble_notification(hid_rx_handles.value_handle, data, len);
		if (ret == NRF_SUCCESS) {
			MDBG2("W %d [%*b]\n", len, len, data);
			return ret;
		}
		if (ret == NRF_ERROR_BUSY || ret == BLE_ERROR_NO_TX_BUFFERS) {
			ble_check_event();
			continue;
		}
		MDBG("message_send: %x\n", ret);
		return ret;
	}
	MDBG("hid_message_send timeout\n");
	return NRF_ERROR_TIMEOUT;
}


static void msg_out_append(uint8_t c)
{
	msg_outbuf[2+msg_outlen] = c;
	msg_outlen++;
	if (msg_outlen == HID_BLOCK_SIZE) {
		msg_outstatus = hid_message_send(msg_outbuf, 2+HID_BLOCK_SIZE);
		msg_outlen = 0;
	}
}

static bool pb_callback_out(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
	(void)stream;
	for (size_t i = 0; i < count; i++) {
		if (msg_outstatus != NRF_SUCCESS) break;
		msg_out_append(buf[i]);
	}
	return (msg_outstatus == NRF_SUCCESS);
}

bool msg_write_common(char type, uint16_t msg_id, const void *msg_ptr)
{
	const pb_field_t *fields = NULL;
	const struct MessagesMap_t *m = outMessagesMap;
	while (m->type) {
		if (m->msg_id == msg_id) {
			fields = m->fields;
		}
		m++;
	}
	if (!fields) {
		MDBG("msg_write_common: unknown message %d\n", msg_id);
		return false;
	}

	pb_ostream_t sizestream = {0, 0, SIZE_MAX, 0, 0};
	bool status = pb_encode(&sizestream, fields, msg_ptr);

	if (!status) {
		return false;
	}

	uint32_t len = sizestream.bytes_written;

	msg_outbuf[0] = 0;
	msg_outbuf[1] = HID_BLOCK_SIZE; // first byte is for all messages
	msg_outbuf[2] = '#';
	msg_outbuf[3] = '#';
	msg_outbuf[4] = (msg_id >> 8) & 0xFF;
	msg_outbuf[5] = msg_id & 0xFF;
	msg_outbuf[6] = (len >> 24) & 0xFF;
	msg_outbuf[7] = (len >> 16) & 0xFF;
	msg_outbuf[8] = (len >> 8) & 0xFF;
	msg_outbuf[9] = len & 0xFF;
	msg_outlen = 8;
	msg_outstatus = NRF_SUCCESS;

	pb_ostream_t stream = {pb_callback_out, 0, SIZE_MAX, 0, 0};
	pb_encode(&stream, fields, msg_ptr);

	// pad last message
	while (msg_outstatus == NRF_SUCCESS && msg_outlen != 0) {
		msg_out_append(0);
	}
	return (msg_outstatus == NRF_SUCCESS);
}

static inline uint8_t msg_in_read(void)
{
	if (msg_inpos < msg_inlen) return msg_inbuf[msg_inpos++];

	msg_inpos = msg_inlen = 0;
	uint32_t t = JIFFIES;
	while (TIMEDIFF(JIFFIES, t) < HID_IO_TIMEOUT) {
		ble_wait_event();
		if (msg_inpos < msg_inlen) return msg_inbuf[msg_inpos++];
	}
	MDBG("msg_in_read timeout\n");
	msg_instatus = NRF_ERROR_TIMEOUT;
	return 0;
}

static bool pb_callback_in(pb_istream_t *stream, uint8_t *buf, size_t count)
{
	(void)stream;
	for (size_t i = 0; i < count; i++) {
		if (msg_instatus != NRF_SUCCESS) break;
		buf[i] = msg_in_read();
	}
	return (msg_instatus == NRF_SUCCESS);
}

void msg_process(void)
{
	const struct MessagesMap_t *m;
	void (*func)(void *) = NULL;
        const pb_field_t *fields = NULL;

	if (msg_inlen < 8 || msg_inbuf[0] != '#' || msg_inbuf[1] != '#') return;

	uint16_t msg_id = (msg_inbuf[2] << 8) + msg_inbuf[3];
	uint32_t msg_size = (msg_inbuf[4] << 24)+ (msg_inbuf[5] << 16) + (msg_inbuf[6] << 8) + msg_inbuf[7];
	//MDBG("MSG:%d (%d)\n", msg_id, msg_size);
	//MDBG("[%*b]\n", (msg_size>24) ? 24 : msg_size, msg_inbuf+8);

	msg_inpos = 8;
	msg_instatus = NRF_SUCCESS;

	// find message handler

	m = inMessagesMap;
	while (m->type) {
		if (m->msg_id == msg_id) {
			func = m->process_func;
			fields = m->fields;
		}
		m++;
	}

	if (!fields) { // unknown message
		fsm_sendFailure(FailureType_Failure_UnexpectedMessage, "Unknown message");
		return;
	}
	if (msg_size > SHARED_BUFFER_SIZE) { // message is too big :(
		fsm_sendFailure(FailureType_Failure_DataError, "Message too big");
		return;
	}

	pb_istream_t stream = {pb_callback_in, 0, msg_size, 0};
	memset(shared_buffer, 0, SHARED_BUFFER_SIZE);
	bool status = pb_decode(&stream, fields, shared_buffer);
	if (status) {
		// process message
		if (func) (func)(shared_buffer);
	} else {
		fsm_sendFailure(FailureType_Failure_DataError, stream.errmsg);
	}

}
