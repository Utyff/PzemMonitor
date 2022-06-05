#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ms.h"
#include "zcl_app.h"

#include "bdb.h"
#include "bdb_interface.h"

#include "OnBoard.h"

/* HAL */
#include "hal_led.h"
#include "hal_key.h"
#include "hal_drivers.h"

#include "Debug.h"
#include "pzem.h"

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

// Состояние реле
uint8 RELAY_STATE = 0;

// Данные о температуре
uint16 zclApp_MeasuredValue;

// Структура для отправки отчета
afAddrType_t zclApp_DstAddr;
// Номер сообщения
uint8 SeqNum = 0;

// last PZEM measured data
static Pzem_measurement_t measurement;
static bool firstRead;

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

// Functions to process ZCL Foundation incoming Command/Response messages
static void zclApp_ProcessIncomingMsg(zclIncomingMsg_t *msg);

#ifdef ZCL_READ

static uint8 zclApp_ProcessInReadRspCmd(zclIncomingMsg_t *pInMsg);

#endif
#ifdef ZCL_WRITE

static uint8 zclApp_ProcessInWriteRspCmd(zclIncomingMsg_t *pInMsg);

#endif

static uint8 zclApp_ProcessInDefaultRspCmd(zclIncomingMsg_t *pInMsg);

void pzemRead(void);

// Изменение состояние реле
static void updateRelay(bool);

// Отображение состояния реле на пинах
static void applyRelay(void);

// Выход из сети
void zclApp_LeaveNetwork(void);

// Отправка отчета о состоянии реле
void zclApp_ReportOnOff(void);

// Отправка отчета о температуре
void zclApp_ReportTemp(void);

/*********************************************************************
 * ZCL General Profile Callback table
 */
static zclGeneral_AppCallbacks_t zclApp_CmdCallbacks = {
        zclApp_BasicResetCB,             // Basic Cluster Reset command
        NULL,                                   // Identify Trigger Effect command
        zclApp_OnOffCB,                     // On/Off cluster commands
        NULL,                                   // On/Off cluster enhanced command Off with Effect
        NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
        NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
        NULL,                                   // Level Control Move to Level command
        NULL,                                   // Level Control Move command
        NULL,                                   // Level Control Step command
        NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
        NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
        NULL,                                  // Scene Store Request command
        NULL,                                  // Scene Recall Request command
        NULL,                                  // Scene Response command
#endif
#ifdef ZCL_ALARMS
        NULL,                                  // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
        NULL,                                  // Get Event Log command
        NULL,                                  // Publish Event Log command
#endif
        NULL,                                  // RSSI Location command
        NULL                                   // RSSI Location Response command
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

    zclApp_TaskID = task_id;
    LREP("zclApp_Init - %d\r\n", task_id);

    // This app is part of the Home Automation Profile
    bdb_RegisterSimpleDescriptor(&zclApp_Desc);

    // Register the ZCL General Cluster Library callback functions
    zclGeneral_RegisterCmdCallbacks(APP_ENDPOINT, &zclApp_CmdCallbacks);

    // Register the application's attribute list
    zcl_registerAttrList(APP_ENDPOINT, zclApp_NumAttributes, zclApp_Attrs);

    // Register the Application to receive the unprocessed Foundation command/response messages
    zcl_registerForMsg(zclApp_TaskID);

    // Register for all key events - This app will handle all key events
    RegisterForKeys(zclApp_TaskID);

    bdb_RegisterCommissioningStatusCB(zclApp_ProcessCommissioningStatus);
    bdb_RegisterIdentifyTimeChangeCB(zclApp_ProcessIdentifyTimeChange);
    bdb_RegisterBindNotificationCB(zclApp_BindNotification);

    LREP("Build %s \r\n", zclApp_DateCodeNT);

    // Установка адреса и эндпоинта для отправки отчета
    zclApp_DstAddr.addrMode = (afAddrMode_t) AddrNotPresent;
    zclApp_DstAddr.endPoint = 0;
    zclApp_DstAddr.addr.shortAddr = 0;

    measurement.voltage = 0;
    measurement.current = 0;
    measurement.power = 0;
    measurement.energy = 0;
    measurement.frequency = 0;
    measurement.powerFactor = 0;
    firstRead = TRUE;

    // инициализируем NVM для хранения RELAY STATE
    if (SUCCESS == osal_nv_item_init(NV_APP_RELAY_STATE_ID, 1, &RELAY_STATE)) {
        // читаем значение RELAY STATE из памяти
        osal_nv_read(NV_APP_RELAY_STATE_ID, 0, 1, &RELAY_STATE);
    }
    // применяем состояние реле
    applyRelay();

    // Старт процесса возвращения в сеть
    bdb_StartCommissioning(BDB_COMMISSIONING_MODE_PARENT_LOST);

    // read PZEM every 5 sec
    osal_start_reload_timer(zclApp_TaskID, APP_PZEM_READ_EVT, 5000);
    // report measured data every 1 min
    osal_start_reload_timer(zclApp_TaskID, APP_REPORT_EVT, ((uint32) 60 * 1000));
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
    LREP("events 0x%x \r\n", events);

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
        LREPMaster("APP_EVT_BLINK\r\n");
        // В сети или не в сети?
        if (bdbAttributes.bdbNodeIsOnANetwork) {
            // гасим светодиод
            HalLedSet(HAL_LED_2, HAL_LED_MODE_OFF);
        } else {
            // переключим светодиод и взведем опять таймер
            HalLedSet(HAL_LED_2, HAL_LED_MODE_TOGGLE);
            osal_start_timerEx(zclApp_TaskID, APP_EVT_BLINK, 500);
        }

        return (events ^ APP_EVT_BLINK);
    }

    if (events & APP_REPORT_EVT) {
//        bdb_RepChangedAttrValue(APP_ENDPOINT, ZCL_CLUSTER_ID_MS_TEMPERATURE_MEASUREMENT, ATTRID_MS_TEMPERATURE_MEASURED_VALUE);
        zclApp_ReportTemp();
        firstRead = TRUE;
        return (events ^ APP_REPORT_EVT);
    }

    if (events & APP_PZEM_READ_EVT) {
        Pzem_RequestData(1);
        osal_start_timerEx(zclApp_TaskID, APP_PZEM_DATA_READY_EVT, PZEM_READ_TIME);
        return (events ^ APP_PZEM_READ_EVT);
    }

    if (events & APP_PZEM_DATA_READY_EVT) {
        pzemRead();
        return (events ^ APP_PZEM_DATA_READY_EVT);
    }

    if (events & APP_BTN_CLICK_EVT) {
        clicks = 0;
        LREPMaster("APP_BTN_CLICK_EVT\r\n");
        zclApp_BtnClick();
        return (events ^ APP_BTN_CLICK_EVT);
    }

    if (events & APP_BTN_DOUBLE_EVT) {
        clicks = 0;
        LREPMaster("APP_BTN_DOUBLE_EVT\r\n");
        zclApp_BtnDblClick();
        return (events ^ APP_BTN_DOUBLE_EVT);
    }

    if (events & APP_BTN_HOLD_EVT) {
        clicks = 0;
        LREPMaster("APP_BTN_HOLD_EVT\r\n");
        zclApp_BtnHold();
        return (events ^ APP_BTN_HOLD_EVT);
    }

    // Discard unknown events
    return 0;
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
        // Notify the device of the results of the its original write attributes
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

void pzemRead(void) {
    Pzem_measurement_t m;
    if (Pzem_getData(&m)) {
        if (firstRead) {
            firstRead = FALSE;
            zclApp_MeasuredValue = m.voltage;
            measurement.voltage = m.voltage;
            measurement.current = m.current;
            measurement.power = m.power;
            measurement.energy = m.energy;
            measurement.frequency = m.frequency;
            measurement.powerFactor = m.powerFactor;
        } else {
            zclApp_MeasuredValue = (zclApp_MeasuredValue + m.voltage) / 2;
            measurement.voltage = (measurement.voltage + m.voltage) / 2;
            measurement.current = (measurement.current + m.current) / 2;
            measurement.power = (measurement.power + m.power) / 2;
            measurement.energy = m.energy;
            measurement.frequency = (measurement.frequency + m.frequency) / 2;
            measurement.powerFactor = (measurement.powerFactor + m.powerFactor) / 2;
        }
        LREP("v:%d c:%ld e:%ld f:%d pf:%d\r\n", measurement.voltage, measurement.current, measurement.energy, measurement.frequency, measurement.powerFactor);
    } else {
        LREPMaster("PZEM read error\r\n");
    }
}

// Изменение состояния реле
void updateRelay(bool value) {
    LREP("updateRelay. value - %d \r\n", value);
    if (value) {
        RELAY_STATE = 1;
    } else {
        RELAY_STATE = 0;
    }
    // сохраняем состояние реле
    osal_nv_write(NV_APP_RELAY_STATE_ID, 0, 1, &RELAY_STATE);
    // Отображаем новое состояние
    applyRelay();
    // отправляем отчет
    zclApp_ReportOnOff();
}

// Применение состояние реле
void applyRelay(void) {
    // если выключено
    if (RELAY_STATE == 0) {
        // то гасим светодиод 1
        HalLedSet(HAL_LED_1, HAL_LED_MODE_OFF);
    } else {
        // иначе включаем светодиод 1
        HalLedSet(HAL_LED_1, HAL_LED_MODE_ON);
    }
}

// Инициализация выхода из сети
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

// Обработчик команд кластера OnOff
static void zclApp_OnOffCB(uint8 cmd) {
    // запомним адрес откуда пришла команда
    // чтобы отправить обратно отчет
    afIncomingMSGPacket_t *pPtr = zcl_getRawAFMsg();
    zclApp_DstAddr.addr.shortAddr = pPtr->srcAddr.addr.shortAddr;

    if (cmd == COMMAND_ON) {            // Включить
        updateRelay(TRUE);
    } else if (cmd == COMMAND_OFF) {    // Выключить
        updateRelay(FALSE);
    } else if (cmd == COMMAND_TOGGLE) { // Переключить
        updateRelay(RELAY_STATE == 0);
    }
}

// Информирование о состоянии реле
void zclApp_ReportOnOff(void) {
    const uint8 NUM_ATTRIBUTES = 1;

    zclReportCmd_t *pReportCmd;

    pReportCmd = osal_mem_alloc(sizeof(zclReportCmd_t) +
                                (NUM_ATTRIBUTES * sizeof(zclReport_t)));
    if (pReportCmd != NULL) {
        pReportCmd->numAttr = NUM_ATTRIBUTES;

        pReportCmd->attrList[0].attrID = ATTRID_ON_OFF;
        pReportCmd->attrList[0].dataType = ZCL_DATATYPE_BOOLEAN;
        pReportCmd->attrList[0].attrData = (void *) (&RELAY_STATE);

        zclApp_DstAddr.addrMode = (afAddrMode_t) Addr16Bit;
        zclApp_DstAddr.addr.shortAddr = 0;
        zclApp_DstAddr.endPoint = 1;

        zcl_SendReportCmd(APP_ENDPOINT, &zclApp_DstAddr,
                          ZCL_CLUSTER_ID_GEN_ON_OFF, pReportCmd,
                          ZCL_FRAME_CLIENT_SERVER_DIR, false, SeqNum++);
    }

    osal_mem_free(pReportCmd);
}

// Информирование о температуре
void zclApp_ReportTemp(void) {
    const uint8 NUM_ATTRIBUTES = 1;

    zclReportCmd_t *pReportCmd;

    pReportCmd = osal_mem_alloc(sizeof(zclReportCmd_t) +
                                (NUM_ATTRIBUTES * sizeof(zclReport_t)));
    if (pReportCmd != NULL) {
        pReportCmd->numAttr = NUM_ATTRIBUTES;

        pReportCmd->attrList[0].attrID = ATTRID_MS_TEMPERATURE_MEASURED_VALUE;
        pReportCmd->attrList[0].dataType = ZCL_DATATYPE_INT16;
        pReportCmd->attrList[0].attrData = (void *) (&zclApp_MeasuredValue);

        zclApp_DstAddr.addrMode = (afAddrMode_t) Addr16Bit;
        zclApp_DstAddr.addr.shortAddr = 0;
        zclApp_DstAddr.endPoint = 1;

        zcl_SendReportCmd(APP_ENDPOINT, &zclApp_DstAddr,
                          ZCL_CLUSTER_ID_MS_TEMPERATURE_MEASUREMENT, pReportCmd,
                          ZCL_FRAME_CLIENT_SERVER_DIR, false, SeqNum++);
    }

    osal_mem_free(pReportCmd);
}

static void zclApp_HandleKeys(byte portAndAction, byte keyCode) {
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
            LREPMaster("Key released\r\n");

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
    updateRelay(RELAY_STATE == 0);
}

static void zclApp_BtnDblClick(void) {
    HalLedBlink(HAL_LED_2, 2, 50, 500);
}

static void zclApp_BtnHold(void) {
    // Проверяем текущее состояние устройства
    // В сети или не в сети?
    if (bdbAttributes.bdbNodeIsOnANetwork) {
        // покидаем сеть
        zclApp_LeaveNetwork();
    } else {
        // инициируем вход в сеть
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_FORMATION |
                               BDB_COMMISSIONING_MODE_NWK_STEERING |
                               BDB_COMMISSIONING_MODE_FINDING_BINDING |
                               BDB_COMMISSIONING_MODE_INITIATOR_TL
        );
        // будем мигать, пока не подключимся
        osal_start_timerEx(zclApp_TaskID, APP_EVT_BLINK, 50);
    }
}
