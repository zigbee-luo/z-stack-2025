/**************************************************************************************************
  Filename:       zcl_appliance_events_alerts.c
  Revised:        $Date: 2013-10-16 16:38:58 -0700 (Wed, 16 Oct 2013) $
  Revision:       $Revision: 35701 $

  Description:    Zigbee Cluster Library - Appliance Events & Alerts


  Copyright (c) 2019, Texas Instruments Incorporated
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  *  Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

  *  Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  *  Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "ti_zstack_config.h"
#include "zcl.h"
#include "zcl_general.h"
#include "zcl_appliance_events_alerts.h"

#if defined ( INTER_PAN ) || defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  #include "stub_aps.h"
#endif

#ifdef ZCL_APPLIANCE_EVENTS_ALERTS
/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */
typedef struct zclApplianceEventsAlertsCBRec
{
  struct zclApplianceEventsAlertsCBRec *next;
  uint8_t endpoint;                                   // Used to link it into the endpoint descriptor
  zclApplianceEventsAlerts_AppCallbacks_t *CBs;     // Pointer to Callback function
} zclApplianceEventsAlertsCBRec_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
static zclApplianceEventsAlertsCBRec_t *zclApplianceEventsAlertsCBs = (zclApplianceEventsAlertsCBRec_t *)NULL;
static uint8_t zclApplianceEventsAlertsPluginRegisted = FALSE;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static ZStatus_t zclApplianceEventsAlerts_HdlIncoming( zclIncoming_t *pInHdlrMsg );
static ZStatus_t zclApplianceEventsAlerts_HdlInSpecificCommands( zclIncoming_t *pInMsg );
static zclApplianceEventsAlerts_AppCallbacks_t *zclApplianceEventsAlerts_FindCallbacks( uint8_t endpoint );
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmds( zclIncoming_t *pInMsg, zclApplianceEventsAlerts_AppCallbacks_t *pCBs );

static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_GetAlerts( zclIncoming_t *pInMsg, zclApplianceEventsAlerts_AppCallbacks_t *pCBs );
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_GetAlertsRsp( zclIncoming_t *pInMsg, zclApplianceEventsAlerts_AppCallbacks_t *pCBs );
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_AlertsNotification( zclIncoming_t *pInMsg, zclApplianceEventsAlerts_AppCallbacks_t *pCBs );
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_EventNotification( zclIncoming_t *pInMsg, zclApplianceEventsAlerts_AppCallbacks_t *pCBs );

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_RegisterCmdCallbacks
 *
 * @brief   Register applications command callbacks
 *
 * @param   endpoint - application's endpoint
 * @param   callbacks - pointer to the callback record.
 *
 * @return  ZMemError if not able to allocate
 */
ZStatus_t zclApplianceEventsAlerts_RegisterCmdCallbacks( uint8_t endpoint, zclApplianceEventsAlerts_AppCallbacks_t *callbacks )
{
  zclApplianceEventsAlertsCBRec_t *pNewItem;
  zclApplianceEventsAlertsCBRec_t *pLoop;

  // Register as a ZCL Plugin
  if ( zclApplianceEventsAlertsPluginRegisted == FALSE )
  {
    zcl_registerPlugin( ZCL_CLUSTER_ID_HA_APPLIANCE_EVENTS_ALERTS,
                        ZCL_CLUSTER_ID_HA_APPLIANCE_EVENTS_ALERTS,
                        zclApplianceEventsAlerts_HdlIncoming );
    zclApplianceEventsAlertsPluginRegisted = TRUE;
  }

  // Fill in the new profile list
  pNewItem = zcl_mem_alloc( sizeof( zclApplianceEventsAlertsCBRec_t ) );
  if ( pNewItem == NULL )
  {
    return ( ZMemError );
  }

  pNewItem->next = (zclApplianceEventsAlertsCBRec_t *)NULL;
  pNewItem->endpoint = endpoint;
  pNewItem->CBs = callbacks;

  // Find spot in list
  if ( zclApplianceEventsAlertsCBs == NULL )
  {
    zclApplianceEventsAlertsCBs = pNewItem;
  }
  else
  {
    // Look for end of list
    pLoop = zclApplianceEventsAlertsCBs;
    while ( pLoop->next != NULL )
      pLoop = pLoop->next;

    // Put new item at end of list
    pLoop->next = pNewItem;
  }

  return ( ZSuccess );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_Send_GetAlerts
 *
 * @brief   Request sent to server for Get Alerts info.
 *
 * @param   srcEP - Sending application's endpoint
 * @param   dstAddr - where you want the message to go
 * @param   disableDefaultRsp - whether to disable the Default Response command
 * @param   seqNum - sequence number
 *
 * @return  ZStatus_t
 */
extern ZStatus_t zclApplianceEventsAlerts_Send_GetAlerts( uint8_t srcEP, afAddrType_t *dstAddr,
                                                          uint8_t disableDefaultRsp, uint8_t seqNum )
{
  // no payload

  return zcl_SendCommand( srcEP, dstAddr, ZCL_CLUSTER_ID_HA_APPLIANCE_EVENTS_ALERTS,
                          COMMAND_APPLIANCE_EVENTS_ALERTS_GET_ALERTS, TRUE,
                          ZCL_FRAME_CLIENT_SERVER_DIR, disableDefaultRsp, 0, seqNum, 0, 0 );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_Send_GetAlertsRsp
 *
 * @brief   Response sent to client due to GetAlertsRsp cmd.
 *
 * @param   srcEP - Sending application's endpoint
 * @param   dstAddr - where you want the message to go
 * @param   pPayload:
 *          alertsCount - Contains the length of the alert structure array
 *          aAlert - Contains the information of the Alert
 * @param   disableDefaultRsp - whether to disable the Default Response command
 * @param   seqNum - sequence number
 *
 * @return  ZStatus_t
 */
extern ZStatus_t zclApplianceEventsAlerts_Send_GetAlertsRsp( uint8_t srcEP, afAddrType_t *dstAddr,
                                                             zclApplianceEventsAlerts_t *pPayload,
                                                             uint8_t disableDefaultRsp, uint8_t seqNum )
{
  uint8_t i;
  uint8_t j;
  uint8_t *pBuf;    // variable length payload
  uint8_t offset;
  uint16_t calculatedBufSize;
  ZStatus_t status;

  // get a buffer large enough to hold the whole packet
  calculatedBufSize = ( (pPayload->alertsCount) * sizeof(alertStructureRecord_t) + 1 );  // size of variable array plus alertsCount

  pBuf = zcl_mem_alloc( calculatedBufSize );
  if ( !pBuf )
  {
    return ( ZMemError );  // no memory
  }

  // over-the-air is always little endian. Break into a byte stream.
  pBuf[0] = pPayload->alertsCount;
  offset = 1;
  for( i = 0; i < ( pPayload->alertsCount ); i++ )
  {
    for( j = 0; j < 3; j++ )
    {
      pBuf[offset++] = pPayload->pAlertStructureRecord[i].aAlert[j];
    }
  }

  status = zcl_SendCommand( srcEP, dstAddr, ZCL_CLUSTER_ID_HA_APPLIANCE_EVENTS_ALERTS,
                           COMMAND_APPLIANCE_EVENTS_ALERTS_GET_ALERTS_RSP, TRUE,
                           ZCL_FRAME_SERVER_CLIENT_DIR, disableDefaultRsp, 0, seqNum, calculatedBufSize, pBuf );
  zcl_mem_free( pBuf );

  return ( status );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_Send_AlertsNotification
 *
 * @brief   Response sent to client due to AlertsNotification cmd.
 *
 * @param   srcEP - Sending application's endpoint
 * @param   dstAddr - where you want the message to go
 * @param   pPayload:
 *          alertsCount - Contains the length of the alert structure array
 *          aAlert - Contains the information of the Alert
 * @param   disableDefaultRsp - whether to disable the Default Response command
 * @param   seqNum - sequence number
 *
 * @return  ZStatus_t
 */
extern ZStatus_t zclApplianceEventsAlerts_Send_AlertsNotification( uint8_t srcEP, afAddrType_t *dstAddr,
                                                                   zclApplianceEventsAlerts_t *pPayload,
                                                                   uint8_t disableDefaultRsp, uint8_t seqNum )
{
  uint8_t i;
  uint8_t j;
  uint8_t *pBuf;    // variable length payload
  uint8_t offset;
  uint16_t calculatedBufSize;
  ZStatus_t status;

  // get a buffer large enough to hold the whole packet
  calculatedBufSize = ( (pPayload->alertsCount) * sizeof(alertStructureRecord_t) + 1 );  // size of variable array plus alertsCount

  pBuf = zcl_mem_alloc( calculatedBufSize );
  if ( !pBuf )
  {
    return ( ZMemError );  // no memory
  }

  // over-the-air is always little endian. Break into a byte stream.
  pBuf[0] = pPayload->alertsCount;
  offset = 1;
  for( i = 0; i < ( pPayload->alertsCount ); i++ )
  {
    for( j = 0; j < 3; j++ )
    {
      pBuf[offset++] = pPayload->pAlertStructureRecord[i].aAlert[j];
    }
  }

  status = zcl_SendCommand( srcEP, dstAddr, ZCL_CLUSTER_ID_HA_APPLIANCE_EVENTS_ALERTS,
                           COMMAND_APPLIANCE_EVENTS_ALERTS_ALERTS_NOTIFICATION, TRUE,
                           ZCL_FRAME_SERVER_CLIENT_DIR, disableDefaultRsp, 0, seqNum, calculatedBufSize, pBuf );
  zcl_mem_free( pBuf );

  return ( status );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_Send_EventNotification
 *
 * @brief   Response sent to client for Event Notification.
 *
 * @param   srcEP - Sending application's endpoint
 * @param   dstAddr - where you want the message to go
 * @param   eventHeader - a reserved field set to 0
 * @param   eventID - Identifies the event to be notified
 * @param   disableDefaultRsp - whether to disable the Default Response command
 * @param   seqNum - sequence number
 *
 * @return  ZStatus_t
 */
extern ZStatus_t zclApplianceEventsAlerts_Send_EventNotification( uint8_t srcEP, afAddrType_t *dstAddr,
                                                                  uint8_t eventHeader, uint8_t eventID,
                                                                  uint8_t disableDefaultRsp, uint8_t seqNum )
{
  uint8_t buf[2]; // 2 byte payload

  buf[0] = eventHeader;
  buf[1] = eventID;

  return zcl_SendCommand( srcEP, dstAddr, ZCL_CLUSTER_ID_HA_APPLIANCE_EVENTS_ALERTS,
                          COMMAND_APPLIANCE_EVENTS_ALERTS_EVENT_NOTIFICATION, TRUE,
                          ZCL_FRAME_SERVER_CLIENT_DIR, disableDefaultRsp, 0, seqNum, sizeof( buf ), buf );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_FindCallbacks
 *
 * @brief   Find the callbacks for an endpoint
 *
 * @param   endpoint - endpoint to find the application callbacks for
 *
 * @return  pointer to the callbacks
 */
static zclApplianceEventsAlerts_AppCallbacks_t *zclApplianceEventsAlerts_FindCallbacks( uint8_t endpoint )
{
  zclApplianceEventsAlertsCBRec_t *pCBs;

  pCBs = zclApplianceEventsAlertsCBs;
  while ( pCBs != NULL )
  {
    if ( pCBs->endpoint == endpoint )
    {
      return ( pCBs->CBs );
    }
    pCBs = pCBs->next;
  }
  return ( (zclApplianceEventsAlerts_AppCallbacks_t *)NULL );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_HdlIncoming
 *
 * @brief   Callback from ZCL to process incoming Commands specific
 *          to this cluster library or Profile commands for attributes
 *          that aren't in the attribute list
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclApplianceEventsAlerts_HdlIncoming( zclIncoming_t *pInMsg )
{
  ZStatus_t stat = ZSuccess;

#if defined ( INTER_PAN ) || defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  if ( StubAPS_InterPan( pInMsg->msg->srcAddr.panId, pInMsg->msg->srcAddr.endPoint ) )
  {
    return ( stat ); // Cluster not supported thru Inter-PAN
  }
#endif
  if ( zcl_ClusterCmd( pInMsg->hdr.fc.type ) )
  {
    // Is this a manufacturer specific command?
    if ( pInMsg->hdr.fc.manuSpecific == 0 )
    {
      if ( zcl_matchClusterId( pInMsg ) ) //match cluster ID support.
      {
#ifdef  ZCL_CMD_MATCH
        zclCommandRec_t cmdRec;
        uint8_t matchFlag = 0;
        stat = ZFailure;
        if( zcl_ServerCmd(pInMsg->hdr.fc.direction) )
        {
          matchFlag = CMD_DIR_SERVER_RECEIVED;
        }
        else
        {
          matchFlag = CMD_DIR_CLIENT_RECEIVED;
        }
        if( TRUE == zclFindCmdRec( pInMsg->msg->endPoint, pInMsg->msg->clusterId,
                                   pInMsg->hdr.commandID, matchFlag, &cmdRec ) )
        {
          stat = zclApplianceEventsAlerts_HdlInSpecificCommands( pInMsg );
        }
#else
        stat = zclApplianceEventsAlerts_HdlInSpecificCommands( pInMsg );
#endif
      }
      else
      {
        stat = ZFailure;
      }
    }
    else
    {
      // We don't support any manufacturer specific command.
      stat = ZFailure;
    }
  }
  else
  {
    // Handle all the normal (Read, Write...) commands -- should never get here
    stat = ZFailure;
  }
  return ( stat );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_HdlInSpecificCommands
 *
 * @brief   Callback from ZCL to process incoming Commands specific
 *          to this cluster library

 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclApplianceEventsAlerts_HdlInSpecificCommands( zclIncoming_t *pInMsg )
{
  ZStatus_t stat = ZSuccess;
  zclApplianceEventsAlerts_AppCallbacks_t *pCBs;

  // make sure endpoint exists
  pCBs = zclApplianceEventsAlerts_FindCallbacks( pInMsg->msg->endPoint );
  if (pCBs == NULL )
  {
    return ( ZFailure );
  }

  stat = zclApplianceEventsAlerts_ProcessInCmds( pInMsg, pCBs );

  return ( stat );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_ProcessInCmds
 *
 * @brief   Callback from ZCL to process incoming Commands specific
 *          to this cluster library on a command ID basis

 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the application callbacks
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmds( zclIncoming_t *pInMsg, zclApplianceEventsAlerts_AppCallbacks_t *pCBs )
{
  ZStatus_t stat;

  // Client-to-Server
  if( zcl_ServerCmd( pInMsg->hdr.fc.direction ) )
  {
    switch( pInMsg->hdr.commandID )
    {
      case COMMAND_APPLIANCE_EVENTS_ALERTS_GET_ALERTS:
        stat = zclApplianceEventsAlerts_ProcessInCmd_GetAlerts( pInMsg, pCBs );
        break;

      default:
        // Unknown command
        stat = ZFailure;
        break;
    }
  }
  // Server-to-Client
  else
  {
    switch( pInMsg->hdr.commandID )
    {
      case COMMAND_APPLIANCE_EVENTS_ALERTS_GET_ALERTS_RSP:
        stat = zclApplianceEventsAlerts_ProcessInCmd_GetAlertsRsp( pInMsg, pCBs );
        break;

      case COMMAND_APPLIANCE_EVENTS_ALERTS_ALERTS_NOTIFICATION:
        stat = zclApplianceEventsAlerts_ProcessInCmd_AlertsNotification( pInMsg, pCBs );
        break;

      case COMMAND_APPLIANCE_EVENTS_ALERTS_EVENT_NOTIFICATION:
        stat = zclApplianceEventsAlerts_ProcessInCmd_EventNotification( pInMsg, pCBs );
        break;

      default:
        // Unknown command
        stat = ZFailure;
        break;
    }
  }

  return ( stat );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_ProcessInCmd_GetAlerts
 *
 * @brief   Process in the received Get Alerts cmd
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the application callbacks
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_GetAlerts( zclIncoming_t *pInMsg,
                                                                  zclApplianceEventsAlerts_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnApplianceEventsAlerts_GetAlerts )
  {
    // no payload

    return ( pCBs->pfnApplianceEventsAlerts_GetAlerts( ) );
  }

  return ( ZFailure );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_ProcessInCmd_GetAlertsRsp
 *
 * @brief   Process in the received Get Alerts Response cmd
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the application callbacks
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_GetAlertsRsp( zclIncoming_t *pInMsg,
                                                                     zclApplianceEventsAlerts_AppCallbacks_t *pCBs )
{
  uint8_t i;
  uint8_t j;
  uint8_t offset;
  uint16_t arrayRecordSize;
  zclApplianceEventsAlerts_t cmd;
  ZStatus_t status;

  if ( pCBs->pfnApplianceEventsAlerts_GetAlertsRsp )
  {
    // calculate size of variable array, accounting for size of aAlert being a uint24
    arrayRecordSize = 3 * pInMsg->pData[0];

    cmd.pAlertStructureRecord = zcl_mem_alloc( arrayRecordSize );
    if ( !cmd.pAlertStructureRecord )
    {
      return ( ZMemError );  // no memory, return failure
    }

    cmd.alertsCount = pInMsg->pData[0];
    offset = 1;
    for( i = 0; i < pInMsg->pData[0]; i++ )
    {
      for( j = 0; j < 3; j++ )
      {
        cmd.pAlertStructureRecord[i].aAlert[j] = pInMsg->pData[offset++];
      }
    }

    status = ( pCBs->pfnApplianceEventsAlerts_GetAlertsRsp( &cmd ) );
    zcl_mem_free( cmd.pAlertStructureRecord );
    return status;
  }

  return ( ZFailure );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_ProcessInCmd_AlertsNotification
 *
 * @brief   Process in the received Alerts Notification cmd
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the application callbacks
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_AlertsNotification( zclIncoming_t *pInMsg,
                                                                           zclApplianceEventsAlerts_AppCallbacks_t *pCBs )
{
  uint8_t i;
  uint8_t j;
  uint8_t offset;
  uint16_t arrayRecordSize;
  zclApplianceEventsAlerts_t cmd;
  ZStatus_t status;

  if ( pCBs->pfnApplianceEventsAlerts_AlertsNotification )
  {
    // calculate size of variable array, accounting for size of aAlert being a uint24
    arrayRecordSize = 3 * pInMsg->pData[0];

    cmd.pAlertStructureRecord = zcl_mem_alloc( arrayRecordSize );
    if ( !cmd.pAlertStructureRecord )
    {
      return ( ZMemError );  // no memory, return failure
    }

    cmd.alertsCount = pInMsg->pData[0];
    offset = 1;
    for( i = 0; i < pInMsg->pData[0]; i++ )
    {
      for( j = 0; j < 3; j++ )
      {
        cmd.pAlertStructureRecord[i].aAlert[j] = pInMsg->pData[offset++];
      }
    }

    status = ( pCBs->pfnApplianceEventsAlerts_AlertsNotification( &cmd ) );
    zcl_mem_free( cmd.pAlertStructureRecord );
    return status;
  }

  return ( ZFailure );
}

/*********************************************************************
 * @fn      zclApplianceEventsAlerts_ProcessInCmd_EventNotification
 *
 * @brief   Process in the received Event Notification cmd
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the application callbacks
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclApplianceEventsAlerts_ProcessInCmd_EventNotification( zclIncoming_t *pInMsg,
                                                                          zclApplianceEventsAlerts_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnApplianceEventsAlerts_EventNotification )
  {
    zclApplianceEventsAlertsEventNotification_t cmd;

    cmd.eventHeader = pInMsg->pData[0];
    cmd.eventID = pInMsg->pData[1];

    return ( pCBs->pfnApplianceEventsAlerts_EventNotification( &cmd ) );
  }

  return ( ZFailure );
}

/****************************************************************************
****************************************************************************/

#endif // ZCL_APPLIANCE_EVENTS_ALERTS
