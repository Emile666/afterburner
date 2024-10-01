#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "afterburner.h"
#include "serial_port.h"

/* GAL info */
_str_galinfo galinfo[] = {
	//                                                              |---- UES ----|- erase -|- PES --|- CFG --|
    // Type     ID0      ID1        Name       fuses pins rows bits row  fuse bytes row  all row bytes row bits
    {UNKNOWN,   0x00   , 0x00   , "unknown"  ,     0,  0,   0,   0,  0,     0,   0,  0,   0,   0,   8,  0,   0},
    {GAL16V8,   0x00   , 0x1A   , "GAL16V8"  ,  2194, 20,  32,  64, 32,  2056,   8, 63,  54,  58,   8, 60,  82},
    {GAL18V10,  0x50   , 0x51   , "GAL18V10" ,  3540, 20,  36,  96, 36,  3476,   8, 61,  60,  58,  10, 16,  20},
    {GAL20V8,   0x20   , 0x3A   , "GAL20V8"  ,  2706, 24,  40,  64, 40,  2568,   8, 63,  59,  58,   8, 60,  82},
    {GAL20RA10, 0x60   , 0x61   , "GAL20RA10",  3274, 24,  40,  80, 40,  3210,   8, 61,  60,  58,  10, 16,  10},
    {GAL20XV10, 0x65   , 0x66   , "GAL20XV10",  1671, 24,  40,  40, 44,  1631,   5, 61,  60,  58,   5, 16,  31},
    {GAL22V10,  0x48   , 0x49   , "GAL22V10" ,  5892, 24,  44, 132, 44,  5828,   8, 61,  60,  58,  10, 16,  20},
    {GAL26CV12, 0x58   , 0x59   , "GAL26CV12",  6432, 28,  52, 122, 52,  6368,   8, 61,  60,  58,  12, 16,  24},
    {GAL26V12,  0x5D   , 0x5D   , "GAL26V12" ,  7912, 28,  52, 150, 52,  7848,   8, 61,  60,  58,  12, 16,  48},
    {GAL6001,   0x40   , 0x41   , "GAL6001"  ,  8294, 24,  78,  75, 97,  8222,   9, 63,  62,  96,   8,  8,  68},
    {GAL6002,   0x44   , 0x44   , "GAL6002"  ,  8330, 24,  78,  75, 97,  8258,   9, 63,  62,  96,   8,  8, 104},
    {ATF16V8B,  0x00   , 0x00   , "ATF16V8B" ,  2194, 20,  32,  64, 32,  2056,   8, 63,  54,  58,   8, 60,  82},
    {ATF20V8B,  0x00   , 0x00   , "ATF20V8B" ,  2706, 24,  40,  64, 40,  2568,   8, 63,  59,  58,   8, 60,  82},
    {ATF22V10B, 0x00   , 0x00   , "ATF22V10B",  5892, 24,  44, 132, 44,  5828,   8, 61,  60,  58,  10, 16,  20},
    {ATF22V10C, 0x00   , 0x00   , "ATF22V10C",  5892, 24,  44, 132, 44,  5828,   8, 61,  60,  58,  10, 16,  20},
    {ATF750C,   0x00   , 0x00   , "ATF750C"  , 14499, 24,  84, 171, 84, 14435,   8, 61,  60, 127,  10, 16,  71},
    {ATF1502AS, JTAG_ID, JTAG_ID, "ATF1502AS",     0,  0,   0,   0,  0,     0,   0,  0,   0,   0,   8,  0,  0},
    {ATF1504AS, JTAG_ID, JTAG_ID, "ATF1504AS",     0,  0,   0,   0,  0,     0,   0,  0,   0,   0,   8,  0,  0},
};

bool  verbose    = false;
char* filename   = NULL;
char* deviceName = NULL;
char* pesString  = NULL;

SerialDeviceHandle serialF = INVALID_HANDLE;
Galtype  gal;
int16_t  security = 0;
uint16_t checksum;
char     galbuffer[GALBUFSIZE];
char     fusemap[MAXFUSES];
bool     noGalCheck = false;
bool     varVppExists = false;
bool     printSerialWhileWaiting = false;
int16_t  calOffset = 0;  //no calibration offset is applied
bool     bigRam = false; // 'BIG-RAM found

bool opRead         = false; /* read fuse map and display */
bool opWrite        = false; /* write fuse map */
bool opErase        = false; /* erase GAL chip */
char opInfo         = false; /* read device info */
char opVerify       = false; /* verify fuse map */
char opTestVPP      = false; /* set Vpp on to check voltage */
char opCalibrateVPP = false; /* calibrate Vpp on new board design */
char opMeasureVPP   = false; /* measure Vpp on new board design */
char opSecureGal    = false; /* -sec: enable security */
char opWritePes     = false; /* write PES */
char flagEraseAll   = true;  /* erase all data including PES */
char flagEnableApd  = 0;

void printGalTypes(void) {
    int16_t i;
    for (i = 1; i < sizeof(galinfo) / sizeof(galinfo[0]); i++) {
        if (i % 8 == 1) {
            printf("\n\t");
        } else
        if (i > 1) {
            printf(" ");
        }
        printf("%s", galinfo[i].name);
    } // for
} // printGalTypes()

void printHelp(void) {
    printf("Afterburner " VERSION_EXTENDED "  a GAL programming tool for Arduino based programmer\n");
    printf("more info: https://github.com/ole00/afterburner\n");
    printf("usage: afterburner command(s) [options]\n");
    printf("commands: ierwvsbm\n");
    printf("   i : read device info and programming voltage\n");
    printf("   r : read fuse map from the GAL chip and display it, -t option must be set\n");
    printf("   w : write fuse map, -f  and -t options must be set\n");
    printf("   v : verify fuse map, -f and -t options must be set\n");
    printf("   e : erase the GAL chip,  -t option must be set. Optionally '-all' can be set.\n");
    printf("   p : write PES. -t and -pes options must be set. GAL must be erased with '-all' option.\n");
    printf("   s : set VPP ON to check the programming voltage. Ensure the GAL is NOT inserted.\n");
    printf("   b : calibrate variable VPP on new board designs. Ensure the GAL is NOT inserted.\n");
    printf("   m : measure variable VPP on new board designs. Ensure the GAL is NOT inserted.\n");   
    printf("options:\n");
    printf("  -v : verbose mode\n");
    printf("  -t <gal_type> : the GAL type. use ");
    printGalTypes();
    printf("\n");
    printf("  -f <file> : JEDEC fuse map file\n");
    printf("  -d <serial_device> : name of the serial device. Without this option the device is guessed.\n");
    printf("                       serial params are: 57600, 8N1\n");
    printf("  -nc : do not check device GAL type before operation: force the GAL type set on command line\n");
    printf("  -sec: enable security - protect the chip. Use with 'w' or 'v' commands.\n");
    printf("  -co <offset>: Set calibration offset. Use with 'b' command. Value: -20 (-0.2V) to 25 (+0.25V)\n");
    printf("  -all: use with 'e' command to erase all data including PES.\n");
    printf("  -pes <PES> : use with 'p' command to specify new PES. PES format is 8 hex bytes with a delimiter.\n");
    printf("               For example 00:03:3A:A1:00:00:00:90\n");
    printf("examples:\n");
    printf("  afterburner i -t ATF16V8B : reads and prints the device info\n");
    printf("  afterburner r -t ATF16V8B : reads the fuse map from the GAL chip and displays it\n");
    printf("  afterburner wv -f fuses.jed -t ATF16V8B : reads fuse map from file and writes it to \n");
    printf("              the GAL chip. Does the fuse map verification at the end.\n");
    printf("  afterburner ep -t GAL20V8 -all -pes 00:03:3A:A1:00:00:00:90  Fully erases the GAL chip\n");
    printf("              and writes new PES. Does not work with Atmel chips.\n");
    printf("hints:\n");
    printf("  - use the 'i' command first to check and set the right programming voltage (VPP)\n");
    printf("         of the chip. If the programing voltage is unknown use 10V.\n");
    printf("  - known VPP voltages as tested on Afterburner with Arduino UNO: \n");
    printf("        Lattice GAL16V8D, GAL20V8B, GAL22V10D: 12V \n");
    printf("        Atmel   ATF16V8B, ATF16V8C, ATF22V10C: 11V \n");
} // printHelp()

/*-----------------------------------------------------------------------------
  Purpose  : This routine verifies the combination of input options
 Variables : type: a string containing the input options
  Returns  : true: error, false: no error
  ---------------------------------------------------------------------------*/
bool verifyArgs(char* type) {
    if (!opRead && !opWrite && !opErase && !opInfo && !opVerify && !opTestVPP && !opCalibrateVPP && !opMeasureVPP && !opWritePes) {
        printHelp();
        printf("Error: no command specified.\n");
        return RETV_ERROR;
    }
    if (opWritePes && (NULL == pesString || strlen(pesString) != 23)) {
        printf("Error: invalid or no PES specified.\n");
        return RETV_ERROR;
    }
    if ((opRead || opWrite || opVerify) && opErase && flagEraseAll) {
        printf("Error: invalid command combination. Use 'Erase all' in a separate step\n");
        return RETV_ERROR;
    }
    if ((opRead || opWrite || opVerify) && (opTestVPP || opCalibrateVPP || opMeasureVPP)) {
        printf("Error: VPP functions can not be conbined with read/write/verify operations\n");
        return RETV_ERROR;
    }
    if ((type == NULL) && (opWrite || opRead || opErase || opVerify || opInfo || opWritePes))  {
        printf("Error: missing GAL type. Use -t <type> to specify.\n");
        return RETV_ERROR;
    } else if (type != NULL) {
        for (int16_t i = 1; i < sizeof(galinfo) / sizeof(galinfo[0]); i++) {
            if (!strcmp(strupr(type), galinfo[i].name)) {
                gal = galinfo[i].type;
                break;
            } // if
        } // for i
        if (UNKNOWN == gal) {
            printf("Error: unknown GAL type. Types: ");
            printGalTypes();
            printf("\n");
            return RETV_ERROR;
        } // if
    } // else if
    if ((NULL == filename) && (opWrite || opVerify)) {
        printf("Error: missing %s filename (param: -f fname)\n", galinfo[gal].id0 == JTAG_ID ? ".xsvf" : ".jed");
        return RETV_ERROR;
    } // if
    return RETV_OK;
} // verifyArgs()

/*-----------------------------------------------------------------------------
  Purpose  : This routine checks the command-line arguments
 Variables : argc: argument count
             argv: string containing the arguments
  Returns  : true: error, false: no error
  ---------------------------------------------------------------------------*/
bool checkArgs(int16_t argc, char** argv) {
    int16_t i;
    char*   type  = '\0';
    char*   modes = '\0';

    gal = UNKNOWN;

    for (i = 1; i < argc; i++) {
        char* param = argv[i];
        if (!strcmp("-t", param)) {
            type = argv[++i];
        } else if (!strcmp("-v", param)) {
            verbose = true;
        } else if (!strcmp("-f", param)) {
            filename = argv[++i];
        } else if (!strcmp("-d", param)) {
            deviceName = argv[++i];
        } else if (!strcmp("-nc", param)) {
            noGalCheck = true;
        } else if (!strcmp("-sec", param)) {
            opSecureGal = true;
        } else if (!strcmp("-all", param)) {
            flagEraseAll = true;
        }  else if (!strcmp("-pes", param)) {
            pesString = argv[++i];
        } else if (!strcmp("-co", param)) {
            calOffset = atoi(argv[++i]);
            if ((calOffset < MIN_CAL_OFFSET) || (calOffset > MAX_CAL_OFFSET)) {
                printf("Calibration offset out of range (-32..32 inclusive).\n");
            } // if
            if (calOffset < MIN_CAL_OFFSET) {
                calOffset = MIN_CAL_OFFSET;
            } else if (calOffset > MAX_CAL_OFFSET) {
                calOffset = MAX_CAL_OFFSET;
            } // else if
        } // else if
        else if (param[0] != '-') {
            modes = param;
        } // else if
    } // for i 

    i = 0;
    while ((modes != NULL) && (modes[i] != '\0')) {
        switch (modes[i]) {
        case 'r':
            opRead = true;
            break;
        case 'w':
            opWrite = true;
            break;
        case 'v':
            opVerify = true;
            break;
        case 'e':
            opErase = true;
            break;
        case 'i':
            opInfo = true;
            break;
        case 's':
            opTestVPP = true;
            break;
        case 'b':
            opCalibrateVPP = true;
            break;
        case 'm':
            opMeasureVPP = true;
            break;
        case 'p':
            opWritePes = true;
            break;
        default:
            printf("Error: unknown operation '%c' \n", modes[i]);
        } // switch
        i++;
    } // while

    if (verifyArgs(type)) {
        return RETV_ERROR;
    } // if
    return RETV_OK;
} // checkArgs()

uint16_t checkSum(uint16_t n) {
    uint16_t c, e, i;
    uint32_t a;

    c = e = 0;
    a = 0;
    for (i = 0; i < n; i++) {
        e++;
        if (e == 9) {
            e = 1;
            a += c;
            c = 0;
        } // if
        c >>= 1;
        if (fusemap[i]) {
            c += 0x80;
        } // if
    } // for i
    return (uint16_t)((c >> (8 - e)) + a);
} // checkSum()

int16_t parseFuseMap(char *ptr) {
    int16_t i, n, type, address;
	int16_t checksumpos = 0;
	int16_t pins        = 0;
	int16_t lastfuse    = 0;
    States  state       = ST_JED_OUT; // 0=outside JEDEC, 1=skipping comment or unknown, 2=read command

    security = 0;
    checksum = 0;

    for (n = 0; ptr[n]; n++) {
        if (ptr[n] == '*') {
            state = ST_RD_CMD; // read command byte
        } else
            switch (state) {
            case ST_RD_CMD: // read command byte
                if (!isspace(ptr[n]))
                    switch (ptr[n]) {
                    case 'L': // address of a fuse
                        address = 0;        // init. address
                        state   = ST_ADDR1; // read 1st digit of address
                        break;
                    case 'F': // not listed fuses are set to 0/1
                        state = ST_FUSE_INIT;
                        break;
                    case 'G': // security fuse command
                        state = ST_G_SEC;
                        break;
                    case 'Q': // QP or QF command
                        state = ST_QPQF_CMD;
                        break;
                    case 'C': // fuse checksum
                        checksumpos = n;
                        state       = ST_C_CHK1;
                        break;
                    default:
                        state = ST_SKIP; // skipping comment or unknown
                    } // switch
                break;
            case ST_ADDR1: // read 1st digit of an address
                if (!isdigit(ptr[n])) {
                    return n;
                } // if
                address = ptr[n] - '0';
                state   = ST_ADDRN; // read remaining address digit
                break;
            case ST_ADDRN: // read remaining address digits
                if (isspace(ptr[n])) {
                    state = ST_RD_BITS; // read bits on Lxxxx line
                } else if (isdigit(ptr[n])) {
                    address = 10 * address + (ptr[n] - '0');
                } else {
                    return n;
                } // else
                break;
            case ST_FUSE_INIT: // init fuses to 0 or 1
                if (isspace(ptr[n])) break; // ignored
                if (ptr[n] == '0' || ptr[n] == '1') {
                    memset(fusemap, ptr[n] - '0', sizeof(fusemap));
                } else {
                    return n;
                } // else
                state = ST_SKIP; // skipping comment or unknown
                break;
            case ST_RD_BITS: // read bits on Lxxxx line
                if (isspace(ptr[n])) break; // ignored
                if (ptr[n] == '0' || ptr[n] == '1') {
                    fusemap[address++] = ptr[n] - '0';
                } else {
                    return n;
                } // else
                break;
            case ST_QPQF_CMD: // QP or QF command
                if (isspace(ptr[n])) break; // ignored
                if (ptr[n] == 'P') {
                    pins  = 0;      // init. number of pins
                    state = ST_QP1; // QP 1st digit
                } else if (ptr[n] == 'F') {
                    lastfuse = 0;      // init. lastfuse address
                    state    = ST_QF1; // QF 1st digit
                } else {
                    state = ST_RD_CMD; // read next command byte
                } // else
                break;
            case ST_QP1: // QP 1st digit
                if (isspace(ptr[n])) break; // ignored
                if (!isdigit(ptr[n])) return n;
                pins = ptr[n] - '0'; // 1st QP digit
                state = ST_QPN;      // read other QP digits
                break;
            case ST_QF1: // QF 1st digit
                if (isspace(ptr[n])) break; // ignored
                if (!isdigit(ptr[n])) return n;
                lastfuse = ptr[n] - '0'; // 1st digit of lastfuse
                state = ST_QFN;          // read other QF digits
                break;
            case ST_QPN: // QP other digits
                if (isdigit(ptr[n])) {
                    pins = 10 * pins + (ptr[n] - '0');
                } else if (isspace(ptr[n])) {
                    state = ST_QPQF_RDY; // done reading
                } else {
                    return n;
                } // else
                break;
            case ST_QFN: // QF other digits
                if (isdigit(ptr[n])) {
                    lastfuse = 10 * lastfuse + (ptr[n] - '0');
                } else if (isspace(ptr[n])) {
                    state = ST_QPQF_RDY; // done reading
                } else {
                    return n;
                } // else
                break;
            case ST_QPQF_RDY: // QP or QF finished
                if (!isspace(ptr[n])) {
                    return n;
                } // if
                break;
            case ST_G_SEC: // G security fuse
                if (isspace(ptr[n])) break; // ignored
                if (ptr[n] == '0' || ptr[n] == '1') {
                    security = ptr[n] - '0';
                } else {
                    return n;
                }
                state = ST_SKIP; // skipping comment or unknown
                break;
            case ST_C_CHK1: // C checksum 1st byte
                if (isspace(ptr[n])) break; // ignored
                if (isdigit(ptr[n])) {
                    checksum = ptr[n] - '0';
                } else if (toupper(ptr[n]) >= 'A' && toupper(ptr[n]) <= 'F') {
                    checksum = toupper(ptr[n]) - 'A' + 10;
                } else return n;
                state = ST_C_CHKN;
                break;
            case ST_C_CHKN: // C checksum other bytes
                if (isdigit(ptr[n])) {
                    checksum = 16 * checksum + ptr[n] - '0';
                } else if (toupper(ptr[n]) >= 'A' && toupper(ptr[n]) <= 'F') {
                    checksum = 16 * checksum + toupper(ptr[n]) - 'A' + 10;
                } else if (isspace(ptr[n])) {
                    state = ST_RD_CMD; // read command byte
                } else return n;
                break;
            } // else switch (state)
    } // for n

    if (lastfuse || pins) {
        uint16_t cs = checkSum(lastfuse);
        if (checksum && (checksum != cs)) {
            printf("Checksum does not match! given=0x%04X calculated=0x%04X last fuse=%i\n", checksum, cs, lastfuse);
        } // if

        for (type = UNKNOWN, i = 1; i < sizeof(galinfo) / sizeof(galinfo[0]); i++) {
            if (
                ((lastfuse == 0) ||
                 (galinfo[i].fuses == lastfuse) ||
                 ((galinfo[i].uesfuse == lastfuse) && (galinfo[i].uesfuse + 8 * galinfo[i].uesbytes == galinfo[i].fuses)))
                &&
                ((pins == 0) ||
                 (galinfo[i].pins == pins) ||
                 ((galinfo[i].pins == 24) && (pins == 28)))
            ) {
                if (gal == 0) {
                    type = i;
                    break;
                } else if (!type) {
                    type = i;
                } // else if
            } // if
        } // for type
    } // if
    if ((lastfuse == 2195) && (gal == ATF16V8B)) {
        flagEnableApd = fusemap[2194];
        if (verbose) {
            printf("PD fuse detected: %i\n", fusemap[2194]);
        } // if
    } // if
    if ((lastfuse == 5893) && (gal == ATF22V10C)) {
        flagEnableApd = fusemap[5892];
        if (verbose) {
            printf("PD fuse detected: %i\n", fusemap[5892]);
        } // if
    } // if
    return n;
} // parseFuseMap()

bool readFile(int16_t* fileSize) {
    FILE*   f;
    int16_t size;

    if (verbose) {
        printf("opening file: '%s'\n", filename);
    }
    f = fopen(filename, "rb");
    if (f) {
        size = fread(galbuffer, 1, GALBUFSIZE, f);
        fclose(f);
        galbuffer[size] = 0;
    } else {
        printf("Error: failed to open file: %s\n", filename);
        return RETV_ERROR;
    }
    if (fileSize != NULL) {
        *fileSize = size;
        if (verbose) {
            printf("file size: %d'\n", size);
        }
    }
    return RETV_OK;
} // readFile()

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

// finds beginnig of the last line
char* findLastLine(char* buf) {
    int16_t   i;
    char* result = buf;

    if (buf == NULL) {
        return 0;
    } // if
    for (i = 0; buf[i] != 0; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') {
            result = buf + i + 1;
        } // if      
    } // for i
    return result;
} // findLastLine()

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
                    printf("ERROR: serial port read buffer is too small!\nAre you dumping large amount of data?\n");
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

void updateProgressBar(char* label, int16_t current, int16_t total) {
    int16_t done = ((current + 1) * 40) / total;
    if (current >= total) {
        printf("%s%5d/%5d |########################################|\n", label, total, total);
    } else {
        printf("%s%5d/%5d |", label, current, total);
        printf("%.*s%*s|\r", done, "########################################", 40 - done, "");
        fflush(stdout); //flush the text out so that the animation of the progress bar looks smooth
    } // else
} // updateProgressBar()

// Upload fusemap in byte format (as opposed to bit format used in JEDEC file).
bool upload(void) {
    char     fuseSet;
    char     buf[MAX_LINE];
    char     line[64];
    uint16_t i, j, n;
    uint16_t csum;
    int16_t  apdFuse = flagEnableApd;
    int16_t  totalFuses = galinfo[gal].fuses;

    if (apdFuse) {
        totalFuses++;
    }

    // Start  upload
    sprintf(buf, "u\r");
    sendLine(buf, MAX_LINE, 20);

    //device type
    sprintf(buf, "#t %c %s\r", '0' + (int16_t) gal, galinfo[gal].name);
    sendLine(buf, MAX_LINE, 300);

    //fuse map
    buf[0]  = 0;
    fuseSet = 0;
    
    printf("Uploading fuse map...\n");
    for (i = 0; i < totalFuses;) {
        unsigned char f = 0;
        if (i % 32 == 0) {
            if (i != 0) {
                strcat(buf, "\r");
                // the buffer contains at least one fuse set to 1
                if (fuseSet) {
#ifdef DEBUG_UPLOAD
                    printf("%s\n", buf);
#endif
                    sendLine(buf, MAX_LINE, 100);
                    buf[0] = 0;
                }
                fuseSet = 0;
            }
            sprintf(buf, "#f %04i ", i);
        }
        f = 0;
        for (j = 0; j < 8 && i < totalFuses; j++,i++) {
            if (fusemap[i]) {
                f |= (1 << j);
                fuseSet = 1;
            }
        }

        sprintf(line, "%02X", f);
        strcat(buf, line);

        updateProgressBar("", i, totalFuses);
    }
    updateProgressBar("", totalFuses, totalFuses);

    // send last unfinished fuse line
    if (fuseSet) {
        strcat(buf, "\r");
#ifdef DEBUG_UPLOAD
        printf("%s\n", buf);
#endif
        sendLine(buf, MAX_LINE, 100);
    }

    csum = checkSum(totalFuses); //checksum
    if (verbose) {
        printf("sending csum: %04X\n", csum);
    }
    sprintf(buf, "#c %04X\r", csum);
    sendLine(buf, MAX_LINE, 300);

    //end of upload
    return sendGenericCommand("#e\r", "Upload failed", 300, NO_PRINT); 
} // upload()

// returns 0 on success
bool sendGenericCommand(const char* command, const char* errorText, int16_t maxDelay, bool printResult) {
    char    buf[MAX_LINE];
    int16_t readSize;

    sprintf(buf, "%s", command);
    readSize = sendLine(buf, MAX_LINE, maxDelay);
    if (readSize < 0)  {
        if (verbose) {
            printf("%s\n", errorText);
        } // if
        return RETV_ERROR;
    } else {
        char* response = stripPrompt(buf);
        char* lastLine = findLastLine(response);
        if (lastLine == 0 || ((lastLine[0] == 'E') && (lastLine[1] == 'R'))) {
            printf("%s\n", response);
            return RETV_ERROR;
        } else if (printResult && !printSerialWhileWaiting) {
            printf("%s\n", response);
        } // else if
    } // else
    return RETV_OK;
} // sendGenericCommand()

bool operationWriteOrVerify(bool doWrite) {
    char    buf[MAX_LINE];
    int16_t readSize;
    bool    result;

    if (readFile(NULL)) {
        return RETV_ERROR;
    } // if

    result = parseFuseMap(galbuffer);
    if (verbose) {
        printf("parse result=%i\n", result);
    } // if

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    // set power-down fuse bit (do it before upload to correctly calculate check-sum)
    result = sendGenericCommand(flagEnableApd ? "z\r" : "Z\r", "APD set failed ?", 4000, NO_PRINT);
    if (result != RETV_OK) {
		closeSerial();
        return RETV_ERROR;
    } // if
    result = upload();
    if (result != RETV_OK) {
        return RETV_ERROR;
    } // if

    // write command
    if (doWrite) {
        result = sendGenericCommand("w\r", "write failed ?", 8000, NO_PRINT);
        if (result != RETV_OK) {
            closeSerial();
			return RETV_ERROR;
        } // if
    } // if

    // verify command
    if (opVerify) {
        result = sendGenericCommand("v\r", "verify failed ?", 8000, NO_PRINT);
    } // if
    closeSerial();
    return result;
} // operationWriteOrVerify()

bool operationReadInfo(void) {

    bool result;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    if (verbose) {
        printf("sending 'p' command...\n");
    }
    result = sendGenericCommand("p\r", "info failed ?", 4000, DO_PRINT);
    closeSerial();
    return result;
} // operationReadInfo()

// Test of programming voltage. Most chips require +12V to start prograaming.
// This test function turns ON the ENable pin so the Programming voltage is set.
// After 20 seconds the ENable pin is turned OFF. This gives you time to turn the
// pot on the MT3608 module and calibrate the right voltage for the GAL chip.
bool operationTestVpp(void) {

    bool result;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    if (verbose) {
        printf("sending 't' command...\n");
    } // if
    if (varVppExists) {
        printf("Turn the Pot on the MT3608 module to set the VPP to 16.5V (+/- 0.05V)\n");
    } else {
        printf("Turn the Pot on the MT3608 module to check / set the VPP\n");
    } // else
    //print the measured voltages if the feature is available
    printSerialWhileWaiting = true;

    //Voltage testing takes ~20 seconds
    result = sendGenericCommand("t\r", "info failed ?", 22000, DO_PRINT);
    printSerialWhileWaiting = false;
    closeSerial();
    return result;
} // operationTestVpp()

bool operationCalibrateVpp(void) {
    bool result;
    char cmd [8] = {0};
    char val = (char)('0' + (calOffset + MAX_CAL_OFFSET));

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    sprintf(cmd, "B%c\r", val);
    if (verbose) {
        printf("sending 'B%c' command...\n", val);
    } // if
    result = sendGenericCommand(cmd, "VPP cal. offset failed", 4000, DO_PRINT);

    if (verbose) {
        printf("sending 'b' command...\n");
    } // if
    
    printf("VPP voltages are scanned - this might take a while...\n");
    printSerialWhileWaiting = true;
    result = sendGenericCommand("b\r", "VPP calibration failed", 34000, DO_PRINT);
    printSerialWhileWaiting = false;
    closeSerial();
    return result;
} // operationCalibrateVpp()

bool operationMeasureVpp(void) {
    bool result;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    if (verbose) {
        printf("sending 'm' command...\n");
    } // if
    
    //print the measured voltages if the feature is available
    printSerialWhileWaiting = true;
    result = sendGenericCommand("m\r", "VPP measurement failed", 40000, DO_PRINT);
    printSerialWhileWaiting = false;
    closeSerial();
    return result;
} // operationMeasureVpp()

bool operationSetGalCheck(void) {
    int16_t readSize;
    bool    result;

    if (openSerial() != 0) {
        return -1;
    } // if
    result = sendGenericCommand(noGalCheck ? "F\r" : "f\r", "noGalCheck failed ?", 4000, NO_PRINT);
    closeSerial();
    return result;    
} // operationSetGalCheck()

bool operationSetGalType(Galtype type) {
    char    buf[MAX_LINE];
    int16_t readSize;
    bool    result;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if
    if (verbose) {
        printf("sending 'g' command type=%i\n", type);
    } // if
    sprintf(buf, "g%c\r", '0' + (int16_t)type); 
    result = sendGenericCommand(buf, "setGalType failed ?", 4000, NO_PRINT);
    closeSerial();
    return result;    
} // operationSetGalType()

bool operationSecureGal(void) {
    int16_t readSize;
    bool    result;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if
    if (verbose) {
        printf("sending 's' command...\n");
    } // if
    result = sendGenericCommand("s\r", "secure GAL failed ?", 4000, NO_PRINT);
    closeSerial();
    return result;
} // operationSecureGal()

bool operationWritePes(void) {
    char    buf[MAX_LINE];
    int16_t readSize;
    char    result;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    //Switch to upload mode to specify GAL
    sprintf(buf, "u\r");
    sendLine(buf, MAX_LINE, 300);

    //set GAL type
    sprintf(buf, "#t %c\r", '0' + (int16_t) gal);
    sendLine(buf, MAX_LINE, 300);

    //set new PES
    sprintf(buf, "#p %s\r", pesString);
    sendLine(buf, MAX_LINE, 300);

    //Exit upload mode (ensure the return texts are discarded by waiting 100 ms)
    sprintf(buf, "#e\r");
    sendLine(buf, MAX_LINE, 100);

    if (verbose) {
        printf("sending 'P' command...\n");
    } // if
    result = sendGenericCommand("P\r", "write PES failed ?", 4000, NO_PRINT);
    closeSerial();
    return result;
} // operationWritePes()

bool operationEraseGal(void) {
    char    buf[MAX_LINE];
    int16_t readSize;
    bool    result;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    //Switch to upload mode to specify GAL
    sprintf(buf, "u\r");
    sendLine(buf, MAX_LINE, 300);

    //set GAL type
    sprintf(buf, "#t %c\r", '0' + (int16_t) gal);
    sendLine(buf, MAX_LINE, 300);

    //Exit upload mode (ensure the return texts are discarded by waiting 100 ms)
    sprintf(buf, "#e\r");
    sendLine(buf, MAX_LINE, 100);

    if (flagEraseAll) {
        result = sendGenericCommand("~\r", "erase all failed ?", 4000, NO_PRINT);
    } else {
        result = sendGenericCommand("c\r", "erase failed ?", 4000, NO_PRINT);
    } // if
    closeSerial();
    return result;
} // operationEraseGal()

bool operationReadFuses(void) {
    char*   response;
    char*   buf = galbuffer;
    int16_t readSize;

    if (openSerial() != RETV_OK) {
        return RETV_ERROR;
    } // if

    //Switch to upload mode to specify GAL
    sprintf(buf, "u\r");
    sendLine(buf, MAX_LINE, 100);

    //set GAL type
    sprintf(buf, "#t %c\r", '0' + (int16_t) gal);
    sendLine(buf, MAX_LINE, 100);

    //Exit upload mode (ensure the texts are discarded by waiting 100 ms)
    sprintf(buf, "#e\r");
    sendLine(buf, MAX_LINE, 1000);

    //READ_FUSE command
    sprintf(buf, "r\r");
    readSize = sendLine(buf, GALBUFSIZE, 12000);
    if (readSize < 0)  {
        return RETV_ERROR;
    } // if
    response = stripPrompt(buf);
    printf("%s\n", response);
    closeSerial();

    if (response[0] == 'E' && response[1] == 'R') {
        return RETV_ERROR;
    } // if
    return RETV_OK;
} // operationReadFuses()

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

int16_t main(int16_t argc, char** argv) {
    bool    result = false;
    int16_t i;

    if (checkArgs(argc, argv) != RETV_OK) {
        return RETV_ERROR;
    } // if
    if (verbose) {
        printf("Afterburner " VERSION " \n");
    } // if

    // process JTAG operations
    if ((gal != UNKNOWN) && (galinfo[gal].id0 == JTAG_ID) && (galinfo[gal].id1 == JTAG_ID)) {
        result = processJtag();
    } // if
	else {
		result = operationSetGalCheck();
		if ((gal != UNKNOWN) && (result == RETV_OK)) {
			result = operationSetGalType(gal);
		} // if

		if (opErase && (result == RETV_OK)) {
			result = operationEraseGal();
		} // if

		if (result == RETV_OK) {
			if (opWrite) {
				result = operationWriteOrVerify(DO_WRITE); // writing fuses and optionally verification
			} else if (opInfo) {
				result = operationReadInfo();
			} else if (opRead) {
				result = operationReadFuses();
			} else if (opVerify) {
				result = operationWriteOrVerify(NO_WRITE); // verification without writing
			} else if (opTestVPP) {
				result = operationTestVpp();
			} else if (opWritePes) {
				result = operationWritePes();
			} // else if
			if ((result == RETV_OK) && (opWrite || opVerify)) {
				if (opSecureGal) {
					operationSecureGal();
				} // if
			} // if
			//variable VPP functions (for new board designs)
			if (varVppExists) {
				if ((result == RETV_OK) && opCalibrateVPP) {
					result = operationCalibrateVpp();
				} // if
				if ((result == RETV_OK) && opMeasureVPP) {
					result = operationMeasureVpp();
				} // if
			} // if
		} // if
	} // else

    if (verbose) {
        printf("result=%s\n", result ? "Error" : "OK!");
    }
    return result;
} // processJtag()
