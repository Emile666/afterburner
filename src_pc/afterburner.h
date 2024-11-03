#ifndef _AFTERBURNER_H_
#define _AFTERBURNER_H_
/* (banner font: aciiart.eu)
_____________________________________________________________
|       _    __ _            _                                \
|      / \  / _| |_ ___  _ _| |__  _   _ _ __ ___  ___  _ _   |\
|     / _ \| |_| '_/ _ \| '_/ '_ \| | | | '_/  _ \/ _ \| '_/  ||
|    / ___ \  _| |_| __/| | | |_) | |_| | | | | | | __/| |    ||
|   /_/   \_\|  \__\___||_| |____/\___,_|_| |_| |_|___||_|    ||
\_____________________________________________________________||
'------------------------------------------------------------'

Afterburner: GAL IC Programmer for Arduino by -= olin =-
http://molej.cz/index_aft.html

Based on ATFblast 3.1 by Bruce Abbott
http://www.bhabbott.net.nz/atfblast.html

Based on GALBLAST by Manfred Winterhoff
http://www.armory.com/%7Erstevew/Public/Pgmrs/GAL/_ClikMe1st.htm

Supports:
* National GAL16V8
* Lattice GAL16V8A, GAL16V8B, GAL16V8D
* Lattice GAL22V10B
* Atmel ATF16V8B, ATF22V10B, ATF22V10CQZ

Requires:
* Arduino UNO with Afterburner sketch uploaded.
* simple programming circuit.

Changelog:
* use 'git log'

This is the PC part that communicates with Arduino UNO by serial line.
To compile: gcc -g3 -O0 -o afterburner afterburner.c

* 2024-02-02  Fixed: Command 'B9' (Calibration Offset = 0,25V) doesn't work
              Note: Also requires elimination of a bug in the PC program afterburner.ino
              Added: Sending B4, if b /wo -co is executed
*/
#include <stdint.h>
#include <stdbool.h>
#include "serial_port.h"

#define VERSION "v.0.6.3"

#ifdef GCOM
#define VERSION_EXTENDED VERSION "-" GCOM
#else
#define VERSION_EXTENDED VERSION
#endif

#define MAX_LINE   (16 * 1024)
#define MAXFUSES   (30000)
#define GALBUFSIZE (256 * 1024)

#define MIN_CAL_OFFSET (-32) /* Min. calibration offset in [E-2 V] */
#define MAX_CAL_OFFSET  (32) /* Max. calibration offset in [E-2 V] */

// Constants for operationWriteOrVerify()
#define DO_WRITE      (true)
#define NO_WRITE     (false)

// Constants for sendGenericCommand()
#define DO_PRINT      (true)
#define NO_PRINT     (false)

// States for parseFuseMap() function
typedef enum {
	ST_JED_OUT = 0,
	ST_SKIP,
	ST_RD_CMD,
	ST_ADDR1,
	ST_ADDRN,
	ST_FUSE_INIT,
	ST_RD_BITS,
	ST_QPQF_CMD,
	ST_QP1,
	ST_QF1,
	ST_QPN,
	ST_QFN,
	ST_QPQF_RDY,
	ST_G_SEC,
	ST_C_CHK1,
	ST_C_CHKN,
} States;

typedef enum {
    UNKNOWN,
    GAL16V8,
    GAL18V10,
    GAL20V8,
    GAL20RA10,
    GAL20XV10,
    GAL22V10,
    GAL26CV12,
    GAL26V12,
    GAL6001,
    GAL6002,
    ATF16V8B,
    ATF20V8B,
    ATF22V10B,
    ATF22V10C,
    ATF750C,
    //jtag based PLDs at the end: they do not have a gal type in MCU software
    ATF1502AS,
    ATF1504AS,
} Galtype;

/* GAL info */
typedef struct {
    Galtype type;
    uint8_t id0, id1;     /* variant 1, variant 2 (eg. 16V8=0x00, 16V8A+=0x1A)*/
    char   *name;         /* pointer to chip name               */
    int16_t fuses;        /* total number of fuses              */
    int16_t pins;         /* number of pins on chip             */
    int16_t rows;         /* number of fuse rows                */
    int16_t bits;         /* number of fuses per row            */
    int16_t uesrow;       /* UES row number                     */
    int16_t uesfuse;      /* first UES fuse number              */
    int16_t uesbytes;     /* number of UES bytes                */
    int16_t eraserow;     /* row adddeess for erase             */
    int16_t eraseallrow;  /* row address for erase all          */
    int16_t pesrow;       /* row address for PES read/write     */
    int16_t pesbytes;     /* number of PES bytes                */
    int16_t cfgrow;       /* row address of config bits         */
    int16_t cfgbits;      /* number of config bits              */
} _str_galinfo;

void     printGalTypes(void);
void     printHelp(void);
bool     verifyArgs(char* type);
bool     checkArgs(int16_t argc, char** argv);
uint16_t checkSum(uint16_t n);
int16_t  parseFuseMap(char *ptr);
bool     readFile(int16_t* fileSize);
char*    findLastLine(char* buf);
void     updateProgressBar(char* label, int16_t current, int16_t total);
bool     upload(void);
bool     sendGenericCommand(const char* command, const char* errorText, int32_t maxDelay, bool printResult);
bool     operationWriteOrVerify(bool doWrite);
bool     operationReadInfo(void);
bool     operationTestVpp(void);
bool     operationCalibrateVpp(void);
bool     operationMeasureVpp(void);
bool     operationSetGalCheck(void);
bool     operationSetGalType(Galtype type);
bool     operationSecureGal();
bool     operationWritePes(void);
bool     operationEraseGal(void);
bool     operationReadFuses(void);

#endif /* _AFTERBURNER_H_ */
