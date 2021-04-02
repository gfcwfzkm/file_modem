/*
 * file_modem.h
 *
 * File modem receiver and transmitter. Currently supports only
 * X-Modem Receiver (both basic X-Modem, with CRC, and with 1k support!)
 * Reference used: http://pauillac.inria.fr/~doligez/zmodem/ymodem.txt
 *
 * Created: 19.03.2021 16:20:59
 *  Author: gfcwfzkm
 */ 


#ifndef FILE_MODEM_H_
#define FILE_MODEM_H_

#include <inttypes.h>
#include "ff.h"

/**
  * @brief Initialize the file modem by passing the nessesairy communication functions
  * Example implementations can be found at the end of this document.
  *
  * @param recByte	Function Pointer for the receiver. Gets called with a single uint8_t
  *                 pointer for the received byte, and a uint16_t that states
  *                 the timeout time in Milliseconds. Has to return 0 if successful or
  *                 1 if a timeout occurred.
  * @param sendByte	Function Pointer for the transmitter. Gets called with a single
  *                 uint8_t character to transmit.
  * @param flushRx	Function Pointer to clear / flush the receive Buffer (if one exists)
  */
void file_modem_init(uint8_t (*recByte)(uint8_t*,uint16_t), void (*sendByte)(uint8_t), void (*flushRx)(void));	// figure out proper init

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
uint8_t xmodem_receive(FIL *ffd, uint32_t *maxsize);

/* Planned functions:
uint8_t xmodem_send(FIL *ffd, uint32_t fileSize);
uint8_t ymoden_receive(FIL *ffd, uint32_t maxsize, char *getName);
uint8_t ymoden_transmit(FIL *ffd, uint32_t fileSize, char *sendName);
*/

/* Example functions to pass to file_modem_init:
 *
 * Receive Function:
 *  uint8_t fm_recByte(uint8_t *ch, uint16_t timeout_ms)
 *  {
 * 		gf_delay_t timeout = {gf_millis(), (uint32_t)timeout_ms};
 *		uint16_t recCh;
 *		while(!gf_TimeCheck(&timeout, gf_millis()))
 *		{
 *			recCh = uart_getc(&(mcuBoard->uartConsole));
 *			if ((recCh >> 8) == UART_DATA_AVAILABLE)
 *			{
 *				*ch = (uint8_t)recCh;
 *				return 0;
 *			}
 *		}
 *		return 1;
 *	}
 *
 * Send Function:
 *	void fm_sendByte(uint8_t ch)
 *	{
 *		uart_putc(&(mcuBoard->uartConsole), ch);
 *	}
 *
 * Flush Receive Buffer Function:
 *	void fm_flushRx(void)
 *	{
 *		do
 *		{
 *		} while ((uart_getc(&(mcuBoard->uartConsole)) >> 8) != UART_NO_DATA);
 *	}
 *
 * .....
 * Initialise file modem:
 * file_modem_init(&fm_recByte, &fm_sendByte, &fm_flushRx);
 */

#endif /* FILE_MODEM_H_ */
