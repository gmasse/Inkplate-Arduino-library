// Add header guards.
#ifndef __INKPLATEPERIPHERALMODE_H__
#define __INKPLATEPERIPHERALMODE_H__

// Add an main Arduino header file.
#include <Arduino.h>

// Add an Inkplate library.
#include <Inkplate.h>

// Include file describing the settings of the peripheral mode.
#include "settings.h"

//#include <HTTPClient.h>

// Make a class for the peripheral mode.
class PeripheralMode
{   
  private:
    // Use singleton class to not being able to use multiple instances of this class (since only makes sense to make only one Peripheral mode).
    // Private constructor to prevent direct instantiation.
    PeripheralMode() {}

    // Static pointer to the single instance.
    static PeripheralMode* _instancePtr;

    // Pointer to the Arduino Serial object,
    HardwareSerial *_serial = nullptr;

    // Pointer to the Inkplate library object,
    Inkplate *_display = nullptr;

    // Variable for counting incoming serial data.
    uint32_t _serialBufferIndex = 0;

    // Pointer to the allocated serial buffer in the PSRAM.
    char *_serialBuffer = nullptr;

    // Size of the buffer for parsing the commands.
    uint32_t _bufferSize = 0;

    // Commonly used responeses
    const char *_okResponseString = "OK";
    const char *_failResponseString = "FAIL";

    // Variable for timestamping last received data for the automatic buffer clear.
    unsigned long _bufferAutoCleanTimestamp = 0;

    bool checkPacket(char *_buffer, uint32_t _maxSize, int *_command, int *_repeatableFlag, int *_payloadSize, char **_payloadDataStart, char **_commandEndPosition);
    void sendResponse(int _command, int _payloadSize, char *_payload);
    char* getArgument(char *_inBuffer, uint32_t _payloadSize, int _noOfArgs, int _n, uint32_t *_size);
    int getNumberOfArgs(char *_inBuffer, uint32_t _payloadSize);
    void parseCommand(int _command, int _repeat, int _payloadSize, char* _payload);
    int hexToChar(int a, int b);
    int charToInt(char a);
    void checkArguments(int *_noOfArgs, int _maxArg, int _repeat);
    void hexAsciiToAscii(char *_buffer, uint32_t _size);

  public:
    static PeripheralMode* getInstance()
    {
        // Check if the instance is already been made. If not, make it.
        if (PeripheralMode::_instancePtr == nullptr)
        {
            // Make a new instance and save the address to the pointer.
            PeripheralMode::_instancePtr = new PeripheralMode();
        }

        // Return the instance pointer. This way only one object is created.
        return PeripheralMode::_instancePtr;
    }

    bool begin(HardwareSerial *_serial, Inkplate *_inkplate, uint32_t _baud = 115200ULL, uint8_t _rxPin = -1, uint8_t _txPin = -1, uint32_t _size = SERIAL_BUFFER_SIZE);
    bool getDataFromSerial(unsigned long _timeout);
};

#endif