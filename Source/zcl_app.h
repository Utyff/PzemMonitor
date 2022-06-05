#ifndef ZCL_APP_H
#define ZCL_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "zcl.h"


// Added to include ZLL Target functionality
#if defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  #include "zcl_general.h"
  #include "bdb_tlCommissioning.h"
#endif

/*********************************************************************
 * CONSTANTS
 */
#define APP_ENDPOINT            1

// Application Events
#define APP_EVT_BLINK                0x0001
#define APP_END_DEVICE_REJOIN_EVT    0x0002
#define APP_PZEM_READ_EVT            0x0004
#define APP_PZEM_DATA_READY_EVT      0x0008
#define APP_BTN_CLICK_EVT            0x0010
#define APP_BTN_HOLD_EVT             0x0020
#define APP_BTN_DOUBLE_EVT           0x0040
#define APP_REPORT_EVT               0x0080

// NVM IDs
#define NV_APP_RELAY_STATE_ID        0x0402

// Added to include ZLL Target functionality
#define APP_NUM_GRPS            2

#define APP_END_DEVICE_REJOIN_DELAY 10000

/*********************************************************************
 * MACROS
 */
/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * VARIABLES
 */

// Added to include ZLL Target functionality
#if defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  extern bdbTLDeviceInfo_t tlApp_DeviceInfo;
#endif

extern SimpleDescriptionFormat_t zclApp_Desc;

extern CONST zclCommandRec_t zclApp_Cmds[];

extern CONST uint8 zclCmdsArraySize;

// attribute list
extern CONST zclAttrRec_t zclApp_Attrs[];
extern CONST uint8 zclApp_NumAttributes;

// Identify attributes
extern uint16 zclApp_IdentifyTime;
extern uint8  zclApp_IdentifyCommissionState;


/*********************************************************************
 * FUNCTIONS
 */

 /*
  * Initialization for the task
  */
extern void zclApp_Init(byte task_id);

/*
 *  Event Process for the task
 */
extern UINT16 zclApp_event_loop(byte task_id, UINT16 events);

/*
 *  Reset all writable attributes to their default values.
 */
extern void zclApp_ResetAttributesToDefaultValues(void);

// Функции команд управления
static void zclApp_OnOffCB(uint8 cmd);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ZCL_APP_H */
