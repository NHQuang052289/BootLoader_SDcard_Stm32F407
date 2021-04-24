/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "fatfs.h"

/* USER CODE BEGIN Includes */
#include "boot.h"
#include "math.h"

#define MAX_NUMBER_OF_LINE 46
#define MAX_NUMBER_OF_DATA 32
#define USER_SECTOR 2
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
SD_HandleTypeDef hsd;
HAL_SD_CardInfoTypedef SDCardInfo;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
FATFS fatfs;
FIL myFile, newFile;
FRESULT fresult;
char fileName[] = "BlinkLed.hex";
char fileTxt[] = "BlinkLed.txt";
char member_of_line[MAX_NUMBER_OF_LINE] = {0}; //a line of hex file when it is read, create a array

volatile int NbrOfData = 0;	//number of datas on a line = 2 * byte count
volatile int countLine = 0, NbrOfLine = 0;	//count number of lines in hex file
volatile uint32_t userAdd = APP_ADDRESS;


typedef struct HexStruct	//a hex format on a line in file .hex
{
	__IO uint8_t byteCount;//1byte
	__IO uint16_t addRess;	//2byte address
	__IO uint8_t recordType; //1byte 
	__IO uint32_t u32data[4];	//4*4 = 16byte
	__IO uint8_t checkSum; //1byte
}HexTypeDef;

//volatile HexTypeDef *aHex;	//pointer constants hex lines
volatile HexTypeDef aHex[200];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SDIO_SD_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
uint8_t Hex2Int(char c)
{
	switch(c)
	{
		case '0':	return 0;
		case '1': return 1;
		case '2':	return 2;
		case '3': return 3;
		case '4':	return 4;
		case '5': return 5;
		case '6':	return 6;
		case '7': return 7;
		case '8':	return 8;
		case '9': return 9;
		case 'A':	return 10;
		case 'B': return 11;
		case 'C':	return 12;
		case 'D': return 13;
		case 'E':	return 14;
		case 'F': return 15;		
	}
	return 0;
}
/*This function will process hex file and output a text file*/
void ProcessHex(void)
{
//	aHex = malloc(200 * sizeof(HexTypeDef));
	if(BSP_SD_Init() == MSD_OK)//init SD succes
	{
	
		fresult = f_mount(&fatfs, "", 1);

		fresult = f_open(&myFile, fileName, FA_READ);//delete old txt file

		while(!f_eof(&myFile))	//if not end of file hex
		{
			 f_gets(member_of_line, MAX_NUMBER_OF_LINE*sizeof(char), &myFile);
			
//			character 0 is ':', ignore
//			character 1,2 are byte count
//			charecter 3,4,5,6 are address
//			charecter 7,8 are record type
//			2 last charecters are check sum
//			the rest is data
			
			//convert byte count, address, record type, check sum  => to DEC
			aHex[countLine].byteCount = Hex2Int(member_of_line[1]) * 16 + Hex2Int(member_of_line[2]);
			aHex[countLine].addRess = Hex2Int(member_of_line[3]) * pow(16,3) + Hex2Int(member_of_line[4])*pow(16,2)
																+ Hex2Int(member_of_line[5]) * pow(16,1) + Hex2Int(member_of_line[6]);
			aHex[countLine].recordType = Hex2Int(member_of_line[7]) * 16 + Hex2Int(member_of_line[8]);
			NbrOfData = 2 * aHex[countLine].byteCount; 
			
			aHex[countLine].checkSum = Hex2Int(member_of_line[9+NbrOfData]) * 16 + Hex2Int(member_of_line[10+NbrOfData]);
			
			if(NbrOfData > 0)
			{
				char temp_data[MAX_NUMBER_OF_DATA] = {0};	
				for(uint8_t i = 0; i < NbrOfData; i++)
				{
					//data from 9,10...,41 on a line
					temp_data[i] = Hex2Int(member_of_line[9+i]);
				}
			
				//convert data => u32bit, eg: 0,1,2,3,4,5,6,7 => 67452301
				for(uint8_t i = 0; i < 4; i++)
				{
					aHex[countLine].u32data[i] = 0;	
					aHex[countLine].u32data[i] = ((temp_data[8*i+0] << 4) | temp_data[8*i+1]) << 0; 
					aHex[countLine].u32data[i] |= ((temp_data[8*i+2] << 4) | temp_data[8*i+3]) << 8;
					aHex[countLine].u32data[i] |= ((temp_data[8*i+4] << 4) | temp_data[8*i+5]) << 16;
					aHex[countLine].u32data[i] |= ((temp_data[8*i+6] << 4) | temp_data[8*i+7]) << 24;
				}
				
			}
			countLine++;
		}
		f_close(&myFile);
		//create text file in SD card
		fresult = f_open(&newFile, fileTxt, FA_CREATE_ALWAYS | FA_WRITE);
		for(int i = 0; i < countLine; i++)
		{
			if(aHex[i].recordType == 0x00)	//if is data to flash
			{
				f_printf(&newFile, "0x%x, 0x%x, 0x%x, 0x%x ", aHex[i].u32data[0],aHex[i].u32data[1],
																										aHex[i].u32data[2],aHex[i].u32data[3]);
			}
		}
	}
}
void Flash_Write(void)
{
	
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
							FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
	//erase sector of app
	FLASH_Erase_Sector(USER_SECTOR, FLASH_VOLTAGE_RANGE_3);

	//NbrOfLine = countLine;
	//write data into flash
	for(int i = 0; i < countLine; i++)
	{
		if(aHex[i].recordType == 0x00)	//if is data to flash
		{
			for(uint8_t j = 0; j < 4; j++)
			{
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, userAdd, aHex[i].u32data[j]);
				userAdd += 4;
			}
		}	
	}
	HAL_FLASH_Lock();
}

void Boot(void)
{
  HAL_RCC_DeInit();
	HAL_DeInit();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;

	/* Clear Pending Interrupt Request, turn  off System Tick*/
	SCB->SHCSR &= ~( SCB_SHCSR_USGFAULTENA_Msk |\
									SCB_SHCSR_BUSFAULTENA_Msk |\
									SCB_SHCSR_MEMFAULTENA_Msk ) ;
	
	__disable_irq ();

	/* Set Main Stack Pointer*/
	__set_MSP(*((volatile uint32_t*) APP_ADDRESS));
	/* Set Program Counter to Blink LED Apptication Address*/
	void (*jump)(void) =  (void(*)(void)) (*((volatile uint32_t *) (APP_ADDRESS + 4)));
  jump();
	//HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
	while(1);

}
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();

  /* USER CODE BEGIN 2 */
	//	Process hex file from SDcard
	ProcessHex();
	//	Write data into flash
	Flash_Write();
	//BootLoader main
	Boot();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* SDIO init function */
void MX_SDIO_SD_Init(void)
{

  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 0;

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PD12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
