#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "CopleyCAN.h"

#include "can_driver.h"

UNS8 canReceive_driver(CAN_HANDLE fd0, Message *m)
{
	int err = COPLEYCAN_ERR_OK;
	CanFrame frame;
	int i;

	err = CopleyCanRecv( fd0, &frame, 500 );
	if( err )
	{
		printf( "Error %d waiting on message\n", err );
		return err;
	}

	m->cob_id = frame.id;
	m->len = frame.length;
	m->rtr = frame.type;

	for(i=0; i<8; i++)
		m->data[i] = frame.data[i];

	return err;
}

/**
 *
 * CAN_HANDLES must have value >=1 while CANLIB wants handles >= 0
 * so fd0 needs to be decremented before use.
 *
 */
UNS8 canSend_driver(CAN_HANDLE fd0, Message const *m)
{
	int err = COPLEYCAN_ERR_OK;
	CanFrame frame;
	int i;

	frame.id = m->cob_id;
	frame.length = m->len;
	frame.type = m->rtr;

	err = CopleyCanXmit( fd0, &frame, 500 );
	if( err )
		printf( "Error %d sending frame\n", err );

	return err;
}



int TranslateBaudRate(char* optarg)
{
	if(!strcmp( optarg, "1M"))
		return COPLEYCAN_BITRATE_1000000;
	if(!strcmp( optarg, "800K"))
		return COPLEYCAN_BITRATE_800000;
	if(!strcmp( optarg, "500K"))
		return COPLEYCAN_BITRATE_500000;
	if(!strcmp( optarg, "250K"))
		return COPLEYCAN_BITRATE_250000;
	if(!strcmp( optarg, "125K"))
		return COPLEYCAN_BITRATE_125000;
	if(!strcmp( optarg, "100K"))
		return COPLEYCAN_BITRATE_100000;
	if(!strcmp( optarg, "50K"))
		return COPLEYCAN_BITRATE_50000;

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
	int channel, baud;
	int err = COPLEYCAN_ERR_OK;
	CAN_HANDLE fd0 = NULL;

	sscanf(board->busname, "%d", &channel);
	baud = TranslateBaudRate(board->baudrate);

	fd0 = CopleyCanOpen( channel, baud, &err );
	if( !fd0 )
	{
		printf( "Error opening CAN port: %d\n", err );
		return fd0;
	}
	return fd0;
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
	int err = COPLEYCAN_ERR_OK;

	err = CopleyCanClose(fd0);
	if( err )
		printf( "Error %d closing port\n", err );

	return err;
}
