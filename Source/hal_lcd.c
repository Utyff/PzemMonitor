#include "hal_types.h"
#include "hal_lcd.h"
#include "OSAL.h"
#include "OnBoard.h"
#include "hal_assert.h"

#define HAL_LCD FALSE

/**************************************************************************************************
 *                                          CONSTANTS
 **************************************************************************************************/
/*
  LCD pins

  //control
  P1.3 - LCD_MODE Data / Command
  P1.1 - LCD_FLASH_RESET
  P1.2 - LCD_CS

  //spi
  P1.5 - CLK
  P1.6 - MOSI
  P1.7 - MISO
*/

/* LCD Control lines */
#define HAL_LCD_MODE_PORT 1
#define HAL_LCD_MODE_PIN  3

#define HAL_LCD_RESET_PORT 1
#define HAL_LCD_RESET_PIN  1

#define HAL_LCD_CS_PORT 1
#define HAL_LCD_CS_PIN  2

/* LCD SPI lines */
#define HAL_LCD_CLK_PORT 1
#define HAL_LCD_CLK_PIN  5

#define HAL_LCD_MOSI_PORT 1
#define HAL_LCD_MOSI_PIN  6

#define HAL_LCD_MISO_PORT 1
#define HAL_LCD_MISO_PIN  7

/* SPI settings */
#define HAL_SPI_CLOCK_POL_LO       0x00
#define HAL_SPI_CLOCK_PHA_0        0x00
#define HAL_SPI_TRANSFER_MSB_FIRST 0x20

/* Defines for HW LCD */


/**************************************************************************************************
 *                                           MACROS
 **************************************************************************************************/

#define HAL_IO_SET(port, pin, val)        HAL_IO_SET_PREP(port, pin, val)
#define HAL_IO_SET_PREP(port, pin, val)   st( P##port##_##pin = val; )

#define HAL_CONFIG_IO_OUTPUT(port, pin, val)      HAL_CONFIG_IO_OUTPUT_PREP(port, pin, val)
#define HAL_CONFIG_IO_OUTPUT_PREP(port, pin, val) st( P##port##SEL &= ~BV(pin); \
                                                      P##port##_##pin = val; \
                                                      P##port##DIR |= BV(pin); )

#define HAL_CONFIG_IO_PERIPHERAL(port, pin)      HAL_CONFIG_IO_PERIPHERAL_PREP(port, pin)
#define HAL_CONFIG_IO_PERIPHERAL_PREP(port, pin) st( P##port##SEL |= BV(pin); )


/* SPI interface control */
#define LCD_SPI_BEGIN()     HAL_IO_SET(HAL_LCD_CS_PORT,  HAL_LCD_CS_PIN,  0); /* chip select */
#define LCD_SPI_END()                                                         \
{                                                                             \
  asm("NOP");                                                                 \
  asm("NOP");                                                                 \
  asm("NOP");                                                                 \
  asm("NOP");                                                                 \
  HAL_IO_SET(HAL_LCD_CS_PORT,  HAL_LCD_CS_PIN,  1); /* chip select */         \
}
/* clear the received and transmit byte status, write tx data to buffer, wait till transmit done */
#define LCD_SPI_TX(x)                   { U1CSR &= ~(BV(2) | BV(1)); U1DBUF = x; while( !(U1CSR & BV(1)) ); }
#define LCD_SPI_WAIT_RXRDY()            { while(!(U1CSR & BV(1))); }


/* Control macros */
#define LCD_DO_WRITE()        HAL_IO_SET(HAL_LCD_MODE_PORT,  HAL_LCD_MODE_PIN,  1);
#define LCD_DO_CONTROL()      HAL_IO_SET(HAL_LCD_MODE_PORT,  HAL_LCD_MODE_PIN,  0);

#define LCD_ACTIVATE_RESET()  HAL_IO_SET(HAL_LCD_RESET_PORT, HAL_LCD_RESET_PIN, 0);
#define LCD_RELEASE_RESET()   HAL_IO_SET(HAL_LCD_RESET_PORT, HAL_LCD_RESET_PIN, 1);

/**************************************************************************************************
 *                                       LOCAL VARIABLES
 **************************************************************************************************/

static uint8 *Lcd_Line1;
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
static uint8 SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/**************************************************************************************************
 *                                       FUNCTIONS - API
 **************************************************************************************************/

void HalLcd_HW_Init(void);

void HalLcd_HW_WaitUs(uint16 i);

void HalLcd_HW_ClearAllSpecChars(void);

void HalLcd_HW_Control(uint8 cmd);

void HalLcd_HW_Write(uint8 data);

void HalLcd_HW_SetContrast(uint8 value);

void HalLcd_HW_WriteChar(uint8 line, uint8 col, char text);

void HalLcd_HW_WriteLine(uint8 line, const char *pText);


/**************************************************************************************************
 * @fn      HalLcdInit
 *
 * @brief   Initilize LCD Service
 *
 * @param   init - pointer to void that contains the initialized value
 *
 * @return  None
 **************************************************************************************************/
void HalLcdInit(void) {
    Lcd_Line1 = NULL;
    HalLcd_HW_Init();
}

/**************************************************************************************************
 *                                    HARDWARE LCD
 **************************************************************************************************/

/**************************************************************************************************
 * @fn      halLcd_ConfigIO
 *
 * @brief   Configure IO lines needed for LCD control.
 *
 * @param   None
 *
 * @return  None
 **************************************************************************************************/
static void halLcd_ConfigIO(void) {
    /* GPIO configuration */
    HAL_CONFIG_IO_OUTPUT(HAL_LCD_MODE_PORT, HAL_LCD_MODE_PIN, 1);
    HAL_CONFIG_IO_OUTPUT(HAL_LCD_RESET_PORT, HAL_LCD_RESET_PIN, 1);
    HAL_CONFIG_IO_OUTPUT(HAL_LCD_CS_PORT, HAL_LCD_CS_PIN, 1);
}

/**************************************************************************************************
 * @fn      halLcd_ConfigSPI
 *
 * @brief   Configure SPI lines needed for talking to LCD.
 *
 * @param   None
 *
 * @return  None
 **************************************************************************************************/
static void halLcd_ConfigSPI(void) {
    /* UART/SPI Peripheral configuration */

    uint8 baud_exponent;
    uint8 baud_mantissa;

    /* Set SPI on UART 1 alternative 2 */
    PERCFG |= 0x02;

    /* Configure clk, master out and master in lines */
    HAL_CONFIG_IO_PERIPHERAL(HAL_LCD_CLK_PORT, HAL_LCD_CLK_PIN);
    HAL_CONFIG_IO_PERIPHERAL(HAL_LCD_MOSI_PORT, HAL_LCD_MOSI_PIN);
    HAL_CONFIG_IO_PERIPHERAL(HAL_LCD_MISO_PORT, HAL_LCD_MISO_PIN);


    /* Set SPI speed to 1 MHz (the values assume system clk of 32MHz)
     * Confirm on board that this results in 1MHz spi clk.
     */
    baud_exponent = 15;
    baud_mantissa = 0;

    /* Configure SPI */
    U1UCR = 0x80;      /* Flush and goto IDLE state. 8-N-1. */
    U1CSR = 0x00;      /* SPI mode, master. */
    U1GCR = HAL_SPI_TRANSFER_MSB_FIRST | HAL_SPI_CLOCK_PHA_0 | HAL_SPI_CLOCK_POL_LO | baud_exponent;
    U1BAUD = baud_mantissa;
}

/**************************************************************************************************
 * @fn      HalLcd_HW_Init
 *
 * @brief   Initilize HW LCD Driver.
 *
 * @param   None
 *
 * @return  None
 **************************************************************************************************/
void HalLcd_HW_Init(void) {
    /* Initialize LCD IO lines */
    halLcd_ConfigIO();

    /* Initialize SPI */
    halLcd_ConfigSPI();

    /* Perform reset */
    LCD_ACTIVATE_RESET();
    HalLcd_HW_WaitUs(15000); // 15 ms
    LCD_RELEASE_RESET();
    HalLcd_HW_WaitUs(15); // 15 us
}

/**************************************************************************************************
 * @fn      HalLcd_HW_Control
 *
 * @brief   Write 1 command to the LCD
 *
 * @param   uint8 cmd - command to be written to the LCD
 *
 * @return  None
 **************************************************************************************************/
void HalLcd_HW_Control(uint8 cmd) {
    LCD_SPI_BEGIN();
    LCD_DO_CONTROL();
    LCD_SPI_TX(cmd);
    LCD_SPI_WAIT_RXRDY();
    LCD_SPI_END();
}

/**************************************************************************************************
 * @fn      HalLcd_HW_Write
 *
 * @brief   Write 1 byte to the LCD
 *
 * @param   uint8 data - data to be written to the LCD
 *
 * @return  None
 **************************************************************************************************/
void HalLcd_HW_Write(uint8 data) {
    LCD_SPI_BEGIN();
    LCD_DO_WRITE();
    LCD_SPI_TX(data);
    LCD_SPI_WAIT_RXRDY();
    LCD_SPI_END();
}

/**************************************************************************************************
 * @fn      HalLcd_HW_WaitUs
 *
 * @brief   wait for x us. @ 32MHz MCU clock it takes 32 "nop"s for 1 us delay.
 *
 * @param   x us. range[0-65536]
 *
 * @return  None
 **************************************************************************************************/
void HalLcd_HW_WaitUs(uint16 microSecs) {
    while (microSecs--) {
        /* 32 NOPs == 1 usecs */
        asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
        asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
        asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
        asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
        asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
        asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
        asm("nop"); asm("nop");
    }
}
