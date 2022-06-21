#include "pzem.h"
#include "Debug.h"
#include "hal_uart.h"

//#define REG_VOLTAGE     0x0000
//#define REG_CURRENT_L   0x0001
//#define REG_CURRENT_H   0X0002
//#define REG_POWER_L     0x0003
//#define REG_POWER_H     0x0004
//#define REG_ENERGY_L    0x0005
//#define REG_ENERGY_H    0x0006
//#define REG_FREQUENCY   0x0007
//#define REG_PF          0x0008
//#define REG_ALARM       0x0009

//#define CMD_RHR         0x03
#define CMD_RIR         0X04
//#define CMD_WSR         0x06
//#define CMD_CAL         0x41
//#define CMD_REST        0x42

//#define WREG_ALARM_THR   0x0001
//#define WREG_ADDR        0x0002

#define PZEM_ERROR_ANSWER_CODE 0x84
#define PZEM_ERROR_ANSWER_SIZE 5
#define PZEM_RESPONSE_LENGTH 25


static uint16 CRC16(uint8 *nData, uint16 wLength);

static void setCRC(uint8 *buf, uint16 len);

static bool checkCRC(uint8 *buf, uint16 len);

static void Pzem_uartCB(uint8 port, uint8 event);


PzemState_t pzemRequestState;
//static const uint8 readCmd1[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0xD0}; // request address 1
static uint8 response[PZEM_RESPONSE_LENGTH];
static uint8 responseCounter;


static uint16 CRC16(uint8 *nData, uint16 wLength) {
    static const uint16 wCRCTable[] = {
            0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
            0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
            0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
            0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
            0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
            0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
            0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
            0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
            0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
            0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
            0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
            0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
            0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
            0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
            0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
            0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
            0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
            0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
            0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
            0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
            0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
            0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
            0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
            0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
            0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
            0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
            0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
            0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
            0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
            0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
            0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
            0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
    };

    uint8 nTemp;
    uint16 wCRCWord = 0xFFFF;

    while (wLength--) {
        nTemp = *nData++ ^ wCRCWord;
        wCRCWord >>= 8;
        wCRCWord ^= wCRCTable[nTemp];
    }
    return wCRCWord;
}

static void setCRC(uint8 *buf, uint16 len) {
    uint16 crc = CRC16(buf, len - 2); // CRC of data

    // Write high and low byte to last two positions
    buf[len - 2] = crc & 0xFF; // Low byte first
    buf[len - 1] = (crc >> 8) & 0xFF; // High byte second
}

/**
 * Check CRC for buf
 * @param buf duffer with data
 * @param len data length
 * @return TRUE if CRC correct, otherwise FALSE
 */
static bool checkCRC(uint8 *buf, uint16 len) {
    uint16 crc = CRC16(buf, len - 2); // CRC of data

    // compare high and low byte to last two positions
    if (buf[len - 2] != (crc & 0xFF)) { // Low byte first
        return FALSE;
    }
    return buf[len - 1] == ((crc >> 8) & 0xFF); // High byte second
}

/**
 * Initialization UART for PZEM communication
 */
void Pzem_initUart(void) {
    halUARTCfg_t halUARTConfig;
    halUARTConfig.configured = TRUE;
    halUARTConfig.baudRate = HAL_UART_BR_9600;
    halUARTConfig.flowControl = FALSE;
    halUARTConfig.flowControlThreshold = 48; // this parameter indicates number of bytes left before Rx Buffer reaches maxRxBufSize
    halUARTConfig.idleTimeout = 10;          // this parameter indicates rx timeout period in millisecond
    halUARTConfig.rx.maxBufSize = 15;
    halUARTConfig.tx.maxBufSize = 15;
    halUARTConfig.intEnable = TRUE;
    halUARTConfig.callBackFunc = Pzem_uartCB;
    HalUARTInit();
    if (HalUARTOpen(PZEM_UART_PORT, &halUARTConfig) == HAL_UART_SUCCESS) {
        LREP("Initialized PZEM UART: %d\r\n", PZEM_UART_PORT);
    }
    pzemRequestState = Idle;
}

/**
 * Callback function for read and processing PZEM answer
 * @param port
 * @param event
 */
static void Pzem_uartCB(uint8 port, uint8 event) {
    (void) event;

    while (Hal_UART_RxBufLen(port)) {
        // Read one byte from UART
        uint8 byte;
        HalUARTRead(port, &byte, 1);

        if (pzemRequestState != Wait) {
            // read but don`t save
            continue;
        }

        response[responseCounter++] = byte;
        if (responseCounter >= PZEM_RESPONSE_LENGTH) {
            responseCounter = 0;
            pzemRequestState = Ready;
            continue;
        }

        if (responseCounter == PZEM_ERROR_ANSWER_SIZE) {
            if (response[1] == PZEM_ERROR_ANSWER_CODE) {
                pzemRequestState = Error;
            }
        }
    }
}

/**
 * Send read the measurement request to PZEM
 * @param pzemAddr address of requested PZEM module
 * @return TRUE if the request was sent successfully, FALSE if fails
 */
bool Pzem_RequestData(uint8 pzemAddr) {
    uint8 sendBuffer[8]; // Send buffer

    sendBuffer[0] = pzemAddr; // Set slave address
    sendBuffer[1] = CMD_RIR;  // Set command

    sendBuffer[2] = 0;        // Set high byte of register address
    sendBuffer[3] = 0;        // Set low byte =//=

    sendBuffer[4] = 0;        // Set high byte of register value
    sendBuffer[5] = 0x0A;     // Set low byte =//=

    setCRC(sendBuffer, 8);   // Set CRC of frame

    HalUARTWrite(PZEM_UART_PORT, (uint8 *) sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));

    responseCounter = 0;
    pzemRequestState = Wait;

    return TRUE;
}

/**
 * Return measured values
 * @param measurement pointer to measurement structure
 * @return FALSE if error
 */
bool Pzem_getData(Pzem_measurement_t *measurement) {
    measurement->voltage = 0;
    measurement->current = 0;
    measurement->power = 0;
    measurement->energy = 0;
    measurement->frequency = 0;
    measurement->powerFactor = 0;

    if (pzemRequestState != Ready) {
        return FALSE;
    }
    if (!checkCRC(response, PZEM_RESPONSE_LENGTH)) {
        return FALSE;
    }
    measurement->voltage = (uint16) response[4] | (uint16) response[3] << 8;
    // checking that the data is correct
    if (measurement->voltage < 1800 || measurement->voltage > 2500) {
        measurement->voltage = 0;
        return FALSE;
    }
    measurement->current = (uint32) response[6] | (uint32) response[5] << 8 | (uint32) response[8] << 16 | (uint32) response[7] << 24;
    measurement->power = (uint32) response[10] | (uint32) response[9] << 8 | (uint32) response[12] << 16 | (uint32) response[11] << 24;
    measurement->energy = (uint32) response[14] | (uint32) response[13] << 8 | (uint32) response[16] << 16 | (uint32) response[15] << 24;
    measurement->frequency = (uint16) response[18] | (uint16) response[17] << 8;
    measurement->powerFactor = (uint16) response[20] | (uint16) response[19] << 8;

    pzemRequestState = Idle;
    return TRUE;
}
