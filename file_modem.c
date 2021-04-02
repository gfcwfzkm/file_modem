/*
 * file_modem.c
 * 
 * File modem receiver and transmitter. Currently supports only
 * X-Modem Receiver (both basic X-Modem, with CRC, and with 1k support!)
 * Reference used: http://pauillac.inria.fr/~doligez/zmodem/ymodem.txt
 *
 * Created: 19.03.2021 16:20:44
 *  Author: gfcwfzkm
 */ 

#include "file_modem.h"

/* Supported Packet Sizes. Theoretically, more sizes could be added
 * (Possible Z-Modem Implementation in the future? */
#define PCK_SIZ	128
#define PCK_1K	1024

#define XMODEM_NON_STANDARD

#define SOH		0x01	// Start of Packet, 256 Bytes
#define STX		0x02	// Start of Packet, 1024 Bytes
#define EOT		0x04	// End of Transmission
#define ACK		0x06	// Received data okay
#define NAK		0x15	// Initiate checksum transmission or report corrupted data
#define CAN		0x18	// Abort by the sender
#define CRC16	0x43	// 'C', initiate CRC-16 transmission
#ifdef XMODEM_NON_STANDARD
#define ABORT1	0x41	// Abort by the sender-client-user, small 'a'
#define ABORT2	0x61	// Abort by the sender-client-user, large 'A'
#endif

#define MAX_ERR	10		// Amount of Retries before the receiver gives up
#define SRT_TRY	5		// Amount of retries to initiate a CRC transmission, followed by retries of checksum transmission,
						// before the receiver gives up
						
#define TIMEOUT	3000	// Timeout time in seconds
						// I know, the original docs state 10 seconds, but I feel
						// like that it's a bit long, at 10 retries.

static uint8_t workbuf[PCK_1K];

/**
  * @brief Calculates the CRC Checksum of a data packet
  *
  * @param buf	The data buffer
  * @param siz
  */
static uint16_t _crc16(const uint8_t *buf, uint32_t count)
{
	uint16_t crc = 0;
	uint8_t i = 0;
	
	while(count--)
	{
		crc = crc ^ *buf++ << 8;
		
		for (i = 0; i < 8; i++)
		{
			if (crc & 0x8000)
			{
				crc = crc << 1 ^ 0x1021;
			}
			else
			{
				crc = crc << 1;
			}
		}
	}
	return crc;
}

/**
  * @brief Checks if the packet is valid or not
  *
  * @param use_crc	If Zero, one-byte checksum will be used, else 2-byte CRC,
  *					- also holds the received CRC number
  * @param buf		Pointer to the data packet
  * @param count	Packet Size Length (128 or 1024)
  *
  * @return			One if the check failed, zero if it was successful       */
static uint8_t _checkPacket(uint8_t use_crc, uint16_t crc_checksum, const uint8_t *buf, uint16_t count)
{
	if (use_crc)
	{
		/* X-Modem with 16-bit CRC */
		uint16_t calced_crc = _crc16(buf, count);
		if (calced_crc != crc_checksum)			return 1;
	}
	else
	{
		/* X-Modem with basic 8-bit checksum */
		uint16_t cnt_i = 0;
		uint8_t checksum = 0;
		for (cnt_i = 0; cnt_i < count; cnt_i++)
		{
			checksum += buf[cnt_i];
		}
		if (checksum != (uint8_t)crc_checksum)	return 1;
	}
	return 0;
}

/**
  * @brief Receive Byte from the UART Buffer
  *	
  * Will wait an x-amount of time defined by TIMEOUT. If no data has been received
  *	until then, the function returns a 1, else a zero.
  *
  * @param c	Pointer to a single unsigned byte, where the received char should be stored
  * @return		1 on error, 0 if successful
  */
uint8_t (*_recByte)(uint8_t*, uint16_t);

/**
  * @brief Send one Byte via UART
  *
  * @param c	Byte to send
  */
void (*_sendByte)(uint8_t);

/**
  * @brief Instantly flushes the Rx Buffer
  */
void (*_flushRx)(void);

/** 
  * @brief Receives a packet, checks it's CRC and the expected packet number
  *
  * @param data			Pointer to the receive Buffer (Must be 1024 Bytes large (1k-XMODEM + 3 Head Chars + 2CRC 
  * @param expPacketNum	Expected Packet Number that the sender should transmit. Will be checked by this function
  * @param useCRC		Zero if basic Checksum has to be used, 1 if 16-bit CRC has to be used
  *
  * @return		Result of the receiving process: 0 for a normal packet, 1 for a 1k packet, 2 for a EndOfFile,
  *             3 for a general timeout, 4 for a Check Error, 5 for Abort */
	
static uint8_t _receivePacket(uint8_t *data, uint8_t expPacketNum, uint8_t useCRC)
{
	uint16_t packet_size, cnt_i, receivedCRC = 0;
	uint8_t packet_number[2], ch = 0;
	
	// receive and process first byte
	if (_recByte(&ch, TIMEOUT))		return 3;	
	switch(ch)
	{
		case SOH:	// Normal Packet Size (128 Bytes)
			packet_size = PCK_SIZ;
			break;
		case STX:	// 1k-XMODEM (1024 Bytes)
			packet_size = PCK_1K;
			break;
		case EOT:	// End of File - No more data to be received
			return 2;
		case CAN:	// Abort (not fully standard?)
#ifdef XMODEM_NON_STANDARD
		case ABORT1:
		case ABORT2:
			/* Cancel-Signal from user received (either small 'a' or large 'A'
			 * This is NOT STANDARD from the XMODEM / 1k-XMODEM / YMODEM protocol! */
			return 5;
#endif
		default:
			/* Gibberish received? Retry */
			return 4;
	}
	
	/* Read the packet Number & inversed packet number */
	if (_recByte(&ch, TIMEOUT))	return 3;
	packet_number[0] = ch;
	if (_recByte(&ch, TIMEOUT))	return 3;
	packet_number[1] = ch;
	
	/* Start receiving the data finally */
	for (cnt_i = 0; cnt_i < packet_size; cnt_i++)
	{
		if (_recByte(&ch, TIMEOUT))	return 3;
		data[cnt_i] = ch;
	}
	
	/* Receive the checksum / CRC at the end */
	if (useCRC)
	{
		if (_recByte(&ch, TIMEOUT))	return 3;
		receivedCRC = (uint16_t)ch << 8;
		if (_recByte(&ch, TIMEOUT))	return 3;
		receivedCRC |= ch;
	}
	else
	{
		if (_recByte(&ch, TIMEOUT))	return 3;
		receivedCRC = ch;
	}
	
	/* Checking of the received data packet integrity starts here (if it didn't timeout before) */
	/* Start by checking the packet ID first */
	packet_number[1] = ~packet_number[1];
	if (packet_number[0] != packet_number[1])	return 4;
	if (packet_number[0] != expPacketNum)		return 4;
	
	/* Check Checksum / CRC of the packet */
	if (_checkPacket(useCRC, receivedCRC, data, packet_size))	return 4;
	
	/* Check if normal or 1k package has been processed, return that info */
	if (packet_size == PCK_SIZ)	return 0;
	return 1;
}
/*
static uint8_t _transmitPacket(uint8_t *data, uint8_t packetID, uint8_t useCRC)
{
	
}
*/

/**
  * @brief Initialize the file modem by passing the nessesairy communication functions
  *
  * @param recByte	Function Pointer for the receiver. Gets called with a single uint8_t
  *                 pointer for the received byte, and a uint16_t that states
  *                 the timeout time in Milliseconds. Has to return 0 if successful or
  *                 1 if a timeout occurred.
  * @param sendByte	Function Pointer for the transmitter. Gets called with a single
  *                 uint8_t character to transmit.
  * @param flushRx	Function Pointer to clear / flush the receive Buffer (if one exists)
  */
void file_modem_init(uint8_t (*recByte)(uint8_t*,uint16_t), void (*sendByte)(uint8_t), void (*flushRx)(void))
{
	_recByte = recByte;
	_sendByte = sendByte;
	_flushRx = flushRx;
}

/**
  * @brief Initialize and run a X-Modem receiver
  * Supports either 128 (default) xmodem or 1k xmodem, either classic checksum or 16-bit CRC
  *
  * @param ffd		fatfs FIL to safe the receiving bytes to
  * @param maxsize	Pointer to the maximum size you're willing to download and safe, before aborting
  *                 The amount of received bytes will be stored back in this pointer.
  *
  * @return			0 for a successful execution, 1 for a failed start of the transmission,
  *                 2 for a failed package request, 3 for an abort by user/transmitter,
  *                 4 for fatfs disk potentially full, 5 for received data exceeds maxsize
  */
uint8_t xmodem_receive(FIL *ffd, uint32_t *maxsize)
{
	uint8_t packetResult;		// Result of the function _receivePacket to process
	uint8_t packetCounter = 1;	// Packet Counter. Xmodem starts with Packet 1. Can roll over
	uint8_t failedAttempts = 0;	// Counter of Timeouts or CRC/Checksum Errors. Resets
								// after every successfully received packet.
	uint8_t useCRC = 0;			// States if 16-bit CRC or basic 8-Bit Checksum has to be used
	uint8_t excecuteLoop = 0;	// Loop and Function-Result variable. This function loops as
								// long as excecuteLoop is Zero
	uint16_t fs_bytesWritten;	// For (fatfs) f_write, to check if all Bytes have been written
	uint16_t bytesReceived;		// Holds if a 128 Bytes or 1k Bytes Packet has been received
	uint32_t totalBytesWritten = 0;	// Amount of total Bytes received & written. Will be
								// copied into *maxsize at function end.
	uint8_t initialTransmission = 1;// States that the transmission just started 
								// For CRC/Checkum negotiation
	
	/* Dump Rx Buffer before we start, just to be safe */
	_flushRx();
	
	/* --- Main Receive Loop --- */
	do{
		/* At the first packet, the receiver has to poke the sender to start the transmission.
		 * Try 3 times to initiate a CRC transmission, else fall back to Checksum transmission.*/
		if (initialTransmission)
		{
			if (useCRC)
			{
				_sendByte(CRC16);
			}
			else
			{
				_sendByte(NAK);
			}
		}
		
		/* Receive the Packet */
		packetResult = _receivePacket(workbuf, packetCounter, useCRC);
		
		/* Process the result of the packet-receiving */
		switch(packetResult)
		{
			case 0:	/* 128 Bytes packet received */		
			case 1:	/*  1k Bytes packet received */
				/* Increment Packet Counter, reset the Error/Timeout Counter (failedAttempts)
				 * and state that the initialTransmission is over */
				packetCounter++;
				failedAttempts = 0;
				initialTransmission = 0;
				
				/* Has a 128 Bytes or 1k Bytes packet been received?
				 * Writing the packet size into bytesReceived */
				bytesReceived = (packetResult ? PCK_SIZ : PCK_1K);
				
				/* Write the received Bytes from the buffer into the fileystem */
				f_write(ffd, workbuf, bytesReceived, &fs_bytesWritten);
				
				/* Disk full? Lets hope not! */
				if (fs_bytesWritten < bytesReceived)
				{
					// Error, Disk full!
					return 5;
				}
				
				/* File size larger than originally allowed/states? */
				totalBytesWritten += bytesReceived;
				if(totalBytesWritten >= *maxsize)
				{
					// Error, max size reached!
					return 6;
				}
				
				/* Syncing the FS to reduce the data loss at a sudden power-down */
				f_sync(ffd);
				
				/* Informing the Sender, that the packet has been recieved, processed
				 * and that we're ready for the next packet. */
				_sendByte(ACK);
				break;
			case 2:	/* End of File received */
				
				_sendByte(ACK);
				failedAttempts = 0;
				excecuteLoop = 1;
				break;
			case 3:	/* Timeout */
			case 4:	/* Checksum / Packet-ID / CRC Error */
				/* Flush the Rx Buffer, assumed we have received only gibberish */
				_flushRx();
				
				if (initialTransmission)
				{
					/* Still negotiating if CRC or Checksum has to be used?
					 * After SRT_TRY amount of failed attempts, fall back to
					 * classic checksum (which client doesn't support CRC anyways?) */
					failedAttempts++;
					if ( (failedAttempts == SRT_TRY) && useCRC)
					{
						useCRC = 0;
						failedAttempts = 0;
					}
					else if ( (failedAttempts == SRT_TRY) && !useCRC)
					{
						excecuteLoop = 2;
					}
				}
				else
				{
					/* Failed transmission, retry MAX_ERR amount of times
					 * before aborting the transmission */
					failedAttempts++;
					if (failedAttempts >= MAX_ERR)
					{
						excecuteLoop = 3;
					}
					_sendByte(NAK);
				}
				break;
			case 5:	/* Aborted by Sender or User */
				_flushRx();
				excecuteLoop = 4;
				break;
		}
	/* Loop as long as executeLoop is Zero.
	 * If not Zero, exit loop, subscract 1 from it
	 * and return it as function result. */
	}while(excecuteLoop == 0);
	
	/* Return the amount of bytes received */
	*maxsize = totalBytesWritten;
	
	/* Return the Result */
	return --excecuteLoop;
}
