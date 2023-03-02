
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "adc.h"
#include "gpio.h"
#include "icache.h"
#include "iwdg.h"
#include "rtc.h"
#include "ucpd.h"
#include "usart.h"
#include "usb_otg.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "bm_l2.h"
#include "bristlemouth.h"
#include "bsp.h"
#include "cli.h"
#include "debug_bm.h"
#include "debug_memfault.h"
#include "debug_sys.h"
#include "gpioISR.h"
#include "memfault_platform_core.h"
#include "pcap.h"
#include "printf.h"
#include "serial.h"
#include "serial_console.h"
#include "usb.h"
#include "watchdog.h"
#include "debug_middleware.h"


#include <stdio.h>

static void defaultTask(void *parameters);

// Serial console (when no usb present)
SerialHandle_t usart1 = {
  .device = USART1,
  .name = "usart1",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 1024,
  .rxBufferSize = 512,
  .rxBytesFromISR = serialGenericRxBytesFromISR,
  .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};

// Serial console USB device
SerialHandle_t usbCLI   = {
  .device = (void *)0, // Using CDC 0
  .name = "vcp-cli",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 1024,
  .rxBufferSize = 512,
  .rxBytesFromISR = NULL,
  .getTxBytesFromISR = NULL,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};


SerialHandle_t usbPcap   = {
  .device = (void *)1, // Using CDC 1
  .name = "vcp-bm",
  .txPin = NULL,
  .rxPin = NULL,
  .txStreamBuffer = NULL,
  .rxStreamBuffer = NULL,
  .txBufferSize = 2048,
  .rxBufferSize = 64,
  .rxBytesFromISR = NULL,
  .getTxBytesFromISR = NULL,
  .processByte = NULL,
  .data = NULL,
  .enabled = false,
  .flags = 0,
};

extern "C" void USART1_IRQHandler(void) {
    serialGenericUartIRQHandler(&usart1);
}

extern "C" int main(void) {

    // Before doing anything, check if we should enter ROM bootloader
    // enterBootloaderIfNeeded();

    HAL_Init();

    SystemClock_Config();

    SystemPower_Config_ext();
    MX_GPIO_Init();
    MX_UCPD1_Init();
    MX_USART1_UART_Init();
    MX_USB_OTG_FS_PCD_Init();
    MX_ICACHE_Init();
    MX_RTC_Init();
    MX_IWDG_Init();

    usbMspInit();

    // rtcInit();

    // Enable hardfault on divide-by-zero
    SCB->CCR |= 0x10;

    // Initialize low power manager
    lpmInit();

    // Inhibit low power mode during boot process
    lpmPeripheralActive(LPM_BOOT);

    BaseType_t rval = xTaskCreate(defaultTask,
                                  "Default",
                                  // TODO - verify stack size
                                  128 * 4,
                                  NULL,
                                  // Start with very high priority during boot then downgrade
                                  // once done initializing everything
                                  2,
                                  NULL);
    configASSERT(rval == pdTRUE);

    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    /* We should never get here as control is now taken by the scheduler */

    while (1){};
}

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;

    printf("Button press! %d\n", value);
    return false;
}

static void defaultTask( void *parameters ) {
    (void)parameters;
    SerialHandle_t *dfuSerial = NULL;

    startIWDGTask();
    startSerial();
    // Use USB for serial console if USB is connected on boot
    // Otherwise use ST-Link serial port
    if(usb_is_connected()) {
      startSerialConsole(&usbCLI);
      // Serial device will be enabled automatically when console connects
      // so no explicit serialEnable is required

#if BM_DFU_HOST
      dfuSerial = &usart1;
      serialEnable(&usart1);
#endif

    } else {
      startSerialConsole(&usart1);

#if BM_DFU_HOST
      printf("WARNING: USB must be connected to use DFU mode. This serial port will be used by the serial console instead.\n");
#endif

      printf("WARNING: PCAP support requires USB connection.\n");
    }
    startCLI();
    pcapInit(&usbPcap);
    serialEnable(&usart1);
    gpioISRStartTask();

    memfault_platform_boot();
    memfault_platform_start();

    bspInit();

    usbInit();

    debugSysInit();
    debugMemfaultInit(&usart1);
    debugBMInit();
    debugMiddlewareInit();

    // Commenting out while we test usart1
    // lpmPeripheralInactive(LPM_BOOT);

    gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

    bcl_init(dfuSerial);

    while(1) {
        /* Do nothing */
        vTaskDelay(1000);
    }
}
