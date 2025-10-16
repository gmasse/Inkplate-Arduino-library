/*
   Inkplate10_Peripheral_Mode sketch for Soldered Inkplate 10.
   For this example you will need only a USB-C cable and Inkplate 10
   Select "Soldered Inkplate10" from Tools -> Board menu.
   Don't have "Soldered Inkplate10" option? Follow our tutorial and add it:
   https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

   Using this sketch, you don't have to program and control e-paper using Arduino code.
   Instead, you can send UART command. This give you flexibility that you can use this Inkplate 5 on any platform!

   Because it uses UART, it's little bit slower and it's not recommended to send bunch of
   drawPixel command to draw some image. Instead, load bitmaps and pictures on SD card and load image from SD.
   If we missed some function, you can modify this and make yor own.
   Also, every Inkplate comes with this peripheral mode right from the factory.

   Learn more about Peripheral Mode:
   https://inkplate.readthedocs.io/en/latest/peripheral-mode.html

   UART settings are: 115200 baud, standard parity, ending with "\n\r" (Both NL & CR)
   You can send commands via USB port or by directly connecting to ESP32 TX and RX pins.

   Want to learn more about Inkplate? Visit https://soldered.com/documentation/inkplate/
   15 April 2024 by Soldered
*/
// Include Inkplate library.
#include "Inkplate.h"

// Include peripheral mode library.
#include "InkplatePeripheralMode.h"

// Include the header files with sketch settngs (buffer size, serial timeput, argument termination char etc).
#include "settings.h"

// Pointer to the singleton class instance.
PeripheralMode *peripheral;

// Inkplate object.
Inkplate display(INKPLATE_3BIT);

void setup()
{
    // Init Inkplate library.
    display.begin();

    // Create one instance of the object.
    peripheral = PeripheralMode::getInstance();

    // Set the baud rate (in this case 115200 bauds), set the serial buffer and it's size.
    if (!peripheral->begin(&Serial, &display, 115200ULL, SERIAL_UART_RX_PIN, SERIAL_UART_TX_PIN, SERIAL_BUFFER_SIZE))
    {
        // Send an error message on serial.
        Serial.println("Peripheral mode init failed!\nProgram halted!");

        // Stop the program from executing!
        while(1);
    }
    Serial.println("READY");
}

void loop()
{
    // Check if there is something in the serial.
    peripheral->getDataFromSerial(SERIAL_TIMEOUT_MS);
}