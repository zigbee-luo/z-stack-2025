/**************************************************************************************************
  Filename:       bdb_finding_and_binding.c
  Revised:        $Date: 2016-02-25 11:51:49 -0700 (Thu, 25 Feb 2016) $
  Revision:       $Revision: - $

  Description:    This file contains the Base Device Behavior functions and attributes.


  Copyright 2006-2015 Texas Instruments Incorporated.

  All rights reserved not granted herein.
  Limited License.

  Texas Instruments Incorporated grants a world-wide, royalty-free,
  non-exclusive license under copyrights and patents it now or hereafter
  owns or controls to make, have made, use, import, offer to sell and sell
  ("Utilize") this software subject to the terms herein. With respect to the
  foregoing patent license, such license is granted solely to the extent that
  any such patent is necessary to Utilize the software alone. The patent
  license shall not apply to any combinations which include this software,
  other than combinations with devices manufactured by or for TI ("TI
  Devices"). No hardware patent is licensed hereunder.

  Redistributions must preserve existing copyright notices and reproduce
  this license (including the above copyright notice and the disclaimer and
  (if applicable) source code license limitations below) in the documentation
  and/or other materials provided with the distribution.

  Redistribution and use in binary form, without modification, are permitted
  provided that the following conditions are met:

    * No reverse engineering, decompilation, or disassembly of this software
      is permitted with respect to any software provided in binary form.
    * Any redistribution and use are licensed by TI for use only with TI Devices.
    * Nothing shall obligate TI to provide you with source code for the software
      licensed and provided to you in object code.

  If software source code is provided to you, modification and redistribution
  of the source code are permitted provided that the following conditions are
  met:

    * Any redistribution and use of the source code, including any resulting
      derivative works, are licensed by TI for use only with TI Devices.
    * Any redistribution and use of any object code compiled from the source
      code and any resulting derivative works, are licensed by TI for use
      only with TI Devices.

  Neither the name of Texas Instruments Incorporated nor the names of its
  suppliers may be used to endorse or promote products derived from this
  software without specific prior written permission.

  DISCLAIMER.

  THIS SOFTWARE IS PROVIDED BY TI AND TI'S LICENSORS "AS IS" AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL TI AND TI'S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "ti_zstack_config.h"
#include "bdb.h"
#include "zd_object.h"
#include "addr_mgr.h"
#include "bdb_interface.h"
#include "rom_jt_154.h"
#include "nwk_util.h"
#include "zcl.h"


#if (BDB_FINDING_BINDING_CAPABILITY_ENABLED==1)

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

uint8_t grpName[6] = {'G','r','o','u','p','\0'};
bdbGCB_IdentifyTimeChange_t      pfnIdentifyTimeChangeCB = NULL;
bdbGCB_BindNotification_t        pfnBindNotificationCB = NULL;

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */



/*********************************************************************
 * LOCAL VARIABLES
 */

SimpleDescriptionFormat_t  bdb_FindingBindingTargetSimpleDesc;

uint8_t bdbIndentifyActiveEndpoint  = 0xFF;

// bdb_ZclType1Clusters & bdb_ZclType2Clusters can be defined in bdb_ZclTypeClustersCustom, luoyiming 2020-04-21
#ifdef   BDB_ZCL_TYPE_CLUSTER_CUSTOM
#include "bdb_ZclTypeClustersCustom.c"
#else
#include "bdb_ZclTypeClusters.c"
#endif

#ifdef ZCL_GROUPS
static zclOptionRec_t zcl_Groups_Options[] =
{
  {
    ZCL_CLUSTER_ID_GENERAL_GROUPS,
    ( AF_ACK_REQUEST ),
  },
};
#endif


 /*********************************************************************
 * LOCAL FUNCTIONS
 */
static ZStatus_t bdb_zclFindingBindingAddBindEntry( byte SrcEndpInt,
                                  uint16_t BindClusterId,
                                  byte Cnt,
                                  uint16_t * ClusterList,
                                  zAddrType_t *DstAddr, byte DstEndpInt, uint8_t addBind, uint8_t isClusterOutput );

uint8_t bdb_FindIfAppCluster( cId_t ClusterId, uint8_t isClusterOutput );


static void bdb_zclSimpleDescClusterListClean( SimpleDescriptionFormat_t *pSimpleDesc );
bdbFindingBindingRespondent_t* bdb_findRespondentNode(uint8_t endpoint, uint16_t shortAddress);
bdbFindingBindingRespondent_t* bdb_getRespondentRetry(bdbFindingBindingRespondent_t* pRespondentHead);
void bdb_checkMatchingEndpoints(uint8_t bindIfMatch, uint16_t shortAddress, bdbFindingBindingRespondent_t **pCurr);
 /*********************************************************************
 * PUBLIC FUNCTIONS
 *********************************************************************/

void bdb_ProcessSimpleDesc( zdoIncomingMsg_t *msgPtr );
void bdb_ProcessIEEEAddrRsp(zdoIncomingMsg_t *pMsg);

/*********************************************************************
 * @fn      bdb_SetIdentifyActiveEndpoint
 *
 * @brief   Set the endpoint which will perform the finding and binding (either Target or Initiator)
 *
 * @param   Active endpoint with which perform F&B. If set to 0xFF all endpoints with Identify will be attempted. The endpoint must be either Initiator or Target or Both
 *
 * @return  ZFailure - F&B commissioning mode already requested
 *          ZInvalidParameter - Endpoint specified not found or reserved by Zigbee
 *          ZSuccess -
 */
ZStatus_t bdb_SetIdentifyActiveEndpoint(uint8_t activeEndpoint)
{
  epList_t *bdb_EpDescriptorListTemp = NULL;

  //Cannot process the request if no endpoints or F&B is under process
  if(bdbAttributes.bdbCommissioningMode & BDB_COMMISSIONING_MODE_FINDING_BINDING)
  {
    return ZFailure;
  }

  if(activeEndpoint == 0xFF)
  {
    bdbIndentifyActiveEndpoint = activeEndpoint;
    return ZSuccess;
  }

  if((activeEndpoint != 0) && (activeEndpoint < BDB_ZIGBEE_RESERVED_ENDPOINTS_START))
  {
    bdb_EpDescriptorListTemp = bdb_HeadEpDescriptorList;

    while(bdb_EpDescriptorListTemp != NULL)
    {
      if(bdb_EpDescriptorListTemp->epDesc->endPoint == activeEndpoint)
      {
        if(bdb_EpDescriptorListTemp->epDesc->epType)
        {
          bdbIndentifyActiveEndpoint = activeEndpoint;
          return ZSuccess;
        }
        else
        {
          return ZInvalidParameter;
        }
      }
      bdb_EpDescriptorListTemp = bdb_EpDescriptorListTemp->nextDesc;
    }
  }

  return ZInvalidParameter;
}


/*********************************************************************
 * @fn      bdb_GetIdentifyActiveEndpoint
 *
 * @brief   Get the endpoint which will perform the finding and binding (either Target or Initiator)
 *
 * @param   Active endpoint with which perform F&B. If set to 0xFF all endpoints with Identify will be attempted.
 *
 * @return  ZFailure - F&B commissioning mode already requested
 *          ZInvalidParameter - Endpoint specified not found or reserved by Zigbee
 *          ZSuccess -
 */
uint8_t bdb_GetIdentifyActiveEndpoint(void)
{
  return bdbIndentifyActiveEndpoint;
}


/*********************************************************************
 * @fn      bdb_setEpDescListToActiveEndpoint
 *
 * @brief   Set the endpoint list to the active endpoint selected by the application for F&B process
 *
 * @return  Current endpoint descriptor
 */

endPointDesc_t* bdb_setEpDescListToActiveEndpoint(void)
{
  bdb_CurrEpDescriptorList = bdb_HeadEpDescriptorList;

  //Check which active endpoit is being requested
  if(bdbIndentifyActiveEndpoint != 0xFF)
  {
    //Search for an specific endpoint
    while(bdb_CurrEpDescriptorList != NULL)
    {
      if(bdb_CurrEpDescriptorList->epDesc->endPoint == bdbIndentifyActiveEndpoint)
      {
        return bdb_CurrEpDescriptorList->epDesc;
      }
      bdb_CurrEpDescriptorList = bdb_CurrEpDescriptorList->nextDesc;
    }
  }
  else
  {
    //Look for the first endpoint that is either intiator or target to perform F&B. Is assumed that the endpoint supports Identify
    while(bdb_CurrEpDescriptorList != NULL)
    {
      if((bdb_CurrEpDescriptorList->epDesc->endPoint != 0) &&
         (bdb_CurrEpDescriptorList->epDesc->endPoint < BDB_ZIGBEE_RESERVED_ENDPOINTS_START) &&
         (bdb_CurrEpDescriptorList->epDesc->epType))
      {
        return bdb_CurrEpDescriptorList->epDesc;
      }
      bdb_CurrEpDescriptorList = bdb_CurrEpDescriptorList->nextDesc;
    }
  }
  //not found
  return NULL;
}


/*********************************************************************
 * @fn      bdb_ProcessIEEEAddrRsp
 *
 * @brief   Process IEEE addr response and mark the entry as done or remove the
 *          bind if not successful.
 *
 * @param   zdoIncomingMsg_t *pMsg
 *
 * @return  none
 */
void bdb_ProcessIEEEAddrRsp(zdoIncomingMsg_t *pMsg)
{
  ZDO_NwkIEEEAddrResp_t *pAddrRsp = NULL;
  bdbFindingBindingRespondent_t *pCurr = NULL;

  pAddrRsp = ZDO_ParseAddrRsp( pMsg );

  if(pAddrRsp == NULL)
  {
    return;
  }

  bdb_setEpDescListToActiveEndpoint();

  pCurr = bdb_findRespondentNode(bdb_FindingBindingTargetSimpleDesc.EndPoint, pAddrRsp->nwkAddr);

  //Does the entry exist and we were waiting an IEEE addr rsp from this device?
  if((pCurr != NULL) && (pCurr->attempts > FINDING_AND_BINDING_MISSING_IEEE_ADDR))
  {
    if(pAddrRsp->status == ZSuccess )
    {
      uint8_t extAddr[8];
      AddrMgrEntry_t entry;

      entry.nwkAddr = pAddrRsp->nwkAddr;
      entry.user = ADDRMGR_USER_BINDING;
      AddrMgrExtAddrSet(entry.extAddr, pAddrRsp->extAddr);

      //Add it as bind entry
      if(AddrMgrEntryUpdate(&entry) == FALSE)
      {
        //No space, then report F&B table full
        //If periodic was triggered, then finish it
        if(FINDING_AND_BINDING_PERIODIC_ENABLE == TRUE)
        {
          bdb_FB_InitiatorCurrentCyclesNumber = 0;
          OsalPortTimers_stopTimer(bdb_TaskID, BDB_FINDING_AND_BINDING_PERIOD_TIMEOUT);
        }

        bdb_zclSimpleDescClusterListClean( &bdb_FindingBindingTargetSimpleDesc );
        OsalPortTimers_stopTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT );
        bdb_exitFindingBindingWStatus( BDB_COMMISSIONING_FB_BINDING_TABLE_FULL );
        return;
      }

      //search for the matching clusters to be added this time as we have the IEEE addrs
      bdb_checkMatchingEndpoints(TRUE, pAddrRsp->nwkAddr, &pCurr);
      (void)extAddr;  //dummy
    }
    //Bind cannot be added if the device was not found
    pCurr->attempts = FINDING_AND_BINDING_RESPONDENT_COMPLETE;
  }

  //release the memory
  OsalPort_free( pAddrRsp );
}


/*********************************************************************
 * @fn      bdb_StopInitiatorFindingBinding
 *
 * @brief   Stops finding and binding for initiator devices.
 *
 * @param   none
 *
 * @return  none
 */
void bdb_StopInitiatorFindingBinding(void)
{
  //If periodic was triggered, then finish it
  if(FINDING_AND_BINDING_PERIODIC_ENABLE == TRUE)
  {
    bdb_FB_InitiatorCurrentCyclesNumber = 0;
    OsalPortTimers_stopTimer(bdb_TaskID, BDB_FINDING_AND_BINDING_PERIOD_TIMEOUT);
  }
  //Clean the respondent list and stop its timer
  bdb_zclRespondentListClean( &pRespondentHead );
  OsalPortTimers_stopTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT );

  //Notify status
  bdb_exitFindingBindingWStatus( BDB_COMMISSIONING_FB_NO_IDENTIFY_QUERY_RESPONSE );

}


/*********************************************************************
 * @fn      bdb_checkMatchingEndpoints
 *
 * @brief   Check active endpoints for F&B and the respondent simple descriptor
 *          for matching application clusters
 *
 * @param   pRespondant - Respondent to be process
 * @param   bindIfMatch - Flag to indicate that binds for matching cluster must
 *                        be done
 *
 * @return  status - Result of the operation
 */
void bdb_checkMatchingEndpoints(uint8_t bindIfMatch, uint16_t shortAddr, bdbFindingBindingRespondent_t **pCurr)
{
  uint8_t matchFound;
  endPointDesc_t *bdb_CurrEpDescriptor;
  uint8_t i;
  uint8_t status = ZFailure;
  zAddrType_t dstAddr;
#ifdef ZCL_GROUPS
  afAddrType_t afDstAddr;
#endif

  //Check all the endpoints active for F&B
  while(bdb_CurrEpDescriptorList != NULL)
  {
    matchFound = FALSE;

    bdb_CurrEpDescriptor = bdb_CurrEpDescriptorList->epDesc;

#ifdef ZCL_GROUPS
    if ( bdbAttributes.bdbCommissioningGroupID != 0xFFFF )
    {
      zcl_registerClusterOptionList(bdb_CurrEpDescriptor->endPoint,1,zcl_Groups_Options);

      dstAddr.addr.shortAddr = bdbAttributes.bdbCommissioningGroupID;
      dstAddr.addrMode = AddrGroup;
    }
    else
#endif
    {
      dstAddr.addrMode = Addr64Bit;
      if(bindIfMatch)
      {
        //if bind is to be created, then we should have the ext address in addr mgr
        AddrMgrExtAddrLookup( shortAddr, dstAddr.addr.extAddr );
      }
    }
    for(i = 0; i < bdb_CurrEpDescriptor->simpleDesc->AppNumOutClusters; i++)
    {
      //Filter for Application clusters (to bind app clusters only)
      status = bdb_zclFindingBindingAddBindEntry( bdb_CurrEpDescriptor->endPoint,
                              bdb_CurrEpDescriptor->simpleDesc->pAppOutClusterList[i],
                              bdb_FindingBindingTargetSimpleDesc.AppNumInClusters,
                              bdb_FindingBindingTargetSimpleDesc.pAppInClusterList,
                              &dstAddr, bdb_FindingBindingTargetSimpleDesc.EndPoint, bindIfMatch, TRUE );

      if ( status == ZApsTableFull )
      {
        break;
      }
      else if(status == ZSuccess)
      {
        matchFound = TRUE;
        //If a match is found and we are not adding due to lack of IEEE addrs, then skip looking
        if(!bindIfMatch)
        {
          break;
        }
      }
    }

    //Only search for other matches if the table is not full and we have not
    //found any match or we have to add bind as many as we can
    if( (status != ZApsTableFull) && (!matchFound || bindIfMatch) )
    {
      for(i = 0; i < bdb_CurrEpDescriptor->simpleDesc->AppNumInClusters; i++)
      {
        //Filter for Application clusters (to bind app clusters only)
        status = bdb_zclFindingBindingAddBindEntry( bdb_CurrEpDescriptor->endPoint,
                                bdb_CurrEpDescriptor->simpleDesc->pAppInClusterList[i],
                                bdb_FindingBindingTargetSimpleDesc.AppNumOutClusters,
                                bdb_FindingBindingTargetSimpleDesc.pAppOutClusterList,
                                &dstAddr, bdb_FindingBindingTargetSimpleDesc.EndPoint, bindIfMatch, FALSE );

        if ( status == ZApsTableFull )
        {
          break;
        }
        else if(status == ZSuccess)
        {
          matchFound = TRUE;

          //If a match is found and we are not adding due to lack of IEEE addrs, then skip looking
          if(!bindIfMatch)
          {
            break;
          }
        }
      }
    }

    //Check if we have found any match
    if( matchFound == TRUE )
    {
      if(bindIfMatch)
      {
        //Mark respondent as complete as simple desc has been process and we do
        //have IEEE addrs
        (*pCurr)->attempts = FINDING_AND_BINDING_RESPONDENT_COMPLETE;
      }
      else
      {
        //Mark as we need IEEE addrs
        (*pCurr)->attempts = FINDING_AND_BINDING_MISSING_IEEE_ADDR;
      }

#ifdef ZCL_GROUPS
      if ( bdbAttributes.bdbCommissioningGroupID != 0xFFFF )
      {
        afDstAddr.addr.shortAddr = shortAddr;
        afDstAddr.addrMode = afAddr16Bit;
        afDstAddr.endPoint = bdb_FindingBindingTargetSimpleDesc.EndPoint;

        zclGeneral_StackSendGroupAdd( bdb_CurrEpDescriptor->endPoint, &afDstAddr,
                                    bdbAttributes.bdbCommissioningGroupID, grpName,
                                    TRUE, 0x00 );
      }
#endif
    }
    else
    {
      //No matching cluster, then we are done with this respondent
      (*pCurr)->attempts = FINDING_AND_BINDING_RESPONDENT_COMPLETE;
    }

    if ( status == ZApsTableFull )
    {
      //If periodic was triggered, then finish it
      if(FINDING_AND_BINDING_PERIODIC_ENABLE == TRUE)
      {
        bdb_FB_InitiatorCurrentCyclesNumber = 0;
        OsalPortTimers_stopTimer(bdb_TaskID, BDB_FINDING_AND_BINDING_PERIOD_TIMEOUT);
      }

      bdb_zclSimpleDescClusterListClean( &bdb_FindingBindingTargetSimpleDesc );
      OsalPortTimers_stopTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT );
      bdb_exitFindingBindingWStatus( BDB_COMMISSIONING_FB_BINDING_TABLE_FULL );

      return;
    }

    //If an specific endpoint was requested, then don't go trough the rest of
    //the endpoints
    if( bdbIndentifyActiveEndpoint != 0xFF )
    {
      break;
    }
    else
    {
      //If active endpoints 'all' is attempted, then process the next endpoint in
      //the list
      bdb_CurrEpDescriptorList = bdb_CurrEpDescriptorList->nextDesc;

      while(bdb_CurrEpDescriptorList != NULL)
      {
        //It has to be different from 0 or reserved for Zigbee
        if((bdb_CurrEpDescriptorList->epDesc->endPoint != 0) && (bdb_CurrEpDescriptorList->epDesc->endPoint < BDB_ZIGBEE_RESERVED_ENDPOINTS_START))
        {
          break;
        }
        bdb_CurrEpDescriptorList = bdb_CurrEpDescriptorList->nextDesc;
      }
    }
  }
}



/*********************************************************************
 * @fn      bdb_ProcessSimpleDesc
 *
 * @brief   Process simple descriptor requested by F&B. Binds will be added if
 *          matching cluster is found and if IEEE addrs of the device is already
 *          stored
 *
 * @param   msgPtr - pointer to simple descriptor response indication message
 *
 * @return  none
 */
void bdb_ProcessSimpleDesc( zdoIncomingMsg_t *msgPtr )
{
  zAddrType_t dstAddr;
  bdbFindingBindingRespondent_t *pCurr = NULL;
  uint8_t SimpleDescSrcEndpoint;
  uint8_t isRespondantReadyToBeAdded = FALSE;
#if defined ( BDB_TL_INITIATOR )
  uint8_t isRespondantForTouchlink = FALSE;
#endif

  bdb_setEpDescListToActiveEndpoint();

  if ( !(bdb_CurrEpDescriptorList->epDesc->epType & BDB_FINDING_AND_BINDING_INITIATOR ))
  {
    bdb_zclSimpleDescClusterListClean( &bdb_FindingBindingTargetSimpleDesc );
#if defined ( BDB_TL_INITIATOR )
    pRespondentCurr = NULL;
    pRespondentNext = NULL;
    bdb_zclRespondentListClean( &pRespondentHead );
#endif
    //We should not be processing these commands as we are not initiator
    return;
  }

  dstAddr.addr.shortAddr = BUILD_UINT16( msgPtr->asdu[1], msgPtr->asdu[2] );
  dstAddr.addrMode = Addr16Bit;
  //Search for the respondent

  SimpleDescSrcEndpoint = msgPtr->asdu[4];
  pCurr = bdb_findRespondentNode(SimpleDescSrcEndpoint, dstAddr.addr.shortAddr);

  //If it is a valid entry then process it
  if(pCurr != NULL)
  {
    uint8_t extAddr[Z_EXTADDR_LEN];

    //If any of the simpleDesc cluster list is not free, release it before overwriting it.
    bdb_zclSimpleDescClusterListClean(&bdb_FindingBindingTargetSimpleDesc);

    //Parse the simpleDesc received into the temporal for processing later
    ZDO_ParseSimpleDescBuf( &msgPtr->asdu[4], &bdb_FindingBindingTargetSimpleDesc );

    if(AddrMgrExtAddrLookup( pCurr->data.addr.shortAddr, extAddr ))
    {
      isRespondantReadyToBeAdded = TRUE;
    }
    else
    {
      //Save the simple desc to don't ask for it again
      pCurr->SimpleDescriptor = &bdb_FindingBindingTargetSimpleDesc;
    }
    (void)extAddr;  //dummy
  }
  else
  {
    //This simple desc rsp was not requested by BDB F&B
    return;
  }

#if defined ( BDB_TL_INITIATOR )
  if(pCurr->attempts == FINDING_AND_BINDING_FOR_TOUCHLINK_SENDED)
  {
      isRespondantForTouchlink = TRUE;
      //Device joined us, then process this as any other F&B with its retries.
      pCurr->attempts = FINDING_AND_BINDING_NEW_RESPONDENT;
  }
#endif

  bdb_checkMatchingEndpoints(isRespondantReadyToBeAdded, dstAddr.addr.shortAddr, &pCurr);

  //If the respondent got process complete, then release the entry
  if(pCurr->attempts == FINDING_AND_BINDING_RESPONDENT_COMPLETE)
  {
    bdb_zclSimpleDescClusterListClean( &bdb_FindingBindingTargetSimpleDesc );
#if defined ( BDB_TL_INITIATOR )
  if(isRespondantForTouchlink == TRUE)
  {
    pRespondentCurr = NULL;
    pRespondentNext = NULL;
    bdb_zclRespondentListClean( &pRespondentHead );
  }
#endif
  }
}

/*********************************************************************
 * @fn      bdb_zclFindingBindingEpType
 *
 * @brief   Gives the Ep Type according to application clusters in
 *          simple descriptor
 *
 * @return  epType - If Target, Initiator or both
 */
uint8_t bdb_zclFindingBindingEpType( endPointDesc_t *epDesc )
{
  uint8_t epType = 0;
  uint8_t status;
  uint8_t type1ClusterCnt;
  uint8_t type2ClusterCnt;

  type1ClusterCnt = sizeof( bdb_ZclType1Clusters )/sizeof( uint16_t );
  type2ClusterCnt = sizeof( bdb_ZclType2Clusters )/sizeof( uint16_t );


  // Are there matching type 1 on server side?
  status = ZDO_AnyClusterMatches( epDesc->simpleDesc->AppNumInClusters,
                                  epDesc->simpleDesc->pAppInClusterList,
                                  type1ClusterCnt,
                                  (uint16_t*)bdb_ZclType1Clusters);

  if( status == TRUE )
  {
    epType |= BDB_FINDING_AND_BINDING_TARGET;
  }

  // Are there matching type 1 on client side?
  status = ZDO_AnyClusterMatches( epDesc->simpleDesc->AppNumOutClusters,
                                  epDesc->simpleDesc->pAppOutClusterList,
                                  type1ClusterCnt,
                                  (uint16_t*)bdb_ZclType1Clusters);

  if( status == TRUE )
  {
    epType |= BDB_FINDING_AND_BINDING_INITIATOR;
  }

  // Are there matching type 2 on server side?
  status = ZDO_AnyClusterMatches( epDesc->simpleDesc->AppNumInClusters,
                                  epDesc->simpleDesc->pAppInClusterList,
                                  type2ClusterCnt,
                                  (uint16_t*)bdb_ZclType2Clusters);

  if( status == TRUE )
  {
    epType |= BDB_FINDING_AND_BINDING_INITIATOR;
  }

  // Are there matching type 2 on client side?
  status = ZDO_AnyClusterMatches( epDesc->simpleDesc->AppNumOutClusters,
                                  epDesc->simpleDesc->pAppOutClusterList,
                                  type2ClusterCnt,
                                  (uint16_t*)bdb_ZclType2Clusters);

  if( status == TRUE )
  {
    epType |= BDB_FINDING_AND_BINDING_TARGET;
  }

  return epType;

}

/*********************************************************************
 * @fn      bdb_zclFindingBindingAddBindEntry
 *
 * @brief   This function is used to Add an entry to the binding table
 *
 * @param   SrcEndpInt - source endpoint
 * @param   BindClusterId - cluster to try bind
 * @param   Cnt - list of remote clusters
 * @param   ClusterList - pointer to the Object ID list
 * @param   DstAddr - Address of remote node
 * @param   DstEndpInt - EndPoint of remote node
 * @param   addBind - Indicate wheter or not bind must be added or not
 * @param   isClusterOutput - True if the bind attempted is output cluster in the local device, false otherwise
 *
 * @return  status - Success if added
 */
static ZStatus_t bdb_zclFindingBindingAddBindEntry( byte SrcEndpInt,
                                  uint16_t BindClusterId,
                                  byte Cnt,
                                  uint16_t * ClusterList,
                                  zAddrType_t *DstAddr, byte DstEndpInt, uint8_t addBind, uint8_t isClusterOutput )
{
  uint8_t status;

  if ( bdb_FindIfAppCluster ( BindClusterId, isClusterOutput ) != SUCCESS )
  {
    return ( ZApsFail ); // No App cluster
  }

  // Are there matching clusters?
  status = ZDO_AnyClusterMatches( Cnt,
                                  ClusterList,
                                  1,
                                 &BindClusterId);
  if ( status == FALSE )
  {
    return ( ZApsFail ); // No matched Cluster
  }

  if(addBind)
  {
    if ( pbindAddEntry )
    {
      // Add the entry into the binding table
      if (!pbindAddEntry( SrcEndpInt, DstAddr, DstEndpInt,
                             1, &BindClusterId ) )
      {
        return ( ZApsTableFull );
      }
    }
  }

  return ( ZSuccess );
}

/*********************************************************************
 * @fn      bdb_exitFindingBindingWStatus
 *
 * @brief   Clean respondent list and reports the status to bdb state machine
 *
 * @return
 */
void bdb_exitFindingBindingWStatus( uint8_t status )
{
  // bdb report status
  bdbAttributes.bdbCommissioningStatus = status;

  bdb_reportCommissioningState( BDB_COMMISSIONING_STATE_FINDING_BINDING, TRUE );
}

/*********************************************************************
 * @fn      bdb_zclSimpleDescClusterListClean
 *
 * @brief   This function free Simple Descriptor cluster lists
 *
 * @param   pSimpleDesc - pointer to simple descriptor
 *
 * @return  status
 */
static void bdb_zclSimpleDescClusterListClean( SimpleDescriptionFormat_t *pSimpleDesc )
{
  if(pSimpleDesc->pAppInClusterList != NULL)
  {
    OsalPort_free( pSimpleDesc->pAppInClusterList );
    pSimpleDesc->pAppInClusterList = ( cId_t* )NULL;
  }
  if(pSimpleDesc->pAppOutClusterList != NULL)
  {
    OsalPort_free( pSimpleDesc->pAppOutClusterList );
    pSimpleDesc->pAppOutClusterList = ( cId_t* )NULL;
  }
}

/*********************************************************************
 * @fn      bdb_RegisterIdentifyTimeChangeCB
 *
 * @brief   Register an Application's Identify Time change callback function
 *          to let know the application when identify is active or not.
 *
 * @param   pfnIdentify - application callback
 *
 * @return  none
 */
void bdb_RegisterIdentifyTimeChangeCB( bdbGCB_IdentifyTimeChange_t pfnIdentifyTimeChange )
{
  pfnIdentifyTimeChangeCB = pfnIdentifyTimeChange;
}


/*********************************************************************
 * @fn      bdb_GetFBInitiatorStatus
 *
 * @brief   Get the F&B initiator status for periodic requests.
 *
 * @param   RemainingTime - in seconds
 * @param   AttemptsLeft - number of attempts to be done
 * @param   MatchesFound - Add the number of matches to this parameter
 *                         since the last bdb_GetFBInitiatorStatus call
 *
 * @return  none
 */
void bdb_GetFBInitiatorStatus(uint8_t *RemainingTime, uint8_t* AttemptsLeft)
{
#if (FINDING_AND_BINDING_PERIODIC_ENABLE==TRUE)
  if(RemainingTime != NULL)
  {
    if (bdb_FB_InitiatorCurrentCyclesNumber == 0)
    {
      *RemainingTime = 0;
    }
    else
    {
      *RemainingTime = (bdb_FB_InitiatorCurrentCyclesNumber - 1) * FINDING_AND_BINDING_PERIODIC_TIME + ((OsalPortTimers_getTimerTimeout(bdb_TaskID, BDB_FINDING_AND_BINDING_PERIOD_TIMEOUT) + 999) / 1000);
    }
  }

  if(AttemptsLeft != NULL)
  {
    *AttemptsLeft = bdb_FB_InitiatorCurrentCyclesNumber;
  }
#endif
}


/*********************************************************************
 * @fn      bdb_RegisterBindNotificationCB
 *
 * @brief   Register an Application's notification callback function to let
 *          know the application when a new bind is added to the binding table.
 *
 * @param   pfnIdentify - application callback
 *
 * @return  none
 */
void bdb_RegisterBindNotificationCB( bdbGCB_BindNotification_t pfnBindNotification )
{
  pfnBindNotificationCB = pfnBindNotification;
}

/*********************************************************************
 * @fn      bdb_SendIdentifyQuery
 *
 * @brief   Sends Identify query from the given endpoint
 *
 * @param   endpoint
 *
 * @return  ZStatus_t
 */
ZStatus_t bdb_SendIdentifyQuery( uint8_t endpoint )
{

  ZStatus_t    status = ZFailure;
#ifdef ZCL_IDENTIFY
  afAddrType_t dstAddr;

  dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR_DEVALL;
  dstAddr.addrMode = afAddr16Bit;
  dstAddr.endPoint = 0xFF;

  status = zclGeneral_StackSendIdentifyQuery( endpoint, &dstAddr, TRUE, zcl_getFrameCounter() );

  if(status == ZSuccess)
  {
    OsalPortTimers_startTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT, IDENTIFY_QUERY_RSP_TIMEOUT );
  }
#endif
  return status;

}

/*********************************************************************
 * @fn      bdb_ZclIdentifyQueryCmdInd
 *
 * @brief   Callback from the ZCL General Cluster Library when
 *          it received an Identity Query Response Command for this
 *          application.
 *
 * @param   srcAddr - source address and endpoint of the response message
 * @param   identifyTime - the number of seconds to identify yourself
 *
 * @return  none
 */
void bdb_ZclIdentifyQueryCmdInd( zclIdentifyQueryRsp_t *pCmd )
{
  bdbFindingBindingRespondent_t *pCurr;

  // Stop the timer before refresh
  OsalPortTimers_stopTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT );

  // add new node to the list
  pCurr = bdb_AddRespondentNode( &pRespondentHead, pCmd );

  if(pCurr != NULL)
  {
    pCurr->data.addrMode = pCmd->srcAddr->addrMode;
    pCurr->data.addr.shortAddr = pCmd->srcAddr->addr.shortAddr;
    pCurr->data.endPoint = pCmd->srcAddr->endPoint;
    pCurr->data.panId = pCmd->srcAddr->panId;
    pCurr->attempts = FINDING_AND_BINDING_NEW_RESPONDENT;
    pCurr->SimpleDescriptor = NULL;
  }

  //Process the identify query rsp
  OsalPort_setEvent(bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT);
}

/*********************************************************************
 * @fn      bdb_ProcessRespondentList
 *
 * @brief   Process the respondent list by sending Simple Descriptor request to
 *          devices respondent in the list. Also send IEEE Addr Req to those
 *          device for which a bind is created buy IEEE addr is missing.
 *
 * @param   none
 *
 * @return  none
 */
void bdb_ProcessRespondentList( void )
{
  zAddrType_t dstAddr = { 0 };

  // Look for the first respondent
  if ( pRespondentCurr == NULL )
  {
    pRespondentCurr = bdb_getRespondentRetry(pRespondentHead);

    // If null, then no responses from Identify query request
    if ( pRespondentCurr == NULL )
    {
      //No responses, then no responses
      if(pRespondentHead == NULL)
      {
        bdb_exitFindingBindingWStatus( BDB_COMMISSIONING_FB_NO_IDENTIFY_QUERY_RESPONSE );
      }
      //Responses and binded to all clusters possible
      else
      {
        bdb_exitFindingBindingWStatus( BDB_COMMISSIONING_SUCCESS );
      }
      return;
    }
  }
  else
  {
    //Validate that we are not processing a missing IEEE Address before changing
    //the current respondent to be processed
    if( (pRespondentCurr->attempts == FINDING_AND_BINDING_RESPONDENT_COMPLETE) ||
       (!(pRespondentCurr->attempts & FINDING_AND_BINDING_MISSING_IEEE_ADDR) &&
        ((pRespondentCurr->attempts & (~FINDING_AND_BINDING_MISSING_IEEE_ADDR)) >= FINDING_AND_BINDING_MAX_ATTEMPTS)) )
    {
      if(pRespondentNext == NULL)
      {
        //Review the whole list if we have simple desc that we need to attempt.
        pRespondentCurr = bdb_getRespondentRetry(pRespondentHead);

        if(pRespondentCurr == NULL)
        {
          bdb_exitFindingBindingWStatus( BDB_COMMISSIONING_SUCCESS );
          return;
        }
      }
      else
      {
        pRespondentCurr = pRespondentNext;
      }
    }
  }

  //Start the timer to process the next respondent
#if ( ZG_BUILD_RTR_TYPE )
  OsalPortTimers_startTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT, SIMPLEDESC_RESPONSE_TIMEOUT_RTR );
#elif (RFD_RX_ALWAYS_ON_CAPABLE == TRUE)
  if ( ZG_DEVICE_ENDDEVICE_TYPE && zgRxAlwaysOn == TRUE )
  {
    OsalPortTimers_startTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT, SIMPLEDESC_RESPONSE_TIMEOUT_RX_ALWAYS_ON );
  }
  else
  {
    OsalPortTimers_startTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT, SIMPLEDESC_RESPONSE_TIMEOUT_SLEEPY );
  }
#else
  OsalPortTimers_startTimer( bdb_TaskID, BDB_RESPONDENT_PROCESS_TIMEOUT, SIMPLEDESC_RESPONSE_TIMEOUT_SLEEPY );
#endif

  //If ParentLost is reported, then do not attempt send SimpleDesc, mark those as pending,
  //if Parent Lost is restored, then these simpleDesc attempts will be restored to 0
  if(bdbCommissioningProcedureState.bdbCommissioningState != BDB_PARENT_LOST)
  {
    dstAddr.addr.shortAddr = pRespondentCurr->data.addr.shortAddr;
    dstAddr.addrMode = pRespondentCurr->data.addrMode;

#if defined ( BDB_TL_INITIATOR )
  if(pRespondentCurr->attempts == FINDING_AND_BINDING_FOR_TOUCHLINK_ADDED)
  {
      //Send simple descriptor
      pRespondentCurr->attempts = FINDING_AND_BINDING_FOR_TOUCHLINK_SENDED;
      ZDP_SimpleDescReq( &dstAddr, pRespondentCurr->data.addr.shortAddr, pRespondentCurr->data.endPoint, 0 );
      return;
  }
  //If already tried and got no answer, then the device might not be there, just clean all.
  else if(pRespondentCurr->attempts == FINDING_AND_BINDING_FOR_TOUCHLINK_SENDED)
  {
      pRespondentCurr = NULL;
      pRespondentNext = NULL;
      bdb_zclRespondentListClean( &pRespondentHead );
      return;
  }
#endif

    //Update the attempts, ahead of actually sending the frame, as this is done just below
    pRespondentCurr->attempts++;

    //Send IEEE addr request or simple desc req
    if(pRespondentCurr->attempts & FINDING_AND_BINDING_MISSING_IEEE_ADDR)
    {
      ZDP_IEEEAddrReq(pRespondentCurr->data.addr.shortAddr,0,0,0);
    }
    else
    {
      //Send simple descriptor
      ZDP_SimpleDescReq( &dstAddr, pRespondentCurr->data.addr.shortAddr, pRespondentCurr->data.endPoint, 0 );
    }
  }
  else
  {
    //Stop any attempt due to parent lost
    pRespondentCurr->attempts |= FINDING_AND_BINDING_PARENT_LOST;
  }

  //Search for the next respondant that has not enough tries in the list
  pRespondentNext = bdb_getRespondentRetry(pRespondentCurr->pNext);
}

/*********************************************************************
 * @fn      bdb_FindIfAppCluster
 *
 * @brief   To verify if cluster is application type and should the local device create a bind for it
 *
 * @param   ClusterId - cluster ID to be verified
 * @param   isClusterOutput - True if the bind attempted is output cluster in the local device, false otherwise
 *
 * @return  true if success
 */
uint8_t bdb_FindIfAppCluster( cId_t ClusterId, uint8_t isClusterOutput )
{
  uint8_t i;
  uint8_t ClusterCnt;

  //
  if(isClusterOutput)
  {
      ClusterCnt = sizeof( bdb_ZclType1Clusters )/sizeof( uint16_t );

      for ( i = 0; i < ClusterCnt; i++ )
      {
        if ( bdb_ZclType1Clusters[i] == ClusterId )
        {
          return ( SUCCESS );
        }
      }
  }
  else
  {
      ClusterCnt = sizeof( bdb_ZclType2Clusters )/sizeof( uint16_t );

      for ( i = 0; i < ClusterCnt; i++ )
      {
        if ( bdb_ZclType2Clusters[i] == ClusterId )
        {
          return ( SUCCESS );
        }
      }
  }
  // If not found, take it as application cluster it will be filtered
  // by simple descriptor at some point
  return ( FAILURE );
}


/*********************************************************************
 * @fn      bdb_getRespondentRetry
 *
 * @brief   Get the next Respondant entry to retry
 *
 * @param   pHead - pointer to a pointer of the list head
 *
 * @return  respondant entry if found, otherwise NULL
 */
bdbFindingBindingRespondent_t* bdb_getRespondentRetry(bdbFindingBindingRespondent_t* pRespondentHead)
{
  bdbFindingBindingRespondent_t *pTemp;

#if defined ( BDB_TL_INITIATOR )
  if(pRespondentHead->attempts == FINDING_AND_BINDING_FOR_TOUCHLINK_ADDED)
  {
      return pRespondentHead;
  }
#endif

  pTemp = pRespondentHead;
  while(pTemp != NULL)
  {
    //does the next entry requires to perform an attempt on simple desc req or IEEE addr req?
    if((pTemp->attempts & ~FINDING_AND_BINDING_MISSING_IEEE_ADDR) < FINDING_AND_BINDING_MAX_ATTEMPTS)
    {
      return pTemp;
    }

    pTemp = pTemp->pNext;
  }
  return NULL;
}


bdbFindingBindingRespondent_t* bdb_findRespondentNode(uint8_t endpoint, uint16_t shortAddress)
{
  bdbFindingBindingRespondent_t* pTemp = pRespondentHead;

  while(pTemp != NULL)
  {
    if((pTemp->data.addr.shortAddr == shortAddress) && (pTemp->data.endPoint == endpoint))
    {
      return pTemp;
    }

    pTemp = pTemp->pNext;
  }

  return NULL;
}

#endif

/*********************************************************************
*********************************************************************/
