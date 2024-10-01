#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "afterburner.h"
#include "serial_port.h"

// Variables defined in afterburner.c and used in this file
extern SerialDeviceHandle serialF;
extern bool               verbose;
extern char               galbuffer[];
extern bool               opInfo;      /* read device info */
extern bool               opErase;
extern bool               opRead;
extern bool               opVerify;
extern bool               opWrite;
extern char*              filename;
extern _str_galinfo       galinfo[];
extern Galtype            gal;

int16_t readJtagSerialLine(char* buf, int16_t bufSize, int16_t maxDelay, int16_t * feedRequest) {
    char*   bufStart = buf;
    int16_t readSize;
    int16_t bufPos = 0;

    memset(buf, 0, bufSize);

    while (maxDelay > 0) {
        readSize = serialDeviceRead(serialF, buf, 1);
        if (readSize > 0) {
            bufPos += readSize;
            buf[1] = 0;
            //handle the feed request
            if (buf[0] == '$') {
                char tmp[5];
                bufPos -= readSize;
                buf[0] = 0;
                //extra 5 bytes should be present: 3 bytes of size, 2 new line chars
                readSize = serialDeviceRead(serialF, tmp, 3);
                if (readSize == 3) {
                    int16_t retry = 1000;
                    tmp[3] = 0;
                    *feedRequest = atoi(tmp);
                    maxDelay = 0; //force exit

                    //read the extra 2 characters (new line chars)
                    while (retry && readSize != 2) {
                        readSize = serialDeviceRead(serialF, tmp, 2);
                        retry--;
                    }
                    if (readSize != 2 || tmp[0] != '\r' || tmp[1] != '\n') {
                        printf("Warning: corrupted feed request ! %d \n", readSize);
                    }
                } else {
                    printf("Warning: corrupted feed request! %d \n", readSize);
                }
                //printf("***\n");
            } else
            if (buf[0] == '\r') {
                readSize = serialDeviceRead(serialF, buf, 1); // read \n coming from Arduino
                //printf("-%c-\n", buf[0] == '\n' ? 'n' : 'r');
                buf[0] = 0;
                bufPos++;
                maxDelay = 0; //force exit
            } else {
                //printf("(0x%02x %d) \n", buf[0], (int16_t) buf[0]);
                buf += readSize;
                if (bufPos == bufSize) {
                    printf("ERROR: serial port read buffer is too small!\nAre you dumping large amount of data?\n");
                    return -1;
                }
            }
        }
        if (maxDelay > 0) {
        /* WIN_API handles timeout itself */
#ifndef _USE_WIN_API_
            usleep(1 * 1000);
            maxDelay -= 10;
#else
            maxDelay -= 30;
#endif
        }
    }
    return bufPos;
} // readJtagSerialLine()

bool playJtagFile(char* label, int16_t fSize, int16_t vpp, int16_t showProgress) {
    char     buf[MAX_LINE] = {'\0'};
    int16_t  sendPos = 0;
    int16_t  lastSendPos = 0;
    char     ready = 0;
    int16_t  result = 0;
    uint16_t csum = 0;
    int16_t  feedRequest = 0;
    // support for XCOMMENT messages which might be interrupted by a feed request
    int16_t  continuePrinting = 0;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    }
    //compute check sum
    if (verbose) {
        int16_t i;
        for (i = 0; i < fSize; i++) {
            csum += (unsigned char) galbuffer[i];
        } // for 
    } // if

    // send start-JTAG-player command
    sprintf(buf, "j%d\r", vpp ? 1: 0);
    sendBuffer(buf);

    // read response from MCU and feed the XSVF player with data
    while(1) {
        int16_t readBytes;

        feedRequest = 0;
        buf[0] = 0;
        readBytes = readJtagSerialLine(buf, MAX_LINE, 3000, &feedRequest);
        //printf(">> read %d  len=%d cp=%d '%s'\n", readBytes, (int16_t) strlen(buf), continuePrinting,  buf);

        //request to send more data was received
        if (feedRequest > 0) {
            if (ready) {
                int16_t chunkSize = fSize - sendPos;
                if (chunkSize > feedRequest) {
                    chunkSize = feedRequest;
                    // make the initial chunk big so the data are buffered by the OS
                    if (sendPos == 0) {
                        chunkSize *= 2;
                        if (chunkSize > fSize) {
                            chunkSize = fSize;
                        } // if
                    } // if
                } // if
                if (chunkSize > 0) {
                    // send the data over serial line
                    int16_t w = serialDeviceWrite(serialF, galbuffer + sendPos, chunkSize);
                    sendPos += w;
                    // print progress / file position
                    if (showProgress && (sendPos - lastSendPos >= 1024 || sendPos == fSize)) {
                        lastSendPos = sendPos;
                        updateProgressBar(label, sendPos, fSize);
                    } // if
               } // if
            } // if
            if (readBytes > 2) {
                continuePrinting = 1;
            } // if
        } // if
        // when the feed request was detected, there might be still some data in the buffer
        if (buf[0] != 0) {
            //prevous line had a feed request - this is a continuation
            if (feedRequest == 0 && continuePrinting) {
                continuePrinting = 0;
                printf("%s\n", buf);
            } else
            //print debug messages
            if (buf[0] == 'D') {
                if (feedRequest) { // the rest of the message will follow
                    printf("%s", buf + 1);
                } else {
                    printf("%s\n", buf + 1);
               } // else 
            } // if
            // quit
            if (buf[0] == 'Q') {
                result = atoi(buf + 1);
                //print error result
                if (result != 0) {
                    printf("%s\n", buf + 1);
                } else
                // when all is OK and verbose mode is on, then print the checksum for comparison
                if (verbose) {
                    printf("PC : 0x%08X\n", csum);
                }
                break;
            } else
            // ready to receive announcement
            if (strcmp("RXSVF", buf) == 0) {
                ready = 1;
            } else
            // print important messages
            if (buf[0] == '!') {
                // in verbose mode print all messages, otherwise print only success or fail messages
                if (verbose || !strcmp("!Success", buf) || !strcmp("!Fail", buf)) {
                    printf("%s\n", buf + 1);
                } // if
            } // if
#if 0
             //print all the rest
             else if (verbose) {
                printf("'%s'\n", buf);
            } // else if
#endif
        } else
        // the buffer is empty but there was a feed request just before - print a new line
        if (readBytes > 0 && continuePrinting) {
            printf("\n");
            continuePrinting = 0;
        } // if
    } // while
    readJtagSerialLine(buf, MAX_LINE, 1000, &feedRequest);
    closeSerial();
	if (!result)
		 return RETV_ERROR;
	else return RETV_OK;
} // playJtagFile()

bool processJtagInfo(void) {
    bool    result;
    int16_t fSize = 0;
    char    tmp[256];

    if (!opInfo) {
        return RETV_OK;
    } // if

    if (!(gal == ATF1502AS || gal == ATF1504AS)) {
        printf("error: info command is unsupported");
        return RETV_ERROR;
    } // if

    // Use default .xsvf file for erase if no file is provided.
    // if the file is provided while write operation is also requested
    // then the file is specified for writing -> do not use it for erasing
    sprintf(tmp, "xsvf/id_ATF150X.xsvf");
    filename = tmp;

    if (readFile(&fSize) != RETV_OK) {
        return RETV_ERROR;
    } // if

    //play the info file and use high VPP
    return playJtagFile("", fSize, 1, 0);
} // processJtagInfo()

bool processJtagErase(void) {
    int16_t fSize = 0;
    char    tmp[256];
    char*   originalFname = filename;

    if (!opErase) {
        return RETV_OK;
    } // if
    // Use default .xsvf file for erase.
    sprintf(tmp, "xsvf/erase_%s.xsvf", galinfo[gal].name);
    filename = tmp;

    if (readFile(&fSize) != RETV_OK) {
        filename = originalFname;
        return RETV_ERROR;
    } // if
    filename = originalFname;

    //play the erase file and use high VPP
    return playJtagFile("erase ", fSize, 1, 1);
} // processJtagErase()

bool processJtagWrite(void) {
    int16_t fSize = 0;

    if (!opWrite) {
        return RETV_OK;
    } // if

    // paranoid: this condition should be already checked during argument's check
    if (filename == NULL) {
        return RETV_ERROR;
    } // if
    if (readFile(&fSize) != RETV_OK) {
        return RETV_ERROR;
    } // if
    //play the file and use low VPP
    return playJtagFile("write ", fSize, 0, 1);
} // processJtagWrite()

bool processJtag(void) {

    if (verbose) {
        printf("JTAG\n");
    } // if

    if ((gal == ATF1502AS || gal == ATF1504AS) && (opRead || opVerify)) {
        printf("error: read and verify operation is not supported\n");
        return RETV_ERROR;
    } // if

    if (processJtagInfo() != RETV_OK) {
        return RETV_ERROR;
    } // if

    if (processJtagErase() != RETV_OK) {
        return RETV_ERROR;
    } // if

    if (processJtagWrite() != RETV_OK) {
        return RETV_ERROR;
    } // if
    return RETV_OK;
} // processJtag()
