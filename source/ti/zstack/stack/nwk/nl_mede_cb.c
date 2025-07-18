/**************************************************************************************************
  Filename:       NLMEDE_CB.c
  Revised:        $Date: 2015-06-02 15:55:43 -0700 (Tue, 02 Jun 2015) $
  Revision:       $Revision: 43961 $

  Description:    Network layer interface NLME and NLDE.  This are the callback
                  parts of this interface.


  Copyright 2004-2015 Texas Instruments Incorporated.

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
#include "zcomdef.h"
#include "nwk_bufs.h"
#include "nl_mede.h"
#include "af.h"
#include "aps_mede.h"
#include "aps_util.h"
#include "assoc_list.h"
#include "debug_trace.h"
#include "binding_table.h"
#if defined ( MT_NWK_CB_FUNC )
 #include "mt_nwk.h"
#endif
#include "nwk_globals.h"
#include "zd_app.h"
#include "rtg.h"
#include "nwk_util.h"
#include "zglobals.h"
#include "zd_nwk_mgr.h"
#include "zdiags.h"
#include "zcomdef.h"

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

/*********************************************************************
 * EXTERN VARIABLES
 */
#if (ZSTACK_ROUTER_BUILD)
extern uint8_t EnableFrameFwdNotification;
#endif
/*********************************************************************
 * EXTERN FUNCTIONS
 */
#if (ZSTACK_ROUTER_BUILD)
extern void nwk_frame_fwd_notification(nwkFrameFwdNotification_t  *nwkFrameFwdNotification);
#endif
/*********************************************************************
 * GLOBAL FUNCTIONS
 */
void NLME_RouteDiscoveryIndication( uint16_t DstAddress, RTG_Status_t status );

/*********************************************************************
 * LOCAL FUNCTIONS
 */
uint16_t NLME_CoordStartPANIDConflictCB( uint16_t panid );
void NLME_NetworkStatusCB( uint16_t nwkDstAddr, uint8_t statusCode, uint16_t dstAddr );


/*********************************************************************
 * NWK Data Service
 *
 */

/*********************************************************************
 * @fn          NLDE_DataCnf
 *
 * @brief       This function reports the results of a request to
 *              transfer a data PDU (NSDU) from a local APS sub-layer
 *              entity to a single peer APS sub-layer entity.
 *
 * @param       cnf  - NLDE_DataCnf_t
 *
 * @return      none
 */
void NLDE_DataCnf( NLDE_DataCnf_t* cnf )
{
  nwkDB_t* db;

  //------------------------------------------------------------------
  #if defined ( MT_NWK_CB_FUNC )
  //------------------------------------------------------------------
  // First check if MT has subscribed for this callback. If so , pass
  // it as a event to MonitorTest and return control to calling
  // function after that
  if ( NWKCB_CHECK( CB_ID_NLDE_DATA_CNF ) )
  {
    nwk_MTCallbackSubDataConfirm( cnf->db->nsduHandle, cnf->status );
    return;
  }
  //------------------------------------------------------------------
  #endif  //defined ( MT_NWK_CB_FUNC )
  //------------------------------------------------------------------

  db = cnf->db;

  // Process the data confirm
  if ( db != NULL )
  {

    // Handle reflection logic
    if ( pAPS_DataConfirmReflect != NULL )
    {
      uint8_t confirmed = pAPS_DataConfirmReflect( db, cnf->status );

       if ( confirmed == TRUE )
       {
        // Remove HANDLE_CNF bit so packets trigger only one confirm
        db->handleOptions &= ~HANDLE_CNF;
       }
    }

    // Handle retry logic
    if ( ( db->state == NWK_DATABUF_SENT           ) &&
         ( db->handleOptions & HANDLE_WAIT_FOR_ACK ) &&
         ( cnf->status != ZNwkNoRoute              ) &&
         ( cnf->status != ZMacTransactionExpired   ) )
    {

      if ( ( db->ud.txOptions & APS_TX_OPTIONS_RETRY_MSG ) &&
           ( db->apsRetries != 0 ) )
      {
        // Save the APS retries value
        db->retries = db->apsRetries;

        if ( ADDR_NOT_BCAST == NLME_IsAddressBroadcast(db->pDataReq->DstAddr.addr.shortAddr) )
        {
          // Increment the APS statistics counter
          ZDiagsUpdateStats( ZDIAGS_APS_TX_UCAST_RETRY );
        }
      }
      else
      {
        // This the first time through
        db->retries = (zgApscMaxFrameRetries + 1);
      }

      if ( cnf->status == ZMacSuccess )
      {
        uint8_t i;

        // Increment the number of MAC retries for every successfully transmitted APS frame
        for (i = 0; i < cnf->retries; i++ )
        {
          ZDiagsUpdateStats( ZDIAGS_MAC_RETRIES_PER_APS_TX_SUCCESS );
        }
      }

      // Decrement the APS retries count
      db->retries--;

      // This is to preserve the APS retry value
      db->apsRetries = db->retries;

      // Assume APS ACK required
      db->state = NWK_DATABUF_SCHEDULED;

      // Calculate the maximum wait time.
      db->dataX = zgApscAckWaitDurationPolled;

      // setup osal event to send this packet..
      OsalPort_setEvent( NWK_TaskID, NWK_DATABUF_SEND );

    }

    // Check for SENT or DONE packets
    if ( ( db->state == NWK_DATABUF_SENT ) ||
         ( db->state == NWK_DATABUF_DONE )    )
    {
      //If not for local device, then this frame is being forwarded
      if(db->macSrcAddr != _NIB.nwkDevAddress)
      {
#if (ZG_BUILD_RTR_TYPE)
          if(EnableFrameFwdNotification)
          {
            nwkFrameFwdNotification_t  nwkFrameFwdNotification;

            nwkFrameFwdNotification.dstAddr = db->pDataReq->DstAddr.addr.shortAddr;
            nwkFrameFwdNotification.srcAddr = db->macSrcAddr;
            nwkFrameFwdNotification.handle = db->nsduHandle;
            nwkFrameFwdNotification.frameState = NWK_FRAME_FWD_MSG_SENT;

            nwkFrameFwdNotification.status = cnf->status;

            nwk_frame_fwd_notification(&nwkFrameFwdNotification);
          }
#endif
      }

      if ( db->handleOptions & HANDLE_CNF )
      {
        // Notify the higher layer of data confirm,
        APSDE_DataConfirm( db, cnf->status );

        // Remove HANDLE_CNF bit so packets trigger only one confirm
        db->handleOptions &= ~HANDLE_CNF;

        // Update statistics for APS send frames
        if ( ADDR_NOT_BCAST == NLME_IsAddressBroadcast(db->pDataReq->DstAddr.addr.shortAddr) )
        {
          if ( cnf->status == ZMacSuccess )
          {
            uint8_t i;

            ZDiagsUpdateStats( ZDIAGS_APS_TX_UCAST_SUCCESS );

            // Increment the number of MAC retries for every successfully transmitted APS frame
            for (i = 0; i < cnf->retries; i++ )
            {
              ZDiagsUpdateStats( ZDIAGS_MAC_RETRIES_PER_APS_TX_SUCCESS );
            }
          }
          else
          {
            ZDiagsUpdateStats( ZDIAGS_APS_TX_UCAST_FAIL );
          }
        }
        else
        {
          if ( cnf->status == ZMacSuccess )
          {
            // Update transmitted broadcast messages
            ZDiagsUpdateStats( ZDIAGS_APS_TX_BCAST );
          }
        }
      }
    }
  }
}

/*********************************************************************
 * @fn          NLDE_DataIndication
 *
 * @brief       This function indicates the transfer of a data PDU
 *              (NSDU) from the NWK layer to the local APS sub-layer
 *              entity.
 *
 * @param       ff        - Pointer to NWK frame format
 * @param       sig       - radio parameters (link quality, rssi, correlation)
 * @param       timestamp - reception time
 *
 * @return      none
 */
void NLDE_DataIndication( NLDE_FrameFormat_t *ff, NLDE_Signal_t *sig, uint32_t timestamp )
{
  aps_FrameFormat_t aff = {0};
  zAddrType_t SrcAddr;
  uint8_t ackExpected = false;
  uint8_t dmIndirect;
  uint8_t frameType;
  APS_CmdInd_t cmdInd;
  uint8_t SecurityUse = false;
  uint8_t accept = TRUE;

#if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLDE_DATA_IND ) )
  {
    nwk_MTCallbackSubDataIndication( ff->srcAddr, ff->nsduLength, ff->nsdu, sig->LinkQuality );
    return;
  }
#endif  //MT_NWK_CB_FUNC

  APSDE_ParseMsg( ff, &aff );

  // Sanity Check
  if ( ((aff.apsHdrLen == 0) && (aff.asduLength == 0))
      || ((aff.FrmCtrl & APS_FC_ACK_REQ) && (NLME_IsAddressBroadcast( ff->dstAddr ) != ADDR_NOT_BCAST)) )
  {
    // Drop the message, something wrong in parsing failed
    ZDiagsUpdateStats( ZDIAGS_APS_INVALID_PACKETS );
    return;
  }

  // TRUE if delivery mode is indirect
  dmIndirect = ((aff.FrmCtrl & APS_DELIVERYMODE_MASK) == APS_FC_DM_INDIRECT);

  if ( aff.FrmCtrl & APS_FC_SECURITY )
  {
    SecurityUse = true;

    // Decrypt the message, unless the message was sent to
    // ourself as a Reflected message.
    if ( !dmIndirect )
    {
      if ( ( ( ff->srcAddr == NLME_GetShortAddr() ) &&
            ( ff->txOptions & APS_TX_OPTIONS_RETRY_MSG ) ) ||
          ( ( ff->srcAddr != NLME_GetShortAddr() ) &&
           !( ff->txOptions & APS_TX_OPTIONS_RETRY_MSG ) ) )
      {

        uint16_t msgSrcAddr;

        // Identify who is the source of the message to decrypt
        if ( ff->txOptions & APS_TX_OPTIONS_RETRY_MSG )
        {
          msgSrcAddr = ff->dstAddr;

          // Save this value to be used by the Security Remove function,
          // this is only done for ASP retry packets
          aff.macSrcAddr = ff->srcAddr;
        }
        else
        {
          msgSrcAddr = ff->srcAddr;
        }

        if ( ( APSME_FrameSecurityRemove( msgSrcAddr, &aff ) != ZSuccess ) )
        {
          ZDiagsUpdateStats( ZDIAGS_APS_DECRYPT_FAILURES );
          return;
        }
      }
    }
  }

  // Only for messages received from remote address
  if ( !( ff->txOptions & APS_TX_OPTIONS_RETRY_MSG ) )
  {
    // Check whether ACK expected
    if ( (aff.FrmCtrl & APS_FC_ACK_REQ) && !(aff.XtndFrmCtrl & APS_XFC_FRAG_MASK)
        && ((aff.FrmCtrl & APS_DELIVERYMODE_MASK) != APS_FC_DM_GROUP)
        && ((aff.FrmCtrl & APS_DELIVERYMODE_MASK) != APS_FC_DM_BROADCAST) )
    {
      ackExpected = true;
    }
    else
    {
      ackExpected = false;
    }
  }

  frameType = aff.FrmCtrl & APS_FRAME_TYPE_MASK;

  if ( frameType == APS_ACK_FRAME )
  {
    apsProcessAck( ff->srcAddr, &aff );
  }
  else if ( frameType == APS_CMD_FRAME )
  {
    if ( ( ackExpected ) &&
        !( ff->txOptions & APS_TX_OPTIONS_RETRY_MSG ) )
    {
      apsGenerateAck( ff->srcAddr, &aff );
    }

    cmdInd.aff        = &aff,
    cmdInd.nwkSrcAddr = ff->srcAddr;
    cmdInd.nwkSecure  = ff->secure;

    APS_CmdInd(&cmdInd);
  }
  else if ( frameType == APS_DATA_FRAME )
  {
    if ( (_NIB.SecureAllFrames == TRUE) && (ff->secure == FALSE) )
    {
      ZDiagsUpdateStats( ZDIAGS_APS_DECRYPT_FAILURES );
      return;  // Drop the message
    }

    SrcAddr.addrMode = Addr16Bit;
    SrcAddr.addr.shortAddr = ff->srcAddr;

    // APS retry packets are handled here, the message is coming from the local device,
    // at this point security has been removed and now the packet shall be send back
    // to the lower layer so the Frame Counter is updated should be encrypted again
    // instead of going all the way up to APS layer.
    if ( ff->txOptions & APS_TX_OPTIONS_RETRY_MSG )
    {
      APSDE_DataReq_t retryReq;

      retryReq.dstAddr.addrMode = Addr16Bit;
      retryReq.dstAddr.addr.shortAddr = ff->dstAddr;
      retryReq.srcEP = aff.SrcEndPoint;
      retryReq.dstEP = aff.DstEndPoint;
      retryReq.dstPanId = _NIB.nwkPanId;
      retryReq.clusterID = aff.ClusterID;
      retryReq.profileID = aff.ProfileID;
      retryReq.asduLen = aff.asduLength;
      retryReq.asdu = aff.asdu;
      retryReq.txOptions = ff->txOptions;
      retryReq.transID = (uint8_t)ff->transID;
      retryReq.discoverRoute = ff->discoverRoute;
      retryReq.radiusCounter = ff->radius;
      retryReq.apsCount = aff.ApsCounter;
      retryReq.blkCount = aff.BlkCount;
      retryReq.apsRetries = ff->apsRetries;
      retryReq.nsduHandle = ff->nsduHandle;

      // this packet needs to be retried by APS layer
      ZStatus_t status = APSDE_DataReq( &retryReq );
      if( status != ZSuccess )
      {
        afDataConfirm(retryReq.srcEP, retryReq.transID, retryReq.clusterID, status);
      }
    }
    else
    {
      // Verify that the incoming indirect data packet is sent from
      // the local device itself
      if ( dmIndirect )
      {
        aff.transID = (uint8_t)ff->transID;
        if ( pAPS_DataIndReflect &&
            SrcAddr.addr.shortAddr == NLME_GetShortAddr() )
        {
          pAPS_DataIndReflect( &SrcAddr, &aff, sig, ackExpected, ff->discoverRoute, SecurityUse, timestamp, ff->txOptions, ff->apsRetries );
        }
        else
        {
          // Discard?
        }
      }
      else    // meant for this device
      {
        // Message is coming from a remote device
        if ( ackExpected )
        {
          apsGenerateAck( ff->srcAddr, &aff );

        }

        // Check for a duplicate broadcast message
#if ( ZG_BUILD_ENDDEVICE_TYPE ) && ( RFD_RX_ALWAYS_ON_CAPABLE == TRUE )
        if( ZG_DEVICE_ENDDEVICE_TYPE && zgRxAlwaysOn == TRUE )
        {
          accept = APS_EndDeviceBroadcastCheck( ff );
        }
#endif  // ( ZG_BUILD_ENDDEVICE_TYPE ) && ( RFD_RX_ALWAYS_ON_CAPABLE == TRUE )

        if ( accept )
        {
          APSDE_DataIndication( &aff, &SrcAddr, _NIB.nwkPanId, sig, ff->broadcastId,
                               SecurityUse, timestamp, ff->radius );
        }
      }
    }
  }
}

/*********************************************************************
 * NWK Management Service
 *
 */

/*********************************************************************
 * @fn          NLME_NetworkDiscoveryConfirm
 *
 * @brief       This function notify the higher layer completion of
 *              a network discovery
 *
 * @param       status - return status of the scan confirm
 *
 * @return      none
 */
void NLME_NetworkDiscoveryConfirm(uint8_t status)
{
  #if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLME_NWK_DISC_CNF ) )
  {
    networkDesc_t *pNwkDesc;
    uint8_t i = 0;

    pNwkDesc = NwkDescList;
    while (pNwkDesc)
    {
      i++;
      pNwkDesc = pNwkDesc->nextDesc;
    }

    nwk_MTCallbackSubNetworkDiscoveryConfirm( i, NwkDescList );
  }
  else
#endif  //MT_NWK_CB_FUNC
  {
    ZDO_NetworkDiscoveryConfirmCB(status);
  }
}

/*********************************************************************
 * @fn          NLME_NetworkFormationConfirm
 *
 * @brief       This function reports the results of the request to
 *              form a new nework.
 *
 * @param       Status - Result of attempt to start new network
 *
 * @return      none
 */
void NLME_NetworkFormationConfirm( ZStatus_t Status )
{
#if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLME_INIT_COORD_CNF ) )
  {
    nwk_MTCallbackSubInitCoordConfirm( Status );
  }
  else
#endif  //MT_NWK_CB_FUNC

    ZDO_NetworkFormationConfirmCB( Status );
}

/*********************************************************************
 * @fn          NLME_StartRouterConfirm
 *
 * @brief       This function reports the results of the request to
 *              start functioning as a router
 *
 * @param       Status - Result of NLME_StartRouterRequest()
 *
 * @return      none
 */
void NLME_StartRouterConfirm( ZStatus_t Status )
{
#if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLME_START_ROUTER_CNF ) )
  {
    nwk_MTCallbackSubStartRouterConfirm( Status );
  }
  else
#endif  //MT_NWK_CB_FUNC

    ZDO_StartRouterConfirmCB( Status );
}


/*********************************************************************
 * @fn          NLME_beaconNotifyInd
 *
 * @brief       This function reports the beacon notification indication
 *
 * @param       pBeacon - pointer to the beacon indication
 *
 * @return      none
 */
void NLME_beaconNotifyInd(NLME_beaconInd_t *pBeacon)
{
  ZDO_beaconNotifyIndCB(pBeacon);
}

/*********************************************************************
 * @fn          NLME_JoinConfirm
 *
 * @brief       This function allows the next hight layer to be notified
 *              of the results of its request to join itself to a network.
 *
 * @param       Status - Result of NLME_JoinRequest()
 *
 * @return      none
 */
void NLME_JoinConfirm( uint16_t PanId, ZStatus_t Status )
{

#if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLME_JOIN_CNF ) )
  {
    nwk_MTCallbackSubJoinConfirm( PanId, Status );
    return;
  }
  else
#endif  //MT_NWK_CB_FUNC
  {
    ZDO_JoinConfirmCB( PanId, Status );
  }
}

/*********************************************************************
 * @fn          NLME_JoinIndication
 *
 * @brief       This function allows the next higher layer of a
 *              coordinator to be notified of a remote join request.
 *
 * @param       ShortAddress - 16-bit address
 * @param       ExtendedAddress - IEEE (64-bit) address
 * @param       CapabilityInformation - Association Capability Information
 * @param       type - of joining -
 *                          NWK_ASSOC_JOIN
 *                          NWK_ASSOC_REJOIN_UNSECURE
 *                          NWK_ASSOC_REJOIN_SECURE
 *
 * @return      ZStatus_t
 */
ZStatus_t NLME_JoinIndication( uint16_t ShortAddress, uint8_t *ExtendedAddress,
                               uint8_t CapabilityInformation, uint8_t type )
{

#if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLME_JOIN_IND ) )
  {
    nwk_MTCallbackSubJoinIndication( ShortAddress, ExtendedAddress,
                                     CapabilityInformation );
    return ( ZSuccess );
  }
  else
#endif  //MT_NWK_CB_FUNC
  {
    return ( ZDO_JoinIndicationCB(ShortAddress, ExtendedAddress, CapabilityInformation, type ) );
  }
}

/*********************************************************************
 * @fn          NLME_LeaveCnf
 *
 * @brief       This function allows the next higher layer to be
 *              notified of the results of a request for itself or
 *              another device to leave the network.
 *
 * @param       cnf - NLME_LeaveCnf_t
 *
 * @return      none
 */
void NLME_LeaveCnf( NLME_LeaveCnf_t* cnf )
{
  //------------------------------------------------------------------
  #if defined ( MT_NWK_CB_FUNC )
  //------------------------------------------------------------------
  // First check if MT has subscribed for this callback. If so , pass
  // it as a event to MonitorTest and return control to calling
  // function after that
  if ( NWKCB_CHECK( CB_ID_NLME_LEAVE_CNF ) )
  {
    nwk_MTCallbackSubLeaveConfirm( cnf->extAddr, cnf->status );
  }
  else
  //------------------------------------------------------------------
  #endif // MT_NWK_CB_FUNC
  //------------------------------------------------------------------
  {
    ZDO_LeaveCnf( cnf );
  }
}

/*********************************************************************
 * @fn          NLME_LeaveInd
 *
 * @brief       This function allows the next higher layer to be
 *              notified of a remote leave request or indication.
 *
 * @param       ind - NLME_LeaveInd_t
 *
 * @return      void
 */
void NLME_LeaveInd( NLME_LeaveInd_t* ind )
{
  //------------------------------------------------------------------
  #if defined ( MT_NWK_CB_FUNC )
  //------------------------------------------------------------------
  // First check if MT has subscribed for this callback. If so , pass
  // it as a event to MonitorTest and return control to calling
  // function after that.
  if ( NWKCB_CHECK( CB_ID_NLME_LEAVE_IND ) )
  {
    nwk_MTCallbackSubLeaveIndication( ind->extAddr );
  }
  else
  //------------------------------------------------------------------
  #endif // MT_NWK_CB_FUNC
  //------------------------------------------------------------------
  {
    // Router only - Ignore messages that are leave "requests"
    if ( ZSTACK_ROUTER_BUILD && (_NIB.CapabilityFlags & CAPINFO_DEVICETYPE_FFD)
        && (ind->request != TRUE) )
    {
      // Ignore message from self or a device with children
      if ( (ind->srcAddr != NLME_GetCoordShortAddr())
           && (ind->removeChildren != TRUE) )
      {
        // Only child end devices
        if ( AssocIsRFChild( ind->srcAddr ) )
        {
          // Remove any queued data bufferes
          nwk_DeleteDataBufs( ind->srcAddr );
        }
      }
    }

    ZDO_LeaveInd( ind );
  }
}

/*********************************************************************
 * @fn          NLME_SyncIndication
 *
 * @brief       This function allows the next higher layer to be
 *              notified of the loss of synchronization at the MAC
 *              sub-layer.
 *
 * @param       none
 *
 * @return      none
 */
void NLME_SyncIndication( byte type, uint16_t shortAddr )
{

#if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLME_SYNC_IND ) )
  {
    nwk_MTCallbackSubSyncIndication( );
  }
  else
#endif  //MT_NWK_CB_FUNC
  {
    ZDO_SyncIndicationCB( type, shortAddr );
  }
}

/*********************************************************************
 * @fn          NLME_PollConfirm
 *
 * @brief       This function allows the next higher layer to be
 *              notified of a Poll Confirmation.
 *
 * @param       none
 *
 * @return      none
 */
void NLME_PollConfirm( byte status )
{

#if defined ( MT_NWK_CB_FUNC )
  /* First check if MT has subscribed for this callback. If so , pass it as
  a event to MonitorTest and return control to calling function after that */
  if ( NWKCB_CHECK( CB_ID_NLME_POLL_CNF ) )
  {
    nwk_MTCallbackSubPollConfirm( status );
  }
  else
#endif  //MT_NWK_CB_FUNC
  {
    // Check for sleeping end device and APS Duplicate Table items
    if ( ((_NIB.CapabilityFlags & CAPINFO_RCVR_ON_IDLE) == 0)
        && (APS_lastDupTime > 0)  )
    {
      // Maintain the APS duplication table (sleeping end device)
      OsalPort_setEvent( APS_TaskID, APS_DUP_REJ_TIMEOUT_EVT );
    }

    ZDO_PollConfirmCB(status);
  }
}

/*********************************************************************
 * @fn          NLME_RouteDiscoveryIndication
 *
 * @brief       This function indicates a routing table entry change
 *
 * @param       DstAddress          - destination address
 *
 * @return      void
 */
void NLME_RouteDiscoveryIndication( uint16_t DstAddress, RTG_Status_t status )
{
  // indicates that the route to the destiantion is added or removed
  if ( status == RTG_FAIL )
  {
    RTG_RemoveRtgEntry( DstAddress, 0 );
    NLME_RouteDiscoveryRequest( DstAddress, 0 , gDEFAULT_ROUTE_REQUEST_RADIUS );
  }
}

/*********************************************************************
 * @fn          NLME_ConcentratorIndication
 *
 * @brief       This function is called by RTG module to indicate the
 *              concentrator existence. It then call the ZDO callback
 *              to indicate the ZDO layer
 *
 * @param       nwkAddr          - Concentrator NWK address
 * @param       extAddr          - pointer to extended Address
 *                                 NULL if not available
 * @param       pktCost          - PktCost from RREQ
 *
 * @return      void
 */
void NLME_ConcentratorIndication( uint16_t nwkAddr, uint8_t *extAddr, uint8_t pktCost )
{
  ZDO_ConcentratorIndicationCB( nwkAddr, extAddr, pktCost );
}

/*********************************************************************
 * @fn          NLME_EDScanConfirm
 *
 * @brief       This function returns list of energy measurements.
 *
 * @param       status - scan status
 * @param       scannedChannels - scanned channels
 * @param       energyDectectList - pointer to list of measured energy
 *
 * @return      void
 */
void NLME_EDScanConfirm( uint8_t status, uint32_t scannedChannels, uint8_t *energyDetectList )
{
  if ( pZDNwkMgr_EDScanConfirmCB )
  {
    NLME_EDScanConfirm_t EDScanConfirm;

    EDScanConfirm.status = status;
    EDScanConfirm.scannedChannels = scannedChannels;
    EDScanConfirm.energyDetectList = energyDetectList;

    pZDNwkMgr_EDScanConfirmCB( &EDScanConfirm );
  }
}

/*********************************************************************
 * @fn          NLME_CoordStartPANIDConflictCB
 *
 * @brief       Coordinator Start PAN ID Conflict callback function
 *
 * @param       panid          - conflicted PAN ID
 *
 * @return      New PANID to try
 */
uint16_t NLME_CoordStartPANIDConflictCB( uint16_t panid )
{
  return ( ZDApp_CoordStartPANIDConflictCB( panid ) );
}

/*********************************************************************
 * @fn          NLME_NetworkStatusCB
 *
 * @brief       Network Status Callback function
 *
 * @param       nwkDstAddr - message's destination address (used to determine
 *                           if the message was intended for this device or
 *                           a sleeping end device.
 * @param       statusCode - message's status code
 * @param       dstAddr - the destination address related to the status code
 *
 * @return      none
 */
void NLME_NetworkStatusCB( uint16_t nwkDstAddr, uint8_t statusCode, uint16_t dstAddr )
{
  ZDO_NetworkStatusCB( nwkDstAddr, statusCode, dstAddr );
}

/****************************************************************************
****************************************************************************/
