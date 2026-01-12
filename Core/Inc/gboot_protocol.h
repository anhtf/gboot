#ifndef GBOOT_PROTOCOL_H
#define GBOOT_PROTOCOL_H

#include <stdint.h>

#define GBOOT_CMD_SYNC        0x5A
#define GBOOT_ACK             0x79
#define GBOOT_NACK            0x1F

/* Commands */
#define CMD_GET_INFO          0x01
#define CMD_ERASE_APP         0x02
#define CMD_WRITE_PAGE        0x03
#define CMD_RESET_DEV         0x04
#define CMD_JUMP_APP          0x05

/* Magic Number for Backup Register (BKP_DR1) */
#define GBOOT_MAGIC_KEY       0x5AA5DEAD

/* App Information */
#define APP_START_ADDRESS     0x08004000
//#define FLASH_PAGE_SIZE       1024       /* STM32F103 Medium Density usually 1KB pages */

#endif // GBOOT_PROTOCOL_H
