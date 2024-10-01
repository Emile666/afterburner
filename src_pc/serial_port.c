#include <stdio.h>
#include "serial_port.h"

char guessedSerialDevice[512] = {'\0'};
#ifdef NO_CLOSE
SerialDeviceHandle serH = INVALID_HANDLE;
#endif
SerialDeviceHandle serialF = INVALID_HANDLE;

char* deviceName = NULL;
bool  bigRam     = false; // 'BIG-RAM found

extern bool verbose;
extern bool varVppExists;
extern bool printSerialWhileWaiting;

void serialDeviceGuessName(char** deviceName) {
#ifdef _USE_WIN_API_
// ideas: https://stackoverflow.com/questions/1388871/how-do-i-get-a-list-of-available-serial-ports-in-win32
    char buf[64 * 1024] = {0};
    int size = QueryDosDevice(NULL, buf, 64 * 1024);
    int topComNum = 0;

    // buffer was filled in
    if (size > 0) {
        char* text = buf;
        int pos = 0;
        int start = 0;

        // search the received buffer for COM string
        while (pos < size) {
            int nameLen;
            start = pos;
            // find the string terminator
            while(pos < size && buf[pos] != 0) {
                pos++;
            }
            nameLen = pos - start;
            // COM port found
            if (nameLen >= 4 && nameLen <=6 && 0 == strncmp(text, "COM", 3) && text[3] >= '0' && text[3] <= '9') {
                int comNum = atoi(text + 3);
                if (comNum > topComNum) {
                    topComNum = comNum;
                    strcpy(guessedSerialDevice, text);
                }
            }
            pos++;
            text = &buf[pos];
        } // while
        
        // if we have found a COM port then pass it back as the result
        if (topComNum > 0) {
            *deviceName = guessedSerialDevice;
        } // if
    } // if
#else
// ideas: https://unix.stackexchange.com/questions/647235/debian-how-to-identify-usb-devices-with-similar-dev-tty-file
//        https://pubs.opengroup.org/onlinepubs/009696799/functions/popen.html
    int i;
    char text[512];
    int topTtyNum = 0;

    FILE* f = popen(LIST_DEVICES, "r");
    if (f == NULL) {
        return;
    } // if
    i = 1;
    while (fgets(text, 512, f) != NULL) {
        // filer out non USB devices and prioritise those with Arduino name
        if (CHECK_SERIAL()) {
            int ttyNum  = i;
            int textLen = strlen(text);
            // prefer Arduino over generic USB serial ports (does not work on OSX)
            if (textLen > 29 && 0 == strncmp(text + 18, "usb-Arduino", 11)) {
                ttyNum += 1000;
            }
            if (ttyNum > topTtyNum) {
                topTtyNum = ttyNum;
                strcpy(guessedSerialDevice, text);
                //strip the new-line trailing markers
                if (guessedSerialDevice[textLen - 1] == '\n' || guessedSerialDevice[textLen - 1] == '\r') {
                    guessedSerialDevice[textLen - 1] = '\0';
                } // if
            } // if
        } // if
        i++;
    } // while
    pclose(f);

    if (topTtyNum > 0) {
        *deviceName = guessedSerialDevice;
    } // if
#endif
} // serialDeviceGuessName()

// https://www.xanthium.in/Serial-Port-Programming-using-Win32-API
SerialDeviceHandle serialDeviceOpen(char* deviceName) {
    SerialDeviceHandle h;
#ifdef NO_CLOSE
    if (serH != INVALID_HANDLE) {
        return serH;
    }
#endif
#ifdef _USE_WIN_API_
    h = CreateFile(
        deviceName,                  //port name
        GENERIC_READ | GENERIC_WRITE, //Read/Write
        0,                            // No Sharing
        NULL,                         // No Security
        OPEN_EXISTING,// Open existing port only
        0,            // Non Overlapped I/O
        NULL);        // Null for Comm Devices

    if (h != INVALID_HANDLE) {
        BOOL result;
        COMMTIMEOUTS timeouts = { 0 };

        DCB dcbSerialParams = { 0 }; // Initializing DCB structure
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    
        result = GetCommState(h, &dcbSerialParams);

        if (!result) {
            return INVALID_HANDLE;
        }

        dcbSerialParams.BaudRate = CBR_57600; // Setting BaudRate
        dcbSerialParams.ByteSize = 8;         // Setting ByteSize = 8
        dcbSerialParams.StopBits = ONESTOPBIT;// Setting StopBits = 1
        dcbSerialParams.Parity   = NOPARITY;  // Setting Parity = None
        dcbSerialParams.fOutxCtsFlow = FALSE;
        dcbSerialParams.fOutxDsrFlow = FALSE;
        dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE;
        dcbSerialParams.fOutX = FALSE;
        dcbSerialParams.fInX = FALSE;
        dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
        dcbSerialParams.fBinary = TRUE;
        
        result = SetCommState(h, &dcbSerialParams);
        if (!result) {
            return INVALID_HANDLE;
        }

        timeouts.ReadIntervalTimeout         = 30; // in milliseconds
        timeouts.ReadTotalTimeoutConstant    = 30; // in milliseconds
        timeouts.ReadTotalTimeoutMultiplier  = 10; // in milliseconds
        timeouts.WriteTotalTimeoutConstant   = 50; // in milliseconds
        timeouts.WriteTotalTimeoutMultiplier = 10; // in milliseconds

        result = SetCommTimeouts(h, &timeouts);
        if (!result) {
            return INVALID_HANDLE;
        }
        //ensure no leftover bytes exist on the serial line
        result = PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
#else
    h = open(deviceName, O_RDWR | O_NOCTTY  | O_NONBLOCK);

    if (h != INVALID_HANDLE) {
        //set the serial port parameters
        struct termios serial;

        memset(&serial, 0, sizeof(struct termios));
        cfsetispeed(&serial, B57600);
        cfsetospeed(&serial, B57600);

        serial.c_cflag |= CS8; // no parity, 1 stop bit
        serial.c_cflag |= CREAD | CLOCAL;

        if (0 != tcsetattr(h, TCSANOW, &serial)) {
            close(h);
            printf("Error: failed to set serial parameters %i, %s\n", errno, strerror(errno));
            return INVALID_HANDLE;
        }

        //ensure no leftover bytes exist on the serial line
        tcdrain(h);
        tcflush(h, TCIOFLUSH); //flush both queues 
#endif
#ifdef NO_CLOSE
        serH = h;
#endif
        return h;
    } else {
        return INVALID_HANDLE;
    }
} // serialDeviceOpen()

void serialDeviceCheckName(char* name, int maxSize) {
#ifdef _USE_WIN_API_
    int nameLen = strlen(name);
    //convert comxx to COMxx
    if (strncmp(name, "com", 3) == 0 && (nameLen > 3 && name[3] >= '0' && name[3] <= '9')) {
        name[0] = 'C';
        name[1] = 'O';
        name[2] = 'M';
    }
    // convert COMxx and higher to \\\\.\\COMxx
    if (strncmp(name, "COM", 3) == 0 && (nameLen > 4 && name[3] >= '0' && name[3] <= '9')) {
        if (nameLen + 4 < maxSize) {
            int i;
            for (i = nameLen - 1; i >= 0; i--) {
                name[ i + 4] = name[i]; 
            }
            name[0] = '\\';
            name[1] = '\\';
            name[2] = '.';
            name[3] = '\\';
        }
    }
#endif
} // serialDeviceCheckName()

void serialDeviceClose(SerialDeviceHandle deviceHandle) {
#ifndef NO_CLOSE
    CloseHandle(deviceHandle);
#endif
} // serialDeviceClose()

int serialDeviceWrite(SerialDeviceHandle deviceHandle, char* buffer, int bytesToWrite) {
#ifdef _USE_WIN_API_
    DWORD written = 0;
    WriteFile(deviceHandle, buffer, bytesToWrite, &written, NULL);
    return (int) written;
#else
	return write(deviceHandle, buffer, bytesToWrite); 
#endif
} // serialDeviceWrite()

int serialDeviceRead(SerialDeviceHandle deviceHandle, char* buffer, int bytesToRead) {
#ifdef _USE_WIN_API_
    DWORD read = 0;
    ReadFile(deviceHandle, buffer, bytesToRead, &read, NULL);
    return (int) read;
#else
	return read(deviceHandle, buffer, bytesToRead); 
#endif
} // serialDeviceRead()

bool checkForString(char* buf, int16_t start, const char* key) {
    int16_t labelPos = strstr(buf + start, key) -  buf;
    return (labelPos > 0 && labelPos < 500);
} // checkForString()

bool openSerial(void) {
    char     buf[512] = {'\0'};
    char     devName[256] = {'\0'};
    int16_t  total;
    int16_t  labelPos;

    //open device name
    if (deviceName == NULL) {
        serialDeviceGuessName(&deviceName);
    }
    snprintf(devName, sizeof(devName), "%s", (deviceName == NULL) ? DEFAULT_SERIAL_DEVICE_NAME : deviceName);
    serialDeviceCheckName(devName, sizeof(devName));

    if (verbose) {
        printf("opening serial: %s\n", devName);
    } // if

    serialF = serialDeviceOpen(devName);
    if (serialF == INVALID_HANDLE) {
        printf("Error: failed to open serial device: %s\n", devName);
        return RETV_ERROR;
    } // if

    // prod the programmer to output it's identification
    sprintf(buf, "*\r");
    serialDeviceWrite(serialF, buf, 2);

    //read programmer's message
    total = waitForSerialPrompt(buf, 512, 3000);
    buf[total] = 0;

    //check we are communicating with Afterburner programmer
    labelPos = strstr(buf, "AFTerburner v.") -  buf;

    bigRam = false;
    if ((labelPos >= 0) && (labelPos < 500) && (buf[total - 3] == '>')) {
        // check for new board desgin: variable VPP
        varVppExists = checkForString(buf, labelPos, " varVpp ");
        if (verbose && varVppExists) {
            printf("variable VPP board detected\n");
        }
        // check for Big Ram
        bigRam = checkForString(buf, labelPos, " RAM-BIG");
        if (verbose && bigRam) {
            printf("MCU Big RAM detected\n");
        } // if
        return RETV_OK; // all OK
    } // if
    if (verbose) {
        printf("Output from programmer not recognised: %s\n", buf);
    } // if
    serialDeviceClose(serialF);
    serialF = INVALID_HANDLE;
    return RETV_ERROR;
} // openSerial()

void closeSerial(void) {
    if (serialF == INVALID_HANDLE) {
        return;
    } // if
    serialDeviceClose(serialF);
    serialF = INVALID_HANDLE;
} // closeSerial()

int16_t checkPromptExists(char* buf, int16_t bufSize) {
    int16_t i;
    for (i = 0; (i < bufSize - 2) && (buf[i] != '\0'); i++) {
        if ((buf[i] == '>') && (buf[i+1] == '\r') && (buf[i+2] == '\n')) {
            return i;
        } // if
    } // for
    return -1;
} // int16_t

int16_t waitForSerialPrompt(char* buf, int16_t bufSize, int16_t maxDelay) {
    char*   bufStart = buf;
    int16_t bufTotal = bufSize;
    int16_t bufPos = 0;
    int16_t readSize;
    char*   bufPrint = buf;
    bool    doPrint = printSerialWhileWaiting;
    
    memset(buf, 0, bufSize);
    while (maxDelay > 0) {
        readSize = serialDeviceRead(serialF, buf, bufSize);
        if (readSize > 0) {
            bufPos += readSize;
            if (checkPromptExists(bufStart, bufTotal) >= 0) {
                maxDelay = 4; //force exit, but read the rest of the line
            } else {
                buf += readSize;
                bufSize -= readSize;
                if (bufSize <= 0) {
                    printf("ERROR: serial port read buffer is too small!\nAre you dumping a large amount of data?\n");
                    return -1;
                } // if
            } // else
            if (printSerialWhileWaiting) {
                bufPrint = printBuffer(bufPrint, readSize);
            } // if
        } // if
        if (maxDelay > 0) {
        /* WIN_API handles timeout itself */
#ifndef _USE_WIN_API_
            usleep(10 * 1000);
            maxDelay -= 10;
#else
            maxDelay -= 30;           
#endif
            if ((maxDelay <= 0) && verbose) {
                printf("waitForSerialPrompt timed out\n");
            } // if
        } // if
    } // while
    return bufPos;
} // WaitForSerialPrompt()

char* printBuffer(char* bufPrint, int16_t readSize) {
    int16_t  i;
    bool doPrint = true;
    for (i = 0; i < readSize; i++) {
        if (*bufPrint == '>') {
            doPrint = false;
        } // if
        if (doPrint) {
            printf("%c", *bufPrint);
            if (*bufPrint == '\n' || *bufPrint == '\r') {
                fflush(stdout);
            } // if
            bufPrint++;
        } // if
    } // for i
    return bufPrint;
} // printBuffer()

bool sendBuffer(char* buf) {
    int16_t total;
    int16_t writeSize;

    if (buf == NULL) {
        return RETV_ERROR;
    }
    total = strlen(buf);
    // write the query into the serial port's file
    // file is opened non blocking so we have to ensure all contents is written
    while (total > 0) {
        writeSize = serialDeviceWrite(serialF, buf, total);
        if (writeSize < 0) {
            printf("ERROR: written: %i (%s)\n", writeSize, strerror(errno));
            return RETV_ERROR;
        }
        buf += writeSize;
        total -= writeSize;
    }
    return RETV_OK;
} // sendBuffer()

bool sendLine(char* buf, int16_t bufSize, int16_t maxDelay) {
    int16_t total;
    char* obuf = buf;

    if (serialF == INVALID_HANDLE) {
        return RETV_ERROR;
    }
    if (sendBuffer(buf) != RETV_OK) {
        return RETV_ERROR;
    }

    total = waitForSerialPrompt(obuf, bufSize, (maxDelay < 0) ? 6 : maxDelay);
    if (total < 0) {
        return RETV_ERROR;
    }
    obuf[total] = 0;
    obuf        = stripPrompt(obuf);
    if (verbose) {
        printf("read: %i '%s'\n", total, obuf);
    } // if
    return total;
} // sendLine()

char* stripPrompt(char* buf) {
    int16_t len, i;
    if (buf == NULL) {
        return '\0';
    } // if
    len = strlen(buf);
    i   = checkPromptExists(buf, len);
    if (i >= 0) {
        buf[i] = '\0';
        len    = i;
    } // if

    //strip rear new line characters
    for (i = len - 1; i >= 0; i--) {
        if ((buf[i] != '\r') && (buf[i] != '\n')) {
            break;
        } else {
            buf[i] = '\0';
        } // else
    } // for i

    //strip frontal new line characters
    for (i = 0; buf[i] != '\0'; i++) {
        if (buf[0] == '\r' || buf[0] == '\n') {
            buf++;
        } // if
    } // for i
    return buf;
} // stripPrompt()