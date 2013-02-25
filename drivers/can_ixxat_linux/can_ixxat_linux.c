#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <ECI109.h>
#include <ECI_pshpack1.h>
#include <ECI_poppack.h>

#include "can_driver.h"

#define ECIDEMO_TX_TIMEOUT 500
#define ECIDEMO_RX_TIMEOUT 500
#define ECIDEMO_CHECKERROR(FuncName) \
		{\
	if(ECI_OK == hResult)\
	{\
		OS_Printf(#FuncName "...succeeded.\n"); \
	}\
	else\
	{\
		OS_Printf( #FuncName "...failed with errorcode: 0x%08X. %s\n", \
				hResult, \
				ECI109_GetErrorString(hResult)); \
	}\
		}


///////////////////////////////////////////////////////////////////////////////
/**
  Returns the nth controller of given controller class.

  @param pstcHwInfo
    Reference to ECI_HW_INFO to find controller from.

  @param eCtrlClass
    Class of controller to find.

  @param dwRelCtrlIndex
    Relative controller index of given controller class.

  @param pdwCtrIndex
    Variable which receives the controller index.

  @retval ECI_RESULT
    ECI_OK on success, otherwise an error code from the @ref e_ECIERROR "ECI error list".

  @ingroup EciDemo
*/
ECI_RESULT EciGetNthCtrlOfClass( const ECI_HW_INFO* pstcHwInfo,
                                 e_CTRLCLASS        eCtrlClass,
                                 DWORD              dwRelCtrlIndex,
                                 DWORD*             pdwCtrIndex)
{
  ECI_RESULT hResult = ECI_ERR_RESOURCE_NOT_FOUND;

  //*** Check Arguments
  if((NULL != pstcHwInfo) && (NULL != pdwCtrIndex))
  {
    //*** Struct Version 0
    if(pstcHwInfo->dwVer == ECI_STRUCT_VERSION_V0)
    {
      DWORD dwIndex = 0;
      //*** Iterate through all controllers
      for(dwIndex=0; dwIndex < pstcHwInfo->u.V0.dwCtrlCount; dwIndex++)
      {
        if(pstcHwInfo->u.V0.sCtrlInfo[dwIndex].wCtrlClass == eCtrlClass)
        {
          //*** Controller not found yet
          if(hResult != ECI_OK)
          {
            if(dwRelCtrlIndex == 0)
            {
              //*** Controller found
              *pdwCtrIndex = dwIndex;
              hResult    = ECI_OK;
            }
            else
            {
              dwRelCtrlIndex--;
            }
          }
        }//endif
      }//end for
    }//endif
  }
  else
  {
    hResult = ECI_ERR_INVALID_POINTER;
  }

  return hResult;
}


/**
 * CAN_HANDLES must have value >=1 while CANLIB wants handles >= 0
 * so fd0 needs to be decremented before use.
 *
 */
UNS8 canReceive_driver(CAN_HANDLE fd0, Message *m)
{
	ECI_RESULT  hResult     = ECI_ERR_TIMEOUT;
	ECI_CTRL_HDL  dwCtrlHandle  = fd0;
	ECI_CTRL_MESSAGE stcCtrlMsg   = {0};
	DWORD dwCount = 1;
	int i;

	//*** Try to read Message
	hResult = ECI109_CtrlReceive( dwCtrlHandle, &dwCount, &stcCtrlMsg, ECIDEMO_RX_TIMEOUT);
//	ECIDEMO_CHECKERROR(ECI109_CtrlReceive);
	if(hResult) return hResult;

	m->cob_id = stcCtrlMsg.u.sCanMessage.u.V0.dwMsgId;
	m->len = stcCtrlMsg.u.sCanMessage.u.V0.uMsgInfo.Bits.dlc;
	m->rtr = stcCtrlMsg.u.sCanMessage.u.V0.uMsgInfo.Bits.rtr;
	if(stcCtrlMsg.u.sCanMessage.u.V0.uMsgInfo.Bits.ext)
		m->cob_id |= 0x20000000;

	for(i=0; i<8; i++)
		m->data[i] = stcCtrlMsg.u.sCanMessage.u.V0.abData[i];

	return hResult;
}

/**
 *
 * CAN_HANDLES must have value >=1 while CANLIB wants handles >= 0
 * so fd0 needs to be decremented before use.
 *
 */
UNS8 canSend_driver(CAN_HANDLE fd0, Message const *m)
{
	ECI_RESULT  hResult     = ECI_OK;
	ECI_CTRL_HDL  dwCtrlHandle  = fd0;
	ECI_CTRL_MESSAGE stcCtrlMsg   = {0};
	int i;

	//*** Prepare CAN Message to send
	stcCtrlMsg.wCtrlClass                            = ECI_CTRL_CAN;
	stcCtrlMsg.u.sCanMessage.dwVer                   = ECI_STRUCT_VERSION_V0;
	stcCtrlMsg.u.sCanMessage.u.V0.dwMsgId            = m->cob_id;
	stcCtrlMsg.u.sCanMessage.u.V0.uMsgInfo.Bits.dlc  = m->len;
	if (m->cob_id & 0x20000000)
		stcCtrlMsg.u.sCanMessage.u.V0.uMsgInfo.Bits.ext = 1;
	if (m->rtr)
		stcCtrlMsg.u.sCanMessage.u.V0.uMsgInfo.Bits.rtr = 1;
	for(i=0; i<8; i++)
		stcCtrlMsg.u.sCanMessage.u.V0.abData[i] = m->data[i];

	//*** Send one message
	hResult = ECI109_CtrlSend( dwCtrlHandle, &stcCtrlMsg, ECIDEMO_TX_TIMEOUT);
//	ECIDEMO_CHECKERROR(ECI109_CtrlSend);
//	if(hResult) return 1;

	return hResult;
}



int TranslateBaudRate(char* optarg)
{
	if(!strcmp( optarg, "1M"))
		return (ECI_CAN_BT0_1000KB<<8)|ECI_CAN_BT1_1000KB;
	if(!strcmp( optarg, "500K"))
		return (ECI_CAN_BT0_500KB<<8)|ECI_CAN_BT1_500KB;
	if(!strcmp( optarg, "250K"))
		return (ECI_CAN_BT0_250KB<<8)|ECI_CAN_BT1_250KB;
	if(!strcmp( optarg, "125K"))
		return (ECI_CAN_BT0_125KB<<8)|ECI_CAN_BT1_125KB;
	if(!strcmp( optarg, "100K"))
		return (ECI_CAN_BT0_100KB<<8)|ECI_CAN_BT1_100KB;
	if(!strcmp( optarg, "50K"))
		return (ECI_CAN_BT0_50KB<<8)|ECI_CAN_BT1_50KB;

	return 0;
}

/**
 * Channels and their descriptors are numbered starting from zero.
 * So I need to increment by 1 the handle returned by CANLIB because
 * CANfestival CAN_HANDLEs with value zero are considered NOT VALID.
 *
 * The baud rate could be given directly as bit/s
 * or using one of the BAUD_* constants defined
 * in canlib.h
 */
CAN_HANDLE canOpen_driver(s_BOARD *board)
{
	ECI_RESULT  hResult     = ECI_OK;
	ECI_CTRL_HDL  dwCtrlHandle  = ECI_INVALID_HANDLE;
	ECI_HW_PARA stcHwPara   = {0};
	ECI_HW_INFO stcHwInfo   = {0};
	DWORD       dwHwIndex   = 0;
	DWORD       dwCtrlIndex = 0;
	int baud;

	//*** Prepare Hardware parameter structure
	stcHwPara.wHardwareClass = ECI_HW_USB;

	//*** At first call Initialize to prepare ECI driver
	//*** with one board
	hResult = ECI109_Initialize(1, &stcHwPara);
	ECIDEMO_CHECKERROR(ECI109_Initialize);
	if(hResult) return 0;

	//*** Retrieve hardware info
	sscanf(board->busname, "%d", &dwHwIndex);
	hResult = ECI109_GetInfo(dwHwIndex, &stcHwInfo);
	ECIDEMO_CHECKERROR(ECI109_GetInfo);
	if(hResult) return 0;

	//*** Find first CAN Controller of Board
	hResult = EciGetNthCtrlOfClass(&stcHwInfo,
			ECI_CTRL_CAN,
			0, //first relative controller
			&dwCtrlIndex);
	ECIDEMO_CHECKERROR(EciGetNthCtrlOfClass);
	if(hResult) return 0;

	//*** Open given controller of given board
	ECI_CTRL_CONFIG stcCtrlConfig = {0};

	//*** Set CAN Controller configuration
	baud = TranslateBaudRate(board->baudrate);
	stcCtrlConfig.wCtrlClass                = ECI_CTRL_CAN;
	stcCtrlConfig.u.sCanConfig.dwVer        = ECI_STRUCT_VERSION_V0;
	stcCtrlConfig.u.sCanConfig.u.V0.bBtReg1 = (BYTE)(baud & 0xff);
	stcCtrlConfig.u.sCanConfig.u.V0.bBtReg0 = (BYTE)(baud >> 8);
	stcCtrlConfig.u.sCanConfig.u.V0.bOpMode = ECI_CAN_OPMODE_STANDARD | ECI_CAN_OPMODE_EXTENDED | ECI_CAN_OPMODE_ERRFRAME;

	//*** Open and Initialize given Controller of given board
	hResult = ECI109_CtrlOpen(&dwCtrlHandle, dwHwIndex, dwCtrlIndex, &stcCtrlConfig);
	ECIDEMO_CHECKERROR(ECI109_CtrlOpen);
	if(hResult) return 0;

	//*** Start Controller
	hResult = ECI109_CtrlStart(dwCtrlHandle);
	ECIDEMO_CHECKERROR(ECI109_CtrlStart);
	if(hResult) return 0;

	return (CAN_HANDLE)dwCtrlHandle;
}


UNS8 canChangeBaudRate_driver( CAN_HANDLE fd0, char* baud)
{
#if 0
	int baudrate;
	canStatus retval = canOK;


	baudrate = TranslateBaudRate(baud);
	if (baudrate == 0)
	{
		sscanf(baud, "%d", &baudrate);
	}


	fprintf(stderr, "%x-> changing to baud rate %s[%d]\n", (int)fd0, baud, baudrate);

	canBusOff((int)fd0);

	/* values for tseg1, tseg2, sjw, noSamp and  syncmode
	 * come from canlib example "simplewrite.c". The doc
	 * says that default values will be taken if baud is one of
	 * the BAUD_* values
	 */
	retval = canSetBusParams((int)fd0, baudrate, 0, 0, 0, 0, 0);
	if (retval != canOK)
	{
		fprintf(stderr, "canChangeBaudRate_driver (Kvaser) :  canSetBusParams() error, returned value = %d, baud=%d, \n", retval, baud);
		canClose((int)fd0);
		return (UNS8)retval;
	}

	canSetBusOutputControl((int)fd0, canDRIVER_NORMAL);
	if (retval != canOK)
	{
		fprintf(stderr, "canChangeBaudRate_driver (Kvaser) :  canSetBusOutputControl() error, returned value = %d\n", retval);
		canClose((int)fd0);
		return (UNS8)retval;
	}

	retval = canBusOn((int)fd0);
	if (retval != canOK)
	{
		fprintf(stderr, "canChangeBaudRate_driver (Kvaser) :  canBusOn() error, returned value = %d\n", retval);
		canClose((int)fd0);
		return (UNS8)retval;
	}

	return 0;
#endif
}


/**
 *
 * CAN_HANDLES must have value >=1 while CANLIB wants handles >= 0
 * so fd0 needs to be decremented before use.
 */
int canClose_driver(CAN_HANDLE fd0)
{
	ECI_RESULT  hResult     = ECI_OK;
	ECI_CTRL_HDL  dwCtrlHandle  = fd0;

	//*** Stop Controller
	hResult = ECI109_CtrlStop(dwCtrlHandle, ECI_STOP_FLAG_NONE);
	ECIDEMO_CHECKERROR(ECI109_CtrlStop);

	//*** Wait some time to ensure bus idle
	OS_Sleep(250);

	//*** Close ECI Controller
	hResult = ECI109_CtrlClose(dwCtrlHandle);
	ECIDEMO_CHECKERROR(ECI109_CtrlClose);
	dwCtrlHandle = ECI_INVALID_HANDLE;

	//*** Clean up ECI driver
	ECI109_Release();

	return (int)hResult;
}

