#ifndef __SETTINGS_H__
#define __SETTINGS_H__

// Size of the Serial buffer (in bytes). Change if needed. It's stored in the PSRAM of the ESP32.
#define SERIAL_BUFFER_SIZE 65535

// Timeout for the incomming data. It's is counted from the last received char from te serial.
#define SERIAL_TIMEOUT_MS 100

// Define default ESP32 UART RX and TX pins
#define SERIAL_UART_RX_PIN  3
#define SERIAL_UART_TX_PIN  1

// Termination char for the arguments (see documentation!)
#define TEMR_CHAR ';'

// How long it takes to clear the buffer from unprocessed or invalid commands.
#define SERIAL_BUFFER_CLEAN_TIME_MS     60000ULL

#endif