#include "gboot.h"
#include "gboot_protocol.h"
#include <string.h>

/* Private Method Prototypes */
static void GBoot_Init(GBoot_t* this, UART_HandleTypeDef* huart);
static void GBoot_Run(GBoot_t* this);
static void GBoot_OnRxEvent(GBoot_t* this, uint16_t Size);
static void GBoot_ProcessCommand(GBoot_t* this);
static void GBoot_SendResponse(GBoot_t* this, uint8_t* data, uint16_t len);
static void GBoot_JumpToApp(GBoot_t* this);
static void GBoot_SendAck(GBoot_t* this);
static void GBoot_SendNack(GBoot_t* this);

/* Define the Global Instance */
GBoot_t gboot;

/* Constructor */
void GBoot_Create(GBoot_t* instance)
{
    /* Bind Methods */
    instance->Init = GBoot_Init;
    instance->Run = GBoot_Run;
    instance->OnRxEvent = GBoot_OnRxEvent;
    
    /* Clear State */
    instance->cmd_received = 0;
    instance->is_busy = 0;
    memset(instance->rx_buffer, 0, GBOOT_RX_BUF_SIZE);
}

/* --- Implementations --- */

static void GBoot_Init(GBoot_t* this, UART_HandleTypeDef* huart)
{
    this->huart = huart;

    /* 1. Hardware Init / Trigger Check */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    
    // Check Magic Number
    if (BKP->DR1 != GBOOT_MAGIC_KEY)
    {
        // No trigger, try to jump to App
        GBoot_JumpToApp(this);
        // If we returned, staying in bootloader
    }
    
    // Clear Magic
    BKP->DR1 = 0;
    
    /* 2. Start Reception (DMA + IDLE) */
    HAL_UARTEx_ReceiveToIdle_DMA(this->huart, this->rx_buffer, GBOOT_RX_BUF_SIZE);
}

static void GBoot_Run(GBoot_t* this)
{
    while (1)
    {
        if (this->cmd_received)
        {
            /* Process the command in the buffer */
            GBoot_ProcessCommand(this);
            
            /* Re-arm Rx is done after processing or response? 
               Usually safe to re-arm immediately if we copy buffer, 
               but here we process in-place then re-arm. */
            
            this->cmd_received = 0;
            
            /* Restart Listen */
             HAL_UARTEx_ReceiveToIdle_DMA(this->huart, this->rx_buffer, GBOOT_RX_BUF_SIZE);
        }
        
        /* Optional: Tickled Watchdog or Blink LED */
    }
}

static void GBoot_OnRxEvent(GBoot_t* this, uint16_t Size)
{
    /* This method is called from the HAL Interrupt Callback */
    this->rx_len = Size;
    this->cmd_received = 1;
    
    /* Note: DMA is automatically stopped by HAL_UARTEx_ReceiveToIdle_DMA upon IDLE event */
}

static void GBoot_ProcessCommand(GBoot_t* this)
{
    if (this->rx_len < 1) return;
    
    uint8_t cmd = this->rx_buffer[0];
    
    switch (cmd)
    {
        case CMD_GET_INFO: // 0x50
        {
            GBoot_SendAck(this);
            // Brief delay or wait for TC? Better to just queue next.
            // Simplified: Just Send data.
            uint8_t version[] = {0x01, 0x02}; // v1.2
            GBoot_SendResponse(this, version, 2);
            break;
        }
        case CMD_ERASE_APP:
        {
            HAL_FLASH_Unlock();
            
            // Fixed for F103 medium density, erasing from App Start to End
            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t PageError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
            EraseInitStruct.PageAddress = APP_START_ADDRESS;
            EraseInitStruct.NbPages = 48; // Adjust based on device
            
            if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK)
            {
                GBoot_SendAck(this);
            }
            else
            {
                GBoot_SendNack(this);
            }
            
            HAL_FLASH_Lock();
            break;
        }
        case CMD_WRITE_PAGE:
        {
            // F103: [CMD][Addr(4)][Data(1024)] = 1029 bytes
            if (this->rx_len < (FLASH_PAGE_SIZE + 5)) 
            {
                GBoot_SendNack(this);
                return;
            }
            
            uint32_t addr = 0;
            addr |= this->rx_buffer[1] << 0;
            addr |= this->rx_buffer[2] << 8;
            addr |= this->rx_buffer[3] << 16;
            addr |= this->rx_buffer[4] << 24;
            
            if (addr < APP_START_ADDRESS)
            {
                GBoot_SendNack(this);
                return;
            }
            
            HAL_FLASH_Unlock();
            uint32_t i;
            uint8_t* pData = &this->rx_buffer[5];
            uint8_t status = 0; // OK
            
            for (i = 0; i < FLASH_PAGE_SIZE; i += 4)
            {
                uint32_t val = 0;
                val |= pData[i+0] << 0;
                val |= pData[i+1] << 8;
                val |= pData[i+2] << 16;
                val |= pData[i+3] << 24;
                
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, val) != HAL_OK)
                {
                    status = 1; // Error
                    break;
                }
            }
            
            HAL_FLASH_Lock();
            
            if (status == 0) GBoot_SendAck(this);
            else GBoot_SendNack(this);
            
            break;
        }
        case CMD_RESET_DEV:
        {
            GBoot_SendAck(this);
            NVIC_SystemReset();
            break;
        }
        case CMD_JUMP_APP:
        {
            GBoot_SendAck(this);
            GBoot_JumpToApp(this);
            break;
        }
        default:
            GBoot_SendNack(this);
            break;
    }
}

static void GBoot_SendResponse(GBoot_t* this, uint8_t* data, uint16_t len)
{
    while(this->huart->gState != HAL_UART_STATE_READY); // Simple blocking wait if busy
    if (len > GBOOT_TX_BUF_SIZE) len = GBOOT_TX_BUF_SIZE;
    
    memcpy(this->tx_buffer, data, len);
    HAL_UART_Transmit_DMA(this->huart, this->tx_buffer, len);
}

static void GBoot_SendAck(GBoot_t* this)
{
    uint8_t ack = GBOOT_ACK;
    GBoot_SendResponse(this, &ack, 1);
}

static void GBoot_SendNack(GBoot_t* this)
{
    uint8_t nack = GBOOT_NACK;
    GBoot_SendResponse(this, &nack, 1);
}

static void GBoot_JumpToApp(GBoot_t* this)
{
    typedef void (*pFunction)(void);
    pFunction JumpToApplication;
    uint32_t JumpAddress;
    
    /* Check Stack Pointer */
    if (((*(__IO uint32_t*)APP_START_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
    {
        JumpAddress = *(__IO uint32_t*) (APP_START_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;
        
        /* De-init */
        HAL_UART_DeInit(this->huart);
        HAL_DMA_DeInit(this->huart->hdmarx);
        HAL_DMA_DeInit(this->huart->hdmatx);
        HAL_RCC_DeInit();
        HAL_DeInit();
        
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;
        
        __set_MSP(*(__IO uint32_t*) APP_START_ADDRESS);
        JumpToApplication();
    }
}

/* --- HAL Callbacks --- */
/* This function overrides the weak symbol if it exists, or we call it from main/it */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        if (gboot.OnRxEvent)
        {
            gboot.OnRxEvent(&gboot, Size);
        }
    }
}
