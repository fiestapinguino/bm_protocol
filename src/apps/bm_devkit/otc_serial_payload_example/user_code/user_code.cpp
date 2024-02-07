#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "debug_pluart_cli.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "sensors.h"
#include "OrderedSeparatorLineParser.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "util.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;

// A buffer for our data from the payload uart
char payload_buffer[2048];
char disable_text[] = "set enable text(no)\r";
char get_sample[] = "do sample\r";

// Eg: 4330 740 269.539 100.322 22.826
ValueType valueTypes[] = {TYPE_UINT64, TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE};
OrderedSeparatorLineParser parser(" ", 256, valueTypes, 5);

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(9600);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  PLUART::enable();
  // Initialize Debug PLUART CLI 
  debugPlUartCliInit();
  // Enable the input to the Vout power supply.
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  bristlefin.enableVout();

  parser.init();
  userConfigurationPartition->getConfig("plUartBaudRate", strlen("plUartBaudRate"));

  // Disable text
  PLUART::write(&disable_text[0], sizeof(disable_text));
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  PLUART::write(&get_sample[0], sizeof(get_sample));

  // Read a line if it is available
  if (PLUART::lineAvailable()) {
    uint16_t read_len = PLUART::readLine(payload_buffer, sizeof(payload_buffer));
    
    // Now when we get a line of text data, our LineParser turns it into numeric values.
    if (parser.parseLine(payload_buffer, read_len)) {
      printf("parsed values: %" PRIu64 " | %" PRIu64 " | %f | %f | %f\n", parser.getValue(0).data, parser.getValue(1).data,
                                                                          parser.getValue(2).data, parser.getValue(3).data,
                                                                          parser.getValue(4).data);
    } else {
      printf("Error parsing line!\n");
    }

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    // Print the payload data to a file, to the bm_printf console, and to the printf console.
    bm_fprintf(0, "payload_data.log", "tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    //bm_printf(0, "[payload] | tick: %llu, rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    printf("[payload] | tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
  }
  vTaskDelay(pdMS_TO_TICKS(30000));
}
