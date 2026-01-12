#ifndef GBOOT_H
#define GBOOT_H

#include "main.h"

#define GBOOT_RX_BUF_SIZE 2100 /* Enough for 1 Flash Page (1KB/2KB) + Overhead */
#define GBOOT_TX_BUF_SIZE 128

/* Forward Declaration */
typedef struct GBoot_t GBoot_t;

/* GBoot Class Definition */
struct GBoot_t {
    /* --- Properties --- */
    UART_HandleTypeDef* huart;
    
    /* Buffers */
    uint8_t rx_buffer[GBOOT_RX_BUF_SIZE];
    uint8_t tx_buffer[GBOOT_TX_BUF_SIZE];
    
    /* State */
    volatile uint8_t  cmd_received;
    volatile uint16_t rx_len;
    volatile uint8_t  is_busy;
    
    /* --- Methods --- */
    
    /**
     * @brief Initialize the Bootloader. Checks entry condition.
     * @param this Pointer to GBoot instance.
     * @param huart Pointer to UART Handle.
     */
    void (*Init)(GBoot_t* this, UART_HandleTypeDef* huart);
    
    /**
     * @brief Main Bootloader Loop. Handles commands.
     * @param this Pointer to GBoot instance.
     */
    void (*Run)(GBoot_t* this);
    
    /**
     * @brief Callback to be called from HAL_UARTEx_RxEventCallback.
     * @param this Pointer to GBoot instance.
     * @param Size Number of bytes received.
     */
    void (*OnRxEvent)(GBoot_t* this, uint16_t Size);
};

/* Public Instance Accessor (Optional, for callbacks to find the instance if singular) */
extern GBoot_t gboot;

/* Constructor-like Init Function to bind methods */
void GBoot_Create(GBoot_t* instance);

#endif // GBOOT_H
