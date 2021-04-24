#ifndef _BOOT_H_
#define _BOOT_H_

#include "stm32f4xx_hal.h"

/*start address of application in Flash*/
#define APP_ADDRESS (uint32_t) 0x08008000

/*end address of application in Flash (address of last WORD)*/
#define END_ADDRESS (uint32_t) 0x080FFFFB

/*address of System Memory (ST bootloader)*/
#define SYSMEM_ADDRESS (uint32_t)0x1FFF0000

/*size of application (WORD)*/
#define APP_SZIE (uint32_t) ( ((END_ADDRESS - APP_ADDRESS) + 3) / 4 )

/* MCU RAM information (to check whether flash contains valid application) */
#define RAM_BASE SRAM1_BASE     /*!< Start address of RAM */
#define RAM_SIZE 0x20000 /*!< RAM size in bytes */

#define FLASH_SIZE 0x100000	//1MB
#define SIZE_OF_SECTOR_0_3 0x4000	//16Kb
/** Bootloader error codes */
enum eBootloaderErrorCodes
{
    BL_OK = 0,      /*!< No error */
    BL_NO_APP,      /*!< No application found in flash */
    BL_SIZE_ERROR,  /*!< New application is too large for flash */
    BL_CHKS_ERROR,  /*!< Application checksum error */
    BL_ERASE_ERROR, /*!< Flash erase error */
    BL_WRITE_ERROR, /*!< Flash write error */
    BL_OBP_ERROR    /*!< Flash option bytes programming error */
};

uint8_t BootLoader_Init(void);
void BootLoader_Erase(void);
void BootLoader_FlashBegin(void);
uint8_t BootLoader_FlashNext(uint64_t data);
uint8_t BootLoader_FlashEnd(void);
uint8_t BootLoader_CheckSize(uint32_t appSize);
uint8_t Bootloader_VerifyChecksum(void);
uint8_t Bootloader_CheckForApplication(void);
void Bootloader_JumpToApplication(void);
void Bootloader_JumpToSysMem(void);


#endif



