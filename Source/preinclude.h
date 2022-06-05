#define SECURE 1
#define TC_LINKKEY_JOIN
#define NV_INIT
#define NV_RESTORE
#define MULTICAST_ENABLED FALSE
#define ZCL_READ
#define ZCL_WRITE
#define ZCL_BASIC
#define ZCL_IDENTIFY
#define ZCL_ON_OFF
#define ZCL_REPORTING_DEVICE

#define BDB_REPORTING TRUE
#define DISABLE_GREENPOWER_BASIC_PROXY
#define DEFAULT_CHANLIST 0x07FFF800  // Маска для работы на всех каналах

#define HAL_LED TRUE
#define HAL_ADC FALSE
#define HAL_LCD FALSE
#define BLINK_LEDS TRUE

// Button - P2_0
#define KEY1_PORT HAL_KEY_PORT2
#define HAL_KEY_P2_INPUT_PINS BV(0)
#define HAL_KEY_P2_INPUT_PINS_EDGE HAL_KEY_FALLING_EDGE

#define BTN_HOLD_TIME 2000
#define BTN_DBL_CLICK_TIME 250

// choose one debug option. UART port number depend on it
#define DEBUG_PZEM_UART
//#define DEBUG_LCD_UART

// UART for PZEM communication
#define PZEM_UART_PORT HAL_UART_PORT_0

#ifdef DEBUG_PZEM_UART
#ifdef DEBUG_LCD_UART
#error "Supported only one debug mode DEBUG_PZEM_UART or DEBUG_LCD_UART"
#endif
// UART0 used for PZEM communications, UART1 used for debug messages
#define DO_DEBUG_UART
#define UART_PORT HAL_UART_PORT_1
#endif

#ifdef DEBUG_LCD_UART
#ifdef DEBUG_PZEM_UART
#error "Supported only one debug mode DEBUG_PZEM_UART or DEBUG_LCD_UART"
#endif
// UART0 used for debug messages, UART1 used for SPI LCD
#define DO_DEBUG_UART
#define UART_PORT HAL_UART_PORT_0
#endif

#define HAL_UART TRUE
#define HAL_UART_DMA 1
#define HAL_UART_ISR 2

#define INT_HEAP_LEN 2060

#include "hal_board_cfg.h"
