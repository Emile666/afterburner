#ifndef _SERIAL_PORT_H_
#define _SERIAL_PORT_H_
#include <stdint.h>
#include <stdbool.h>

// Return-values for functions
#define RETV_ERROR (true)
#define RETV_OK    (false)

#ifdef _USE_WIN_API_

#include <windows.h>

#define SerialDeviceHandle         HANDLE
#define DEFAULT_SERIAL_DEVICE_NAME ("COM1")
#define INVALID_HANDLE             (INVALID_HANDLE_VALUE)

#else

#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include <termios.h>

#define SerialDeviceHandle int
#define DEFAULT_SERIAL_DEVICE_NAME "/dev/ttyUSB0"

#define INVALID_HANDLE -1

#ifdef _OSX_
    #define CHECK_SERIAL() (text != NULL)
    #define LIST_DEVICES  "ls -1 /dev/tty.usb* /dev/tty.wchusb* 2>/dev/null"
#else    
    #define CHECK_SERIAL() (text != NULL && 0 == strncmp(text + 18, "usb-", 4))
    #define LIST_DEVICES "ls -1 /dev/serial/by-id/* 2>/dev/null"
#endif /* _OS_X_ */   

#endif /* _USE_WIN_API else */

// Function prototypes
SerialDeviceHandle serialDeviceOpen(char* deviceName);
void    serialDeviceGuessName(char** deviceName);
void    serialDeviceCheckName(char* name, int maxSize);
void    serialDeviceClose(SerialDeviceHandle deviceHandle);
int32_t serialDeviceWrite(SerialDeviceHandle deviceHandle, char* buffer, int32_t bytesToWrite);
int32_t serialDeviceRead(SerialDeviceHandle deviceHandle, char* buffer, int32_t bytesToRead);

bool    checkForString(char* buf, int16_t start, const char* key);
bool    openSerial(void);
void    closeSerial(void);
int32_t checkPromptExists(char* buf, int32_t bufSize);
int32_t waitForSerialPrompt(char* buf, int32_t bufSize, int32_t maxDelay);
char*   printBuffer(char* bufPrint, int32_t readSize);
bool    sendBuffer(char* buf);
int32_t sendLine(char* buf, int32_t bufSize, int32_t maxDelay);
char*   stripPrompt(char* buf);

#endif /* _SERIAL_PORT_H_ */

