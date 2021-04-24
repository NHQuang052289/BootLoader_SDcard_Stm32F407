#include "boot.h"

/*Function Pointer*/
typedef void (*pFunction) (void);

/*variable tracking flashing progress*/
static uint32_t flash_ptr = APP_ADDRESS;

/* This function initializes bootloader and flash.*/
uint8_t BootLoader_Init(void)
{
//	__HAL_RCC_SYSCFG_CLK_ENABLE();

	
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |FLASH_FLAG_PGAERR
												| FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR | FLASH_FLAG_RDERR);
//	FLASH->SR = (FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |FLASH_FLAG_PGAERR
//												| FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR | FLASH_FLAG_RDERR);
	HAL_FLASH_Lock();
	
	return BL_OK;
}

/*This function erase flash memory from address of APP*/
void BootLoader_Erase(void)
{
	HAL_FLASH_Unlock();
//	
//	uint8_t startSector = (APP_ADDRESS - FLASH_BASE) / SIZE_OF_SECTOR_0_3;	
//	uint8_t endSector = 11;
	//erase from ADDRESS of APP in Flash
	for(uint8_t i = 2; i <= 11; i++)
		FLASH_Erase_Sector(i, FLASH_VOLTAGE_RANGE_3);
	
	HAL_FLASH_Lock();
}
/*Begin flash programming*/
void BootLoader_FlashBegin(void)
{
	/*Reset flash destiantion address*/
	flash_ptr = APP_ADDRESS;
	
	HAL_FLASH_Unlock();
}

/*Program a WORD data into flash nad increments the data pointer*/
uint8_t BootLoader_FlashNext(uint64_t data)
{
	if( !(flash_ptr <= (FLASH_BASE + FLASH_SIZE) - 4)
			|| (flash_ptr < APP_ADDRESS)) //if (flash_ptr > 1MB -4byte) or (<address of app)
	{
		HAL_FLASH_Lock();
		return BL_WRITE_ERROR;
	}
	
	if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flash_ptr, data) == HAL_OK)
	{
		/*check the written value*/
		if(*(uint32_t*)flash_ptr != data)
		{
			/*flash content donesn't match source content*/
			HAL_FLASH_Lock();
			return BL_WRITE_ERROR;
		}
		//if ok, increment flash destination address
		flash_ptr += 4;
	}
	else
	{
		/* Error occurred while writing data into Flash */
        HAL_FLASH_Lock();
        return BL_WRITE_ERROR;
	}
		
	return BL_OK;
}
	
/*Finish flash programming*/
uint8_t BootLoader_FlashEnd(void)
{
    /* Lock flash */
    HAL_FLASH_Lock();

    return BL_OK;
}

/*This function checks whether the new application fits into flash*/
uint8_t BootLoade_CheckSize(uint32_t appSize)
{
	return ((FLASH_BASE + FLASH_SIZE - APP_ADDRESS) > appSize) ? BL_OK : BL_SIZE_ERROR;
}
/* This function verifies the checksum of application located in flash.
 *         If ::USE_CHECKSUM configuration parameter is disabled then the
 *         function always returns an error code.*/
uint8_t Bootloader_VerifyChecksum(void)
{
#if(USE_CHECKSUM)
    CRC_HandleTypeDef CrcHandle;
    volatile uint32_t calculatedCrc = 0;

    __HAL_RCC_CRC_CLK_ENABLE();
    CrcHandle.Instance                     = CRC;
    CrcHandle.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_ENABLE;
    CrcHandle.Init.DefaultInitValueUse     = DEFAULT_INIT_VALUE_ENABLE;
    CrcHandle.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_NONE;
    CrcHandle.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
    CrcHandle.InputDataFormat              = CRC_INPUTDATA_FORMAT_WORDS;
    if(HAL_CRC_Init(&CrcHandle) != HAL_OK)
    {
        return BL_CHKS_ERROR;
    }

    calculatedCrc =
        HAL_CRC_Calculate(&CrcHandle, (uint32_t*)APP_ADDRESS, APP_SIZE);

    __HAL_RCC_CRC_FORCE_RESET();
    __HAL_RCC_CRC_RELEASE_RESET();

    if((*(uint32_t*)CRC_ADDRESS) == calculatedCrc)
    {
        return BL_OK;
    }
#endif
    return BL_CHKS_ERROR;
}
/**
 * @brief  This function checks whether a valid application exists in flash.
 *         The check is performed by checking the very first DWORD (4 bytes) of
 *         the application firmware. In case of a valid application, this DWORD
 *         must represent the initialization location of stack pointer - which
 *         must be within the boundaries of RAM.
 * @return Bootloader error code ::eBootloaderErrorCodes
 * @retval BL_OK: if first DWORD represents a valid stack pointer location
 * @retval BL_NO_APP: first DWORD value is out of RAM boundaries
 */
uint8_t Bootloader_CheckForApplication(void)
{
    return (((*(uint32_t*)APP_ADDRESS) - RAM_BASE) <= RAM_SIZE) ? BL_OK
                                                                : BL_NO_APP;
}

/**
 * @brief  This function performs the jump to the user application in flash.
 * @details The function carries out the following operations:
 *  - De-initialize the clock and peripheral configuration
 *  - Stop the systick
 *  - Set the vector table location (if ::SET_VECTOR_TABLE is enabled)
 *  - Sets the stack pointer location
 *  - Perform the jump
 */
void Bootloader_JumpToApplication(void)
{
    uint32_t  JumpAddress = *(__IO uint32_t*)(APP_ADDRESS + 4);
    pFunction Jump        = (pFunction)JumpAddress;

    HAL_RCC_DeInit();
    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
		SCB->SHCSR &= ~(	SCB_SHCSR_USGFAULTENA_Msk |\
									SCB_SHCSR_BUSFAULTENA_Msk |\
									SCB_SHCSR_MEMFAULTENA_Msk );
		__disable_irq();
#if(SET_VECTOR_TABLE)
    SCB->VTOR = APP_ADDRESS;
#endif

    __set_MSP(*(__IO uint32_t*)APP_ADDRESS);
    Jump();
}
/**
 * @brief  This function performs the jump to the MCU System Memory (ST
 *         Bootloader).
 * @details The function carries out the following operations:
 *  - De-initialize the clock and peripheral configuration
 *  - Stop the systick
 *  - Remap the system flash memory
 *  - Perform the jump
 */
void Bootloader_JumpToSysMem(void)
{
    uint32_t  JumpAddress = *(__IO uint32_t*)(SYSMEM_ADDRESS + 4);
    pFunction Jump        = (pFunction)JumpAddress;

    HAL_RCC_DeInit();
    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();

    __set_MSP(*(__IO uint32_t*)SYSMEM_ADDRESS);
    Jump();

    while(1);
}

