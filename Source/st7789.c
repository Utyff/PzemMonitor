#include "OSAL.h"
#include "OnBoard.h"
#include "hal_assert.h"
#include "hal_types.h"
#include "st7789.h"

/**
  LCD pins

  // control pins
  P1.1 - LCD_RESET
  P1.2 - LCD_CS chip select
  P1.3 - LCD_DC Data / Command

  // spi
  P1.4 - SSN - not used
  P1.5 - CLK
  P1.6 - MOSI
  P1.7 - MISO - not used
*/

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 280

#define X_SHIFT 0
#define Y_SHIFT 20

#define ST7789_COLMOD  0x3A
#define ST7789_COLOR_MODE_16bit 0x55    //  RGB565 (16bit)
#define ST7789_ROTATION 2               //  use Normally on 240x240
#define ST7789_INVON   0x21
#define ST7789_SLPOUT  0x11
#define ST7789_NORON   0x13
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
/* Page Address Order ('0': Top to Bottom, '1': the opposite) */
#define ST7789_MADCTL_MY  0x80
/* Column Address Order ('0': Left to Right, '1': the opposite) */
#define ST7789_MADCTL_MX  0x40
/* Page/Column Order ('0' = Normal Mode, '1' = Reverse Mode) */
#define ST7789_MADCTL_MV  0x20
/* Line Address Order ('0' = LCD Refresh Top to Bottom, '1' = the opposite) */
#define ST7789_MADCTL_ML  0x10
/* RGB/BGR Order ('0' = RGB, '1' = BGR) */
#define ST7789_MADCTL_RGB 0x00

// LCD Control lines
#define LCD_RESET_PORT 1
#define LCD_RESET_PIN  1
#define LCD_CS_PORT 1
#define LCD_CS_PIN  2
#define LCD_MODE_PORT 1
#define LCD_MODE_PIN  3

// LCD SPI lines
#define LCD_CLK_PORT 1
#define LCD_CLK_PIN  5
#define LCD_MOSI_PORT 1
#define LCD_MOSI_PIN  6
#define LCD_MISO_PORT 1
#define LCD_MISO_PIN  7

// Defines for HW LCD

#define IO_SET(port, pin, val)        IO_SET_PREP(port, pin, val)
#define IO_SET_PREP(port, pin, val)   st( P##port##_##pin = val; )

#define CONFIG_IO_OUTPUT(port, pin, val)      CONFIG_IO_OUTPUT_PREP(port, pin, val)
#define CONFIG_IO_OUTPUT_PREP(port, pin, val) st( P##port##SEL &= ~BV(pin); \
                                                      P##port##_##pin = val; \
                                                      P##port##DIR |= BV(pin); )

#define CONFIG_IO_PERIPHERAL(port, pin)      CONFIG_IO_PERIPHERAL_PREP(port, pin)
#define CONFIG_IO_PERIPHERAL_PREP(port, pin) st( P##port##SEL |= BV(pin); )

// SPI interface control
#define LCD_CS_ACTIVE()     IO_SET(LCD_CS_PORT, LCD_CS_PIN, 0); // activate chip select
#define LCD_CS_DEACTIVE() {                                         \
  asm("NOP");                                                       \
  asm("NOP");                                                       \
  asm("NOP");                                                       \
  asm("NOP");                                                       \
  IO_SET(LCD_CS_PORT, LCD_CS_PIN, 1); /* deactivate chip select */  \
}

// macros for transmit byte
// clear the received and transmit byte status, write tx data to buffer, wait till transmit done
#define SPI_TX(byte)          { U1CSR &= ~(BV(2) | BV(1)); U1DBUF = byte; while(!(U1CSR & BV(1))); }

// macros for DC pin control
#define LCD_DATA_MODE()       IO_SET(LCD_MODE_PORT,  LCD_MODE_PIN,  1);
#define LCD_CONTROL_MODE()    IO_SET(LCD_MODE_PORT,  LCD_MODE_PIN,  0);

#define LCD_ACTIVATE_RESET()  IO_SET(LCD_RESET_PORT, LCD_RESET_PIN, 0);
#define LCD_RELEASE_RESET()   IO_SET(LCD_RESET_PORT, LCD_RESET_PIN, 1);

#define FONT_WIDTH 11
#define FONT_HEIGHT 18
extern const uint16 Font11x18[];

static void SPI_Config(void);

static void SPI_WriteCommand(uint8 cmd);

static void SPI_WriteData8(uint8 d8);

static void SPI_WriteData(const uint8 *data, uint8 len);

void LCD_WriteChar(uint16 x, uint16 y, char ch, uint16 color, uint16 bgcolor);

static void LCD_SetAddressWindow(uint16 x0, uint16 y0, uint16 x1, uint16 y1);

static void LCD_FillScreen(uint16 color);

/**
 *  Configure IO lines needed for LCD control.
 */
static void ST7789_ConfigIO(void) {
    // GPIO configuration
    CONFIG_IO_OUTPUT(LCD_MODE_PORT, LCD_MODE_PIN, 1);
    CONFIG_IO_OUTPUT(LCD_RESET_PORT, LCD_RESET_PIN, 1);
    CONFIG_IO_OUTPUT(LCD_CS_PORT, LCD_CS_PIN, 1);
}

/**
 * Configure SPI lines needed for talking to LCD.
 */
static void SPI_Config(void) {
    // Set SPI on UART 1 alternative 2
    PERCFG |= 0x02;

    /* Configure SPI pins: clk, master out and master in */
    CONFIG_IO_PERIPHERAL(LCD_CLK_PORT, LCD_CLK_PIN);
    CONFIG_IO_PERIPHERAL(LCD_MOSI_PORT, LCD_MOSI_PIN);
    CONFIG_IO_PERIPHERAL(LCD_MISO_PORT, LCD_MISO_PIN);

    // Set SPI speed to 8 MHz (the values assume system clk of 32MHz)
    // Confirm on board that this results in 8MHz spi clk.
    const uint8 baud_exponent = 18; // 15 - 1mhz, 16 - 2mhz, 17 - 4mhz, 18 - 8mhz
    const uint8 baud_mantissa = 0;

// SPI settings
#define SPI_CLOCK_POL_LO       0x00
#define SPI_CLOCK_PHA_0        0x00
#define SPI_TRANSFER_MSB_FIRST 0x20

    // Configure SPI
    U1UCR = 0x80;      // Flush and goto IDLE state. 8-N-1.
    U1CSR = 0x00;      // SPI mode, master.
    U1GCR = SPI_TRANSFER_MSB_FIRST | SPI_CLOCK_PHA_0 | SPI_CLOCK_POL_LO | baud_exponent;
    U1BAUD = baud_mantissa;
}

/**
 * Send 1 byte command
 */
static void SPI_WriteCommand(uint8 cmd) {
    LCD_CS_ACTIVE()
    LCD_CONTROL_MODE()
    SPI_TX(cmd)
    LCD_CS_DEACTIVE()
}

/**
 * Send 1 byte data
 */
static void SPI_WriteData8(uint8 d8) {
    LCD_CS_ACTIVE()
    LCD_DATA_MODE()
    SPI_TX(d8)
    LCD_CS_DEACTIVE()
}

/**
 * Send 2 bytes data
 */
static void SPI_WriteData16(uint16 d16) {
    LCD_CS_ACTIVE()
    LCD_DATA_MODE()
    uint8 low = d16 & 0xff;
    uint8 hi = d16 >> 8;
    SPI_TX(hi)    // hi-byte first
    SPI_TX(low)
    LCD_CS_DEACTIVE()
}

/**
 * Send array as data
 */
static void SPI_WriteData(const uint8 *data, uint8 len) {
    LCD_CS_ACTIVE()
    LCD_DATA_MODE()
    for (uint8 i = 0; i < len; i++) {
        SPI_TX(*data++)
    }
    LCD_CS_DEACTIVE()
}

/**
 * Initialize ST7789 screen and SPI
 */
void LCD_Init() {
    // Initialize LCD IO lines
    ST7789_ConfigIO();

    // Initialize SPI
    SPI_Config();

    // Perform reset
    LCD_ACTIVATE_RESET()
    HW_DelayUs(15000);  // 15 ms
    LCD_RELEASE_RESET()
    HW_DelayUs(10000);  // 10 ms

    // Init ST7789

    SPI_WriteCommand(ST7789_COLMOD);        // Set color mode
    SPI_WriteData8(ST7789_COLOR_MODE_16bit);
    SPI_WriteCommand(0xB2);               // Porch control
    {
        const uint8 data[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
        SPI_WriteData(data, sizeof(data));
    }

//    ST7789_SetRotation(ST7789_ROTATION);    // MADCTL (Display Rotation)
    SPI_WriteCommand(ST7789_MADCTL);    // MADCTL
//    case 0:
//        SPI_WriteData8(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
//    case 1:
//        SPI_WriteData8(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
//    case 2:
    SPI_WriteData8(ST7789_MADCTL_RGB);
//    case 3:
//        SPI_WriteData8(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);

    // Internal LCD Voltage generator settings
    SPI_WriteCommand(0XB7);          // Gate Control
    SPI_WriteData8(0x35);            // Default value
    SPI_WriteCommand(0xBB);          // VCOM setting
    SPI_WriteData8(0x19);            //	0.725v (default 0.75v for 0x20)
    SPI_WriteCommand(0xC0);          // LCMCTRL
    SPI_WriteData8(0x2C);            //	Default value
    SPI_WriteCommand(0xC2);          // VDV and VRH command Enable
    SPI_WriteData8(0x01);            //	Default value
    SPI_WriteCommand(0xC3);          // VRH set
    SPI_WriteData8(0x12);            //	+-4.45v (defalut +-4.1v for 0x0B)
    SPI_WriteCommand(0xC4);          // VDV set
    SPI_WriteData8(0x20);            //	Default value
    SPI_WriteCommand(0xC6);          // Frame rate control in normal mode
    SPI_WriteData8(0x0F);            // Default value (60HZ)
    SPI_WriteCommand(0xD0);          // Power control
    SPI_WriteData8(0xA4);            //	Default value
    SPI_WriteData8(0xA1);            //	Default value

    SPI_WriteCommand(0xE0);
    {
        const uint8 data[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
        SPI_WriteData(data, sizeof(data));
    }

    SPI_WriteCommand(0xE1);
    {
        const uint8 data[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
        SPI_WriteData(data, sizeof(data));
    }
    SPI_WriteCommand(ST7789_INVON);     // Inversion ON
    SPI_WriteCommand(ST7789_SLPOUT);    // Out of sleep mode
    SPI_WriteCommand(ST7789_NORON);     // Normal Display on
    SPI_WriteCommand(ST7789_DISPON);    // Main screen turned on

    HW_DelayUs(50000);
    LCD_FillScreen(BLUE);               // Clean screen.

    LCD_Print(25, 5, "PZEM Monitor v1.0", YELLOW, BLUE);
}

/**
 * Fill screen with color
 */
static void LCD_FillScreen(uint16 color) {
    uint16 i;
    uint16 j;
    uint8 low = color & 0xff;
    uint8 hi = color >> 8;

    LCD_SetAddressWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    LCD_CS_ACTIVE()
    LCD_DATA_MODE()
    for (i = 0; i < SCREEN_HEIGHT; i++) {
        for (j = 0; j < SCREEN_WIDTH; j++) {
            SPI_TX(hi)    // hi-byte first
            SPI_TX(low)
        }
    }
    LCD_CS_DEACTIVE()
}

/**
 * @brief Set address of DisplayWindow
 * @param xi and yi -> coordinates of window
 * @return none
 */
static void LCD_SetAddressWindow(uint16 x0, uint16 y0, uint16 x1, uint16 y1) {
    uint16 x_start = x0 + X_SHIFT;
    uint16 x_end = x1 + X_SHIFT;
    uint16 y_start = y0 + Y_SHIFT;
    uint16 y_end = y1 + Y_SHIFT;

    // Column Address set
    SPI_WriteCommand(ST7789_CASET);
    SPI_WriteData16(x_start);
    SPI_WriteData16(x_end);

    // Row Address set
    SPI_WriteCommand(ST7789_RASET);
    SPI_WriteData16(y_start);
    SPI_WriteData16(y_end);

    // Write to RAM
    SPI_WriteCommand(ST7789_RAMWR);
}

/**
 * @brief Write a char
 * @param  x&y -> cursor of the start point.
 * @param ch -> char to write
 * @param color -> color of the char
 * @param bgcolor -> background color of the char
 * @return  none
 */
void LCD_WriteChar(uint16 x, uint16 y, char ch, uint16 color, uint16 bgcolor) {
    uint16 i, j;
    uint16 b;
    LCD_SetAddressWindow(x, y, x + FONT_WIDTH - 1, y + FONT_HEIGHT - 1);

    LCD_CS_ACTIVE()
    LCD_DATA_MODE()
    for (i = 0; i < FONT_HEIGHT; i++) {
        b = Font11x18[(ch - 32) * FONT_HEIGHT + i];
        for (j = 0; j < FONT_WIDTH; j++) {
            if ((b << j) & 0x8000) {
                SPI_TX(color >> 8)    // hi-byte first
                SPI_TX(color & 0xFF)
            } else {
                SPI_TX(bgcolor >> 8)    // hi-byte first
                SPI_TX(bgcolor & 0xFF)
            }
        }
    }
    LCD_CS_DEACTIVE()
}

/** 
 * @brief Write a string 
 * @param  x&y -> cursor of the start point.
 * @param str -> string to write
 * @param color -> color of the string
 * @param bgcolor -> background color of the string
 * @return  none
 */
void LCD_Print(uint16 x, uint16 y, const char *str, uint16 color, uint16 bgcolor) {
    while (*str) {
        if (x + FONT_WIDTH >= SCREEN_WIDTH) {
            x = 0;
            y += FONT_HEIGHT;
            if (y + FONT_HEIGHT >= SCREEN_HEIGHT) {
                break;
            }
//            if (*str == ' ') {
//                // skip spaces in the beginning of the new line
//                str++;
//                continue;
//            }
        }
        LCD_WriteChar(x, y, *str, color, bgcolor);
        x += FONT_WIDTH;
        str++;
    }
}

/**
 * Wait for x us. @ 32MHz MCU clock it takes 32 "nop"s for 1 us delay.
 */
void HW_DelayUs(uint16 microSecs) {
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

const uint16 Font11x18 [] = {
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // sp
        0x0000, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000,   // !
        0x0000, 0x1B00, 0x1B00, 0x1B00, 0x1B00, 0x1B00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // "
        0x0000, 0x1980, 0x1980, 0x1980, 0x1980, 0x7FC0, 0x7FC0, 0x1980, 0x3300, 0x7FC0, 0x7FC0, 0x3300, 0x3300, 0x3300, 0x3300, 0x0000, 0x0000, 0x0000,   // #
        0x0000, 0x1E00, 0x3F00, 0x7580, 0x6580, 0x7400, 0x3C00, 0x1E00, 0x0700, 0x0580, 0x6580, 0x6580, 0x7580, 0x3F00, 0x1E00, 0x0400, 0x0400, 0x0000,   // $
        0x0000, 0x7000, 0xD800, 0xD840, 0xD8C0, 0xD980, 0x7300, 0x0600, 0x0C00, 0x1B80, 0x36C0, 0x66C0, 0x46C0, 0x06C0, 0x0380, 0x0000, 0x0000, 0x0000,   // %
        0x0000, 0x1E00, 0x3F00, 0x3300, 0x3300, 0x3300, 0x1E00, 0x0C00, 0x3CC0, 0x66C0, 0x6380, 0x6180, 0x6380, 0x3EC0, 0x1C80, 0x0000, 0x0000, 0x0000,   // &
        0x0000, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // '
        0x0080, 0x0100, 0x0300, 0x0600, 0x0600, 0x0400, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0400, 0x0600, 0x0600, 0x0300, 0x0100, 0x0080,   // (
        0x2000, 0x1000, 0x1800, 0x0C00, 0x0C00, 0x0400, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0400, 0x0C00, 0x0C00, 0x1800, 0x1000, 0x2000,   // )
        0x0000, 0x0C00, 0x2D00, 0x3F00, 0x1E00, 0x3300, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // *
        0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0xFFC0, 0xFFC0, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // +
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0400, 0x0400, 0x0800,   // ,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E00, 0x1E00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // -
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000,   // .
        0x0000, 0x0300, 0x0300, 0x0300, 0x0600, 0x0600, 0x0600, 0x0600, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x1800, 0x1800, 0x1800, 0x0000, 0x0000, 0x0000,   // /
        0x0000, 0x1E00, 0x3F00, 0x3300, 0x6180, 0x6180, 0x6180, 0x6D80, 0x6D80, 0x6180, 0x6180, 0x6180, 0x3300, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // 0
        0x0000, 0x0600, 0x0E00, 0x1E00, 0x3600, 0x2600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000,   // 1
        0x0000, 0x1E00, 0x3F00, 0x7380, 0x6180, 0x6180, 0x0180, 0x0300, 0x0600, 0x0C00, 0x1800, 0x3000, 0x6000, 0x7F80, 0x7F80, 0x0000, 0x0000, 0x0000,   // 2
        0x0000, 0x1C00, 0x3E00, 0x6300, 0x6300, 0x0300, 0x0E00, 0x0E00, 0x0300, 0x0180, 0x0180, 0x6180, 0x7380, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // 3
        0x0000, 0x0600, 0x0E00, 0x0E00, 0x1E00, 0x1E00, 0x1600, 0x3600, 0x3600, 0x6600, 0x7F80, 0x7F80, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000,   // 4
        0x0000, 0x7F00, 0x7F00, 0x6000, 0x6000, 0x6000, 0x6E00, 0x7F00, 0x6380, 0x0180, 0x0180, 0x6180, 0x7380, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // 5
        0x0000, 0x1E00, 0x3F00, 0x3380, 0x6180, 0x6000, 0x6E00, 0x7F00, 0x7380, 0x6180, 0x6180, 0x6180, 0x3380, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // 6
        0x0000, 0x7F80, 0x7F80, 0x0180, 0x0300, 0x0300, 0x0600, 0x0600, 0x0C00, 0x0C00, 0x0C00, 0x0800, 0x1800, 0x1800, 0x1800, 0x0000, 0x0000, 0x0000,   // 7
        0x0000, 0x1E00, 0x3F00, 0x6380, 0x6180, 0x6180, 0x2100, 0x1E00, 0x3F00, 0x6180, 0x6180, 0x6180, 0x6180, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // 8
        0x0000, 0x1E00, 0x3F00, 0x7300, 0x6180, 0x6180, 0x6180, 0x7380, 0x3F80, 0x1D80, 0x0180, 0x6180, 0x7300, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // 9
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000,   // :
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0400, 0x0400, 0x0800,   // ;
        0x0000, 0x0000, 0x0000, 0x0000, 0x0080, 0x0380, 0x0E00, 0x3800, 0x6000, 0x3800, 0x0E00, 0x0380, 0x0080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // <
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7F80, 0x7F80, 0x0000, 0x0000, 0x7F80, 0x7F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // =
        0x0000, 0x0000, 0x0000, 0x0000, 0x4000, 0x7000, 0x1C00, 0x0700, 0x0180, 0x0700, 0x1C00, 0x7000, 0x4000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // >
        0x0000, 0x1F00, 0x3F80, 0x71C0, 0x60C0, 0x00C0, 0x01C0, 0x0380, 0x0700, 0x0E00, 0x0C00, 0x0C00, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000,   // ?
        0x0000, 0x1E00, 0x3F00, 0x3180, 0x7180, 0x6380, 0x6F80, 0x6D80, 0x6D80, 0x6F80, 0x6780, 0x6000, 0x3200, 0x3E00, 0x1C00, 0x0000, 0x0000, 0x0000,   // @
        0x0000, 0x0E00, 0x0E00, 0x1B00, 0x1B00, 0x1B00, 0x1B00, 0x3180, 0x3180, 0x3F80, 0x3F80, 0x3180, 0x60C0, 0x60C0, 0x60C0, 0x0000, 0x0000, 0x0000,   // A
        0x0000, 0x7C00, 0x7E00, 0x6300, 0x6300, 0x6300, 0x6300, 0x7E00, 0x7E00, 0x6300, 0x6180, 0x6180, 0x6380, 0x7F00, 0x7E00, 0x0000, 0x0000, 0x0000,   // B
        0x0000, 0x1E00, 0x3F00, 0x3180, 0x6180, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6180, 0x3180, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // C
        0x0000, 0x7C00, 0x7F00, 0x6300, 0x6380, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6300, 0x6300, 0x7E00, 0x7C00, 0x0000, 0x0000, 0x0000,   // D
        0x0000, 0x7F80, 0x7F80, 0x6000, 0x6000, 0x6000, 0x6000, 0x7F00, 0x7F00, 0x6000, 0x6000, 0x6000, 0x6000, 0x7F80, 0x7F80, 0x0000, 0x0000, 0x0000,   // E
        0x0000, 0x7F80, 0x7F80, 0x6000, 0x6000, 0x6000, 0x6000, 0x7F00, 0x7F00, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x0000, 0x0000, 0x0000,   // F
        0x0000, 0x1E00, 0x3F00, 0x3180, 0x6180, 0x6000, 0x6000, 0x6000, 0x6380, 0x6380, 0x6180, 0x6180, 0x3180, 0x3F80, 0x1E00, 0x0000, 0x0000, 0x0000,   // G
        0x0000, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x7F80, 0x7F80, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x0000, 0x0000, 0x0000,   // H
        0x0000, 0x3F00, 0x3F00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x3F00, 0x3F00, 0x0000, 0x0000, 0x0000,   // I
        0x0000, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x6180, 0x6180, 0x7380, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // J
        0x0000, 0x60C0, 0x6180, 0x6300, 0x6600, 0x6600, 0x6C00, 0x7800, 0x7C00, 0x6600, 0x6600, 0x6300, 0x6180, 0x6180, 0x60C0, 0x0000, 0x0000, 0x0000,   // K
        0x0000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x7F80, 0x7F80, 0x0000, 0x0000, 0x0000,   // L
        0x0000, 0x71C0, 0x71C0, 0x7BC0, 0x7AC0, 0x6AC0, 0x6AC0, 0x6EC0, 0x64C0, 0x60C0, 0x60C0, 0x60C0, 0x60C0, 0x60C0, 0x60C0, 0x0000, 0x0000, 0x0000,   // M
        0x0000, 0x7180, 0x7180, 0x7980, 0x7980, 0x7980, 0x6D80, 0x6D80, 0x6D80, 0x6580, 0x6780, 0x6780, 0x6780, 0x6380, 0x6380, 0x0000, 0x0000, 0x0000,   // N
        0x0000, 0x1E00, 0x3F00, 0x3300, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x3300, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // O
        0x0000, 0x7E00, 0x7F00, 0x6380, 0x6180, 0x6180, 0x6180, 0x6380, 0x7F00, 0x7E00, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x0000, 0x0000, 0x0000,   // P
        0x0000, 0x1E00, 0x3F00, 0x3300, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6580, 0x6780, 0x3300, 0x3F80, 0x1E40, 0x0000, 0x0000, 0x0000,   // Q
        0x0000, 0x7E00, 0x7F00, 0x6380, 0x6180, 0x6180, 0x6380, 0x7F00, 0x7E00, 0x6600, 0x6300, 0x6300, 0x6180, 0x6180, 0x60C0, 0x0000, 0x0000, 0x0000,   // R
        0x0000, 0x0E00, 0x1F00, 0x3180, 0x3180, 0x3000, 0x3800, 0x1E00, 0x0700, 0x0380, 0x6180, 0x6180, 0x3180, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // S
        0x0000, 0xFFC0, 0xFFC0, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000,   // T
        0x0000, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x7380, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // U
        0x0000, 0x60C0, 0x60C0, 0x60C0, 0x3180, 0x3180, 0x3180, 0x1B00, 0x1B00, 0x1B00, 0x1B00, 0x0E00, 0x0E00, 0x0E00, 0x0400, 0x0000, 0x0000, 0x0000,   // V
        0x0000, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xCCC0, 0x4C80, 0x4C80, 0x5E80, 0x5280, 0x5280, 0x7380, 0x6180, 0x6180, 0x0000, 0x0000, 0x0000,   // W
        0x0000, 0xC0C0, 0x6080, 0x6180, 0x3300, 0x3B00, 0x1E00, 0x0C00, 0x0C00, 0x1E00, 0x1F00, 0x3B00, 0x7180, 0x6180, 0xC0C0, 0x0000, 0x0000, 0x0000,   // X
        0x0000, 0xC0C0, 0x6180, 0x6180, 0x3300, 0x3300, 0x1E00, 0x1E00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000,   // Y
        0x0000, 0x3F80, 0x3F80, 0x0180, 0x0300, 0x0300, 0x0600, 0x0C00, 0x0C00, 0x1800, 0x1800, 0x3000, 0x6000, 0x7F80, 0x7F80, 0x0000, 0x0000, 0x0000,   // Z
        0x0F00, 0x0F00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0F00, 0x0F00,   // [
        0x0000, 0x1800, 0x1800, 0x1800, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0300, 0x0300, 0x0300, 0x0000, 0x0000, 0x0000,   /* \ */
        0x1E00, 0x1E00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x1E00, 0x1E00,   // ]
        0x0000, 0x0C00, 0x0C00, 0x1E00, 0x1200, 0x3300, 0x3300, 0x6180, 0x6180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // ^
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0, 0x0000,   // _
        0x0000, 0x3800, 0x1800, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // `
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x3F80, 0x6180, 0x0180, 0x1F80, 0x3F80, 0x6180, 0x6380, 0x7F80, 0x38C0, 0x0000, 0x0000, 0x0000,   // a
        0x0000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6E00, 0x7F00, 0x7380, 0x6180, 0x6180, 0x6180, 0x6180, 0x7380, 0x7F00, 0x6E00, 0x0000, 0x0000, 0x0000,   // b
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E00, 0x3F00, 0x7380, 0x6180, 0x6000, 0x6000, 0x6180, 0x7380, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // c
        0x0000, 0x0180, 0x0180, 0x0180, 0x0180, 0x1D80, 0x3F80, 0x7380, 0x6180, 0x6180, 0x6180, 0x6180, 0x7380, 0x3F80, 0x1D80, 0x0000, 0x0000, 0x0000,   // d
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E00, 0x3F00, 0x7300, 0x6180, 0x7F80, 0x7F80, 0x6000, 0x7180, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // e
        0x0000, 0x07C0, 0x0FC0, 0x0C00, 0x0C00, 0x7F80, 0x7F80, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000,   // f
        0x0000, 0x0000, 0x0000, 0x0000, 0x1D80, 0x3F80, 0x7380, 0x6180, 0x6180, 0x6180, 0x6180, 0x7380, 0x3F80, 0x1D80, 0x0180, 0x6380, 0x7F00, 0x3E00,   // g
        0x0000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6F00, 0x7F80, 0x7180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x0000, 0x0000, 0x0000,   // h
        0x0000, 0x0600, 0x0600, 0x0000, 0x0000, 0x3E00, 0x3E00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000,   // i
        0x0600, 0x0600, 0x0000, 0x0000, 0x3E00, 0x3E00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x4600, 0x7E00, 0x3C00,   // j
        0x0000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6180, 0x6300, 0x6600, 0x6C00, 0x7C00, 0x7600, 0x6300, 0x6300, 0x6180, 0x60C0, 0x0000, 0x0000, 0x0000,   // k
        0x0000, 0x3E00, 0x3E00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000,   // l
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xDD80, 0xFFC0, 0xCEC0, 0xCCC0, 0xCCC0, 0xCCC0, 0xCCC0, 0xCCC0, 0xCCC0, 0xCCC0, 0x0000, 0x0000, 0x0000,   // m
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6F00, 0x7F80, 0x7180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x0000, 0x0000, 0x0000,   // n
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E00, 0x3F00, 0x7380, 0x6180, 0x6180, 0x6180, 0x6180, 0x7380, 0x3F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // o
        0x0000, 0x0000, 0x0000, 0x0000, 0x6E00, 0x7F00, 0x7380, 0x6180, 0x6180, 0x6180, 0x6180, 0x7380, 0x7F00, 0x6E00, 0x6000, 0x6000, 0x6000, 0x6000,   // p
        0x0000, 0x0000, 0x0000, 0x0000, 0x1D80, 0x3F80, 0x7380, 0x6180, 0x6180, 0x6180, 0x6180, 0x7380, 0x3F80, 0x1D80, 0x0180, 0x0180, 0x0180, 0x0180,   // q
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6700, 0x3F80, 0x3900, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x0000, 0x0000, 0x0000,   // r
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1E00, 0x3F80, 0x6180, 0x6000, 0x7F00, 0x3F80, 0x0180, 0x6180, 0x7F00, 0x1E00, 0x0000, 0x0000, 0x0000,   // s
        0x0000, 0x0000, 0x0800, 0x1800, 0x1800, 0x7F00, 0x7F00, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1F80, 0x0F80, 0x0000, 0x0000, 0x0000,   // t
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6180, 0x6380, 0x7F80, 0x3D80, 0x0000, 0x0000, 0x0000,   // u
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x60C0, 0x3180, 0x3180, 0x3180, 0x1B00, 0x1B00, 0x1B00, 0x0E00, 0x0E00, 0x0600, 0x0000, 0x0000, 0x0000,   // v
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xDD80, 0xDD80, 0xDD80, 0x5500, 0x5500, 0x5500, 0x7700, 0x7700, 0x2200, 0x2200, 0x0000, 0x0000, 0x0000,   // w
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6180, 0x3300, 0x3300, 0x1E00, 0x0C00, 0x0C00, 0x1E00, 0x3300, 0x3300, 0x6180, 0x0000, 0x0000, 0x0000,   // x
        0x0000, 0x0000, 0x0000, 0x0000, 0x6180, 0x6180, 0x3180, 0x3300, 0x3300, 0x1B00, 0x1B00, 0x1B00, 0x0E00, 0x0E00, 0x0E00, 0x1C00, 0x7C00, 0x7000,   // y
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7FC0, 0x7FC0, 0x0180, 0x0300, 0x0600, 0x0C00, 0x1800, 0x3000, 0x7FC0, 0x7FC0, 0x0000, 0x0000, 0x0000,   // z
        0x0380, 0x0780, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0E00, 0x1C00, 0x1C00, 0x0E00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0780, 0x0380,   // {
        0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600,   // |
        0x3800, 0x3C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0E00, 0x0700, 0x0700, 0x0E00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x3C00, 0x3800,   // }
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3880, 0x7F80, 0x4700, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,   // ~
};