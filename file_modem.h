/*
 * file_modem.h
 *
 * Created: 19.03.2021 16:20:59
 *  Author: gfcwfzkm
 */ 


#ifndef FILE_MODEM_H_
#define FILE_MODEM_H_

#include <inttypes.h>
#include "ff.h"

#define XMODEM_NON_STANDARD

enum file_modem {FM_OK,FM_INVALID_START,FM_TIMEOUT,FM_ABORTED};

void file_modem_init(uint8_t (*recByte)(uint8_t*,uint16_t), void (*sendByte)(uint8_t), void (*flushRx)(void));
enum file_modem xmodem_receive(FIL *p_ffd, uint32_t *p_maxsize);

/*
This is in the works / To do:
uint8_t xmodem_send(FIL *ffd, uint32_t u32_fileSize);
uint8_t ymoden_receive(FATFS *p_fs, uint32_t *p_maxsize, char *p_filename);
uint8_t ymoden_transmit(FIL *ffd, uint32_t u32_fileSize, char *sendName);
*/

#endif /* FILE_MODEM_H_ */