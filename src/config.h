#ifndef _CONFIG_H_
#define _CONFIG_H_

#define RAM_START                      0x20002000
#define RAM_SIZE                           0x2000

#define SHARED_BUFFER_SIZE 2048

#define STACK_SIZE 2900

#define CONSOLE 1

#define CONSOLE_RX_BUFFER_SIZE 80
#define CONSOLE_TX_BUFFER_SIZE 160

#define APPLICATION_NAME                  'H','i','d','e','e','z','C','o','i','n'
#define APPLICATION_NAME_SIZE             10

#define MANUFACTURER_NAME 'H','i','d','e','e','z'
#define MODEL_NUMBER '1','0','2'
#define FIRMWARE_REVISION '1','.','2'
#define PNP_VID 0x534c
#define PNP_PID 0x0001

#define VERSION_MAJOR 1
#define VERSION_MINOR 5
#define VERSION_PATCH 2

#define PASSKEY "123456"

//#define USE_DIALOG

#define POWEROFF_TIMEOUT   300
#define CONFIRM_TIMEOUT       10      // in seconds
#define PINCODE_TIMEOUT       7000
#define PINCODE_DIGIT_TIMEOUT 1000
#define PINCODE_LONG          300

#define HID_BLOCK_SIZE  18

#define HID_IO_TIMEOUT  8192    // 2 seconds


#define NV_TABLE_BONDS  5
#define NV_TABLE_COIN   6

#define NV_KEY_UUID     1
#define NV_KEY_ENTROPY  2
#define NV_KEY_PIN      3
#define NV_KEY_PINFAILS 4
#define NV_KEY_FLAGS    5
#define NV_KEY_BACKUP   6

#define UART_RX_PIN_NUMBER 	29
#define UART_TX_PIN_NUMBER 	28

#define BUTTON_PIN_NUMBER  16
#define BUTTON_ACTIVE_LEVEL     0

#define LED0_PIN_NUMBER    18
#define LED1_PIN_NUMBER    19
#define BUZZ_PIN_NUMBER    8

#define GPIOTE_CH_BUZZ     1

#define PPI_CH_BUZZ        1
#define PPI_CH_BUZZ_2      2

#define RTC_TIMER_UI 0

#endif
