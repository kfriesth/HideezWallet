#include "main.h"

static const char DeviceName[] = { APPLICATION_NAME };

static const  ble_gap_conn_params_t ble_connection_param =
 { //SD
  .min_conn_interval  =  MSEC_TO_UNITS(50, 1250),
  .max_conn_interval  =  MSEC_TO_UNITS(50/*48*/, 1250),
  .slave_latency    =  0,
  .conn_sup_timeout  =  MSEC_TO_UNITS(6000, 10000),
 };

static ble_gatts_attr_md_t attr_stack_md = {
	.vloc                        = BLE_GATTS_VLOC_STACK,
	.rd_auth                     = 0,
	.wr_auth                     = 0,
	.vlen                        = 1,
	.read_perm                   = { .sm=1, .lv=1 },
	.write_perm                  = { .sm=1, .lv=1 },
};

static ble_gatts_attr_md_t attr_const_md = {
	.vloc                        = BLE_GATTS_VLOC_USER,
	.rd_auth                     = 0,
	.wr_auth                     = 0,
	.vlen                        = 0,
	.read_perm                   = { .sm=1, .lv=1 },
	.write_perm                  = { .sm=1, .lv=1 },
};

/*
static ble_gatts_attr_md_t attr_stack_auth_md = {
	.vloc                        = BLE_GATTS_VLOC_STACK,
	.rd_auth                     = 0,
	.wr_auth                     = 0,
	.vlen                        = 1,
	.read_perm                   = { .sm=1, .lv=3 },
	.write_perm                  = { .sm=1, .lv=3 },
};

static ble_gatts_attr_md_t attr_const_auth_md = {
	.vloc                        = BLE_GATTS_VLOC_USER,
	.rd_auth                     = 0,
	.wr_auth                     = 0,
	.vlen                        = 0,
	.read_perm                   = { .sm=1, .lv=3 },
	.write_perm                  = { .sm=1, .lv=3 },
};
*/

static ble_gatts_attr_md_t attr_cccd_md = {
	.vloc                        = BLE_GATTS_VLOC_STACK,
	.rd_auth                     = 0,
	.wr_auth                     = 0,
	.vlen                        = 0,
	.read_perm                   = { .sm=1, .lv=1 },
	.write_perm                  = { .sm=1, .lv=1 },
};

static ble_gatts_char_md_t tx_char_md = {

	.char_props.write            = 1,
	.char_props.write_wo_resp    = 1,
	.char_props.notify           = 0,

};

static ble_gatts_char_md_t txrx_char_md = {

	.char_props.read             = 1,
	.char_props.write            = 1,
	.char_props.write_wo_resp    = 1,
	.char_props.notify           = 0,

};

static ble_gatts_char_md_t rx_char_md = {

	.p_cccd_md                   = &attr_cccd_md,
	.char_props.read             = 1,
	.char_props.notify           = 1,

};

static ble_gatts_char_md_t info_char_md = {

	.char_props.read             = 1,

};

static const ble_uuid_t battery_service_uuid = { BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE };
static const ble_uuid_t battery_level_uuid = { BLE_UUID_BATTERY_LEVEL, BLE_UUID_TYPE_BLE };
static const ble_uuid_t presentation_format_uuid = { BLE_UUID_PRESENTATION_FORMAT, BLE_UUID_TYPE_BLE };

static u8  battery_presentation_format[] = { 4, 1, 0xad, 0x27, 1, 0, 0 };

u8 current_battery_level = 100;

static const ble_gatts_attr_t battery_level_char_value = {
	.p_uuid = (ble_uuid_t *) &battery_level_uuid,
	.p_attr_md = &attr_const_md,
	.p_value = (uint8_t *) &current_battery_level,
	.init_len = 1,
	.init_offs = 0,
	.max_len = 1,
};

static const ble_gatts_attr_t battery_presentation_desc_value = {
	.p_uuid = (ble_uuid_t *) &presentation_format_uuid,
	.p_attr_md = &attr_const_md,
	.p_value = (uint8_t *) battery_presentation_format,
	.init_len = 7,
	.init_offs = 0,
	.max_len = 7,
};

static const ble_uuid_t btdis_service_uuid = { BLE_UUID_BTDIS_SERVICE, BLE_UUID_TYPE_BLE };
static const ble_uuid_t btdis_manufacturer_uuid = { BLE_UUID_MANUFACTURER_NAME, BLE_UUID_TYPE_BLE };
static const ble_uuid_t btdis_model_uuid = { BLE_UUID_MODEL_NUMBER, BLE_UUID_TYPE_BLE };
static const ble_uuid_t btdis_fw_uuid = { BLE_UUID_FIRMWARE_REVISION, BLE_UUID_TYPE_BLE };
static const ble_uuid_t btdis_pnpid_uuid = { BLE_UUID_PNP_ID, BLE_UUID_TYPE_BLE };

static char manufacturer_name[] = { MANUFACTURER_NAME };
static char model_number[] = { MODEL_NUMBER };
static char firmware_revision[] = { FIRMWARE_REVISION };
static u8   pnp_id[] = { 0x02, (PNP_VID & 0xff), (PNP_VID >> 8), (PNP_PID & 0xff), (PNP_PID >> 8), 0x00, 0x01 };

static const ble_gatts_attr_t btdis_manuf_char_value = {
	.p_uuid = (ble_uuid_t *) &btdis_manufacturer_uuid,
	.p_attr_md = &attr_const_md,
	.p_value = (uint8_t *) manufacturer_name,
	.init_len = sizeof(manufacturer_name),
	.init_offs = 0,
	.max_len = sizeof(manufacturer_name),
};

static const ble_gatts_attr_t btdis_model_char_value = {
	.p_uuid = (ble_uuid_t *) &btdis_model_uuid,
	.p_attr_md = &attr_const_md,
	.p_value = (uint8_t *) model_number,
	.init_len = sizeof(model_number),
	.init_offs = 0,
	.max_len = sizeof(model_number),
};

static const ble_gatts_attr_t btdis_fw_char_value = {
	.p_uuid = (ble_uuid_t *) &btdis_fw_uuid,
	.p_attr_md = &attr_const_md,
	.p_value = (uint8_t *) firmware_revision,
	.init_len = sizeof(firmware_revision),
	.init_offs = 0,
	.max_len = sizeof(firmware_revision),
};

static const ble_gatts_attr_t btdis_pnpid_char_value = {
	.p_uuid = (ble_uuid_t *) &btdis_pnpid_uuid,
	.p_attr_md = &attr_const_md,
	.p_value = (uint8_t *) pnp_id,
	.init_len = sizeof(pnp_id),
	.init_offs = 0,
	.max_len = sizeof(pnp_id),
};

static const ble_uuid_t hid_service_uuid = { BLE_UUID_HID_SERVICE, BLE_UUID_TYPE_BLE };
static const ble_uuid_t hid_info_uuid = { BLE_UUID_HID_INFORMATION, BLE_UUID_TYPE_BLE };
static const ble_uuid_t hid_controlpoint_uuid = { BLE_UUID_HID_CONTROLPOINT, BLE_UUID_TYPE_BLE };
static const ble_uuid_t hid_reportmap_uuid = { BLE_UUID_HID_REPORTMAP, BLE_UUID_TYPE_BLE };
static const ble_uuid_t hid_report_uuid = { BLE_UUID_HID_REPORT, BLE_UUID_TYPE_BLE };
static const ble_uuid_t hid_reportref_uuid = { BLE_UUID_REPORT_REFERENCE, BLE_UUID_TYPE_BLE };

static char hid_info[] = { 0x00, 0x01, 0x00, 0x02 };
static char hid_report_map[] = {

	0x06, 0x00, 0xff,  // USAGE_PAGE (Vendor Defined)
	0x09, 0x01,        // USAGE (1)
	0xa1, 0x01,        // COLLECTION (Application)
	0x09, 0x20,        // USAGE (Input Report Data)
	0x15, 0x00,        // LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,  // LOGICAL_MAXIMUM (255)
	0x75, 0x08,        // REPORT_SIZE (8)
	0x95, 0x14,        // REPORT_COUNT (20)
	0x81, 0x02,        // INPUT (Data,Var,Abs)
	0x09, 0x21,        // USAGE (Output Report Data)
	0x15, 0x00,        // LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,  // LOGICAL_MAXIMUM (255)
	0x75, 0x08,        // REPORT_SIZE (8)
	0x95, 0x14,        // REPORT_COUNT (20)
	0x91, 0x02,        // OUTPUT (Data,Var,Abs)
	0xc0               // END_COLLECTION

};

/* 1=input 2=output 3=feature */
static char in_report_ref[2] = { 0, 0x01 };
static char out_report_ref[2] = { 0, 0x02 };

static const ble_gatts_attr_t hid_info_char_value = {
	.p_uuid = (ble_uuid_t *) &hid_info_uuid,
	.p_attr_md = (ble_gatts_attr_md_t *) &attr_const_md,
	.p_value = (uint8_t *) hid_info,
	.init_len = 4,
	.init_offs = 0,
	.max_len = 4,
};

static const ble_gatts_attr_t hid_reportmap_char_value = {
	.p_uuid = (ble_uuid_t *) &hid_reportmap_uuid,
	.p_attr_md = (ble_gatts_attr_md_t *) &attr_const_md,
	.p_value = (uint8_t *) hid_report_map,
	.init_len = sizeof(hid_report_map),
	.init_offs = 0,
	.max_len = sizeof(hid_report_map),
};

static const ble_gatts_attr_t hid_report_char_value = {
	.p_uuid = (ble_uuid_t *) &hid_report_uuid,
	.p_attr_md = (ble_gatts_attr_md_t *) &attr_stack_md,
	.init_len = 0,
	.init_offs = 0,
	.max_len = BLE_MAX_DATA_LEN,
};

static const ble_gatts_attr_t hid_controlpoint_char_value = {
	.p_uuid = (ble_uuid_t *) &hid_controlpoint_uuid,
	.p_attr_md = (ble_gatts_attr_md_t *) &attr_stack_md,
	.init_len = 1,
	.init_offs = 0,
	.max_len = BLE_MAX_DATA_LEN,
};

static const ble_gatts_attr_t in_reportref_desc_value = {
	.p_uuid = (ble_uuid_t *) &hid_reportref_uuid,
	.p_attr_md = (ble_gatts_attr_md_t *) &attr_const_md,
	.p_value = (uint8_t *) in_report_ref,
	.init_len = 2,
	.init_offs = 0,
	.max_len = 2,
};
static const ble_gatts_attr_t out_reportref_desc_value = {
	.p_uuid = (ble_uuid_t *) &hid_reportref_uuid,
	.p_attr_md = (ble_gatts_attr_md_t *) &attr_const_md,
	.p_value = (uint8_t *) out_report_ref,
	.init_len = 2,
	.init_offs = 0,
	.max_len = 2,
};

static const uint8_t adv_data[] = {
	2, BLE_GAP_AD_TYPE_FLAGS,BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
	3, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE,
		BLE_UUID_HID_SERVICE&0xff, BLE_UUID_HID_SERVICE>>8,
	3,BLE_GAP_AD_TYPE_APPEARANCE,BLE_APPEARANCE_GENERIC_KEYRING&0xff,BLE_APPEARANCE_GENERIC_KEYRING>>8,
	(APPLICATION_NAME_SIZE + 1), 0x08, APPLICATION_NAME
};

static const uint8_t scan_data[] = {
	2, BLE_GAP_AD_TYPE_TX_POWER_LEVEL, 0
};

static const ble_gap_adv_params_t adv_params =
{
	.type        = BLE_GAP_ADV_TYPE_ADV_IND,
	.p_peer_addr = NULL, // Undirected advertisement
	.p_whitelist = NULL,
	.fp          = BLE_GAP_ADV_FP_ANY,
	.interval    = APP_ADV_INTERVAL,
	.timeout     = APP_ADV_TIMEOUT_IN_SECONDS,
};

static const ble_gap_sec_params_t m_sec_params = {
	.bond = 1,
#ifdef PASSKEY
	.mitm = 1,
	.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY,
#else
	.mitm = 0,
	.io_caps = BLE_GAP_IO_CAPS_NONE,
#endif
	.oob = 0,
	.min_key_size = 0x07,
	.max_key_size = 0x10,
	.kdist_periph.enc = 1,
};

__align(4) u8  gs_evt_buf[(sizeof(ble_evt_t) + BLE_L2CAP_MTU_DEF + 3) & ~3];

static bool connected = false;

static ble_enable_params_t enable_params;

uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;

uint16_t btdis_service_handle;
uint16_t battery_service_handle;
uint16_t hid_service_handle;

ble_gatts_char_handles_t hid_cp_handles;
ble_gatts_char_handles_t hid_rx_handles;
ble_gatts_char_handles_t hid_tx_handles;

ble_gap_enc_key_t         m_enc_key;    //24b could be changed with pointer to dummy.
ble_gap_id_key_t 					m_peer_id;   		/**< IRK and/or address of peer. */

static u8 sys_attr_buffer[32];
static u16 sys_attr_len = 0;

static void restart_advertising(void)
{
#ifdef PASSKEY
	u8 passkey[] = PASSKEY;
	ble_opt_t opt;
	opt.gap_opt.passkey.p_passkey = passkey;
	sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &opt);
#endif
	sd_ble_gap_adv_data_set(adv_data, sizeof(adv_data), scan_data, sizeof(scan_data));
	sd_ble_gap_adv_start(&adv_params);
}

void ble_init(void)
{
	ble_gap_conn_params_t cp_init;
	ble_gap_conn_sec_mode_t sec_mode;
	ble_gatts_char_handles_t handle;
	uint16_t dhandle;
	int ret;

	ret = sd_ble_enable(&enable_params);

	// Initialize and set-up connection parameters
	cp_init.min_conn_interval = 50;
	cp_init.max_conn_interval = 100;
	cp_init.slave_latency = 0;
	cp_init.conn_sup_timeout = 3000;
	ret = sd_ble_gap_ppcp_set(&cp_init);
	if (ret != NRF_SUCCESS) dprintf("sd_ble_gap_ppcp_set: %x\n", ret);
	
	ret =  sd_ble_gap_appearance_set(576);
	if (ret != NRF_SUCCESS) dprintf("sd_ble_gap_appearance_set: %x\n", ret);

	// Adding Battery Service
	
	ret = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &battery_service_uuid, &battery_service_handle);
	ret = sd_ble_gatts_characteristic_add(battery_service_handle, &info_char_md, &battery_level_char_value, &handle);
	sd_ble_gatts_descriptor_add(BLE_GATT_HANDLE_INVALID, &battery_presentation_desc_value, (uint16_t *)&handle);

	// Adding Device Information Service

	sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &btdis_service_uuid, &btdis_service_handle);
	sd_ble_gatts_characteristic_add(btdis_service_handle, &info_char_md, &btdis_manuf_char_value, &handle);
	sd_ble_gatts_characteristic_add(btdis_service_handle, &info_char_md, &btdis_model_char_value, &handle);
	sd_ble_gatts_characteristic_add(btdis_service_handle, &info_char_md, &btdis_fw_char_value, &handle);
	sd_ble_gatts_characteristic_add(btdis_service_handle, &info_char_md, &btdis_pnpid_char_value, &handle);

	// Adding HID service

	sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &hid_service_uuid, &hid_service_handle);
	sd_ble_gatts_characteristic_add(hid_service_handle, &info_char_md, &hid_info_char_value, &handle);
	sd_ble_gatts_characteristic_add(hid_service_handle, &info_char_md, &hid_reportmap_char_value, &handle);
	sd_ble_gatts_characteristic_add(hid_service_handle, &tx_char_md, &hid_controlpoint_char_value, &hid_cp_handles);
	sd_ble_gatts_characteristic_add(hid_service_handle, &rx_char_md, &hid_report_char_value, &hid_rx_handles);
	sd_ble_gatts_descriptor_add(BLE_GATT_HANDLE_INVALID, &in_reportref_desc_value, (uint16_t *)&dhandle);  // device -> host (notify)
	sd_ble_gatts_characteristic_add(hid_service_handle, &txrx_char_md, &hid_report_char_value, &hid_tx_handles);
	sd_ble_gatts_descriptor_add(BLE_GATT_HANDLE_INVALID, &out_reportref_desc_value, (uint16_t *)&dhandle); // host -> device (write)
	DBG("HID: CP=%4x TX=%4x RX=%4x CCCD=%4x\n", hid_cp_handles.value_handle, hid_tx_handles.value_handle, hid_rx_handles.value_handle, hid_rx_handles.cccd_handle);

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
	sd_ble_gap_device_name_set(&sec_mode,(const uint8_t *)DeviceName, sizeof(DeviceName));
	restart_advertising();
}

static void save_context(u16 handle, u8 *addr)
{
	sys_attr_len = sizeof(sys_attr_buffer);
	if (sd_ble_gatts_sys_attr_get(handle, sys_attr_buffer, &sys_attr_len, BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS) == NRF_SUCCESS) {
		DBG("sys attr: %d bytes\n", sys_attr_len);
	} else {
		sys_attr_len = 0;
	}
	ctx.attr_len = 
	ctx.
}

static void restore_context(u16 handle, u8 *addr)
{
	id = nvs_lookup_data(NV_TABLE_BONDS, 0, addr, 6);
	if (id) {
		ctx = nvs_read_record(NV_TABLE_BONDS, key, &ctxlen);
	}

	if (! sys_attr_len) return;
	sd_ble_gatts_sys_attr_set(handle, sys_attr_buffer, sys_attr_len, BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS);
}

static void handle_event(ble_evt_t *ble_evt)
{
	u16 evt_id = ble_evt->header.evt_id;
	dprintf("BLE EVT: %x\n", evt_id);

	if (evt_id == BLE_GAP_EVT_CONNECTED)
	{
		DBG("Connect\n");
		conn_handle = ble_evt->evt.gap_evt.conn_handle;
		restore_context(conn_handle);
		connected = true;
	}
	else if (evt_id == BLE_GAP_EVT_DISCONNECTED)
	{
		u8 reason=ble_evt->evt.gap_evt.params.disconnected.reason; 
		DBG("Disconnect %2x\n",reason);
		save_context(conn_handle);
		conn_handle = BLE_CONN_HANDLE_INVALID;
		restart_advertising();
		connected = false;
	}
	else if (evt_id == BLE_GAP_EVT_TIMEOUT)
	{
		restart_advertising();
	}
	else if (evt_id == BLE_GATTS_EVT_WRITE)
	{
		ble_gatts_evt_write_t *evt_write = &ble_evt->evt.gatts_evt.params.write;
		uint16_t len = evt_write->len;
		uint8_t *data = evt_write->data;
		//DBG("WriteReq %x len=%d\n", evt_write->handle, len);
		if (evt_write->handle == hid_tx_handles.value_handle) {
			hid_message_receive(data, len);
		}
	}
	else if (evt_id == BLE_GAP_EVT_SEC_PARAMS_REQUEST)
	{
		DBG("SecParamsReq\n");
		ble_gap_sec_keyset_t keys_exchanged;
		memset(&m_enc_key,0xff,sizeof(ble_gap_enc_key_t)); //28b
		keys_exchanged.keys_periph.p_enc_key   = &m_enc_key;
		keys_exchanged.keys_central.p_id_key   = &m_peer_id; 
		sd_ble_gap_sec_params_reply(conn_handle,BLE_GAP_SEC_STATUS_SUCCESS,&m_sec_params,&keys_exchanged);
	}
	else if (evt_id == BLE_GAP_EVT_SEC_INFO_REQUEST)
	{
		DBG("SecInfoReq\n");
		ble_gap_enc_info_t *    p_enc_info;
		ble_gap_irk_t *         p_id_info;
		p_enc_info 	= 	&m_enc_key.enc_info;
		p_id_info 	= 	&m_peer_id.id_info;
		sd_ble_gap_sec_info_reply(conn_handle,p_enc_info,p_id_info,NULL);
	}
	else	if (evt_id == BLE_GAP_EVT_CONN_SEC_UPDATE)
	{
		DBG("ConnSecUpdate M:%2x L:%2x\n", ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm,
                   ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);		
	}
	else if (evt_id == BLE_GATTS_EVT_SYS_ATTR_MISSING)
	{
		sd_ble_gatts_sys_attr_set(conn_handle,NULL,0,BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS);
	}
	else if (evt_id == BLE_GAP_EVT_AUTH_STATUS)
	{
		sd_ble_gap_conn_param_update(conn_handle, &ble_connection_param);
	}	
}


uint32_t ble_check_event(void)
{
	uint16_t evt_len = sizeof(gs_evt_buf);
	uint32_t ret = sd_ble_evt_get(gs_evt_buf, &evt_len);
	if (ret == NRF_SUCCESS) {
		ble_evt_t *ble_evt = (ble_evt_t *)gs_evt_buf;
		handle_event(ble_evt);
		CHECK_STACK();
	}
	return ret;
}

uint32_t ble_wait_event(void)
{
	
	uint32_t ret = ble_check_event();
	if (ret != NRF_SUCCESS) {
		sd_app_evt_wait();
		ret = ble_check_event();
	}
	return ret;	
}

uint32_t ble_notification(u16 handle, u8 *data, u16 length)
{
	ble_gatts_hvx_params_t hvx_params;

	hvx_params.handle = handle;
	hvx_params.p_data = data;
	hvx_params.p_len  = &length;
	hvx_params.offset = 0;
	hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
	return sd_ble_gatts_hvx(conn_handle, &hvx_params);
}

bool ble_is_connected(void) { return connected; }

