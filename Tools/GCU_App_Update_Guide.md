# How to Update GCU Application for gboot

Changes required for the User Application (GCU Firmware) to be compatible with `gboot`.

## 1. Update Linker Script (.ld)
Locate your linker script (e.g., `STM32F103X_FLASH.ld`).
Change the `FLASH` memory definition to start at **0x08004000**.

```ld
/* Before */
/* MEMORY {
    FLASH (rx) : ORIGIN = 0x8000000, LENGTH = 64K
    RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 20K
} */

/* After (Adjust LENGTH as needed: 64K - 16K = 48K) */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08004000, LENGTH = 48K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 20K
}
```

## 2. Relocate Vector Table (VTOR)
In your `main.c` (at the very beginning) or `system_stm32f1xx.c`, set the Vector Table Offset.

```c
/* In main() begin */
int main(void)
{
  SCB->VTOR = 0x08004000; // Offset Vector Table for App
  HAL_Init();
  ...
```

## 3. Implement "Enter Bootloader" Command
Add a function to trigger the bootloader mode from your application code (e.g., upon receiving a specific CAN/UART command).

```c
void Enter_Bootloader_Mode(void)
{
    /* 1. Enable Backup Access */
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_BKP_CLK_ENABLE();
    
    /* 2. Write Magic Key */
    // Access Backup Register 1 directly
    BKP->DR1 = 0x5AA5DEAD;
    
    /* 3. Reset System */
    NVIC_SystemReset();
}
```

## 4. Build Output
Ensure you generate a **Binary (.bin)** file to use with the flasher tool.
- In STM32CubeIDE: Project Properties -> C/C++ Build -> Settings -> MCU Post build outputs -> [x] Convert to binary file.
