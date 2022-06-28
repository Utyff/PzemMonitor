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

extern const uint8 SmallFont[];

static void SPI_Config(void);

static void SPI_WriteCommand(uint8 cmd);

static void SPI_WriteData8(uint8 d8);

static void SPI_WriteData(const uint8 *data, uint8 len);

static void ST7789_SetAddressWindow(uint16 x0, uint16 y0, uint16 x1, uint16 y1);

static void ST7789_FillScreen(uint16 color);

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

    // Set SPI speed to 1 MHz (the values assume system clk of 32MHz)
    // Confirm on board that this results in 1MHz spi clk.
    const uint8 baud_exponent = 15;
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
    ST7789_FillScreen(BLUE);           // Fill with Black.

    LCD_Print(2, 0, "PZEM Monitor v1.0");
}

/**
 * Fill screen black
 */
static void ST7789_FillScreen(uint16 color) {
    uint16 i;
    ST7789_SetAddressWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    LCD_CS_ACTIVE()
    LCD_DATA_MODE()

    uint16 j;
    uint16 ii = 0;
    for (i = 0; i < SCREEN_HEIGHT; i++) {
        ii++;
        switch ((ii >> 3) % 3) {
            case 0:
                color = BLUE;
                break;
            case 1:
                color = GREEN;
                break;
            case 2:
                color = RED;
                break;
        }
        uint8 low = color & 0xff;
        uint8 hi = color >> 8;
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
static void ST7789_SetAddressWindow(uint16 x0, uint16 y0, uint16 x1, uint16 y1) {
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
 * Print string to screen
 * @param x column of first char
 * @param y row of first char
 * @param str string for print. 0x00 at the string end
 */
void LCD_Print(uint8 x, uint8 y, const char *str) {
    const uint8 fontWidth = 6;

    LCD_CS_ACTIVE()
    LCD_DATA_MODE()

    LCD_CS_DEACTIVE()
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

// Ўрифт SmallFont
const uint8 SmallFont[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 001) 0x20=032 пробел
        0x00, 0x00, 0x00, 0x2F, 0x00, 0x00,   // 002) 0x21=033 !
        0x00, 0x00, 0x07, 0x00, 0x07, 0x00,   // 003) 0x22=034 "
        0x00, 0x14, 0x7F, 0x14, 0x7F, 0x14,   // 004) 0x23=035 #
        0x00, 0x24, 0x2A, 0x7F, 0x2A, 0x12,   // 005) 0x24=036 $
        0x00, 0x23, 0x13, 0x08, 0x64, 0x62,   // 006) 0x25=037 %
        0x00, 0x36, 0x49, 0x55, 0x22, 0x50,   // 007) 0x26=038 &
        0x00, 0x00, 0x05, 0x03, 0x00, 0x00,   // 008) 0x27=039 '
        0x00, 0x00, 0x1C, 0x22, 0x41, 0x00,   // 009) 0x28=040 (
        0x00, 0x00, 0x41, 0x22, 0x1C, 0x00,   // 010) 0x29=041 )
        0x00, 0x14, 0x08, 0x3E, 0x08, 0x14,   // 011) 0x2A=042 *
        0x00, 0x08, 0x08, 0x3E, 0x08, 0x08,   // 012) 0x2B=043 +
        0x00, 0x00, 0x00, 0xA0, 0x60, 0x00,   // 013) 0x2C=044 ,
        0x00, 0x08, 0x08, 0x08, 0x08, 0x08,   // 014) 0x2D=045 -
        0x00, 0x00, 0x60, 0x60, 0x00, 0x00,   // 015) 0x2E=046 .
        0x00, 0x20, 0x10, 0x08, 0x04, 0x02,   // 016) 0x2F=047 /

        0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E,   // 017) 0x30=048 0
        0x00, 0x00, 0x42, 0x7F, 0x40, 0x00,   // 018) 0x31=049 1
        0x00, 0x42, 0x61, 0x51, 0x49, 0x46,   // 019) 0x32=050 2
        0x00, 0x21, 0x41, 0x45, 0x4B, 0x31,   // 020) 0x33=051 3
        0x00, 0x18, 0x14, 0x12, 0x7F, 0x10,   // 021) 0x34=052 4
        0x00, 0x27, 0x45, 0x45, 0x45, 0x39,   // 022) 0x35=053 5
        0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30,   // 023) 0x36=054 6
        0x00, 0x01, 0x71, 0x09, 0x05, 0x03,   // 024) 0x37=055 7
        0x00, 0x36, 0x49, 0x49, 0x49, 0x36,   // 025) 0x38=056 8
        0x00, 0x06, 0x49, 0x49, 0x29, 0x1E,   // 026) 0x39=057 9
        0x00, 0x00, 0x36, 0x36, 0x00, 0x00,   // 027) 0x3A=058 :
        0x00, 0x00, 0x56, 0x36, 0x00, 0x00,   // 028) 0x3B=059 ;
        0x00, 0x08, 0x14, 0x22, 0x41, 0x00,   // 029) 0x3C=060 <
        0x00, 0x14, 0x14, 0x14, 0x14, 0x14,   // 030) 0x3D=061 =
        0x00, 0x00, 0x41, 0x22, 0x14, 0x08,   // 031) 0x3E=062 >
        0x00, 0x02, 0x01, 0x51, 0x09, 0x06,   // 032) 0x3F=063 ?

        0x00, 0x32, 0x49, 0x59, 0x51, 0x3E,   // 033) 0x40=064 @
        0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C,   // 034) 0x41=065 A
        0x00, 0x7F, 0x49, 0x49, 0x49, 0x36,   // 035) 0x42=066 B
        0x00, 0x3E, 0x41, 0x41, 0x41, 0x22,   // 036) 0x43=067 C
        0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C,   // 037) 0x44=068 D
        0x00, 0x7F, 0x49, 0x49, 0x49, 0x41,   // 038) 0x45=069 E
        0x00, 0x7F, 0x09, 0x09, 0x09, 0x01,   // 039) 0x46=070 F
        0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A,   // 040) 0x47=071 G
        0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F,   // 041) 0x48=072 H
        0x00, 0x00, 0x41, 0x7F, 0x41, 0x00,   // 042) 0x49=073 I
        0x00, 0x20, 0x40, 0x41, 0x3F, 0x01,   // 043) 0x4A=074 J
        0x00, 0x7F, 0x08, 0x14, 0x22, 0x41,   // 044) 0x4B=075 K
        0x00, 0x7F, 0x40, 0x40, 0x40, 0x40,   // 045) 0x4C=076 L
        0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F,   // 046) 0x4D=077 M
        0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F,   // 047) 0x4E=078 N
        0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E,   // 048) 0x4F=079 O

        0x00, 0x7F, 0x09, 0x09, 0x09, 0x06,   // 049) 0x50=080 P
        0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E,   // 050) 0x51=081 Q
        0x00, 0x7F, 0x09, 0x19, 0x29, 0x46,   // 051) 0x52=082 R
        0x00, 0x46, 0x49, 0x49, 0x49, 0x31,   // 052) 0x53=083 S
        0x00, 0x01, 0x01, 0x7F, 0x01, 0x01,   // 053) 0x54=084 T
        0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F,   // 054) 0x55=085 U
        0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F,   // 055) 0x56=086 V
        0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F,   // 056) 0x57=087 W
        0x00, 0x63, 0x14, 0x08, 0x14, 0x63,   // 057) 0x58=088 X
        0x00, 0x07, 0x08, 0x70, 0x08, 0x07,   // 058) 0x59=089 Y
        0x00, 0x61, 0x51, 0x49, 0x45, 0x43,   // 059) 0x5A=090 Z
        0x00, 0x00, 0x7F, 0x41, 0x41, 0x00,   // 060) 0x5B=091 [
        0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,   // 061) 0x5C=092 обратный слеш
        0x00, 0x00, 0x41, 0x41, 0x7F, 0x00,   // 062) 0x5D=093 ]
        0x00, 0x04, 0x02, 0x01, 0x02, 0x04,   // 063) 0x5E=094 ^
        0x00, 0x40, 0x40, 0x40, 0x40, 0x40,   // 064) 0x5F=095 _

        0x00, 0x00, 0x03, 0x05, 0x00, 0x00,   // 065) 0x60=096 `
        0x00, 0x20, 0x54, 0x54, 0x54, 0x78,   // 066) 0x61=097 a
        0x00, 0x7F, 0x28, 0x44, 0x44, 0x38,   // 067) 0x62=098 b
        0x00, 0x38, 0x44, 0x44, 0x44, 0x20,   // 068) 0x63=099 c
        0x00, 0x38, 0x44, 0x44, 0x48, 0x7F,   // 069) 0x64=100 d
        0x00, 0x38, 0x54, 0x54, 0x54, 0x18,   // 070) 0x65=101 e
        0x00, 0x08, 0x7E, 0x09, 0x01, 0x02,   // 071) 0x66=102 f
        0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C,   // 072) 0x67=103 g
        0x00, 0x7F, 0x08, 0x04, 0x04, 0x78,   // 073) 0x68=104 h
        0x00, 0x00, 0x44, 0x7D, 0x40, 0x00,   // 074) 0x69=105 i
        0x00, 0x40, 0x80, 0x84, 0x7D, 0x00,   // 075) 0x6A=106 j
        0x00, 0x7F, 0x10, 0x28, 0x44, 0x00,   // 076) 0x6B=107 k
        0x00, 0x00, 0x41, 0x7F, 0x40, 0x00,   // 077) 0x6C=108 l
        0x00, 0x7C, 0x04, 0x18, 0x04, 0x78,   // 078) 0x6D=109 m
        0x00, 0x7C, 0x08, 0x04, 0x04, 0x78,   // 079) 0x6E=110 n
        0x00, 0x38, 0x44, 0x44, 0x44, 0x38,   // 080) 0x6F=111 o

        0x00, 0xFC, 0x24, 0x24, 0x24, 0x18,   // 081  0x70=112 p
        0x00, 0x18, 0x24, 0x24, 0x18, 0xFC,   // 082  0x71=113 q
        0x00, 0x7C, 0x08, 0x04, 0x04, 0x08,   // 083  0x72=114 r
        0x00, 0x48, 0x54, 0x54, 0x54, 0x20,   // 084  0x73=115 s
        0x00, 0x04, 0x3F, 0x44, 0x40, 0x20,   // 085  0x74=116 t
        0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C,   // 086  0x75=117 u
        0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C,   // 087  0x76=118 v
        0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C,   // 088  0x77=119 w
        0x00, 0x44, 0x28, 0x10, 0x28, 0x44,   // 089  0x78=120 x
        0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C,   // 090  0x79=121 y
        0x00, 0x44, 0x64, 0x54, 0x4C, 0x44,   // 091  0x7A=122 z
        0x00, 0x00, 0x10, 0x7C, 0x82, 0x00,   // 092  0x7B=123 {
        0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,   // 093  0x7C=124 |
        0x00, 0x00, 0x82, 0x7C, 0x10, 0x00,   // 094  0x7D=125 }
        0x00, 0x00, 0x06, 0x09, 0x09, 0x06    // 095  0x7E=126 ~
};
