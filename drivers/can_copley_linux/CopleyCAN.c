/* Copley Controls CAN-PCI-02 interface functions */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define _INCLUDE_INTERNAL_DRIVER_INFO
#include "CopleyCAN.h"

// local functions
static void *ccAllocLocals( void );
static void  ccFreeLocals( void *lcl );
static int   ccOpenFile( int port, void *lcl );
static int   ccCloseFile( void *lcl );
static int   ccSendCMD( void *lcl, uint32_t *cmd );
static int   ccRecvMsg( void *lcl, CANCARD_MSG *can );
static int   ccSendMsg( void *lcl, CANCARD_MSG *can );

/**
 * Open a CAN port on a Copley CAN card.
 *
 * Parameters:
 *   port  The port number to open.  Ports are numbered starting at zero.
 *         There are two ports for each card, so if the system contains
 *         N CAN cards, then legal port numbers range from 0 to 2*N-1.
 *
 *   baud  The baud rate to set the port to.  This should be one of the 
 *         values defined in the CopleyCAN.h header file.
 *
 *   error If an error occurs opening the card, an appropriate error code
 *         will be returned here.  This parameter may be passed as NULL
 *         if no error information is desired.
 *
 * Returns:
 *    A pointer to a local structure that should be passed to other functions.
 *    NULL will be returned on failure.
 */
void *CopleyCanOpen( int port, int baud, int *error )
{
   int err = 0;
   void *lcl;
   uint32_t cmd[64];

   // Allocate a structure to hold local data
   lcl = ccAllocLocals();
   if( lcl == NULL )
   {
      err = COPLEYCAN_ERR_ALLOC;
      goto done;
   }

   // Open a handle to the device driver
   err = ccOpenFile( port, lcl );
   if( err )
      goto freeLocals;

   // Set the port's baud rate
   cmd[0] = (CANCARD_CMD_SETBPS<<16) | 1;
   cmd[1] = baud;
   err = ccSendCMD( lcl, cmd );
   if( err ) goto closeHandle;

   // Open the CAN port
   cmd[0] = (CANCARD_CMD_OPENPORT<<16);
   err = ccSendCMD( lcl, cmd );
   if( err ) goto closeHandle;

   return lcl;

closeHandle:
   ccCloseFile( lcl );

freeLocals:
   ccFreeLocals( lcl );

done:
   if( *error ) *error = err;
   return NULL;
}

/**
 * Close the CAN port.
 *
 * Parameters:
 *   local Pointer to the local data returned by CopleyCanOpen
 *
 * Returns:
 *    An error code or zero on success.
 */
int CopleyCanClose( void *local )
{
   uint32_t cmd[64];

   assert( local != NULL );

   // Close the CAN port
   cmd[0] = (CANCARD_CMD_CLOSEPORT<<16);
   ccSendCMD( local, cmd );

   // Close the handle to the driver
   return ccCloseFile( local );
}

/**
 * Read the next CAN message from the port.  This function will block until
 * a message is available, or until the passed timeout expires.
 *
 * Parameters:
 *   local Pointer to the local data returned by CopleyCanOpen
 *
 *   frame Pointer to the CAN message structure that will be filled in on success.
 *
 *   timeout Timeout in milliseconds.  Zero timeouts indicate that the thread should
 *           never block if no data is available.  Negative timeouts indicate that the
 *           thread should wait forever.
 *
 * Returns:
 *    An error code or zero on success.
 *
 */
int CopleyCanRecv( void *local, CanFrame *frame, int32_t timeout )
{
   CANCARD_MSG can;
   int i, err;

   assert( local != NULL );
   assert( frame != NULL );

   can.timeout = timeout;

   err = ccRecvMsg( local, &can );
   if( err ) return err;

   frame->id     = can.id;
   frame->length = can.flags & COPLEYCAN_CANFLG_LENGTH;
   frame->type   = (can.flags & COPLEYCAN_CANFLG_RTR) ? 
                    CAN_FRAME_REMOTE : CAN_FRAME_DATA;

   if( frame->length > 8 ) frame->length = 8;
   if( can.flags & COPLEYCAN_CANFLG_EXTENDED )
      frame->id |= 0x20000000;

   for( i=0; i<frame->length; i++ )
      frame->data[i] = can.data[i];

   return 0;
}

/**
 * Write a message to the CAN network.
 *
 * Parameters:
 *   local Pointer to the local data returned by CopleyCanOpen
 *
 *   frame Pointer to the CAN message to transmit.
 *
 *   timeout Timeout in milliseconds.  Zero timeouts indicate that the thread should
 *           never block if no data is available.  Negative timeouts indicate that the
 *           thread should wait forever.
 *
 * Returns:
 *    An error code or zero on success.
 *
 */
int CopleyCanXmit( void *local, CanFrame *frame, int32_t timeout )
{
   CANCARD_MSG can;
   int i;

   assert( local != NULL );
   assert( frame != NULL );

   if( frame->length > 8 )
      return COPLEYCAN_ERR_BAD_PARAM;

   can.timeout = timeout;
   can.id      = frame->id;
   can.flags   = frame->length;

   switch( frame->type )
   {
      case CAN_FRAME_DATA:
	 break;

      case CAN_FRAME_REMOTE:
	 can.flags |= COPLEYCAN_CANFLG_RTR;
	 break;

      default:
	 return COPLEYCAN_ERR_BAD_PARAM;
   }

   if( frame->id & 0x20000000 )
      can.flags |= COPLEYCAN_CANFLG_EXTENDED;

   for( i=0; i<frame->length; i++ )
      can.data[i] = frame->data[i];

   // Send the message
   return ccSendMsg( local, &can );
}

/*********************************************************************************
 * Code below this section is operating system specific.
 ********************************************************************************/
#ifdef WIN32

#include <windows.h>
typedef struct
{
   HANDLE hndl;
} DriverLocal;

// Allocate a structure to hold operating system specific local data
static void *ccAllocLocals( void )
{
   return malloc( sizeof(DriverLocal) );
}

static void ccFreeLocals( void *lcl )
{
   free( lcl );
}

// Open the specified driver and initialize the local data structure 
// as necessary.
static int ccOpenFile( int port, void *lcl )
{
   DriverLocal *local = (DriverLocal *)lcl;
   char name[400];
   int cardNum = port/2;
   port &= 1;

   // Convert the name into a device name
   _snprintf( name, sizeof(name), "\\\\.\\copleycan%02d\\%d", cardNum, port );

   local->hndl = CreateFile( name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 
			     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, NULL );
   if( local->hndl == INVALID_HANDLE_VALUE )
      return COPLEYCAN_ERR_CANT_OPEN_PORT;

   return 0;
}

int ccCloseFile( void *lcl )
{
   DriverLocal *local = (DriverLocal *)lcl;
   CloseHandle( local->hndl );
   return 0;
}

int DoIoCtl( DriverLocal *local, uint32_t code, void *buff, uint32_t inBytes, 
                             uint32_t *outBytes, int32_t timeout )
{
   OVERLAPPED overlap;
   DWORD tot, ret;
   int err;
   BOOL ok;

   memset( &overlap, 0, sizeof(OVERLAPPED) );
   overlap.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

   ok = DeviceIoControl( local->hndl, code, buff, inBytes, buff, *outBytes, &tot, &overlap );
   if( ok ) return COPLEYCAN_ERR_DRIVER;

   err = GetLastError();
   if( err != ERROR_IO_PENDING )
      return COPLEYCAN_ERR_DRIVER;

   if( timeout < 0 ) timeout = INFINITE;
   ret = WaitForSingleObject( overlap.hEvent, timeout );

   // If the wait timed out, try another wait with zero timeout.  Windows
   // will sometimes timeout incorrectly.
   if( ret == WAIT_TIMEOUT )
      ret = WaitForSingleObject( overlap.hEvent, 0 );

   switch( ret )
   {
      case WAIT_OBJECT_0:
	 err = 0;
	 break;

      case WAIT_TIMEOUT:
	 err = COPLEYCAN_ERR_TIMEOUT;
	 break;

      default:
	 err = COPLEYCAN_ERR_DRIVER;
	 break;
   }

   if( err )
   {
      CancelIo( local->hndl );
      CloseHandle( overlap.hEvent );
      return err;
   }

   ok = GetOverlappedResult( local->hndl, &overlap, &tot, FALSE );

   *outBytes = tot;

   CloseHandle( overlap.hEvent );
   return 0;
}

/*
 * Write a command to the card.
 * @param lcl Local parameters
 * @param cmd Array of command data.  This array should be at least 64 words in length.
 * @return An error, or null on success.
 */
static int ccSendCMD( void *lcl, uint32_t *cmd )
{
   int sendBytes, err;
   uint32_t tot;
   DriverLocal *local = (DriverLocal *)lcl;

   // Find the number of bytes of data sent with the message.
   sendBytes = 4 * ((cmd[0] & 0x3F) + 1);

   tot = 256;
   err = DoIoCtl( local, COPLEYCAN_IOCTL_CMD, cmd, sendBytes, &tot, 5000 );
   if( err ) return err;

   return cmd[0]>>16;
}

static int ccRecvMsg( void *lcl, CANCARD_MSG *can )
{
   DriverLocal *local = (DriverLocal *)lcl;
   uint32_t tot = sizeof(CANCARD_MSG);

   int err = DoIoCtl( local, COPLEYCAN_IOCTL_RECVCAN, can, sizeof(CANCARD_MSG), &tot, can->timeout );
   if( err ) return err;

   if( tot == 4 )
      err = *(int32_t *)can;

   return err;
}

static int ccSendMsg( void *lcl, CANCARD_MSG *can )
{
   DriverLocal *local = (DriverLocal *)lcl;
   uint32_t tot = sizeof(CANCARD_MSG);

   int err = DoIoCtl( local, COPLEYCAN_IOCTL_SENDCAN, can, sizeof(CANCARD_MSG), &tot, can->timeout );
   if( err ) return err;

   if( tot == 4 )
      err = *(int32_t *)can;
   return err;
}

#else

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef struct
{
   int fd;
} DriverLocal;

// Allocate a structure to hold operating system specific local data
static void *ccAllocLocals( void )
{
   return malloc( sizeof(DriverLocal));
}

static void ccFreeLocals( void *lcl )
{
   free( lcl );
}

// Open the specified driver and initialize the local data structure 
// as necessary.
static int ccOpenFile( int port, void *lcl )
{
   DriverLocal *local = (DriverLocal *)lcl;

   // Convert the name into a device name
   char name[400];
   snprintf( name, sizeof(name), "/dev/copleycan%02d", port );

   local->fd = open( name, O_RDWR );
   if( local->fd < 0 )
      return COPLEYCAN_ERR_CANT_OPEN_PORT;
   return 0;
}

static int ccCloseFile( void *lcl )
{
   DriverLocal *local = (DriverLocal *)lcl;
   close(local->fd);
   return 0;
}

/**
 * Write a command to the card
 */
static int ccSendCMD( void *lcl, uint32_t *cmd )
{
   DriverLocal *local = (DriverLocal *)lcl;
   int err = ioctl( local->fd, COPLEYCAN_IOCTL_CMD, cmd );
   if( !err ) err = cmd[0]>>16;
   return err;
}

static int ccRecvMsg( void *lcl, CANCARD_MSG *can )
{
   DriverLocal *local = (DriverLocal *)lcl;
   return ioctl( local->fd, COPLEYCAN_IOCTL_RECVCAN, can );
}

static int ccSendMsg( void *lcl, CANCARD_MSG *can )
{
   DriverLocal *local = (DriverLocal *)lcl;
   return ioctl( local->fd, COPLEYCAN_IOCTL_SENDCAN, can );
}

#endif

