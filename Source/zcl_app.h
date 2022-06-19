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
#include "pzem.h"


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
#define APP_REPORT_CLOCK_EVT         0x0100

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

// attribute list
extern CONST zclAttrRec_t zclApp_Attrs[];
extern CONST uint8 zclApp_NumAttributes;

extern uint32 zclApp_GenTime_TimeUTC;

extern Pzem_measurement_t measurement;

/*********************************************************************
 * FUNCTIONS
 */

 /**
  * Initialization for the task
  */
extern void zclApp_Init(byte task_id);

/**
 *  Event Process for the task
 */
extern UINT16 zclApp_event_loop(byte task_id, UINT16 events);

/**
 *  Reset all writable attributes to their default values.
 */
extern void zclApp_ResetAttributesToDefaultValues(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ZCL_APP_H */
