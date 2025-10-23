/*
   Inkplate2_Peripheral_Mode sketch for Soldered Inkplate 2 (IP2).
   For this example you will need only a USB-C cable and Inkplate 2.
   Select "Soldered Inkplate 2" from Tools -> Board menu.
   Don’t have "Soldered Inkplate 2" option? Follow our tutorial and add it:
   https://soldered.com/learn/add-inkplate-2-board-definition-to-arduino-ide/

   Using this sketch, you don’t have to program and control the e-paper display using Arduino code.
   Instead, you can send UART commands. This gives you the flexibility to use Inkplate 2 on any platform!

   Because it uses UART, it’s a bit slower and not recommended to send a large number of
   drawPixel commands to render images. Instead, store bitmaps or pictures on an SD card
   and load them directly from there.
   If any functionality is missing, you can modify this code and make your own version.
   Every Inkplate 2 comes with this Peripheral Mode preloaded from the factory.

   Learn more about Peripheral Mode:
   https://inkplate.readthedocs.io/en/latest/peripheral-mode.html

   UART settings are: 115200 baud, standard parity, ending with "\n\r" (Both NL & CR)
   You can send commands via the USB port or by directly connecting to the ESP32 TX and RX pins.

   Want to learn more about Inkplate? Visit:
   https://soldered.com/documentation/inkplate/

   23 October 2025 by Soldered
*/

// Include Inkplate library
#include "Inkplate.h"

// Include Peripheral Mode library
#include "InkplatePeripheralMode.h"

// Include the header file with sketch settings (buffer size, serial timeout, argument termination char, etc.)
#include "settings.h"

// Pointer to the singleton Peripheral Mode instance
PeripheralMode *peripheral;

// Create Inkplate 2 display object
Inkplate display;

void setup()
{
    // Initialize Inkplate library
    display.begin();

    // Create instance of Peripheral Mode object
    peripheral = PeripheralMode::getInstance();

    // Initialize Peripheral Mode (UART, display, buffer, etc.)
    if (!peripheral->begin(&Serial, &display, 115200ULL, SERIAL_UART_RX_PIN, SERIAL_UART_TX_PIN, SERIAL_BUFFER_SIZE))
    {
        // Send an error message to serial
        Serial.println("Peripheral Mode init failed!\nProgram halted!");

        // Stop program execution
        while (1);
    }

    Serial.println("READY");
}

void loop()
{
    // Check if there is incoming data on serial and process commands
    peripheral->getDataFromSerial(SERIAL_TIMEOUT_MS);
}
