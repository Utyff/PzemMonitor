#include "ZComDef.h"
#include "OSAL.h"
#include "OSAL_Clock.h"
#include "AF.h"
#include "ZDApp.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_electrical_measurement.h"
#include "zcl_app.h"

#include "bdb.h"
#include "bdb_interface.h"

#include "OnBoard.h"

/* HAL */
#include "hal_led.h"
#include "hal_key.h"
#include "hal_drivers.h"

#include "st7789.h"
#include "Debug.h"

#include "version.h"

/*********************************************************************
 * MACROS
 */


/*********************************************************************
 * CONSTANTS
 */


/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
byte zclApp_TaskID;


/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// double click count
uint8 clicks = 0;

// data for zcl report
Pzem_measurement_t zcl_measured[3];
// last PZEM measured data
Pzem_measurement_t measured[3];
// for calculation of the arithmetic mean
static bool firstRead[3];
// 0, 1, 2 - PZEM address that we will read.
static uint8 pzemAddrRead;

static uint32 zclApp_GenTime_old = 0;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void zclApp_HandleKeys(byte portAndAction, byte keyCode);

static void zclApp_BtnClick(void);

static void zclApp_BtnDblClick(void);

static void zclApp_BtnHold(void);

static void zclApp_BasicResetCB(void);

static void zclApp_ProcessIdentifyTimeChange(uint8 endpoint);

static void zclApp_BindNotification(bdbBindNotificationData_t *data);

static void zclApp_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg);

static void zclApp_initPWM(void);

// Functions to process ZCL Foundation incoming Command/Response messages
static void zclApp_ProcessIncomingMsg(zclIncomingMsg_t *msg);

#ifdef ZCL_READ

static uint8 zclApp_ProcessInReadRspCmd(zclIncomingMsg_t *pInMsg);

#endif
#ifdef ZCL_WRITE

static uint8 zclApp_ProcessInWriteRspCmd(zclIncomingMsg_t *pInMsg);

#endif

static uint8 zclApp_ProcessInDefaultRspCmd(zclIncomingMsg_t *pInMsg);

static void zclApp_SetTimeDate(void);

static void pzemRead(void);

// ����� �� ����
void zclApp_LeaveNetwork(void);

// Report measured data
void zclApp_ReportData(void);

/*********************************************************************
 * ZCL General Profile Callback table
 */
static zclGeneral_AppCallbacks_t zclApp_CmdCallbacks = {
        zclApp_BasicResetCB,      // Basic Cluster Reset command
        NULL,                     // Identify Trigger Effect command
        NULL,                     // On/Off cluster commands
        NULL,                     // On/Off cluster enhanced command Off with Effect
        NULL,                     // On/Off cluster enhanced command On with Recall Global Scene
        NULL,                     // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
        NULL,                     // Level Control Move to Level command
        NULL,                     // Level Control Move command
        NULL,                     // Level Control Step command
        NULL,                     // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
        NULL,                     // Group Response commands
#endif
#ifdef ZCL_SCENES
        NULL,                     // Scene Store Request command
        NULL,                     // Scene Recall Request command
        NULL,                     // Scene Response command
#endif
#ifdef ZCL_ALARMS
        NULL,                     // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
        NULL,                     // Get Event Log command
        NULL,                     // Publish Event Log command
#endif
        NULL,                     // RSSI Location command
        NULL                      // RSSI Location Response command
};


/*********************************************************************
 * @fn          zclApp_Init
 *
 * @brief       Initialization function for the zclGeneral layer.
 *
 * @param       none
 *
 * @return      none
 */
void zclApp_Init(byte task_id) {
    HalLedSet(HAL_LED_ALL, HAL_LED_MODE_BLINK);
#ifndef DEBUG_LCD_UART
    Pzem_initUart();
#endif
#ifndef DEBUG_PZEM_UART
    LCD_Init();
#endif
    zclApp_initPWM();

    zclApp_TaskID = task_id;
    LREP("\r\nBuild %s\r\n", zclApp_DateCodeNT);
    LREP("zclApp_Init - %d\r\n", task_id);

    // This app is part of the Home Automation Profile
    bdb_RegisterSimpleDescriptor(&zclApp_Desc1);
    bdb_RegisterSimpleDescriptor(&zclApp_Desc2);
    bdb_RegisterSimpleDescriptor(&zclApp_Desc3);

    // Register the ZCL General Cluster Library callback functions
    zclGeneral_RegisterCmdCallbacks(APP_ENDPOINT1, &zclApp_CmdCallbacks);

    // Register the application's attribute list
    zcl_registerAttrList(APP_ENDPOINT1, zclApp_NumAttributes1, zclApp_Attrs1);
    zcl_registerAttrList(APP_ENDPOINT2, zclApp_NumAttributes2, zclApp_Attrs2);
    zcl_registerAttrList(APP_ENDPOINT3, zclApp_NumAttributes3, zclApp_Attrs3);

    // Register the Application to receive the unprocessed Foundation command/response messages
    zcl_registerForMsg(zclApp_TaskID);

    // Register for all key events - This app will handle all key events
    RegisterForKeys(zclApp_TaskID);

    bdb_RegisterCommissioningStatusCB(zclApp_ProcessCommissioningStatus);
    bdb_RegisterIdentifyTimeChangeCB(zclApp_ProcessIdentifyTimeChange);
    bdb_RegisterBindNotificationCB(zclApp_BindNotification);

    for (int i = 0; i < 3; i++) {
        measured[i].voltage = 0;
        measured[i].current = 0;
        measured[i].power = 0;
        measured[i].energy = 0;
        measured[i].frequency = 0;
        measured[i].powerFactor = 0;
        firstRead[i] = TRUE;
    }

    // ����� �������� ����������� � ����
    bdb_StartCommissioning(BDB_COMMISSIONING_MODE_PARENT_LOST);

    pzemAddrRead = 0;
    // read PZEM every 1 sec
    osal_start_reload_timer(zclApp_TaskID, APP_PZEM_READ_EVT, 1000);
    // report measured data every 1 min
    osal_start_reload_timer(zclApp_TaskID, APP_REPORT_EVT, ((uint32) 60 * 1000));
    // report time every 1 min
//    osal_start_reload_timer(zclApp_TaskID, APP_REPORT_CLOCK_EVT, (uint32) 60000);

    zclApp_SetTimeDate();
}

/*********************************************************************
 * @fn          zclSample_event_loop
 *
 * @brief       Event Loop Processor for zclGeneral.
 *
 * @param       none
 *
 * @return      none
 */
uint16 zclApp_event_loop(uint8 task_id, uint16 events) {

    (void) task_id;  // Intentionally unreferenced parameter
    LREP("events 0x%x\r\n", events);

    if (events & SYS_EVENT_MSG) {
        afIncomingMSGPacket_t *MSGpkt;
        while ((MSGpkt = (afIncomingMSGPacket_t *) osal_msg_receive(zclApp_TaskID))) {
            LREP("MSGpkt->hdr.event 0x%X clusterId=0x%X\r\n", MSGpkt->hdr.event, MSGpkt->clusterId);
            switch (MSGpkt->hdr.event) {
                case ZCL_INCOMING_MSG:
                    // Incoming ZCL Foundation command/response messages
                    zclApp_ProcessIncomingMsg((zclIncomingMsg_t *) MSGpkt);
                    break;

                case KEY_CHANGE:
                    zclApp_HandleKeys(((keyChange_t *) MSGpkt)->state, ((keyChange_t *) MSGpkt)->keys);
                    break;

                default:
                    break;
            }

            // Release the memory
            osal_msg_deallocate((uint8 *) MSGpkt);
        }

        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

#if ZG_BUILD_ENDDEVICE_TYPE
    if (events & APP_END_DEVICE_REJOIN_EVT) {
        bdb_ZedAttemptRecoverNwk();
        return (events ^ APP_END_DEVICE_REJOIN_EVT);
    }
#endif

    // blink when join network
    if (events & APP_EVT_BLINK) {
        LREPMaster((uint8 *) "APP_EVT_BLINK\r\n");
        // � ���� ��� �� � ����?
        if (bdbAttributes.bdbNodeIsOnANetwork) {
            // ����� ���������
            HalLedSet(HAL_LED_2, HAL_LED_MODE_OFF);
        } else {
            // ���������� ��������� � ������� ����� ������
            HalLedSet(HAL_LED_2, HAL_LED_MODE_TOGGLE);
            osal_start_timerEx(zclApp_TaskID, APP_EVT_BLINK, 500);
        }

        return (events ^ APP_EVT_BLINK);
    }

    if (events & APP_REPORT_EVT) {
        zclApp_ReportData();
        return (events ^ APP_REPORT_EVT);
    }

    if (events & APP_PZEM_READ_EVT) {
#ifndef DEBUG_LCD_UART
        // PZEM address - 1, 2, 3
        Pzem_RequestData(pzemAddrRead + 1);
#endif
        osal_start_timerEx(zclApp_TaskID, APP_PZEM_DATA_READY_EVT, PZEM_READ_TIME);
        return (events ^ APP_PZEM_READ_EVT);
    }

    if (events & APP_PZEM_DATA_READY_EVT) {
#ifndef DEBUG_LCD_UART
        pzemRead();
#endif
        // read data from next PZEM module
        if (pzemAddrRead < 2) {
            pzemAddrRead++;
#ifndef DEBUG_LCD_UART
            // PZEM address - 1, 2, 3
            Pzem_RequestData(pzemAddrRead + 1);
#endif
            osal_start_timerEx(zclApp_TaskID, APP_PZEM_DATA_READY_EVT, PZEM_READ_TIME);
        } else {
            // after read all 3 modules  TODO reset readFirst here
            pzemAddrRead = 0;
        }
        return (events ^ APP_PZEM_DATA_READY_EVT);
    }

    if (events & APP_BTN_CLICK_EVT) {
        clicks = 0;
        LREPMaster((uint8 *) "APP_BTN_CLICK_EVT\r\n");
        zclApp_BtnClick();
        return (events ^ APP_BTN_CLICK_EVT);
    }

    if (events & APP_BTN_DOUBLE_EVT) {
        clicks = 0;
        LREPMaster((uint8 *) "APP_BTN_DOUBLE_EVT\r\n");
        zclApp_BtnDblClick();
        return (events ^ APP_BTN_DOUBLE_EVT);
    }

    if (events & APP_BTN_HOLD_EVT) {
        clicks = 0;
        LREPMaster((uint8 *) "APP_BTN_HOLD_EVT\r\n");
        zclApp_BtnHold();
        return (events ^ APP_BTN_HOLD_EVT);
    }

    if (events & APP_REPORT_CLOCK_EVT) {
        LREP("REPORTCLOCK %ld %ld\r\n", zclApp_GenTime_TimeUTC, zclApp_GenTime_old);

        // Fix osalclock bug 88 min in 15 days
        osalTimeUpdate();
        zclApp_GenTime_TimeUTC = osal_getClock();

        // if the interval is more than 70 seconds, then adjust the time
        if ((zclApp_GenTime_TimeUTC - zclApp_GenTime_old) > 70) {
            // Update OSAL time
            osal_setClock(zclApp_GenTime_old + 60);
            osalTimeUpdate();
            zclApp_GenTime_TimeUTC = osal_getClock();
        }
        zclApp_GenTime_old = zclApp_GenTime_TimeUTC;

        return (events ^ APP_REPORT_CLOCK_EVT);
    }

    // Discard unknown events
    return 0;
}

/**
 * Configure Timer 3 Ch1 as PWM for LCD backlight
 */
static void zclApp_initPWM(void) {
    PERCFG &= ~(0x20); // Select Timer 3 Alternative 1 location
    P2SEL |= 0x20;
    P2DIR |= 0xC0;     // Give priority to Timer 1 channel2-3
    P1SEL |= BV(4);    // Set P1_4 to peripheral, Timer 1,channel 2
    P1DIR |= BV(4);

    T3CTL &= ~BV(4);   // Stop timer 3 (if it was running)
    T3CTL |= BV(2);    // Clear timer 3
    T3CTL &= ~0x08;    // Disable Timer 3 overflow interrupts
//    T3CTL &= ~0x03;    // Timer 3 mode = 0 - Free-running, repeatedly count from 0x00 to 0xFF

    T3CCTL1 &= ~0x40;  // Disable channel 1 interrupts
    T3CCTL1 |= BV(2);  // Ch1 mode = compare
    T3CCTL1 |= BV(5) | BV(3); // Ch1 output compare mode = Set output on compare, clear on 0xFF

    T3CTL |= BV(7) | BV(6) | BV(5); // Prescaler divider - Tick frequency / 128
    T3CC1 = 230;                    // PWM duty cycle: 10 - bright, 230 - dark

    T3CTL |= BV(4);    // start timer 3
}

void zclApp_SetTimeDate(void) {
    // Set Time and Date
    UTCTimeStruct time;
    time.seconds = 0;
    time.minutes = (zclApp_DateCode[15] - 48) * 10 + (zclApp_DateCode[16] - 48);
    time.hour = (zclApp_DateCode[12] - 48) * 10 + (zclApp_DateCode[13] - 48);
    time.day = (zclApp_DateCode[1] - 48) * 10 + (zclApp_DateCode[2] - 48) - 1;
    time.month = (zclApp_DateCode[4] - 48) * 10 + (zclApp_DateCode[5] - 48) - 1;
    time.year = 2000 + (zclApp_DateCode[9] - 48) * 10 + (zclApp_DateCode[10] - 48);
    // Update OSAL time
    osal_setClock(osal_ConvertUTCSecs(&time));
    // Get time structure from OSAL
    osal_ConvertUTCTime(&time, osal_getClock());
    osalTimeUpdate();
    zclApp_GenTime_TimeUTC = osal_getClock();

    zclApp_GenTime_old = zclApp_GenTime_TimeUTC;
}


static void zclApp_SaveAttributesToNV(void) {
    uint8 writeStatus = 0;
    // osal_nv_write(NW_APP_CONFIG, 0, sizeof(application_config_t), &zclApp_Config);
    LREP("Saving attributes to NV write=%d\r\n", writeStatus);

    zclApp_GenTime_old = zclApp_GenTime_TimeUTC;

    osal_setClock(zclApp_GenTime_TimeUTC);
}


/*********************************************************************
 * @fn      zclApp_ProcessCommissioningStatus
 *
 * @brief   Callback in which the status of the commissioning process are reported
 *
 * @param   bdbCommissioningModeMsg - Context message of the status of a commissioning process
 *
 * @return  none
 */
static void zclApp_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg) {
    switch (bdbCommissioningModeMsg->bdbCommissioningMode) {
        case BDB_COMMISSIONING_FORMATION:
            if (bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS) {
                //After formation, perform nwk steering again plus the remaining commissioning modes that has not been process yet
                bdb_StartCommissioning(
                        BDB_COMMISSIONING_MODE_NWK_STEERING | bdbCommissioningModeMsg->bdbRemainingCommissioningModes);
            } else {
                //Want to try other channels?
                //try with bdb_setChannelAttribute
            }
            break;
        case BDB_COMMISSIONING_NWK_STEERING:
            if (bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS) {
                //YOUR JOB:
                //We are on the nwk, what now?
            } else {
                //See the possible errors for nwk steering procedure
                //No suitable networks found
                //Want to try other channels?
                //try with bdb_setChannelAttribute
            }
            break;
        case BDB_COMMISSIONING_FINDING_BINDING:
            if (bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS) {
                //YOUR JOB:
            } else {
                //YOUR JOB:
                //retry?, wait for user interaction?
            }
            break;
        case BDB_COMMISSIONING_INITIALIZATION:
            //Initialization notification can only be successful. Failure on initialization
            //only happens for ZED and is notified as BDB_COMMISSIONING_PARENT_LOST notification

            //YOUR JOB:
            //We are on a network, what now?

            break;
#if ZG_BUILD_ENDDEVICE_TYPE
        case BDB_COMMISSIONING_PARENT_LOST:
            if (bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED) {
                //We did recover from losing parent
            } else {
                //Parent not found, attempt to rejoin again after a fixed delay
                osal_start_timerEx(zclApp_TaskID, APP_END_DEVICE_REJOIN_EVT, APP_END_DEVICE_REJOIN_DELAY);
            }
            break;
#endif
    }
}

/*********************************************************************
 * @fn      zclApp_ProcessIdentifyTimeChange
 *
 * @brief   Called to process any change to the IdentifyTime attribute.
 *
 * @param   endpoint - in which the identify has change
 *
 * @return  none
 */
static void zclApp_ProcessIdentifyTimeChange(uint8 endpoint) {
    (void) endpoint;
}

/*********************************************************************
 * @fn      zclApp_BindNotification
 *
 * @brief   Called when a new bind is added.
 *
 * @param   data - pointer to new bind data
 *
 * @return  none
 */
static void zclApp_BindNotification(bdbBindNotificationData_t *data) {
    // APP_TODO: process the new bind information
}

/*********************************************************************
 * @fn      zclApp_BasicResetCB
 *
 * @brief   Callback from the ZCL General Cluster Library
 *          to set all the Basic Cluster attributes to default values.
 *
 * @param   none
 *
 * @return  none
 */
static void zclApp_BasicResetCB(void) {

    /* APP_TODO: remember to update this function with any
       application-specific cluster attribute variables */

    zclApp_ResetAttributesToDefaultValues();

}

/******************************************************************************
 *
 *  Functions for processing ZCL Foundation incoming Command/Response messages
 *
 *****************************************************************************/

/*********************************************************************
 * @fn      zclApp_ProcessIncomingMsg
 *
 * @brief   Process ZCL Foundation incoming message
 *
 * @param   msg - pointer to the received message
 *
 * @return  none
 */
static void zclApp_ProcessIncomingMsg(zclIncomingMsg_t *msg) {
    switch (msg->zclHdr.commandID) {
#ifdef ZCL_READ
        case ZCL_CMD_READ_RSP:
            zclApp_ProcessInReadRspCmd(msg);
            break;
#endif
#ifdef ZCL_WRITE
        case ZCL_CMD_WRITE_RSP:
            zclApp_ProcessInWriteRspCmd(msg);
            break;
#endif
        case ZCL_CMD_CONFIG_REPORT:
        case ZCL_CMD_CONFIG_REPORT_RSP:
        case ZCL_CMD_READ_REPORT_CFG:
        case ZCL_CMD_READ_REPORT_CFG_RSP:
        case ZCL_CMD_REPORT:
            //bdb_ProcessIncomingReportingMsg(msg);
            break;

        case ZCL_CMD_DEFAULT_RSP:
            zclApp_ProcessInDefaultRspCmd(msg);
            break;

        default:
            break;
    }

    if (msg->attrCmd)
        osal_mem_free(msg->attrCmd);
}

#ifdef ZCL_READ

/*********************************************************************
 * @fn      zclApp_ProcessInReadRspCmd
 *
 * @brief   Process the "Profile" Read Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclApp_ProcessInReadRspCmd(zclIncomingMsg_t *pInMsg) {
    zclReadRspCmd_t *readRspCmd;
    uint8 i;

    readRspCmd = (zclReadRspCmd_t *) pInMsg->attrCmd;
    for (i = 0; i < readRspCmd->numAttr; i++) {
        // Notify the originator of the results of the original read attributes
        // attempt and, for each successfull request, the value of the requested
        // attribute
    }

    return (TRUE);
}

#endif // ZCL_READ

#ifdef ZCL_WRITE

/*********************************************************************
 * @fn      zclApp_ProcessInWriteRspCmd
 *
 * @brief   Process the "Profile" Write Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclApp_ProcessInWriteRspCmd(zclIncomingMsg_t *pInMsg) {
    zclWriteRspCmd_t *writeRspCmd;
    uint8 i;

    writeRspCmd = (zclWriteRspCmd_t *) pInMsg->attrCmd;
    for (i = 0; i < writeRspCmd->numAttr; i++) {
        // Notify the device of the results of the original write attributes
        // command.
    }

    return (TRUE);
}

#endif // ZCL_WRITE

/*********************************************************************
 * @fn      zclApp_ProcessInDefaultRspCmd
 *
 * @brief   Process the "Profile" Default Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclApp_ProcessInDefaultRspCmd(zclIncomingMsg_t *pInMsg) {
    // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

    // Device is notified of the Default Response command.
    (void) pInMsg;

    return (TRUE);
}

#define BX 40
#define BY 55
#define STEP_X 11
#define STEP_Y 20

static void pzemRead(void) {
    Pzem_measurement_t m;
    char str[22];
    if (Pzem_getData(&m)) {
#ifndef DEBUG_PZEM_UART
        if (pzemAddrRead == 0) {
//            sprintf(str, "v %6d", m.voltage);
//            LCD_Print(BX, BY, str, CYAN, BLUE);
//            sprintf(str, "c %8ld", m.current);
//            LCD_Print(BX, BY + STEP_Y, str, CYAN, BLUE);
//            sprintf(str, "p %8ld", m.power);
//            LCD_Print(BX, BY + STEP_Y * 2, str, CYAN, BLUE);
//            sprintf(str, "e %8ld", m.energy);
//            LCD_Print(BX, BY + STEP_Y * 3, str, CYAN, BLUE);
            sprintf(str, "f %8d", m.frequency);
            LCD_Print(BX, BY + STEP_Y * 4, str, CYAN, BLUE);
//            sprintf(str, "pf %7d", m.powerFactor);
//            LCD_Print(BX, BY + STEP_Y * 5, str, CYAN, BLUE);
        }
        if (pzemAddrRead == 1) {
            sprintf(str, "f2 %8d", m.frequency);
            LCD_Print(BX, BY + STEP_Y * 6, str, CYAN, BLUE);
        }
        if (pzemAddrRead == 2) {
            sprintf(str, "f3 %8d", m.frequency);
            LCD_Print(BX, BY + STEP_Y * 7, str, CYAN, BLUE);
        }
        LCD_WriteChar(BX + pzemAddrRead * STEP_X, BY + STEP_Y * 9, ' ', RED, BLUE);
#endif
        LREP("%d - v:%d c:%d e:%ld f:%d pf:%d\r\n", pzemAddrRead, m.voltage, m.current, m.energy, m.frequency, m.powerFactor);
        measured[pzemAddrRead].energy = m.energy;
        if (firstRead[pzemAddrRead]) {
            measured[pzemAddrRead].voltage = m.voltage;
            measured[pzemAddrRead].current = m.current;
            measured[pzemAddrRead].power = m.power;
            measured[pzemAddrRead].frequency = m.frequency;
            measured[pzemAddrRead].powerFactor = m.powerFactor;
            firstRead[pzemAddrRead] = FALSE;
        } else {
            measured[pzemAddrRead].voltage = (measured[pzemAddrRead].voltage + m.voltage) / 2;
            measured[pzemAddrRead].current = (measured[pzemAddrRead].current + m.current) / 2;
            measured[pzemAddrRead].power = (measured[pzemAddrRead].power + m.power) / 2;
            measured[pzemAddrRead].frequency = (measured[pzemAddrRead].frequency + m.frequency) / 2;
            measured[pzemAddrRead].powerFactor = (measured[pzemAddrRead].powerFactor + m.powerFactor) / 2;
        }
    } else {
        LREP("PZEM %d read error\r\n", pzemAddrRead);
#ifndef DEBUG_PZEM_UART
        LCD_WriteChar(BX + pzemAddrRead * STEP_X, BY + STEP_Y * 9, '0' + pzemAddrRead, RED, BLUE);
#endif
    }
}

void zclApp_LeaveNetwork(void) {
    zclApp_ResetAttributesToDefaultValues();

    NLME_LeaveReq_t leaveReq;
    // Set every field to 0
    osal_memset(&leaveReq, 0, sizeof(NLME_LeaveReq_t));

    // This will enable the device to rejoin the network after reset.
    leaveReq.rejoin = FALSE;

    // Set the NV startup option to force a "new" join.
    zgWriteStartupOptions(ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE);

    // Leave the network, and reset afterwards
    if (NLME_LeaveReq(&leaveReq) != ZSuccess) {
        // Couldn't send out leave; prepare to reset anyway
        ZDApp_LeaveReset(FALSE);
    }
}

// Report measured data
void zclApp_ReportData(void) {
    firstRead[0] = firstRead[1] = firstRead[2] = TRUE;
    for (uint8 i = 0; i < 3; i++) {
        zcl_measured[i].voltage = measured[i].voltage;
        zcl_measured[i].current = measured[i].current;
        zcl_measured[i].power = measured[i].power;
        zcl_measured[i].energy = measured[i].energy;
        zcl_measured[i].frequency = measured[i].frequency;
        zcl_measured[i].powerFactor = measured[i].powerFactor;
    }

    // one call for all attributes report
    ZStatus_t res = bdb_RepChangedAttrValue(APP_ENDPOINT1, ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, ATTRID_ELECTRICAL_MEASUREMENT_AC_FREQUENCY);
    LREP("RepData - %d f: %d %d %d\r\n", res, measured[0].frequency, measured[1].frequency, measured[2].frequency);
    bdb_RepChangedAttrValue(APP_ENDPOINT2, ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, ATTRID_ELECTRICAL_MEASUREMENT_AC_FREQUENCY);
    bdb_RepChangedAttrValue(APP_ENDPOINT3, ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, ATTRID_ELECTRICAL_MEASUREMENT_AC_FREQUENCY);
}

static void zclApp_HandleKeys(byte portAndAction, byte keyCode) {
    (void) keyCode;

    if (portAndAction & KEY1_PORT) { // P2_0 Btn1 TODO add check BUTTON pin
        if (portAndAction & HAL_KEY_PRESS) {
            LREP("Key pressed. Clicks - %d\r\n", clicks);
            if (clicks < 2) {
                clicks++;
            }

            osal_start_timerEx(zclApp_TaskID, APP_BTN_HOLD_EVT, BTN_HOLD_TIME);
            osal_stop_timerEx(zclApp_TaskID, APP_BTN_CLICK_EVT);
        }
        if (portAndAction & HAL_KEY_RELEASE) {
            LREPMaster((uint8 *) "Key released\r\n");

            osal_stop_timerEx(zclApp_TaskID, APP_BTN_HOLD_EVT);
            if (clicks == 1) {
                osal_start_timerEx(zclApp_TaskID, APP_BTN_CLICK_EVT, BTN_DBL_CLICK_TIME);
            }
            if (clicks == 2) {
                osal_start_timerEx(zclApp_TaskID, APP_BTN_DOUBLE_EVT, 50);
            }
        }
    }
}

static void zclApp_BtnClick(void) {

}

static void zclApp_BtnDblClick(void) {
    HalLedBlink(HAL_LED_2, 2, 50, 500);
}

static void zclApp_BtnHold(void) {
    // ��������� ������� ��������� ����������
    // � ���� ��� �� � ����?
    if (bdbAttributes.bdbNodeIsOnANetwork) {
        // �������� ����
        zclApp_LeaveNetwork();
    } else {
        // ���������� ���� � ����
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_FORMATION |
                               BDB_COMMISSIONING_MODE_NWK_STEERING |
                               BDB_COMMISSIONING_MODE_FINDING_BINDING |
                               BDB_COMMISSIONING_MODE_INITIATOR_TL
        );
        // ����� ������, ���� �� �����������
        osal_start_timerEx(zclApp_TaskID, APP_EVT_BLINK, 50);
    }
}
