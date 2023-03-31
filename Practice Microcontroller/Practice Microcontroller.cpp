#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_uart.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_rtc.h"
#include "stdint.h"
#include <stdint.h>
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_tim.c"
#include <stdio.h>
#include "string.h"
#include "stm32f4xx_hal_flash_ex.h"

#define FLASH_START_ADDR 0x080E0000
#define FLASH_END_ADDR   0x080FFFFF
#define DATA_SIZE        10
#define FLASH_PAGE_SIZE  2048

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2;
RTC_HandleTypeDef hrtc;
FLASH_EraseInitTypeDef EraseInitStruct;
TIM_HandleTypeDef htim6;
RTC_TimeTypeDef sTime;

uint32_t adc_value = 0;
uint32_t flash_addr = FLASH_START_ADDR;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RTC_Init(void);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim);
void send_adc_value(void);
void write_adc_value_to_flash(void);
void read_adc_values_from_flash(void);
void erase_flash_memory(void);

#ifdef __cplusplus
extern "C" {
#endif

    void HAL_Delay(uint32_t Delay);

#ifdef __cplusplus
}
#endif


int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_RTC_Init();

    HAL_ADC_Start(&hadc1);

    htim6.Instance = TIM6;
    htim6.Init.Prescaler = 83;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = 999;
    htim6.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim6);
    HAL_TIM_Base_Start_IT(&htim6);

    while (1)
    {
        HAL_Delay(1000);
        send_adc_value();
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM6)
    {
        HAL_ADC_PollForConversion(&hadc1, 100);
        adc_value = HAL_ADC_GetValue(&hadc1);

        if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK)
        {
            if (sTime.Seconds % 5 == 0)
            {
                write_adc_value_to_flash();
                if (flash_addr >= FLASH_END_ADDR)
                {
                    erase_flash_memory();
                }
            }
        }
    }
}

void send_adc_value(void)
{
    char buffer[20];
    sprintf(buffer, "%ld\n", adc_value);
    HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), 100);
}

void write_adc_value_to_flash(void)
{
    HAL_FLASH_Unlock();

    uint32_t data[DATA_SIZE];
    for (int i = 0; i < DATA_SIZE; i++)
    {
        data[i] = 0;
    }

    // Read the data already stored in the flash memory
    read_adc_values_from_flash();

    // Update the data buffer with the new ADC value
    data[flash_addr / 4 - FLASH_START_ADDR / 4] = adc_value;

    // Erase the page containing the flash address
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = flash_addr;
    EraseInitStruct.NbPages = 1;

    uint32_t PageError = 0;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    {
        // Error occurred while erasing flash memory
        // Handle the error here
        return;
    }

    // Write the updated data buffer to the flash memory
    for (int i = 0; i < DATA_SIZE; i++)
    {
        if (FLASH_Program(FLASH_TYPEPROGRAM_WORD, flash_addr + i * 4, data[i]) != HAL_OK)
        {
            // Error occurred while writing to flash memory
            // Handle the error here
            return;
        }
    }

    flash_addr += DATA_SIZE * 4;

    HAL_FLASH_Lock();
}