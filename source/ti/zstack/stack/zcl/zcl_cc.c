/**************************************************************************************************
  Filename:       zcl_cc.c
  Revised:        $Date: 2013-06-11 13:53:09 -0700 (Tue, 11 Jun 2013) $
  Revision:       $Revision: 34523 $

  Description:    Zigbee Cluster Library - Commissioning Cluster


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
#include "zcl_cc.h"

#if defined ( INTER_PAN ) || defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  #include "stub_aps.h"
#endif

#ifdef ZCL_CC
/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */
typedef struct zclCCCBRec
{
  struct zclCCCBRec    *next;
  uint8_t                endpoint; // Used to link it into the endpoint descriptor
  zclCC_AppCallbacks_t *CBs;     // Pointer to Callback function
} zclCCCBRec_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
static zclCCCBRec_t *zclCCCBs = (zclCCCBRec_t *)NULL;
static uint8_t zclCCPluginRegisted = FALSE;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*
 * Callback from ZCL to process incoming Commands specific to this cluster library
 */
static ZStatus_t zclCC_HdlIncoming(  zclIncoming_t *pInMsg );

/*
 * Function to process incoming Commands specific to this cluster library
 */
static ZStatus_t zclCC_HdlInSpecificCommands( zclIncoming_t *pInMsg );

/*
 * Process received Restart Device Command
 */
static ZStatus_t zclCC_ProcessInCmd_RestartDevice( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );

/*
 * Process received Save Startup Parameters Command
 */
static ZStatus_t zclCC_ProcessInCmd_SaveStartupParams( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );

/*
 * Process received Restore Startup Parameters Command
 */
static ZStatus_t zclCC_ProcessInCmd_RestoreStartupParams( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );

/*
 * Process received Reset Startup Parameters Command
 */
static ZStatus_t zclCC_ProcessInCmd_ResetStartupParams( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );

/*
 * Process received Restart Device Response
 */
static ZStatus_t zclCC_ProcessInCmd_RestartDeviceRsp( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );

/*
 * Process received Save Startup Parameters Response
 */
static ZStatus_t zclCC_ProcessInCmd_SaveStartupParamsRsp( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );

/*
 * Process received Restore Startup Parameters Response
 */
static ZStatus_t zclCC_ProcessInCmd_RestoreStartupParamsRsp( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );

/*
 * Process received Reset Startup Parameters Response
 */
static ZStatus_t zclCC_ProcessInCmd_ResetStartupParamsRsp( zclIncoming_t *pInMsg, zclCC_AppCallbacks_t *pCBs );


/*********************************************************************
 * @fn      zclCC_RegisterCmdCallbacks
 *
 * @brief   Register an applications command callbacks
 *
 * @param   endpoint - application's endpoint
 * @param   callbacks - pointer to the callback record.
 *
 * @return  ZSuccess if successful. ZMemError if not able to allocate
 */
ZStatus_t zclCC_RegisterCmdCallbacks( uint8_t endpoint, zclCC_AppCallbacks_t *callbacks )
{
  zclCCCBRec_t *pNewItem;
  zclCCCBRec_t *pLoop;

  // Register as a ZCL Plugin
  if ( !zclCCPluginRegisted )
  {
    zcl_registerPlugin( ZCL_CLUSTER_ID_GENERAL_COMMISSIONING,
                        ZCL_CLUSTER_ID_GENERAL_COMMISSIONING,
                        zclCC_HdlIncoming );
    zclCCPluginRegisted = TRUE;
  }

  // Fill in the new profile list
  pNewItem = zcl_mem_alloc( sizeof( zclCCCBRec_t ) );
  if ( pNewItem == NULL )
  {
    return ( ZMemError );
  }

  pNewItem->next = (zclCCCBRec_t *)NULL;
  pNewItem->endpoint = endpoint;
  pNewItem->CBs = callbacks;

  // Find spot in list
  if ( zclCCCBs == NULL )
  {
    zclCCCBs = pNewItem;
  }
  else
  {
    // Look for end of list
    pLoop = zclCCCBs;
    while ( pLoop->next != NULL )
    {
      pLoop = pLoop->next;
    }

    // Put new item at end of list
    pLoop->next = pNewItem;
  }

  return ( ZSuccess );
}

/*********************************************************************
 * @fn      zclCC_FindCallbacks
 *
 * @brief   Find the callbacks for an endpoint
 *
 * @param   endpoint
 *
 * @return  pointer to the callbacks
 */
static zclCC_AppCallbacks_t *zclCC_FindCallbacks( uint8_t endpoint )
{
  zclCCCBRec_t *pCBs;

  pCBs = zclCCCBs;
  while ( pCBs != NULL )
  {
    if ( pCBs->endpoint == endpoint )
    {
      return ( pCBs->CBs );
    }

    pCBs = pCBs->next;
  }

  return ( (zclCC_AppCallbacks_t *)NULL );
}

/*********************************************************************
 * @fn      zclCC_HdlIncoming
 *
 * @brief   Callback from ZCL to process incoming Commands specific
 *          to this cluster library or Profile commands for attributes
 *          that aren't in the attribute list
 *
 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclCC_HdlIncoming(  zclIncoming_t *pInMsg )
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
          stat = zclCC_HdlInSpecificCommands( pInMsg );
        }
#else
        stat = zclCC_HdlInSpecificCommands( pInMsg );
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
 * @fn      zclCC_HdlInSpecificCommands
 *
 * @brief   Function to process incoming Commands specific
 *          to this cluster library

 * @param   pInMsg - pointer to the incoming message
 *
 * @return  ZStatus_t
 */
static ZStatus_t zclCC_HdlInSpecificCommands( zclIncoming_t *pInMsg )
{
  ZStatus_t stat;
  zclCC_AppCallbacks_t *pCBs;

  // Make sure endpoint exists
  pCBs = zclCC_FindCallbacks( pInMsg->msg->endPoint );
  if ( pCBs == NULL )
  {
    return ( ZFailure );
  }

  if ( zcl_ServerCmd( pInMsg->hdr.fc.direction ) )
  {
    // Process Client commands, received by server
    switch ( pInMsg->hdr.commandID )
    {
      case COMMAND_CC_RESTART_DEVICE:
        stat = zclCC_ProcessInCmd_RestartDevice( pInMsg, pCBs );
        break;

      case COMMAND_CC_SAVE_STARTUP_PARAMS:
        stat = zclCC_ProcessInCmd_SaveStartupParams( pInMsg, pCBs );
        break;

      case COMMAND_CC_RESTORE_STARTUP_PARAMS:
        stat = zclCC_ProcessInCmd_RestoreStartupParams( pInMsg, pCBs );
        break;

      case COMMAND_CC_RESET_STARTUP_PARAMS:
        stat = zclCC_ProcessInCmd_ResetStartupParams( pInMsg, pCBs );
        break;

      default:
        stat = ZFailure;
        break;
    }
  }
  else
  {
    switch ( pInMsg->hdr.commandID )
    {
      case COMMAND_CC_RESTART_DEVICE_RSP:
        stat = zclCC_ProcessInCmd_RestartDeviceRsp( pInMsg, pCBs );
        break;

      case COMMAND_CC_SAVE_STARTUP_PARAMS_RSP:
        stat = zclCC_ProcessInCmd_SaveStartupParamsRsp( pInMsg, pCBs );
        break;

      case COMMAND_CC_RESTORE_STARTUP_PARAMS_RSP:
        stat = zclCC_ProcessInCmd_RestoreStartupParamsRsp( pInMsg, pCBs );
        break;

      case COMMAND_CC_RESET_STARTUP_PARAMS_RSP:
        stat = zclCC_ProcessInCmd_ResetStartupParamsRsp( pInMsg, pCBs );
        break;

      default:
        stat = ZFailure;
        break;
    }
  }

  return ( stat );
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_RestartDevice
 *
 * @brief   Process in the received Restart Device Command.
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_RestartDevice( zclIncoming_t *pInMsg,
                                                   zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnRestart_Device )
  {
    zclCCRestartDevice_t cmd;

    cmd.options = pInMsg->pData[0];
    cmd.delay = pInMsg->pData[1];
    cmd.jitter = pInMsg->pData[2];

    pCBs->pfnRestart_Device( &cmd, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZCL_STATUS_CMD_HAS_RSP;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_SaveStartupParams
 *
 * @brief   Process in the received Save Startup Parameters Command.
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_SaveStartupParams( zclIncoming_t *pInMsg,
                                                       zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnSave_StartupParams )
  {
    zclCCStartupParams_t cmd;

    cmd.options = pInMsg->pData[0];
    cmd.index = pInMsg->pData[1];

    pCBs->pfnSave_StartupParams( &cmd, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZCL_STATUS_CMD_HAS_RSP;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_RestoreStartupParams
 *
 * @brief   Process in the received Restore Startup Parameters Command.
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_RestoreStartupParams( zclIncoming_t *pInMsg,
                                                          zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnRestore_StartupParams )
  {
    zclCCStartupParams_t cmd;

    cmd.options = pInMsg->pData[0];
    cmd.index = pInMsg->pData[1];

    pCBs->pfnRestore_StartupParams( &cmd, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZCL_STATUS_CMD_HAS_RSP;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_ResetStartupParams
 *
 * @brief   Process in the received Reset Startup Parameters Command.
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_ResetStartupParams( zclIncoming_t *pInMsg,
                                                        zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnReset_StartupParams )
  {
    zclCCStartupParams_t cmd;

    cmd.options = pInMsg->pData[0];
    cmd.index = pInMsg->pData[1];

    pCBs->pfnReset_StartupParams( &cmd, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZCL_STATUS_CMD_HAS_RSP;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_RestartDeviceRsp
 *
 * @brief   Process in the received Restart Device Response
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_RestartDeviceRsp( zclIncoming_t *pInMsg,
                                                      zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnRestart_DeviceRsp )
  {
    zclCCServerParamsRsp_t rsp;

    rsp.status = pInMsg->pData[0];

    pCBs->pfnRestart_DeviceRsp( &rsp, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZSuccess;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_SaveStartupParamsRsp
 *
 * @brief   Process in the received Save Startup Parameters Response
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_SaveStartupParamsRsp( zclIncoming_t *pInMsg,
                                                          zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnSave_StartupParamsRsp )
  {
    zclCCServerParamsRsp_t rsp;

    rsp.status = pInMsg->pData[0];

    pCBs->pfnSave_StartupParamsRsp( &rsp, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZSuccess;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_RestoreStartupParamsRsp
 *
 * @brief   Process in the received Restore Startup Parameters Response
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_RestoreStartupParamsRsp( zclIncoming_t *pInMsg,
                                                             zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnRestore_StartupParamsRsp )
  {
    zclCCServerParamsRsp_t rsp;

    rsp.status = pInMsg->pData[0];

    pCBs->pfnRestore_StartupParamsRsp( &rsp, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZSuccess;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_ProcessInCmd_ResetStartupParamsRsp
 *
 * @brief   Process in the received Reset Startup Parameters Response
 *
 * @param   pInMsg - pointer to the incoming message
 * @param   pCBs - pointer to the Application callback
 *
 * @return  ZStatus_t - status of the command processing
 */
static ZStatus_t zclCC_ProcessInCmd_ResetStartupParamsRsp( zclIncoming_t *pInMsg,
                                                           zclCC_AppCallbacks_t *pCBs )
{
  if ( pCBs->pfnReset_StartupParamsRsp )
  {
    zclCCServerParamsRsp_t rsp;

    rsp.status = pInMsg->pData[0];

    pCBs->pfnReset_StartupParamsRsp( &rsp, &(pInMsg->msg->srcAddr), pInMsg->hdr.transSeqNum );

    return ZSuccess;
  }

  return ZFailure;
}

/*********************************************************************
 * @fn      zclCC_Send_RestartDevice
 *
 * @brief   Call to send out a Restart Device command
 *
 * @param   srcEP - Sending application's endpoint
 * @param   dstAddr - where you want the message to go
 * @param   pCmd - pointer to Restart Command data structure
 * @param   disableDefaultRsp - disable default response
 * @param   seqNum - ZCL sequence number
 *
 * @return  ZStatus_t
 */
ZStatus_t zclCC_Send_RestartDevice( uint8_t srcEP, afAddrType_t *dstAddr,
                                    zclCCRestartDevice_t *pCmd,
                                    uint8_t disableDefaultRsp, uint8_t seqNum )
{
  uint8_t buf[CC_PACKET_LEN_RESTART_DEVICE];

  buf[0] = pCmd->options;
  buf[1] = pCmd->delay;
  buf[2] = pCmd->jitter;

  return zcl_SendCommand( srcEP, dstAddr, ZCL_CLUSTER_ID_GENERAL_COMMISSIONING,
                          COMMAND_CC_RESTART_DEVICE, TRUE,
                          ZCL_FRAME_CLIENT_SERVER_DIR, disableDefaultRsp, 0,
                          seqNum, CC_PACKET_LEN_RESTART_DEVICE, buf );
}

/*********************************************************************
 * @fn      zclCC_Send_StartupParamsCmd
 *
 * @brief   Call to send out a Startup parameter command (Restore, Save, Reset)
 *
 * @param   srcEP - Sending application's endpoint
 * @param   dstAddr - where you want the message to go
 * @param   pCmd - pointer to Startup Parameter Command data structure
 * @param   cmdId - Command type ( Restore, Save or Reset)
 * @param   disableDefaultRsp - disable default response
 * @param   seqNum - ZCL sequence number
 *
 * @return  ZStatus_t
 */
ZStatus_t zclCC_Send_StartupParamsCmd( uint8_t srcEP, afAddrType_t *dstAddr,
                                       zclCCStartupParams_t *pCmd, uint8_t cmdId,
                                       uint8_t disableDefaultRsp, uint8_t seqNum )
{
  uint8_t buf[CC_PACKET_LEN_STARTUP_PARAMS_CMD];

  buf[0] = pCmd->options;
  buf[1] = pCmd->index;

  return zcl_SendCommand( srcEP, dstAddr, ZCL_CLUSTER_ID_GENERAL_COMMISSIONING,
                          cmdId, TRUE, ZCL_FRAME_CLIENT_SERVER_DIR,
                          disableDefaultRsp, 0, seqNum,
                          CC_PACKET_LEN_STARTUP_PARAMS_CMD, buf );
}

/*********************************************************************
 * @fn      zclCC_Send_ServerParamsRsp
 *
 * @brief   Call to send out a Server Parameters Response to a client request
 *
 * @param   srcEP - Sending application's endpoint
 * @param   dstAddr - where you want the message to go
 * @param   pRsp - pointer to Startup Parameter Response data structure
 * @param   cmdId - Command type ( Restore, Save or Reset)
 * @param   disableDefaultRsp - disable default response
 * @param   seqNum - ZCL sequence number
 *
 * @return  ZStatus_t
 */
ZStatus_t zclCC_Send_ServerParamsRsp( uint8_t srcEP, afAddrType_t *dstAddr,
                                      zclCCServerParamsRsp_t *pRsp, uint8_t cmdId,
                                      uint8_t disableDefaultRsp, uint8_t seqNum )
{
  return zcl_SendCommand( srcEP, dstAddr, ZCL_CLUSTER_ID_GENERAL_COMMISSIONING,
                          cmdId, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR,
                          disableDefaultRsp, 0, seqNum,
                          CC_PACKET_LEN_SERVER_RSP, &(pRsp->status) );
}


/********************************************************************************************
*********************************************************************************************/

#endif // ZCL_CC

