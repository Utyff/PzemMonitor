#include "ZComDef.h"
#include "AF.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_ms.h"
#include "zcl_electrical_measurement.h"

#include "zcl_app.h"

#include "version.h"

/*********************************************************************
 * CONSTANTS
 */

#define APP_DEVICE_VERSION     1
#define APP_FLAGS              0

#define APP_HWVERSION          1
#define APP_ZCLVERSION         1

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * MACROS
 */

#define R ACCESS_CONTROL_READ
// ACCESS_CONTROL_AUTH_WRITE
#define W  (R | ACCESS_CONTROL_WRITE)
#define RW (R | ACCESS_CONTROL_WRITE | ACCESS_CONTROL_AUTH_WRITE)
#define RR (R | ACCESS_REPORTABLE)

//READ REPORT WRITE
#define RRW (R | ACCESS_REPORTABLE | ACCESS_CONTROL_WRITE | ACCESS_CONTROL_AUTH_WRITE)

#define BASIC       ZCL_CLUSTER_ID_GEN_BASIC
#define POWER_CFG   ZCL_CLUSTER_ID_GEN_POWER_CFG
#define GEN_TIME    ZCL_CLUSTER_ID_GEN_TIME

#define ZCL_BOOLEAN  ZCL_DATATYPE_BOOLEAN
#define ZCL_CHAR_STR ZCL_DATATYPE_CHAR_STR
#define ZCL_ENUM8    ZCL_DATATYPE_ENUM8
#define ZCL_UINT8    ZCL_DATATYPE_UINT8
#define ZCL_UINT16   ZCL_DATATYPE_UINT16
#define ZCL_INT16    ZCL_DATATYPE_INT16
#define ZCL_INT8     ZCL_DATATYPE_INT8
#define ZCL_INT32    ZCL_DATATYPE_INT32
#define ZCL_UINT32   ZCL_DATATYPE_UINT32
#define ZCL_SINGLE   ZCL_DATATYPE_SINGLE_PREC
#define ZCL_UTC      ZCL_DATATYPE_UTC

/*********************************************************************
 * GLOBAL VARIABLES
 */

// Global attributes
const uint16 zclApp_clusterRevision_all = 0x0001;

uint32 zclApp_GenTime_TimeUTC = 0;

// Basic Cluster
const uint8 zclApp_HWRevision = APP_HWVERSION;
const uint8 zclApp_ZCLVersion = APP_ZCLVERSION;
const uint8 zclApp_ManufacturerName[] = { 6, 'D', 'I', 'Y', 'R', 'u', 'Z' };
const uint8 zclApp_ModelId[] = {18, 'D', 'I', 'Y', 'R', 'u', 'Z', '_', 'P', 'z', 'e', 'm', 'M', 'o', 'n', 'i', 't', 'o', 'r' };
const uint8 zclApp_PowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 zclApp_LocationDescription[17] = { 16, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
uint8 zclApp_PhysicalEnvironment = 0;
uint8 zclApp_DeviceEnable = DEVICE_ENABLED;

// Состояние реле
extern uint8 RELAY_STATE;

/*********************************************************************
 * ATTRIBUTE DEFINITIONS - Uses REAL cluster IDs
 */
CONST zclAttrRec_t zclApp_Attrs[] = {
  // *** General Basic Cluster Attributes ***
  // Cluster IDs - defined in the foundation (ie. zcl.h)
  // Attribute record
  // Attribute ID - Found in Cluster Library header (ie. zcl_general.h)
  // Data Type - found in zcl.h
  // Variable access control - found in zcl.h
  // Pointer to attribute variable
  {BASIC,{ATTRID_BASIC_HW_VERSION,        ZCL_UINT8,    R, (void *)&zclApp_HWRevision}},
  {BASIC,{ATTRID_BASIC_ZCL_VERSION,       ZCL_UINT8,    R, (void *)&zclApp_ZCLVersion}},
  {BASIC,{ATTRID_BASIC_APPL_VERSION,      ZCL_UINT8,    R, (void *)&zclApp_ZCLVersion}},
  {BASIC,{ATTRID_BASIC_STACK_VERSION,     ZCL_UINT8,    R, (void *)&zclApp_ZCLVersion}},
  {BASIC,{ATTRID_BASIC_SW_BUILD_ID,       ZCL_CHAR_STR, R, (void *)zclApp_DateCode}},
  {BASIC,{ATTRID_BASIC_MANUFACTURER_NAME, ZCL_CHAR_STR, R, (void *)zclApp_ManufacturerName}},
  {BASIC,{ATTRID_BASIC_MODEL_ID,          ZCL_CHAR_STR, R, (void *)zclApp_ModelId}},
  {BASIC,{ATTRID_BASIC_DATE_CODE,         ZCL_CHAR_STR, R, (void *)zclApp_DateCode}},
  {BASIC,{ATTRID_BASIC_POWER_SOURCE,      ZCL_ENUM8,    R, (void *)&zclApp_PowerSource}},
  {BASIC,{ATTRID_BASIC_LOCATION_DESC,     ZCL_CHAR_STR, W, (void *)zclApp_LocationDescription}},
  {BASIC,{ATTRID_BASIC_PHYSICAL_ENV,      ZCL_ENUM8,    W, (void *)&zclApp_PhysicalEnvironment}},
  {BASIC,{ATTRID_BASIC_DEVICE_ENABLED,    ZCL_BOOLEAN,  W, (void *)&zclApp_DeviceEnable}},

  {BASIC,{ATTRID_CLUSTER_REVISION, ZCL_UINT16, R, (void *)&zclApp_clusterRevision_all}},

  // Electrical Measurements Cluster Attributes
  {ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, {ATTRID_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE,        ZCL_UINT16, R, (void *) &measurement.voltage}},
  {ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, {ATTRID_ELECTRICAL_MEASUREMENT_RMS_CURRENT,        ZCL_UINT32, R, (void *) &measurement.current}},
  {ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, {ATTRID_ELECTRICAL_MEASUREMENT_ACTIVE_POWER,       ZCL_UINT32, R, (void *) &measurement.power}},
  {ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, {ATTRID_ELECTRICAL_MEASUREMENT_TOTAL_ACTIVE_POWER, ZCL_UINT32, R, (void *) &measurement.energy}},
  {ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, {ATTRID_ELECTRICAL_MEASUREMENT_AC_FREQUENCY,       ZCL_UINT16, R, (void *) &measurement.frequency}},
  {ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT, {ATTRID_ELECTRICAL_MEASUREMENT_POWER_FACTOR,       ZCL_UINT16, R, (void *) &measurement.powerFactor}},

  {ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,{ATTRID_CLUSTER_REVISION,  ZCL_UINT16, R, (void *)&zclApp_clusterRevision_all}},

  {GEN_TIME, {ATTRID_TIME_TIME,       ZCL_UTC,    RRW, (void *) &zclApp_GenTime_TimeUTC}},
  {GEN_TIME, {ATTRID_TIME_LOCAL_TIME, ZCL_UINT32, RRW, (void *) &zclApp_GenTime_TimeUTC}},
};

uint8 CONST zclApp_NumAttributes = (sizeof(zclApp_Attrs) / sizeof(zclApp_Attrs[0]));

/*********************************************************************
 * SIMPLE DESCRIPTOR
 */
// This is the Cluster ID List and should be filled with Application
// specific cluster IDs.
const cId_t zclApp_InClusterList[] = {
  BASIC,
  ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT,
  GEN_TIME
};

#define ZCLAPP_MAX_INCLUSTERS   (sizeof(zclApp_InClusterList) / sizeof(zclApp_InClusterList[0]))


const cId_t zclApp_OutClusterList[] = {
  BASIC
};

#define ZCLAPP_MAX_OUTCLUSTERS  (sizeof(zclApp_OutClusterList) / sizeof(zclApp_OutClusterList[0]))


SimpleDescriptionFormat_t zclApp_Desc = {
  APP_ENDPOINT,                  //  int Endpoint;
  ZCL_HA_PROFILE_ID,             //  uint16 AppProfId;
  ZCL_HA_DEVICEID_METER_INTERFACE,//  uint16 AppDeviceId;
  APP_DEVICE_VERSION,            //  int   AppDevVer:4;
  APP_FLAGS,                     //  int   AppFlags:4;
  ZCLAPP_MAX_INCLUSTERS,         //  byte  AppNumInClusters;
  (cId_t *)zclApp_InClusterList, //  byte *pAppInClusterList;
  ZCLAPP_MAX_OUTCLUSTERS,        //  byte  AppNumInClusters;
  (cId_t *)zclApp_OutClusterList //  byte *pAppInClusterList;
};

// Added to include ZLL Target functionality
#if defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
bdbTLDeviceInfo_t tlApp_DeviceInfo =
{
  APP_ENDPOINT,                  //uint8 endpoint;
  ZCL_HA_PROFILE_ID,                        //uint16 profileID;
  // APP_TODO: Replace ZCL_HA_DEVICEID_ON_OFF_LIGHT with application specific device ID
  ZCL_HA_DEVICEID_ON_OFF_LIGHT,          //uint16 deviceID;
  APP_DEVICE_VERSION,                    //uint8 version;
  APP_NUM_GRPS                   //uint8 grpIdCnt;
};
#endif

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL FUNCTIONS
 */

// initialize cluster attribute variables.
void zclApp_ResetAttributesToDefaultValues(void) {
    int i;

    zclApp_LocationDescription[0] = 16;
    for (i = 1; i <= 16; i++) {
        zclApp_LocationDescription[i] = ' ';
    }

    zclApp_PhysicalEnvironment = PHY_UNSPECIFIED_ENV;
    zclApp_DeviceEnable = DEVICE_ENABLED;

    measurement.voltage = 0;
    measurement.current = 0;
    measurement.power = 0;
    measurement.energy = 0;
    measurement.frequency = 0;
    measurement.powerFactor = 0;
}
