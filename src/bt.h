#ifndef _BT_H_
#define _BT_H_

#include "types.h"

#define MSEC_TO_UNITS(TIME, RESOLUTION) (((TIME) * 1000) / (RESOLUTION))

#define BLE_UUID_SERVICE_DESCRIPTOR       0x2902
#define BLE_UUID_PRESENTATION_FORMAT      0x2904
#define BLE_UUID_REPORT_REFERENCE         0x2908
#define BLE_UUID_SOFTWARE_REVISION        0x2a28
#define BLE_UUID_MANUFACTURER_NAME        0x2a29
#define BLE_UUID_MODEL_NUMBER             0x2a24
#define BLE_UUID_FIRMWARE_REVISION        0x2a26
#define BLE_UUID_PNP_ID                   0x2a50
#define BLE_UUID_BATTERY_LEVEL            0x2a19
#define BLE_MAX_DATA_LEN                  20
#define BLE_MAX_MESSAGE_SIZE              256

#define BLE_UUID_HID_INFORMATION          0x2a4a
#define BLE_UUID_HID_CONTROLPOINT         0x2a4c
#define BLE_UUID_HID_REPORTMAP            0x2a4b
#define BLE_UUID_HID_REPORT               0x2a4d

#define BLE_UUID_BTDIS_SERVICE            0x180a
#define BLE_UUID_BATTERY_SERVICE          0x180f
#define BLE_UUID_HID_SERVICE              0x1812

#define APP_ADV_INTERVAL                  MSEC_TO_UNITS(100, 625)   /* The advertising interval (in units of 0.625 ms. */
#define APP_ADV_TIMEOUT_IN_SECONDS        3000

//7.5ms-4s in units of 1.25ms. FFFF - none
#define MIN_CONNECTION_INTERVAL          MSEC_TO_UNITS(25, 1250)                /**< Determines minimum connection interval in millisecond. */
//7.5ms-4s in units of 1.25ms. FFFF - none
#define MAX_CONNECTION_INTERVAL          MSEC_TO_UNITS(30, 1250)                /**< Determines maximum connection interval in millisecond. */
//In number of connection events. Max 499 
#define SLAVE_LATENCY                    4                                     /**< Determines slave latency in counts of connection events. */
//100ms-32s in units of 10 ms. FFFF - none
#define SUPERVISION_TIMEOUT              MSEC_TO_UNITS(10000, 10000)             /**< Determines supervision time-out in units of 10 millisecond. */

#define DATA_PKT_MAX_LEN 20

extern uint16_t conn_handle;

extern ble_gatts_char_handles_t hid_cp_handles;
extern ble_gatts_char_handles_t hid_rx_handles;
extern ble_gatts_char_handles_t hid_tx_handles;

void     ble_init(void);
bool ble_is_connected(void);
uint32_t ble_check_event(void);
uint32_t ble_wait_event(void);
uint32_t ble_notification(u16 handle, u8 *data, u16 length);


void hid_message_receive(uint8_t *data, int len);

#endif
